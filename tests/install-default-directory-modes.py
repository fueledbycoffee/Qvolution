#!/usr/bin/env python3
"""CMake file(INSTALL) regression for b823b47dbb; run under a build slot.
All fixtures live under the supplied output directory and are preserved.
"""
from pathlib import Path
import json, os, re, subprocess, sys
root=Path(__file__).resolve().parents[1]; out=Path(sys.argv[1]).resolve(); out.mkdir(parents=True,exist_ok=True)
text=(root/'CMakeLists.txt').read_text()
setting=re.search(r'set\(CMAKE_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS\s+[^)]+\)',text).group()
(out/'payload').write_text('source-built fixture\n')
result={}
for label,config in [('before',''),('after',setting)]:
    folder=out/label; script=out/(label+'.cmake')
    script.write_text(config+f'\nfile(INSTALL "{out}/payload" DESTINATION "{folder}/System/Library/LaunchDaemons")\n')
    subprocess.run(['cmake','-P',str(script)],check=True,umask=0o002)
    mode=(folder/'System/Library/LaunchDaemons').stat().st_mode&0o777
    result[label]=oct(mode)
    assert mode == (0o775 if label=='before' else 0o755),result
# Existing paths are deliberately not repaired by the CMake setting.
old=out/'existing/System/Library/LaunchDaemons'; old.mkdir(parents=True,mode=0o775); old.chmod(0o775)
script=out/'existing.cmake'; script.write_text(setting+f'\nfile(INSTALL "{out}/payload" DESTINATION "{old}")\n')
subprocess.run(['cmake','-P',str(script)],check=True,umask=0o002)
result['existing']=oct(old.stat().st_mode&0o777); assert result['existing']=='0o775',result
(out/'results.json').write_text(json.dumps(result,indent=2)+'\n'); print(result)
