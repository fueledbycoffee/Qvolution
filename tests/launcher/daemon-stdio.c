#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include "../../src/startup/daemon-stdio.h"
int main(int argc, char **argv) {
 if(argc==4 && !strcmp(argv[1], "--exec")) {
  struct stat st;
  for(int fd=0;fd<3;++fd) {
   assert(fcntl(fd,F_GETFD)>=0 && !(fcntl(fd,F_GETFD)&FD_CLOEXEC));
   assert(!fstat(fd,&st)); assert(fd==2 ? S_ISREG(st.st_mode) : S_ISCHR(st.st_mode));
  }
  char byte; assert(read(0,&byte,1)==0); assert(write(1,"x",1)==1); assert(write(2,"authored fixture\n",17)==17);
  assert(write(atoi(argv[2]),"r",1)==1); assert(read(atoi(argv[3]),&byte,1)==1);
  return 0;
 }
 char root[]="/tmp/qv-stdio-XXXXXX"; assert(mkdtemp(root));
 char log[256]; snprintf(log,sizeof(log),"%s/daemon.log",root);
 for(int closed=0;closed<2;closed++) {
  int p[2],ready[2],release[2]; assert(!pipe(p) && !pipe(ready) && !pipe(release));
  pid_t child=fork(); assert(child>=0);
  if(!child) {
   assert(setsid()>0);
   close(p[0]);close(ready[0]);close(release[1]);
   assert(dup2(p[1],1)==1); assert(dup2(p[1],2)==2);close(p[1]);
   setenv("DARLING_DEBUG","1",1);setenv("DSERVER_LOG_STDERR","0",1);
   if(closed) {close(0);close(1);close(2);}
   assert(darlingDetachStdio(log));
   char r[32],e[32];snprintf(r,sizeof(r),"%d",ready[1]);snprintf(e,sizeof(e),"%d",release[0]);
   execl("/proc/self/exe","authored-stdio","--exec",r,e,NULL);_exit(99);
  }
  close(p[1]);close(ready[1]);close(release[0]);char byte;assert(read(ready[0],&byte,1)==1);
  struct pollfd fd={.fd=p[0],.events=POLLIN|POLLHUP};assert(poll(&fd,1,1000)>0);assert(read(p[0],&byte,1)==0);
  assert(write(release[1],"x",1)==1);int status;assert(waitpid(child,&status,0)==child);assert(WIFEXITED(status)&&!WEXITSTATUS(status));
  close(p[0]);close(ready[0]);close(release[1]);
 }
 assert(!darlingDetachStdio("/nonexistent/qv-stdio/log"));
 char alias[256], fifo[256];
 snprintf(alias,sizeof(alias),"%s/alias",root);snprintf(fifo,sizeof(fifo),"%s/fifo",root);
 assert(!symlink(log,alias));assert(!darlingDetachStdio(alias));
 assert(!mkfifo(fifo,0600));int reader=open(fifo,O_RDONLY|O_NONBLOCK);assert(reader>=0);
 assert(!darlingDetachStdio(fifo));close(reader);
 puts("PASS: detached daemon has valid usable 0-2 across exec and releases pipes while alive; closed stdio and errors covered");
}
