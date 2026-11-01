#!/usr/bin/env python3
"""Native test of unchanged production CAClock.c with a scalar ABI boundary.
This is not a guest AudioToolbox load; only the legacy clock unit is loaded.
Invoke under qv-slot build. Pass a lane-owned output directory as argument.
"""
from pathlib import Path
import subprocess, sys
root=Path(__file__).resolve().parents[4]
out=Path(sys.argv[1]).resolve();out.mkdir(parents=True,exist_ok=True)
shim=out/'include/CoreAudio';shim.mkdir(parents=True,exist_ok=True)
# Public typedef widths and error from vendored MacTypes/CoreAudioTypes.h.
# Avoid bringing Darwin system headers into a Linux-native test.
(shim/'CoreAudioTypes.h').write_text('#include <stdint.h>\ntypedef uint32_t UInt32;\ntypedef int32_t OSStatus;\nenum { kAudio_UnimplementedError = -4 };\n')
args=['clang','-g','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(out/'include'),'-I'+str(root/'src/CoreAudio/include')]
def run(a):subprocess.run(a,check=True)
run(args+['-shared','-fPIC',str(root/'src/CoreAudio/AudioToolbox/CAClock.c'),'-o',str(out/'clock.so')])
run(args+[str(Path(__file__).with_suffix('.c')),'-ldl','-o',str(out/'clock-native')])
run([str(out/'clock-native'),str(out/'clock.so')])
