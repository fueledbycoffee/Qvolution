#define _GNU_SOURCE
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <pty.h>
#include <unistd.h>
#include "../../src/startup/container-shutdown.h"
static pid_t guest(const char *entry, int *release) {
 int ready[2],end[2]; assert(!pipe(ready)&&!pipe(end));
 pid_t intermediate=fork();assert(intermediate>=0);
 if(!intermediate) {
  close(ready[0]);close(end[1]);
  pid_t child=fork();assert(child>=0);
  if(child) _exit(0);
  char r[32],e[32];snprintf(r,sizeof(r),"%d",ready[1]);snprintf(e,sizeof(e),"%d",end[0]);
  assert(!setenv("__mldr_sockpath",entry,1));
  execl("/proc/self/exe","authored-pty","--guest",r,e,NULL);_exit(99);
 }
 close(ready[1]);close(end[0]);pid_t pid;assert(read(ready[0],&pid,sizeof(pid))==sizeof(pid));close(ready[0]);
 int status;assert(waitpid(intermediate,&status,0)==intermediate);assert(!status);
 *release=end[1];return pid;
}
int main(int argc,char **argv) {
 if(argc==4&&!strcmp(argv[1],"--guest")) {
  signal(SIGTERM,SIG_IGN);signal(SIGHUP,SIG_IGN);
  int master,slave;assert(!openpty(&master,&slave,NULL,NULL,NULL));assert(setsid()>0);assert(!ioctl(slave,TIOCSCTTY,0));
  pid_t pid=getpid();assert(write(atoi(argv[2]),&pid,sizeof(pid))==sizeof(pid));close(atoi(argv[2]));
  char byte;assert(read(atoi(argv[3]),&byte,1)==1);return 0;
 }
 assert(!prctl(PR_SET_CHILD_SUBREAPER,1));
 char root[]="/tmp/qv-shutdown-pty-XXXXXX";assert(mkdtemp(root));
 char entry[256];snprintf(entry,sizeof(entry),"%s/.darlingserver.sock",root);
 int release,otherRelease;pid_t target=guest(entry,&release);pid_t other=guest("/different-owned-test/.darlingserver.sock",&otherRelease);
 assert(shutdownParent(target)==getpid());
 int targetHandle=shutdownHandle(target),otherHandle=shutdownHandle(other);assert(targetHandle>=0&&otherHandle>=0);
 pid_t server=fork();assert(server>=0);if(!server){for(;;)pause();}
 int serverHandle=shutdownHandle(server);assert(serverHandle>=0);
 char exe[4096];ssize_t n=readlink("/proc/self/exe",exe,sizeof(exe)-1);assert(n>0);exe[n]=0;
 const char *mldr[2]={exe,"/not-an-mldr"};
 bool stopped=shutdownContainer(server,serverHandle,0,-1,root,getuid(),mldr);
 struct pollfd p={.fd=targetHandle,.events=POLLIN};bool targetStopped=poll(&p,1,1000)>0;
 struct pollfd q={.fd=otherHandle,.events=POLLIN};assert(poll(&q,1,0)==0);
 signal(SIGPIPE,SIG_IGN);if(!targetStopped)assert(write(release,"x",1)==1);
 assert(write(otherRelease,"x",1)==1);close(release);close(otherRelease);
 while(wait(NULL)>0) {}
 close(targetHandle);close(otherHandle);close(serverHandle);
 if(!stopped||!targetStopped){fprintf(stderr,"FAIL: reparented TERM-resistant PTY guest survived prefix shutdown\n");return 1;}
 puts("PASS: reparented TERM-resistant PTY session killed after grace; other-prefix PTY survived");
}
