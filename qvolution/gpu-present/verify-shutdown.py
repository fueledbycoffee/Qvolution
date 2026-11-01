#!/usr/bin/env python3
"""Check runner shutdown commands without booting a container or invoking a launcher."""
import ast
from pathlib import Path
import tempfile
from types import SimpleNamespace

root = Path(__file__).resolve().parent
W = Path('/home/sean/Documents/Codex/Projects/Qvolution-work')
helper = str(W/'dashboard/qv-shutdown')
launcher = Path('/authored/non-setuid-launcher')
calls = []
def run(command, **kwargs):
    calls.append(command)
    assert command[-2:] == [helper, str(launcher)], command
    assert kwargs['env']['DPREFIX'] == '/authored/prefix'
    return SimpleNamespace(returncode=0, stdout='', stderr='')

with tempfile.TemporaryDirectory(prefix='qv-r4-shutdown-') as directory:
    tree = ast.parse((root/'run.py').read_text())
    functions = [n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == 'shutdown']
    for timing in (False, True):
        namespace = dict(W=W, LAUNCHER=launcher, timing=timing,
            env={'DPREFIX':'/authored/prefix'}, run=Path(directory),
            subprocess=SimpleNamespace(run=run),
            json=__import__('json'), uuid=__import__('uuid'), time=__import__('time'))
        # Old shutdown calls launch_command; keep this to demonstrate its failure.
        namespace['launch_command'] = lambda args: [str(launcher), *args]
        exec(compile(ast.Module(body=functions, type_ignores=[]), str(root/'run.py'), 'exec'), namespace)
        assert namespace['shutdown']() == 0
        assert calls[-1] == ([str(W/'dashboard/qv-slot'), 'guest'] if timing else []) + [helper, str(launcher)]
    # measure.py runs inside perf; verify it cannot nest another slot on timeout.
    tree = ast.parse((root/'measure.py').read_text())
    shutdowns = [n for n in ast.walk(tree) if isinstance(n, ast.Call) and
                 isinstance(n.func, ast.Attribute) and n.func.attr == 'run']
    assert len(shutdowns) == 1
    command = eval(compile(ast.Expression(shutdowns[0].args[0]), '<measure shutdown>', 'eval'),
                   {'W':W, 'launcher':str(launcher), 'str':str})
    assert command == [helper, str(launcher)], command
print('PASS: runner uses exact-prefix qv-shutdown; timing cleanup acquires guest once; perf timeout never nests slots')
