#!/usr/bin/env python3
"""Exercise real cp -al/private replacement using authored files; retain evidence."""
from pathlib import Path
import json
import os
import tempfile
from stage_farm import L, MANIFEST, IMAGES, create, digest, replace, verify

root = Path(tempfile.mkdtemp(prefix='farm-regression-', dir=L / 'evidence'))
base, lane, evidence = (root / name for name in ('base', 'lane', 'review'))
for path in (base, lane, evidence):
    path.mkdir()
(base / MANIFEST).write_text('{"authored_fixture": true}\n')
for name in IMAGES:
    path = base / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text('original fixture: ' + name + '\n')
    path.chmod(0o755)
(base / 'symlink').symlink_to('usr', target_is_directory=True)
before = {name: digest(base / name) for name in (MANIFEST, *IMAGES)}
stage = create(evidence, base, lane)
second_evidence = root / 'second-review'
second_evidence.mkdir()
second = create(second_evidence, base, lane)
assert stage != second
assert (stage / 'symlink').is_symlink()
for name in IMAGES:
    assert (stage / name).samefile(base / name)
    assert (stage / name).stat().st_nlink == 3
files = {}
for index, name in enumerate(IMAGES):
    source = root / ('candidate-' + str(index))
    source.write_text('replacement fixture: ' + name + '\n')
    files[name] = source
replace(evidence, files, base, lane)
verify(evidence, True, base, lane)
verify(second_evidence, False, base, lane)
assert {name: digest(base / name) for name in before} == before

def rejects(label, operation):
    try:
        operation()
    except AssertionError as error:
        print('PASS rejection:', label, str(error))
    else:
        raise AssertionError('failed to reject: ' + label)

manifest = (base / MANIFEST).read_bytes()
(base / MANIFEST).write_text('deliberately modified fixture\n')
rejects('source manifest modification', lambda: verify(evidence, True, base, lane))
(base / MANIFEST).write_bytes(manifest)
target = stage / IMAGES[0]
candidate = target.read_bytes()
target.write_text('deliberately modified candidate\n')
rejects('candidate modification', lambda: verify(evidence, True, base, lane))
target.write_bytes(candidate)
os.link(target, root / 'unexpected-shared-candidate')
rejects('non-private candidate', lambda: verify(evidence, True, base, lane))
report = {'fixture': str(root), 'unique_farms': [str(stage), str(second)],
          'source_sha256': before, 'private_replacements': len(files),
          'negative_checks': ['source manifest', 'candidate hash', 'candidate link count']}
(root / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
print('PASS: rootless hardlink creation and private replacement;', root / 'result.json')
