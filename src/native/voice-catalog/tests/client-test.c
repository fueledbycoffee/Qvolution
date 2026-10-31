#include "catalog-client.h"
#include "test-checks.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static void put32(uint8_t *p,uint32_t n) { for(int i=0;i<4;i++)p[i]=(uint8_t)(n>>(i*8)); }
static size_t locate(const uint8_t *data,size_t length,const char *value)
{
    size_t n=strlen(value);
    for(size_t i=0;i+n<=length;i++)if(!memcmp(data+i,value,n))return i;
    CHECK(0);return 0;
}
static void bad(const uint8_t *data,size_t n)
{
    struct qv_catalog_snapshot *s=(void *)1;
    CHECK(qv_catalog_decode(data,n,&s)==QV_CATALOG_BAD_DATA&&s==NULL);
}
int main(int argc,char **argv)
{
    CHECK(argc==5);
    uint8_t *bytes=malloc(QV_CATALOG_MAX_BYTES),*original=malloc(QV_CATALOG_MAX_BYTES);
    CHECK(bytes&&original);size_t n;
    struct qv_language_input language[]={ {"en-gb",2},{"en-uk-north",3} };
    struct qv_voice_input voice={"fixture/key","Fixture \xc3\xa9",2,language,2};
    CHECK(qv_catalog_encode(&voice,1,bytes,QV_CATALOG_MAX_BYTES,&n)==QV_CATALOG_OK);
    memcpy(original,bytes,n);
    struct qv_catalog_snapshot *s=NULL;
    CHECK(qv_catalog_decode(bytes,n,&s)==QV_CATALOG_OK&&s&&s->count==1&&s->provider_status==QV_CATALOG_OK);
    CHECK(!strcmp(s->voices[0].name,"Fixture \xc3\xa9")&&!strcmp(s->voices[0].languages[0].canonical,"en-GB"));
    memset(bytes,0,n);CHECK(!strcmp(s->voices[0].name,"Fixture \xc3\xa9"));qv_catalog_snapshot_free(s);
    for(size_t i=0;i<n;i++)bad(original,i);
    memcpy(bytes,original,n);bytes[n]=0;bad(bytes,n+1);
    memcpy(bytes,original,n);put32(bytes+4,2);bad(bytes,n);
    memcpy(bytes,original,n);put32(bytes+8,99);bad(bytes,n);
    memcpy(bytes,original,n);put32(bytes+16,QV_CATALOG_MAX_VOICES+1);bad(bytes,n);
    memcpy(bytes,original,n);put32(bytes+40,UINT32_MAX);bad(bytes,n);
    memcpy(bytes,original,n);put32(bytes+44,UINT32_MAX);bad(bytes,n);
    memcpy(bytes,original,n);bytes[49]='\0';bad(bytes,n);
    memcpy(bytes,original,n);bytes[49]=0xc0;bad(bytes,n);
    memcpy(bytes,original,n);bytes[locate(bytes,n,QV_CATALOG_IDENTIFIER_PREFIX)]='x';bad(bytes,n);
    memcpy(bytes,original,n);bytes[locate(bytes,n,"en-GB")+4]='X';bad(bytes,n);
    struct qv_voice_input pair[]={voice,voice};pair[0].identifier="fixture/one";pair[1].identifier="fixture/two";
    CHECK(qv_catalog_encode(pair,2,bytes,QV_CATALOG_MAX_BYTES,&n)==QV_CATALOG_OK);
    memcpy(bytes+locate(bytes,n,"fixture/two"),"fixture/one",11);
    memcpy(bytes+locate(bytes,n,"fixture%2Ftwo"),"fixture%2Fone",13);bad(bytes,n);
    CHECK(qv_catalog_error(QV_CATALOG_UNAVAILABLE,bytes,QV_CATALOG_MAX_BYTES,&n)==QV_CATALOG_OK);
    CHECK(qv_catalog_decode(bytes,n,&s)==QV_CATALOG_OK&&s->provider_status==QV_CATALOG_UNAVAILABLE&&!s->count);qv_catalog_snapshot_free(s);
    CHECK(qv_catalog_encode(NULL,0,bytes,QV_CATALOG_MAX_BYTES,&n)==QV_CATALOG_OK);
    CHECK(qv_catalog_decode(bytes,n,&s)==QV_CATALOG_OK&&s->provider_status==QV_CATALOG_OK&&!s->count);qv_catalog_snapshot_free(s);
    int child;
    CHECK(qv_catalog_run(argv[1],5000,bytes,QV_CATALOG_MAX_BYTES,&n,&child)==QV_TRANSPORT_OK&&child==0&&n);
    CHECK(qv_catalog_decode(bytes,n,&s)==QV_CATALOG_OK&&s->count>0);qv_catalog_snapshot_free(s);
    pid_t low_fd=fork();CHECK(low_fd>=0);
    if(low_fd==0) {
        close(STDIN_FILENO);close(STDOUT_FILENO);
        enum qv_transport_status status=qv_catalog_run(argv[1],5000,bytes,QV_CATALOG_MAX_BYTES,&n,&child);
        _exit(status==QV_TRANSPORT_OK&&child==0&&n?0:1);
    }
    CHECK(waitpid(low_fd,&child,0)==low_fd&&WIFEXITED(child)&&WEXITSTATUS(child)==0);
    CHECK(qv_catalog_run(argv[2],50,bytes,QV_CATALOG_MAX_BYTES,&n,&child)==QV_TRANSPORT_TIMEOUT&&!n&&child==-1);
    CHECK(waitpid(-1,&child,WNOHANG)==-1&&errno==ECHILD);
    CHECK(qv_catalog_run(argv[3],5000,bytes,8,&n,&child)==QV_TRANSPORT_TOO_LARGE&&!n);
    CHECK(waitpid(-1,&child,WNOHANG)==-1&&errno==ECHILD);
    CHECK(qv_catalog_run(argv[4],5000,bytes,QV_CATALOG_MAX_BYTES,&n,&child)==QV_TRANSPORT_OK&&child==12);
    CHECK(qv_catalog_run("/nonexistent/qv-missing-helper",50,bytes,QV_CATALOG_MAX_BYTES,&n,&child)==QV_TRANSPORT_SPAWN_ERROR&&!n);
    free(original);free(bytes);puts("PASS: owning decoder and actual POSIX child transport/errors/deadline/reap");return 0;
}
