#!/usr/bin/env python3
"""Run real implementation units with controlled GNU ObjC/Foundation doubles.
This checks our methods, not Darling's Mach-O loader or Foundation runtime.
"""
from pathlib import Path
import argparse, subprocess
p = argparse.ArgumentParser()
p.add_argument('kind', choices=['gamepad', 'haptic'])
p.add_argument('output', type=Path)
a = p.parse_args()
f = Path(__file__).resolve().parents[2]
fixture = Path(__file__).resolve().parent / 'native'
a.output.mkdir(parents=True, exist_ok=True)
if a.kind == 'gamepad':
    modern = a.output / 'modern.m'
    # Replace only the umbrella import to keep unrelated frameworks out of the fixture.
    imports = '\n'.join('#import <GameController/'+n+'.h>' for n in ['GCDualShockGamepad','GCDualSenseGamepad','GCXboxGamepad','GCHaptics'])
    modern.write_text((f/'GameController/src/GCModernGamepads.m').read_text().replace('#import <GameController/GameController.h>', imports))
    sources = [f/'GameController/src/GCExtendedGamepad.m', modern, f/'GameController/tests/getters.m']
else:
    sources = [f/'CoreHaptics/src/CoreHaptics.m', f/'CoreHaptics/tests/pattern.m']
exe = a.output / a.kind
subprocess.run(['clang','-g','-fobjc-runtime=gcc','-fconstant-string-class=NativeConstantString',
    '-fsanitize=address,undefined','-I/usr/lib/gcc/x86_64-linux-gnu/15/include',
    '-I'+str(fixture), '-I'+str(f/'GameController/include'), '-I'+str(f/'CoreHaptics/include'),
    str(fixture/'runtime.m'), *map(str,sources), '-lobjc','-o',str(exe)], check=True)
subprocess.run([str(exe)], check=True)
