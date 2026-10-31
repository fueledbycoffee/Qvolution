#!/usr/bin/env python3
"""Render the evidence-backed conversation tracker; no network or runtime activity."""
import argparse
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
parser = argparse.ArgumentParser()
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
catalog = json.loads((HERE / 'apps.json').read_text())
apps = catalog['apps']
progress = json.loads((HERE / 'verification/current-progress.json').read_text())
assert len(apps) == catalog['target_count'] and len({app['id'] for app in apps}) == catalog['target_count']
qualified = set(progress['qualified_app_ids'])
assert qualified <= {app['id'] for app in apps}
items = []
for app in apps:
    source = app.get('tested_variant') == 'Darling source fixture'
    official = app.get('tested_variant') == 'Official unmodified macOS vendor bundle'
    if app['id'] in qualified:
        assert official and all(value == 'pass' for value in app['quality_gates'].values())
    if source:
        category, label = 'source', 'Source fixture · partial workflow'
    elif app['status'].startswith('blocked'):
        category, label = 'blocked', 'Startup blocked'
    elif app['status'].startswith('partial'):
        category, label = 'partial', 'Partial startup / render'
    else:
        category, label = 'pending', 'Not tried'
    gates = {}
    for key, title in [('workflow', 'Workflow'), ('ui_ux', 'UI / UX'), ('gpu', 'GPU'), ('stability', 'Stability')]:
        value = app['quality_gates'][key]
        gates[title] = 'Passed' if value == 'pass' else 'Partial · source' if value == 'partial_source_fixture' else 'Not qualified' if value == 'not_qualified' else 'Not verified' if value == 'not_verified' else 'Not tested'
    default = 'Official input, architecture check and first launch still required.'
    if app['id'] in ['terminal', 'stickies', 'calculator', 'preview', 'chess']:
        default = 'Legitimate Mac app bundle and first workflow test still required.'
    if app['id'] == 'dia':
        default = 'Official input and ARM64 execution path remain unverified.'
    items.append(dict(id=app['id'], name=app['name'], category=category, statusLabel=label,
                      version='Darling source TextEdit' if source else app.get('tested_version'),
                      checkpoint=progress['app_checkpoints'].get(app['id'], 'No app run recorded'),
                      blocker=progress['blockers'].get(app['id'], default), gates=gates,
                      workflow=app['workflow']))
data = dict(apps=items, updated=progress['updated_utc'], stage=progress['stage'],
            qualified=len(qualified), officialAttempted=sum(app.get('tested_variant') == 'Official unmodified macOS vendor bundle' for app in apps),
            checkCount=sum(progress['framework_checks'].values()), checkGroups=progress['framework_checks'],
            builtSource=progress['built_source'], workstreams=progress['workstreams'],
            userFeatures=progress['verified_user_features'], openUI=progress['open_ui'],
            cliCycles=progress['cli_cycles'], transportChecks=progress['transport_checks'],
            pngVectors=progress['png_pixel_vectors'], windowPixels=progress['window_pixel_artifacts'],
            sourceURL=progress['public_component_url'].split('/tree/')[0] + '/tree/' + progress['public_component_commit'],
            suiteStage=progress['framework_suite_stage'], latestChecks=progress['latest_checks'],
            independentScope=progress['independent_scope'], sourceLabel=progress['source_label'])
template = (HERE / 'progress-template.html').read_text()
assert template.count('__QV_DATA__') == 1
payload = json.dumps(data, ensure_ascii=False, separators=(',', ':')).replace('</', '<\\/')
fragment = template.replace('__QV_DATA__', payload)
assert len(fragment.encode()) < 1_000_000
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(fragment)
print(f"Rendered {len(items)} apps, {data['checkCount']} framework checks, {data['qualified']} qualified")
