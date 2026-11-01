#!/usr/bin/env python3
"""Print run.py job arguments. jobs.py SET RENDERER
  textedit  the 71 Stage41c3 TextEdit suite fixtures
  theme     theme-p4 (appkit-control-theme) and theme-p5 (appkit-scroller-menu-theme)"""
import json,sys
from pathlib import Path
W=Path('/home/sean/Documents/Codex/Projects/Qvolution-work');F=W/'parallel/stage41c/fixtures/stage41c3'
which,renderer=sys.argv[1],sys.argv[2]
fx=json.loads((F/'suite-fixtures.json').read_text())
assert len(fx['textedit']) == 71, 'TextEdit gate must contain exactly 71 fixtures'
jobs=[]
if which=='textedit':
 for c in fx['textedit']:
  jobs.append('|'.join([c['label']+'-'+renderer,renderer,str(c['timeout']+60),' '.join(['/Volumes/SystemRoot'+c['binary'],*c['args']])]))
elif which=='theme':
 jobs.append('|'.join(['theme-p4-'+renderer,renderer,'120','/Volumes/SystemRoot'+str(F/'appkit-control-theme'),'QV_RUN_REQUIRE=^RESULT failures=0$']))
 jobs.append('|'.join(['theme-p5-'+renderer,renderer,'150','/Volumes/SystemRoot'+str(F/'appkit-scroller-menu-theme'),'DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent-theme-p5-bus,QV_RUN_REQUIRE=^RESULT appkit-scroller-menu-theme failures=0']))
else:
 raise SystemExit('unknown job set: '+which)
print('\n'.join(jobs))
