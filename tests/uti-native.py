#!/usr/bin/env python3
"""Run production UTI queries/methods with native libobjc and SQLite controls."""
import argparse
from pathlib import Path
import plistlib
import sqlite3
import subprocess
p=argparse.ArgumentParser();p.add_argument('--revision');p.add_argument('--out',type=Path,required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[1];a.out.mkdir(parents=True,exist_ok=True)
def read(path):
 return subprocess.check_output(['git','-C',str(r),'show',a.revision+':'+path],text=True) if a.revision else (r/path).read_text()
s=read('src/frameworks/CoreServices/src/LaunchServices/UTType.m');at=s.index('Boolean\nUTTypeConformsTo(');end=s.index('\n_Nullable',at);(a.out/'uti-conforms.inc').write_text(s[at:end])
s=read('src/frameworks/CoreServices/src/LaunchServices/launchservicesd/LSBundle.m');at=s.index('-(BOOL)process\n');end=s.index('\n+(BOOL)',at);(a.out/'uti-process.inc').write_text(s[at:end])
s=read('src/frameworks/CoreServices/Info.plist');s=s[:s.index('</plist>')+len('</plist>')];info=plistlib.loads(s.encode())
dbpath=a.out/'types.sqlite';db=sqlite3.connect(dbpath)
db.executescript('DROP TABLE IF EXISTS uti_conforms; DROP TABLE IF EXISTS uti_tag; DROP TABLE IF EXISTS uti; DROP TABLE IF EXISTS globals; DROP TABLE IF EXISTS bundle; DROP TABLE IF EXISTS uti_icon; DROP TABLE IF EXISTS app_doc; DROP TABLE IF EXISTS app_doc_uti; DROP TABLE IF EXISTS app_doc_mime; DROP TABLE IF EXISTS app_doc_extension; DROP TABLE IF EXISTS bundle_url_type; DROP TABLE IF EXISTS bundle_url_type_scheme;')
db.executescript(read('src/frameworks/CoreServices/src/LaunchServices/launchservicesd/schema.sql'))
# Populate documented vendored declarations, plus an authored cyclic extension graph.
for d in info.get('UTImportedTypeDeclarations',[])+info.get('UTExportedTypeDeclarations',[])+[{'UTTypeIdentifier':'own.cycle-a','UTTypeConformsTo':'own.cycle-b'},{'UTTypeIdentifier':'own.cycle-b','UTTypeConformsTo':'own.cycle-a'}]:
 row=db.execute('INSERT INTO uti(type_identifier,description) VALUES(?,?)',(d['UTTypeIdentifier'],d.get('UTTypeDescription',''))).lastrowid
 parents=d.get('UTTypeConformsTo',[]);parents=[parents] if isinstance(parents,str) else parents
 for parent in parents:db.execute('INSERT INTO uti_conforms(uti,conforms_to) VALUES(?,?)',(row,parent))
db.commit();db.close()
blocks='/home/sean/Documents/Codex/Projects/Qvolution-work/toolchains/swift-6.3.3-linux-x64/compiler/usr/lib/swift/linux'
cc=['/usr/bin/clang','-fblocks','-fobjc-exceptions','-fexceptions','-fconstant-string-class=NativeConstantString','-I'+str(a.out),'-I'+subprocess.check_output(['gcc','-print-file-name=include'],text=True).strip(),str(r/'tests/uti-native.m'),'-l:libobjc.so.4','-lsqlite3','-L'+blocks,'-Wl,-rpath,'+blocks,'-lBlocksRuntime','-o',str(a.out/'uti-native')]
subprocess.run(cc,check=True)
raise SystemExit(subprocess.run([str(a.out/'uti-native'),str(dbpath)]).returncode)
