"""Native readiness contract for the actual spawnInitProcess source function.
Filesystem, runtime selection and privilege helpers are controlled stubs; no guest
or built runtime is required. Generated source and executables stay temporary.
"""
import argparse,ctypes,json,os,select,shlex,signal,subprocess,tempfile,time
from pathlib import Path
# Linux prctl.h: make only this test process the reaper for its own controls.
PR_SET_CHILD_SUBREAPER=36
signal.signal(signal.SIGCHLD,signal.SIG_DFL)
libc=ctypes.CDLL(None,use_errno=True)
if libc.prctl(PR_SET_CHILD_SUBREAPER,1,0,0,0):
    raise OSError(ctypes.get_errno(),'cannot supervise native controls')
def owned_children():
    return [int(pid) for pid in Path('/proc/self/task/'+str(os.getpid())+'/children').read_text().split()]
def reap_controls(deadline):
    while True:
        try:
            while os.waitpid(-1,os.WNOHANG)[0]: pass
        except ChildProcessError:
            return True
        if time.monotonic()>=deadline:return False
        time.sleep(0.005)
def stop_controls():
    deadline=time.monotonic()+2
    while owned_children() and time.monotonic()<deadline:
        for child in owned_children():
            try:os.kill(child,signal.SIGKILL)
            except ProcessLookupError:pass
        reap_controls(min(deadline,time.monotonic()+0.05))
    return not owned_children()
def run_control(argv,timeout=10,env=None,cwd=None,ready_marker=None,finalize=True):
    with tempfile.TemporaryFile() as output,tempfile.TemporaryFile() as errors:
        process=subprocess.Popen(argv,stdout=output,stderr=errors,start_new_session=True,env=env,cwd=cwd)
        expired=False;cleaned=True;ready_seen=ready_marker is None
        if ready_marker is not None:
            deadline=time.monotonic()+2
            while time.monotonic()<deadline:
                ready_seen=ready_marker in os.pread(output.fileno(),1000001,0)
                if ready_seen:break
                if process.poll() is not None:
                    ready_seen=ready_marker in os.pread(output.fileno(),1000001,0)
                    break
                time.sleep(0.005)
        try:process.wait(timeout=timeout if ready_seen else 0)
        except subprocess.TimeoutExpired:
            expired=True
            # This fresh group contains only the authored controller/stubs.
            try:os.killpg(process.pid,signal.SIGKILL)
            except ProcessLookupError:pass
            try:process.wait(timeout=1)
            except subprocess.TimeoutExpired:cleaned=False
            # Actual launcher children call setsid; after their controller dies,
            # the subreaper owns them directly. Never inspect or signal host peers.
            cleaned=stop_controls() and cleaned
        strays=bool(owned_children())
        if finalize and strays:cleaned=stop_controls() and cleaned
        output.seek(0);errors.seek(0)
        stdout=output.read(1000001);stderr=errors.read(1000001)
        overflow=len(stdout)>1000000 or len(stderr)>1000000
        result=subprocess.CompletedProcess(argv,124 if expired else process.returncode,
            stdout[:1000000].decode(errors='replace'),stderr[:1000000].decode(errors='replace'))
        return result,expired,cleaned and not overflow,ready_seen,strays

root=Path(__file__).resolve().parent.parent
parser=argparse.ArgumentParser()
parser.add_argument('--source-revision',help='Run spawnInitProcess from this revision')
args=parser.parse_args()
source=subprocess.check_output(['git','-C',str(root),'show',args.source_revision+':src/startup/darling.c'],text=True) if args.source_revision else (root/'src/startup/darling.c').read_text()
start=source.index('pid_t spawnInitProcess(void)\n{')
function=source[start:source.index('\nvoid putInitPid(',start)]
code=r'''
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
static int readMode,readCalls;
static ssize_t controlledRead(int fd,void *value,size_t size){
    ++readCalls;
    if(readMode==1 && readCalls==1){errno=EINTR;return -1;}
    if(readMode==2){errno=EIO;return -1;}
    return read(fd,value,size);
}
#define read controlledRead
#include "READY_HEADER"
static bool g_nonroot=true,g_rootless=true,g_fixPermissions=false;
static uid_t g_originalUid;
static gid_t g_originalGid;
static char *prefix,*install;
static void restrictEnvironmentForDarlingserver(void){}
static const char *getInstallPrefix(void){return install;}
PRODUCTION_FUNCTION
int main(int argc,char **argv){
    if(argc!=4)return 2;
    if(!strcmp(argv[3],"timeout-control") || !strcmp(argv[3],"orphan-control")){
        int ack[2];if(pipe(ack))return 90;
        pid_t blocked=fork();if(blocked<0)return 91;
        if(blocked==0){close(ack[0]);if(setsid()<0)return 92;if(write(ack[1],".",1)!=1)return 93;for(;;)pause();}
        close(ack[1]);char byte;if(read(ack[0],&byte,1)!=1 || byte!='.')return 94;
        puts("TIMEOUT_CHILD_READY");fflush(stdout);if(!strcmp(argv[3],"orphan-control"))return 0;for(;;)pause();
    }
    readMode=!strcmp(argv[3],"eintr")?1:!strcmp(argv[3],"error")?2:0;
    g_originalUid=getuid();g_originalGid=getgid();prefix=argv[1];install=argv[2];
    pid_t child=spawnInitProcess();
    printf("PROCEEDED_AFTER_READY read_calls=%d\n",readCalls);
    int status;waitpid(child,&status,0);
    return WIFEXITED(status)&&WEXITSTATUS(status)==0?0:3;
}
'''.replace('READY_HEADER',str(root/'src/startup/startup-ready.h')).replace('PRODUCTION_FUNCTION',function)
records=[]
with tempfile.TemporaryDirectory(prefix='qv-launcher-tests-') as directory:
    directory=Path(directory);native=directory/'control.c';native.write_text(code)
    binary=directory/'control'
    subprocess.run(shlex.split(os.environ.get('CC','cc'))+['-std=gnu11','-O0',str(native),'-o',str(binary)],check=True)
    for mode in ['valid','eof','invalid','eintr','error','timeout-control','orphan-control']:
        prefix=directory/mode;install=prefix/'install';(install/'bin').mkdir(parents=True)
        os.mkfifo(prefix/'done');done=os.open(prefix/'done',os.O_RDWR|os.O_NONBLOCK)
        helper=install/'bin/darlingserver'
        action="os.write(ready,b'.')" if mode in ('valid','eintr','error') else "os.write(ready,b'!')" if mode=='invalid' else 'pass'
        helper.write_text('#!'+os.sys.executable+'\n'+'''import os,sys
from pathlib import Path
prefix=Path(sys.argv[1]);ready=int(sys.argv[4])
try:
    ACTION
except BrokenPipeError:
    pass
os.close(ready)
with (prefix/'done').open('wb',buffering=0) as done:
    done.write(b'.')
'''.replace('ACTION',action));helper.chmod(0o700)
        env={k:v for k,v in os.environ.items() if k not in ('DARLING_DEBUG','DSERVER_LOG_STDERR')}
        result,expired,cleaned,ready_seen,strays=run_control([str(binary),str(prefix),str(install),mode],timeout=0.1 if mode=='timeout-control' else 10,env=env,ready_marker=b'TIMEOUT_CHILD_READY\n' if mode in ('timeout-control','orphan-control') else None,finalize=False)
        complete=False if mode in ('timeout-control','orphan-control') else bool(select.select([done],[],[],2)[0]) and os.read(done,1)==b'.'
        os.close(done)
        unexpected=False
        if mode=='orphan-control':unexpected=strays;cleaned=stop_controls() and cleaned
        elif not expired:
            if not reap_controls(time.monotonic()+2):
                unexpected=True;cleaned=stop_controls() and cleaned
        expected=mode in ('valid','eintr')
        output='PROCEEDED_AFTER_READY read_calls='+('2' if mode=='eintr' else '1')+'\n' if expected else ''
        passed=expired and cleaned and ready_seen if mode=='timeout-control' else not expired and cleaned and not unexpected and result.returncode==(0 if expected else 1) and result.stdout==output and complete
        if mode=='orphan-control':passed=not expired and cleaned and ready_seen and unexpected and result.returncode==0
        records.append(dict(mode=mode,exit=result.returncode,stdout=result.stdout,stderr=result.stderr,helper_completed=complete,timed_out=expired,cleaned=cleaned,child_ready=ready_seen,unexpected_children=unexpected,passed=passed))
print(json.dumps(records,indent=2))
raise SystemExit(0 if all(record['passed'] for record in records) else 1)
