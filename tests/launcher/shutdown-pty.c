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
#include <time.h>
#include "../../src/startup/container-shutdown.h"
static int termReport;
static void term(int signal) { (void)signal; char byte='t'; if(write(termReport,&byte,1)<0) _exit(98); }
static double now(void) { struct timespec t; assert(!clock_gettime(CLOCK_MONOTONIC,&t)); return t.tv_sec+t.tv_nsec/1e9; }
static pid_t guest(const char *entry, int *release, int report, pid_t *login) {
 int ready[2],end[2]; assert(!pipe(ready)&&!pipe(end));
 pid_t intermediate=fork();assert(intermediate>=0);
 if(!intermediate) {
  close(ready[0]);close(end[1]);
  pid_t child=fork();assert(child>=0);
  if(child) _exit(0);
  char r[32],e[32],t[32];snprintf(r,sizeof(r),"%d",ready[1]);snprintf(e,sizeof(e),"%d",end[0]);snprintf(t,sizeof(t),"%d",report);
  assert(!setenv("__mldr_sockpath",entry,1));
  execl("/proc/self/exe","authored-pty","--guest",r,e,t,NULL);_exit(99);
 }
 close(ready[1]);close(end[0]);pid_t ids[2];assert(read(ready[0],ids,sizeof(ids))==sizeof(ids));close(ready[0]);*login=ids[1];
 int status;assert(waitpid(intermediate,&status,0)==intermediate);assert(!status);
 *release=end[1];return ids[0];
}
int main(int argc,char **argv) {
 if(argc==5&&!strcmp(argv[1],"--guest")) {
  termReport=atoi(argv[4]);
  struct sigaction action={.sa_handler=term,.sa_flags=SA_RESTART};assert(!sigaction(SIGTERM,&action,NULL));signal(SIGHUP,SIG_IGN);
  int master,slave;assert(!openpty(&master,&slave,NULL,NULL,NULL));assert(setsid()>0);assert(!ioctl(slave,TIOCSCTTY,0));
  int ready[2];assert(!pipe(ready));
  pid_t login=fork();assert(login>=0);
  if(!login) { close(ready[0]);assert(tcgetsid(slave)==getppid());assert(write(ready[1],"r",1)==1);close(ready[1]);close(atoi(argv[2]));char byte;assert(read(atoi(argv[3]),&byte,1)==1);return 0; }
  close(ready[1]);char byte;assert(read(ready[0],&byte,1)==1);close(ready[0]);
  pid_t ids[2]={getpid(),login};assert(write(atoi(argv[2]),ids,sizeof(ids))==sizeof(ids));close(atoi(argv[2]));
  assert(read(atoi(argv[3]),&byte,1)==1);int status;assert(waitpid(login,&status,0)==login && status==0);return 0;
 }
 assert(!prctl(PR_SET_CHILD_SUBREAPER,1));
 char root[]="/tmp/qv-shutdown-pty-XXXXXX";assert(mkdtemp(root));
 char entry[256];snprintf(entry,sizeof(entry),"%s/.darlingserver.sock",root);
 int report[2];assert(!pipe2(report,O_NONBLOCK));
 int release,otherRelease;pid_t login,otherLogin;
 pid_t target=guest(entry,&release,report[1],&login);pid_t other=guest("/different-owned-test/.darlingserver.sock",&otherRelease,report[1],&otherLogin);
 assert(shutdownParent(target)==getpid());
 int targetHandle=shutdownHandle(target),otherHandle=shutdownHandle(other);assert(targetHandle>=0&&otherHandle>=0);
 pid_t server=fork();assert(server>=0);if(!server){for(;;)pause();}
 int serverHandle=shutdownHandle(server);assert(serverHandle>=0);
 char exe[4096];ssize_t n=readlink("/proc/self/exe",exe,sizeof(exe)-1);assert(n>0);exe[n]=0;
 const char *mldr[2]={exe,"/not-an-mldr"};
 int loginHandle=shutdownHandle(login);assert(loginHandle>=0);
 double start=now();
 bool stopped=shutdownContainer(server,serverHandle,0,-1,root,getuid(),mldr);
 double elapsed=now()-start;
 char terms[16];ssize_t termCount=read(report[0],terms,sizeof(terms));
 struct pollfd l={.fd=loginHandle,.events=POLLIN};bool loginStopped=poll(&l,1,1000)>0;
 struct pollfd p={.fd=targetHandle,.events=POLLIN};bool targetStopped=poll(&p,1,1000)>0;
 struct pollfd q={.fd=otherHandle,.events=POLLIN};assert(poll(&q,1,0)==0);
 signal(SIGPIPE,SIG_IGN);if(!targetStopped)assert(write(release,"xx",2)==2);
 assert(write(otherRelease,"xx",2)==2);close(release);close(otherRelease);
 while(wait(NULL)>0) {}
 close(targetHandle);close(loginHandle);close(otherHandle);close(serverHandle);close(report[0]);close(report[1]);
 if(!stopped||!targetStopped||!loginStopped||termCount!=2||elapsed<.9||elapsed>5){fprintf(stderr,"FAIL: reparented TERM-resistant PTY guest survived prefix shutdown\n");return 1;}
 puts("PASS: PTY leader and login child received TERM, survived grace, then exited by KILL; other-prefix pair survived");
}
