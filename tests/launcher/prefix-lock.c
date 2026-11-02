#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <sys/wait.h>
#include "../../src/startup/prefix-lock.h"

int main(int argc, char **argv)
{
 (void)argv;
 char root[] = "/tmp/qv-prefix-lock-XXXXXX"; assert(mkdtemp(root));
 char state[256]; snprintf(state, sizeof(state), "%s/pid-record", root);
 int ready[2], start[2], seen[2]; assert(!pipe(ready) && !pipe(start) && !pipe(seen));
 pid_t children[2];
 for (int i = 0; i < 2; ++i) {
  children[i] = fork(); assert(children[i] >= 0);
  if (!children[i]) {
   close(ready[0]); close(start[1]); close(seen[0]);
   assert(write(ready[1], "r", 1) == 1);
   char byte; assert(read(start[0], &byte, 1) == 1);
   int lock = darlingLockPrefix(root); assert(lock >= 0);
   if (argc > 1) assert(!flock(lock, LOCK_UN)); /* pre-fix control */
   int record = open(state, O_RDONLY);
   if (record < 0) {
    /* Hold the real check->ready->PID-publication window open for the peer. */
    assert(errno == ENOENT); assert(write(seen[1], "b", 1) == 1);
    usleep(200000);
    record = open(state, O_CREAT | O_WRONLY, 0600); assert(record >= 0);
    assert(write(record, "ready", 5) == 5);
   } else {
    char value[5]; assert(read(record, value, sizeof(value)) == sizeof(value));
    assert(write(seen[1], "s", 1) == 1);
   }
   close(record); assert(!flock(lock, LOCK_UN)); close(lock);
   _exit(0);
  }
 }
 close(ready[1]); close(start[0]); close(seen[1]); char byte;
 for (int i=0;i<2;++i) assert(read(ready[0], &byte, 1)==1);
 assert(write(start[1], "xx", 2)==2); close(start[1]);
 int boots=0;
 for(int i=0;i<2;++i) { assert(read(seen[0], &byte, 1)==1); boots += byte=='b'; }
 for(int i=0;i<2;++i) { int status; assert(waitpid(children[i], &status, 0)==children[i]); assert(status==0); }
 /* A daemon closing its inherited description cannot prolong or release the lock. */
 int lock = darlingLockPrefix(root); assert(lock>=0);
 int held[2], end[2]; assert(!pipe(held) && !pipe(end));
 pid_t daemon=fork(); assert(daemon>=0);
 if(!daemon) { close(lock); close(held[0]); close(end[1]); assert(write(held[1],"r",1)==1); assert(read(end[0],&byte,1)==1); _exit(0); }
 close(held[1]);close(end[0]); assert(read(held[0],&byte,1)==1);
 int peer=open(root,O_RDONLY|O_DIRECTORY|O_CLOEXEC);assert(peer>=0);
 assert(flock(peer,LOCK_EX|LOCK_NB)<0 && errno==EWOULDBLOCK);
 assert(!flock(lock,LOCK_UN));close(lock);assert(!flock(peer,LOCK_EX|LOCK_NB));close(peer);
 assert(write(end[1],"x",1)==1);int status;assert(waitpid(daemon,&status,0)==daemon && status==0);
 close(ready[0]);close(seen[0]);close(held[0]);close(end[1]);
 if(boots!=1) { fprintf(stderr,"FAIL: concurrent cold clients booted %d servers\n",boots); return 1; }
 puts("PASS: concurrent cold clients boot once; daemon closes lock; shell clients can overlap after unlock");
 return 0;
}
