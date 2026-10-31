/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "catalog-client.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char **environ;
static int64_t milliseconds(void)
{
    struct timespec t;
    if(clock_gettime(CLOCK_MONOTONIC,&t))return -1;
    return (int64_t)t.tv_sec*1000+t.tv_nsec/1000000;
}
static void reap(pid_t pid,int *status)
{ while(waitpid(pid,status,0)<0&&errno==EINTR){} }

enum qv_transport_status qv_catalog_run(const char *path,unsigned int timeout,
                                      uint8_t *out,size_t capacity,size_t *written,int *child_status)
{
    if(written)*written=0;
    if(child_status)*child_status=-1;
    if(!path||path[0]!='/'||!timeout||timeout>60000||!out||!capacity||capacity>QV_CATALOG_MAX_BYTES||!written||!child_status)
        return QV_TRANSPORT_BAD_ARGUMENT;
    int pipefd[2];
    if(pipe(pipefd))return QV_TRANSPORT_IO_ERROR;
    for(int i=0;i<2;i++)if(pipefd[i]<3) {
        int moved=fcntl(pipefd[i],F_DUPFD,3);
        if(moved<0) { close(pipefd[0]);close(pipefd[1]);return QV_TRANSPORT_IO_ERROR; }
        close(pipefd[i]);pipefd[i]=moved;
    }
    if(fcntl(pipefd[0],F_SETFD,FD_CLOEXEC)||fcntl(pipefd[1],F_SETFD,FD_CLOEXEC)||fcntl(pipefd[0],F_SETFL,O_NONBLOCK)) {
        close(pipefd[0]);close(pipefd[1]);return QV_TRANSPORT_IO_ERROR;
    }
    posix_spawn_file_actions_t actions;
    if(posix_spawn_file_actions_init(&actions)) { close(pipefd[0]);close(pipefd[1]);return QV_TRANSPORT_SPAWN_ERROR; }
    posix_spawnattr_t attributes;
    if(posix_spawnattr_init(&attributes)) { posix_spawn_file_actions_destroy(&actions);close(pipefd[0]);close(pipefd[1]);return QV_TRANSPORT_SPAWN_ERROR; }
    int prepared=posix_spawn_file_actions_addopen(&actions,STDIN_FILENO,"/dev/null",O_RDONLY,0);
#ifdef POSIX_SPAWN_CLOEXEC_DEFAULT
    if(!prepared)prepared=posix_spawnattr_setflags(&attributes,POSIX_SPAWN_CLOEXEC_DEFAULT);
#endif
    if(!prepared)prepared=posix_spawn_file_actions_adddup2(&actions,pipefd[1],STDOUT_FILENO);
    if(!prepared)prepared=posix_spawn_file_actions_addclose(&actions,pipefd[0]);
    if(!prepared&&pipefd[1]!=STDOUT_FILENO)prepared=posix_spawn_file_actions_addclose(&actions,pipefd[1]);
    pid_t pid=-1;char *argv[]={(char *)path,NULL};
    int spawned=prepared?prepared:posix_spawn(&pid,path,&actions,&attributes,argv,environ);
    posix_spawnattr_destroy(&attributes);posix_spawn_file_actions_destroy(&actions);close(pipefd[1]);
    if(spawned||pid<=0) { close(pipefd[0]);return QV_TRANSPORT_SPAWN_ERROR; }
    int64_t start=milliseconds();
    enum qv_transport_status result=start<0?QV_TRANSPORT_IO_ERROR:QV_TRANSPORT_OK;
    size_t used=0;int status=0,exited=0,eof=0;
    while(result==QV_TRANSPORT_OK&&(!eof||!exited)) {
        if(!exited) {
            pid_t observed=waitpid(pid,&status,WNOHANG);
            if(observed==pid)exited=1;
            else if(observed<0&&errno!=EINTR) { if(errno==ECHILD)exited=1;result=QV_TRANSPORT_IO_ERROR;break; }
        }
        int64_t now=milliseconds();
        if(now<0) { result=QV_TRANSPORT_IO_ERROR;break; }
        int64_t remaining=(int64_t)timeout-(now-start);
        if(remaining<=0) { result=QV_TRANSPORT_TIMEOUT;break; }
        struct pollfd descriptor={eof?-1:pipefd[0],POLLIN,0};
        int ready=poll(&descriptor,1,remaining>20?20:(int)remaining);
        if(ready<0) { if(errno==EINTR)continue;result=QV_TRANSPORT_IO_ERROR;break; }
        if(ready&&(descriptor.revents&(POLLIN|POLLHUP))) {
            uint8_t buffer[8192];ssize_t count;
            while((count=read(pipefd[0],buffer,sizeof(buffer)))>0) {
                if((size_t)count>capacity-used) { result=QV_TRANSPORT_TOO_LARGE;break; }
                for(ssize_t i=0;i<count;i++)out[used++]=buffer[i];
                if(milliseconds()-start>=(int64_t)timeout) { result=QV_TRANSPORT_TIMEOUT;break; }
            }
            if(count==0)eof=1;
            else if(count<0&&errno!=EAGAIN&&errno!=EWOULDBLOCK&&errno!=EINTR)result=QV_TRANSPORT_IO_ERROR;
        }
        if(ready&&(descriptor.revents&(POLLERR|POLLNVAL)))result=QV_TRANSPORT_IO_ERROR;
    }
    close(pipefd[0]);
    if(!exited) { if(result!=QV_TRANSPORT_OK)kill(pid,SIGKILL);reap(pid,&status); }
    if(result!=QV_TRANSPORT_OK)return result;
    if(!WIFEXITED(status))return QV_TRANSPORT_CHILD_SIGNAL;
    *written=used;*child_status=WEXITSTATUS(status);return QV_TRANSPORT_OK;
}
