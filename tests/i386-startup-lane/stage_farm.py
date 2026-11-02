#!/usr/bin/env python3
"""Create and verify a disposable Stage41d hardlink farm without privileges."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import sys
import tempfile

L = Path('/home/sean/Documents/Codex/Projects/Qvolution-work/parallel/i386-guest')
BASE = L.parents[1] / 'stage41d'
MANIFEST = 'qvolution-runtime-source.json'
IMAGES = (
    'usr/local/libexec/darling/bin/mldr',
    'usr/local/libexec/darling/usr/libexec/darling/mldr',
    'usr/local/libexec/darling/usr/libexec/darling/mldr32',
    'usr/local/libexec/darling/usr/lib/dyld',
    'usr/local/libexec/darling/usr/lib/system/libsystem_kernel.dylib',
)


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def create(evidence, base=BASE, lane=L):
    assert base.resolve() == base and lane.resolve() == lane
    before = {name: digest(base / name) for name in (MANIFEST, *IMAGES)}
    stage = Path(tempfile.mkdtemp(prefix='stage-hardlinks-', dir=lane))
    record = {'base': str(base), 'stage': str(stage), 'source_sha256': before}
    (evidence / 'stage-farm.json').write_text(json.dumps(record, indent=2) + '\n')
    subprocess.run(['cp', '-al', str(base) + '/.', str(stage)], check=True)
    count = 0
    for root, dirs, files in os.walk(base, followlinks=False):
        for name in files:
            source = Path(root) / name
            if source.is_symlink():
                continue
            target = stage / source.relative_to(base)
            assert source.is_file() and not target.is_symlink()
            assert source.samefile(target) and target.stat().st_nlink >= 2, str(target)
            count += 1
    record['shared_regular_files'] = count
    record['initial_image_links'] = {name: (base / name).stat().st_nlink for name in IMAGES}
    (evidence / 'stage-farm.json').write_text(json.dumps(record, indent=2) + '\n')
    verify(evidence, False, base, lane)
    return stage


def verify(evidence, installed, base=BASE, lane=L):
    record = json.loads((evidence / 'stage-farm.json').read_text())
    stage = Path(record['stage'])
    assert record['base'] == str(base)
    assert stage.parent == lane and stage.name.startswith('stage-hardlinks-')
    assert stage.resolve() == stage and stage.is_dir()
    for name, expected in record['source_sha256'].items():
        assert digest(base / name) == expected, 'source stage changed: ' + name
    assert (base / MANIFEST).samefile(stage / MANIFEST)
    images = json.loads((evidence / 'installed-candidate.json').read_text()) if installed else {}
    for name in IMAGES:
        source, target = base / name, stage / name
        assert target.resolve() == target and target.is_file(), str(target)
        if installed:
            assert not source.samefile(target) and target.stat().st_nlink == 1, str(target)
            assert digest(target) == images[name]['sha256'], 'candidate changed: ' + name
        else:
            assert source.samefile(target) and target.stat().st_nlink >= 2, str(target)
    return record


def replace(evidence, files, base=BASE, lane=L):
    record = verify(evidence, False, base, lane)
    assert set(files) == set(IMAGES)
    stage = Path(record['stage'])
    result = {}
    for name, source in files.items():
        target, original = stage / name, base / name
        links = original.stat().st_nlink
        mode = original.stat().st_mode & 0o777
        subprocess.run(['cp', '--remove-destination', str(source), str(target)], check=True)
        target.chmod(mode)
        assert target.stat().st_nlink == 1 and not target.samefile(source)
        assert not target.samefile(original)
        assert original.stat().st_nlink == links - 1, 'source link count changed unexpectedly'
        assert digest(target) == digest(source)
        result[name] = {'source': str(source), 'sha256': digest(source),
                        'base_sha256': record['source_sha256'][name],
                        'private_links': target.stat().st_nlink,
                        'base_links_before': links, 'base_links_after': original.stat().st_nlink}
    (evidence / 'installed-candidate.json').write_text(json.dumps(result, indent=2) + '\n')
    verify(evidence, True, base, lane)


if __name__ == '__main__':
    action, directory = sys.argv[1:]
    evidence = Path(directory).resolve(strict=True)
    if action == 'create':
        print(create(evidence))
    elif action == 'verify':
        verify(evidence, (evidence / 'installed-candidate.json').exists())
        print('PASS: source manifest/images unchanged; farm link counts and hashes verified')
    else:
        raise SystemExit('usage: stage_farm.py create|verify EVIDENCE')
