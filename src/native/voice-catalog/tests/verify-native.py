from pathlib import Path
import argparse
import json
import os
import struct
import subprocess
import urllib.parse

here = Path(__file__).resolve().parent
parser = argparse.ArgumentParser()
parser.add_argument("--build", type=Path, default=here / "native-build")
parser.add_argument("--summary", type=Path)
args = parser.parse_args()
build = args.build.resolve()


def decode(data):
    at = 0

    def number():
        nonlocal at
        assert at + 4 <= len(data)
        result = struct.unpack_from("<I", data, at)[0]
        at += 4
        return result

    def string():
        nonlocal at
        length = number()
        assert length <= 4096 and at + length <= len(data)
        result = data[at:at + length].decode("utf-8", "strict")
        assert "\x00" not in result
        at += length
        return result

    assert len(data) <= 4 * 1024 * 1024 and data[:4] == b"QVVC"
    at = 4
    assert number() == 1
    status = number()
    assert number() == len(data)
    count = number()
    assert count <= 4096 and string() == "espeak-ng"
    records = []
    for _ in range(count):
        gender, languages, primary = number(), number(), number()
        assert gender <= 3 and languages <= 64
        identifier, project, name = string(), string(), string()
        assert project == "org.qvolution.voice.v1/espeak-ng/" + urllib.parse.quote(identifier, safe="-._~")
        entries = []
        for _ in range(languages):
            priority, raw, canonical = number(), string(), string()
            assert 1 <= priority <= 255 and raw
            entries.append({"priority": priority, "raw": raw, "canonical": canonical})
        mapped = [i for i in range(languages) if entries[i]["canonical"]]
        expected = min(mapped, key=lambda i: (entries[i]["priority"], entries[i]["canonical"])) if mapped else 2 ** 32 - 1
        assert primary == expected
        records.append({"identifier": identifier, "project": project, "name": name, "gender": gender, "languages": entries})
    assert at == len(data) and (not status or not records)
    assert len({v["identifier"] for v in records}) == count
    assert records == sorted(records, key=lambda v: v["identifier"].encode())
    return status, records


def run(executable, mode=None):
    env = dict(os.environ)
    if mode is not None:
        env["QV_TEST_PROVIDER_CASE"] = mode
    result = subprocess.run([str(build / executable)], env=env, capture_output=True, timeout=15)
    status, records = decode(result.stdout)
    assert result.returncode == status, (result.returncode, result.stderr)
    return status, records


subprocess.run([str(build / "frozen-native-test")], check=True)
for mode, expected in [("valid", 0), ("empty", 0), ("init-error", 2), ("list-error", 2),
                       ("duplicate", 3), ("malformed", 3), ("terminate-error", 2)]:
    status, records = run("fault-catalog", mode)
    assert status == expected, (mode, status)
    if mode == "valid":
        assert len(records) == 1 and records[0]["name"] == "Fixture é"
        assert records[0]["languages"][0]["canonical"] == "en-GB"
        assert records[0]["languages"][2]["canonical"] == ""
    if mode == "empty":
        assert records == []
assert run("unavailable-catalog") == (1, [])
print("PASS: controlled provider lifecycle/errors/empty/malformed/ownership; controls are not provider evidence")

reference = subprocess.run([str(build / "independent-provider")], capture_output=True, timeout=15)
if reference.returncode == 77:
    print("UNAVAILABLE: independent provider absent; no positive catalog qualification")
    raise SystemExit(77)
assert reference.returncode == 0, (reference.returncode, reference.stderr)
raw = json.loads(reference.stdout)
status, actual = run("qv-voice-catalog")
assert status == 0


def unhex(value):
    return bytes.fromhex(value).decode("utf-8")


expected = {unhex(v["identifier"]): {"name": unhex(v["name"]), "gender": v["gender"],
            "languages": [{"priority": e["priority"], "raw": unhex(e["raw"])} for e in v["languages"]]} for v in raw}
observed = {v["identifier"]: {"name": v["name"], "gender": v["gender"],
            "languages": [{"priority": e["priority"], "raw": e["raw"]} for e in v["languages"]]} for v in actual}
assert expected == observed
if not actual:
    print("EMPTY_PROVIDER: true empty catalog; no positive populated-provider qualification")
    raise SystemExit(77)
print(f"PASS: independent public provider API agrees on all {len(actual)} real voice identities/names/genders/language priorities")
unmapped = sorted({e["raw"] for v in actual for e in v["languages"] if not e["canonical"]})
print("Explicit unsupported language labels:", json.dumps(unmapped))
if args.summary:
    args.summary.write_text(json.dumps({"count": len(actual), "unsupportedLanguages": unmapped,
        "voices": actual}, ensure_ascii=False, indent=2) + "\n")
