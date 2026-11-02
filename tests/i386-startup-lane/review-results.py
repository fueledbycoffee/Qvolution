#!/usr/bin/env python3
from pathlib import Path
import csv, json, sys
E = Path(sys.argv[1])
problems = []
results = {}
for kind in ('baseline', 'candidate'):
    rows = list(csv.DictReader((E / (kind + '-results.tsv')).open(), delimiter='\t'))
    assert len(rows) == 8
    for row in rows:
        name, status = row['case'], int(row['status'])
        output = (E / (name + '.stdout')).read_text()
        error = (E / (name + '.stderr')).read_text()
        records = [line for line in error.splitlines() if line.startswith('[darling-i386] ')]
        results[name] = {'status': status, 'hello': 'i386-guest hello' in output, 'records': records}
        if (kind == 'candidate' or '-x86_64-' in name) and (status or 'i386-guest hello' not in output):
            problems.append(name + ': hello did not complete')
        expected_records = kind == 'candidate' and '-i386-option1-' in name
        if records and not expected_records:
            problems.append(name + ': unexpected startup records')
        if expected_records and '[darling-i386] mldr32-handoff-enter' not in records:
            problems.append(name + ': mldr32 handoff not reached')
        if expected_records:
            for stage in ('task-self-trap-return', 'reply-port-return', 'bootstrap-port-return', 'mig-reply-tsd-return', 'dyld-main-enter', 'dyld-root-present', 'dyld-simulator-open-enter', 'dyld-simulator-open-missing', 'dyld-simulator-check-return'):
                if '[darling-i386] ' + stage not in records:
                    problems.append(name + ': missing ' + stage)
        if expected_records and not any('[darling-i386] mldr32-root-' in r for r in records):
            problems.append(name + ': missing root origin record')
        if expected_records and '[darling-i386] dyld-simulator-enter' in records:
            problems.append(name + ': unexpectedly tried loading simulator dyld')
        if expected_records and '[darling-i386] libkernel-init-return' not in records:
            problems.append(name + ': libSystem kernel initialization not completed')
report = {'results': results, 'problems': problems, 'guest_startup_gate': not problems}
(E / 'review.json').write_text(json.dumps(report, indent=2) + '\n')
print('PASS' if not problems else 'NEEDS_REVIEW', str(E / 'review.json'))
raise SystemExit(bool(problems))
