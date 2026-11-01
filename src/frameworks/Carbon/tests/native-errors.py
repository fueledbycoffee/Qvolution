#!/usr/bin/env python3
"""Load an ELF build of the production Carbon error fallbacks on Linux.
The adapter supplies types only; error constants come from vendored MacErrors.h.
This verifies eight C exports/behaviors, not the Mach-O framework's loader.
"""
from pathlib import Path
import ctypes, subprocess, tempfile
root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='carbon-import-errors-') as tmp:
    tmp = Path(tmp)
    (tmp/'Carbon').mkdir(); (tmp/'CoreFoundation').mkdir()
    (tmp/'CoreFoundation/CFBase.h').write_text('')
    types = '#include <stdint.h>\ntypedef int32_t OSStatus; typedef uint8_t UInt8; typedef uint32_t UInt32, OSType, TSMDocumentID; typedef const void *CFArrayRef; typedef struct AppleEvent AppleEvent;\n'
    (tmp/'MacTypes.h').write_text(types)
    (tmp/'Carbon/Carbon.h').write_text('#include <stddef.h>\n#include <CarbonCore/MacErrors.h>\n')
    shared = tmp/'Carbon-errors.so'
    theme = (root/'HIToolbox/src/HITheme.c').read_text().split('OSStatus HIThemeBeginFocus',1)[1]
    (tmp/'theme.c').write_text('#include <Carbon/Carbon.h>\ntypedef void *CGContextRef; typedef UInt32 HIThemeFocusOptions;\nOSStatus HIThemeBeginFocus'+theme)
    subprocess.run(['cc','-shared','-fPIC','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-I'+str(tmp),'-I'+str(root.parent/'CoreServices/include'),str(root/'src/ImportCompatibility.c'),str(tmp/'theme.c'),'-o',str(shared)],check=True)
    lib = ctypes.CDLL(str(shared))
    symbols = ['AEProcessEvent','LMGetKbdLast','CopySymbolicHotKeys','GetCurrentEventButtonState','TSMSetDocumentProperty','TSMRemoveDocumentProperty','HIThemeBeginFocus','HIThemeEndFocus']
    for symbol in symbols: getattr(lib,symbol)
    lib.AEProcessEvent.argtypes=[ctypes.c_void_p];lib.AEProcessEvent.restype=ctypes.c_int32
    assert lib.AEProcessEvent(None) == -50
    assert lib.AEProcessEvent(ctypes.c_void_p(1)) == -1708
    lib.CopySymbolicHotKeys.argtypes=[ctypes.POINTER(ctypes.c_void_p)]
    assert lib.CopySymbolicHotKeys(None) == -50
    keys = ctypes.c_void_p(1)
    assert lib.CopySymbolicHotKeys(ctypes.byref(keys)) == -4 and keys.value is None
    lib.LMGetKbdLast.restype=ctypes.c_uint8
    lib.GetCurrentEventButtonState.restype=ctypes.c_uint32
    assert lib.LMGetKbdLast() == 0 and lib.GetCurrentEventButtonState() == 0
    lib.TSMSetDocumentProperty.argtypes=[ctypes.c_uint32,ctypes.c_uint32,ctypes.c_uint32,ctypes.c_void_p]
    assert lib.TSMSetDocumentProperty(1,0,1,ctypes.c_void_p(1)) == -2505
    assert lib.TSMRemoveDocumentProperty(1,0) == -2505
    lib.HIThemeBeginFocus.argtypes=[ctypes.c_void_p,ctypes.c_uint32,ctypes.c_void_p]
    assert lib.HIThemeBeginFocus(None,0,None) == -50
    assert lib.HIThemeBeginFocus(ctypes.c_void_p(1),0,ctypes.c_void_p(1)) == -4
    lib.HIThemeEndFocus.argtypes=[ctypes.c_void_p]
    assert lib.HIThemeEndFocus(None) == -50
    assert lib.HIThemeEndFocus(ctypes.c_void_p(1)) == -4
    print('PASS native dlopen/dlsym: eight production Carbon fallback exports; invalid inputs and unsupported services')
