#!/usr/bin/env python3
"""Execute production job_mig_register2 with controlled service ownership/state.
No Mach IPC or whole launchd coverage is claimed.
"""
import argparse
from pathlib import Path
import subprocess
import re
p=argparse.ArgumentParser();p.add_argument('--revision');p.add_argument('--out',type=Path,required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[1]
s=subprocess.check_output(['git','-C',str(r),'show',a.revision+':src/launchd/src/core.c'],text=True) if a.revision else (r/'src/launchd/src/core.c').read_text()
at=s.index('kern_return_t\njob_mig_register2(');end=s.index('\nkern_return_t',at+1);method=s[at:end]
fixture=r'''
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#define DARLING 1
#define HAVE_SANDBOX 0
#define unlikely(x) (x)
#define likely(x) (x)
#define job_log(...) ((void)0)
#define MACH_PORT_NULL 0
CONSTANTS
#define BOOTSTRAP_PER_PID_SERVICE 1
/* Numeric values are the vendored bootstrap.h interface, not inferred. */
typedef int kern_return_t, mach_port_t;
typedef const char *name_t;
typedef struct job { bool legacy_LS_job; void *mgr; bool active; } *job_t;
struct ldcred { int pid; };static struct ldcred cred={42};
struct machservice { job_t owner; bool active,recv; };
static struct machservice service;static bool exists;static int deletes,creates;
static struct ldcred *runtime_get_caller_creds(void){return &cred;}
static struct machservice *jobmgr_lookup_service(void *m,name_t n,bool b,int p){return exists?&service:0;}
static job_t machservice_job(struct machservice *m){return m->owner;}
static bool job_active(job_t j){return j->active;}
static bool machservice_active(struct machservice *m){return m->active;}
static void machservice_delete(job_t j,struct machservice *m,bool b){deletes++;exists=false;}
static void job_checkin(job_t j){}
static struct machservice *machservice_new(job_t j,name_t n,mach_port_t *p,bool b){creates++;return &service;}
static void machservice_request_notifications(struct machservice *m){}
METHOD
int main(void){
 struct job owner={.active=true},other={.active=true};int failed=0;
 struct {job_t who;bool active,recv;int port,expected,deleted,created;} cases[]={
  {&owner,true,false,0,BOOTSTRAP_NAME_IN_USE,0,0},
  {&owner,true,false,77,BOOTSTRAP_SERVICE_ACTIVE,0,0},
  {&owner,false,false,0,BOOTSTRAP_SUCCESS,1,0},
  {&owner,false,true,77,BOOTSTRAP_NOT_PRIVILEGED,0,0},
  {&other,true,false,0,BOOTSTRAP_NOT_PRIVILEGED,0,0},
  {&owner,false,false,77,BOOTSTRAP_SUCCESS,1,1},
 };
 for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++){
  service=(struct machservice){cases[i].who,cases[i].active,cases[i].recv};exists=true;deletes=creates=0;
  int got=job_mig_register2(&owner,"own-service",cases[i].port,0);
  printf("case %u: result=%d deletes=%d creates=%d\n",i,got,deletes,creates);
  if(got!=cases[i].expected||deletes!=cases[i].deleted||creates!=cases[i].created)failed++;
 }
 return failed?1:0;
}
'''.replace('METHOD',method)
header=(r/'src/launchd/liblaunch/bootstrap.h').read_text()
constants='\n'.join(re.findall(r'^#define\s+BOOTSTRAP_(?:SUCCESS|NOT_PRIVILEGED|NAME_IN_USE|SERVICE_ACTIVE|NO_MEMORY)\s+\d+',header,re.M))
assert len(constants.splitlines())==5
fixture=fixture.replace('CONSTANTS',constants)
a.out.mkdir(parents=True,exist_ok=True);(a.out/'bootstrap.c').write_text(fixture)
subprocess.run(['/usr/bin/clang','-Wall','-Wno-parentheses',str(a.out/'bootstrap.c'),'-o',str(a.out/'bootstrap')],check=True)
raise SystemExit(subprocess.run([str(a.out/'bootstrap')]).returncode)
