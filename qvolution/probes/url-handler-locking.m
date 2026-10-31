// Original public-API/SQLite contention and private namespace failure contracts.
#include <CoreFoundation/CoreFoundation.h>
#include <LaunchServices/LaunchServices.h>
#include <dlfcn.h>
#include <sqlite3.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define CHECK(c,m) do{if(!(c)){fprintf(stderr,"FAIL: %s\n",m);exit(1);}}while(0)
typedef OSStatus (*Setter)(CFStringRef,CFStringRef);
static Setter setDefault;
static char directory[PATH_MAX],path[PATH_MAX];
static void paths(void) {
    CHECK(snprintf(directory,sizeof(directory),"/private/var/db/qvolution-url-handlers-%lu",(unsigned long)getuid())<(int)sizeof(directory),"directory extent");
    CHECK(snprintf(path,sizeof(path),"%s/preferences.db",directory)<(int)sizeof(path),"path extent");
}
static void mode(const char *name,unsigned wanted,BOOL isDirectory) {
    struct stat s;CHECK(lstat(name,&s)==0,"metadata exists");
    CHECK(s.st_uid==getuid()&&(s.st_mode&0777)==wanted,"private owner/permissions");
    CHECK(isDirectory?S_ISDIR(s.st_mode):S_ISREG(s.st_mode),"regular directory/file, not symlink");
}
static void readByte(int fd,char expected){char c=0;CHECK(read(fd,&c,1)==1&&c==expected,"interprocess control byte");}
static void writeByte(int fd,char c){CHECK(write(fd,&c,1)==1,"interprocess control write");}
static int contender(int start,int result,BOOL expectBusy) {
    sqlite3 *db=NULL;CHECK(sqlite3_open_v2(path,&db,SQLITE_OPEN_READWRITE|SQLITE_OPEN_NOFOLLOW,NULL)==SQLITE_OK,"external process database opens");
    CHECK(sqlite3_busy_timeout(db,0)==SQLITE_OK,"external immediate contention result");
    writeByte(result,'R');readByte(start,'G');
    unsigned attempts=expectBusy?60:1;
    for(unsigned i=0;i<attempts;i++) {
        int status=sqlite3_exec(db,"BEGIN IMMEDIATE",NULL,NULL,NULL);
        if(expectBusy) {
            if(status==SQLITE_OK) {
                sqlite3_exec(db,"ROLLBACK",NULL,NULL,NULL);
                fprintf(stderr,"FAIL: external writer acquired a transaction while original holder remained locked\n");return 1;
            }
            CHECK(status==SQLITE_BUSY,"blocked by lock, not another error");
            usleep(10000);
        } else {
            CHECK(status==SQLITE_OK,"unlocked external positive control acquires transaction");
            CHECK(sqlite3_exec(db,"ROLLBACK",NULL,NULL,NULL)==SQLITE_OK,"external rollback");
        }
    }
    CHECK(sqlite3_close(db)==SQLITE_OK,"external close");writeByte(result,expectBusy?'B':'F');return 0;
}
struct Child {pid_t pid;int start,result;};
static struct Child startChild(const char *self,BOOL expectBusy) {
    int toChild[2],fromChild[2];CHECK(pipe(toChild)==0&&pipe(fromChild)==0,"control pipes");
    char input[24],output[24];snprintf(input,sizeof(input),"%d",toChild[0]);snprintf(output,sizeof(output),"%d",fromChild[1]);
    pid_t pid=fork();CHECK(pid>=0,"fork external writer");
    if(pid==0) {
        close(toChild[1]);close(fromChild[0]);
        char *args[]={(char *)self,"--external-contender",input,output,expectBusy?"busy":"free",NULL};
        execv(self,args);_exit(127);
    }
    close(toChild[0]);close(fromChild[1]);readByte(fromChild[0],'R');
    return (struct Child){pid,toChild[1],fromChild[0]};
}
static void finishChild(struct Child child,BOOL expectBusy) {
    writeByte(child.start,'G');close(child.start);
    char result=0;ssize_t count=read(child.result,&result,1);close(child.result);
    int status=0;CHECK(waitpid(child.pid,&status,0)==child.pid,"external writer joined");
    CHECK(WIFEXITED(status)&&WEXITSTATUS(status)==0&&count==1&&result==(expectBusy?'B':'F'),"external lock exclusion/positive control");
}
static atomic_int started,finished;
static OSStatus writeStatus;
static void *setterThread(void *unused) {
    atomic_store_explicit(&started,1,memory_order_release);
    writeStatus=setDefault(CFSTR("qvolution-lock-concurrent"),CFSTR("org.qvolution.Concurrent"));
    atomic_store_explicit(&finished,1,memory_order_release);return NULL;
}
static void lockContract(const char *self) {
    CHECK(setDefault(CFSTR("qvolution-lock-original"),CFSTR("org.qvolution.Original"))==noErr,"initial API commit");
    mode(directory,0700,YES);mode(path,0600,NO);
    sqlite3 *owner=NULL;CHECK(sqlite3_open_v2(path,&owner,SQLITE_OPEN_READWRITE|SQLITE_OPEN_NOFOLLOW,NULL)==SQLITE_OK,"independent local holder opens");
    CHECK(sqlite3_exec(owner,"PRAGMA journal_mode=DELETE; BEGIN IMMEDIATE; UPDATE url_handler SET bundle_id='org.qvolution.Held' WHERE scheme='qvolution-lock-original'",NULL,NULL,NULL)==SQLITE_OK,"real dirty writer transaction holds lock");
    char journal[PATH_MAX];snprintf(journal,sizeof(journal),"%s-journal",path);mode(journal,0600,NO);
    struct Child child=startChild(self,YES);
    pthread_t thread;CHECK(pthread_create(&thread,NULL,setterThread,NULL)==0,"concurrent public API setter starts");
    while(!atomic_load_explicit(&started,memory_order_acquire))usleep(1000);
    finishChild(child,YES);
    CHECK(!atomic_load_explicit(&finished,memory_order_acquire),"public setter overlaps holder/external probes rather than failing before contention");
    CHECK(sqlite3_exec(owner,"ROLLBACK",NULL,NULL,NULL)==SQLITE_OK,"holder rollback");CHECK(sqlite3_close(owner)==SQLITE_OK,"holder SQLite-owned close");
    CHECK(pthread_join(thread,NULL)==0&&writeStatus==noErr,"queued setter commits after lock released");
    CFStringRef original=LSCopyDefaultHandlerForURLScheme(CFSTR("qvolution-lock-original"));
    CFStringRef written=LSCopyDefaultHandlerForURLScheme(CFSTR("qvolution-lock-concurrent"));
    CHECK(original&&CFEqual(original,CFSTR("org.qvolution.Original")),"holder transaction rollback preserved");
    CHECK(written&&CFEqual(written,CFSTR("org.qvolution.Concurrent")),"concurrent API preference committed");CFRelease(original);CFRelease(written);
    finishChild(startChild(self,NO),NO);
    puts("PASS: held local SQLite writer plus concurrent API setter excludes independent external writer; released positive control succeeds");
}
static void policyContract(void) {
    CHECK(setDefault(CFSTR("qvolution-policy-original"),CFSTR("org.qvolution.Original"))==noErr,"own test store");
    mode(directory,0700,YES);mode(path,0600,NO);
    sqlite3 *wal=NULL;CHECK(sqlite3_open_v2(path,&wal,SQLITE_OPEN_READWRITE|SQLITE_OPEN_NOFOLLOW,NULL)==SQLITE_OK,"own WAL sidecar control");
    CHECK(sqlite3_exec(wal,"PRAGMA journal_mode=WAL;BEGIN IMMEDIATE;UPDATE url_handler SET bundle_id='org.qvolution.Uncommitted' WHERE scheme='qvolution-policy-original'",NULL,NULL,NULL)==SQLITE_OK,"real WAL writer");
    char walPath[PATH_MAX],shmPath[PATH_MAX];snprintf(walPath,sizeof(walPath),"%s-wal",path);snprintf(shmPath,sizeof(shmPath),"%s-shm",path);
    mode(walPath,0600,NO);mode(shmPath,0600,NO);
    CFStringRef committed=LSCopyDefaultHandlerForURLScheme(CFSTR("qvolution-policy-original"));
    CHECK(committed&&CFEqual(committed,CFSTR("org.qvolution.Original")),"getter reads committed value through actual private WAL/SHM");CFRelease(committed);
    CHECK(sqlite3_exec(wal,"ROLLBACK;PRAGMA journal_mode=DELETE",NULL,NULL,NULL)==SQLITE_OK&&sqlite3_close(wal)==SQLITE_OK,"own WAL control cleanup");
    CHECK(chmod(path,0644)==0,"own-file permission negative setup");
    CHECK(setDefault(CFSTR("qvolution-policy-original"),CFSTR("org.qvolution.Unwanted"))==kLSDataUnavailableErr,"insecure published file rejected");mode(path,0644,NO);
    CHECK(chmod(path,0600)==0,"restore own test file");
    CHECK(chmod(directory,0755)==0,"own-directory permission negative setup");
    CHECK(setDefault(CFSTR("qvolution-policy-original"),CFSTR("org.qvolution.Unwanted"))==kLSDataUnavailableErr,"insecure directory rejected");mode(directory,0755,YES);
    CHECK(chmod(directory,0700)==0,"restore own test directory");
    char sentinel[PATH_MAX];snprintf(sentinel,sizeof(sentinel),"%s/original-sentinel",directory);
    int fd=open(sentinel,O_WRONLY|O_CREAT|O_EXCL,0600);CHECK(fd>=0,"new own sentinel");CHECK(write(fd,"original",8)==8&&close(fd)==0,"sentinel bytes");
    const char *suffixes[]={"-journal","-wal","-shm"};
    for(unsigned i=0;i<3;i++) {
        char sidecar[PATH_MAX];snprintf(sidecar,sizeof(sidecar),"%s%s",path,suffixes[i]);
        CHECK(symlink(sentinel,sidecar)==0,"own symlink sidecar setup");
        CHECK(setDefault(CFSTR("qvolution-policy-original"),CFSTR("org.qvolution.Unwanted"))==kLSDataUnavailableErr,"symlink sidecar rejected");
        CHECK(unlink(sidecar)==0,"remove only own negative symlink");
        fd=open(sidecar,O_WRONLY|O_CREAT|O_EXCL,0644);CHECK(fd>=0&&close(fd)==0,"own insecure sidecar setup");CHECK(chmod(sidecar,0644)==0,"explicit own negative permissions");
        CHECK(setDefault(CFSTR("qvolution-policy-original"),CFSTR("org.qvolution.Unwanted"))==kLSDataUnavailableErr,"insecure sidecar rejected");mode(sidecar,0644,NO);
        CHECK(unlink(sidecar)==0,"remove own negative sidecar");
    }
    fd=open(sentinel,O_RDONLY);char text[8];CHECK(fd>=0&&read(fd,text,8)==8&&close(fd)==0&&!memcmp(text,"original",8),"sidecar target never changed");CHECK(unlink(sentinel)==0,"remove own sentinel");
    char renamed[PATH_MAX];snprintf(renamed,sizeof(renamed),"%s-own-backup",directory);struct stat info;CHECK(lstat(renamed,&info)!=0,"backup name unused");
    CHECK(rename(directory,renamed)==0&&symlink(renamed,directory)==0,"own directory-symlink setup");
    CHECK(setDefault(CFSTR("qvolution-policy-original"),CFSTR("org.qvolution.Unwanted"))==kLSDataUnavailableErr,"symlink directory rejected");
    CHECK(unlink(directory)==0&&rename(renamed,directory)==0,"restore exact own namespace");
    CFStringRef value=LSCopyDefaultHandlerForURLScheme(CFSTR("qvolution-policy-original"));
    CHECK(value&&CFEqual(value,CFSTR("org.qvolution.Original")),"failure paths did not overwrite stored original");CFRelease(value);
    puts("PASS: own namespace/main/sidecar modes and symlinks reject without overwrite, chmod or sentinel mutation");
}
int main(int argc,char **argv) {
    setbuf(stdout,NULL);paths();
    if(argc==5&&!strcmp(argv[1],"--external-contender"))return contender(atoi(argv[2]),atoi(argv[3]),!strcmp(argv[4],"busy"));
    CHECK(argc==2&&(!strcmp(argv[1],"--lock-conflict")||!strcmp(argv[1],"--path-policy")),"explicit disposable-prefix mode");
    void *library=dlopen("/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/LaunchServices",RTLD_LAZY|RTLD_LOCAL);
    CHECK(library,"actual LaunchServices");setDefault=(Setter)dlsym(library,"LSSetDefaultHandlerForURLScheme");CHECK(setDefault,"real setter");
    if(!strcmp(argv[1],"--lock-conflict"))lockContract(argv[0]);else policyContract();
    dlclose(library);return 0;
}
