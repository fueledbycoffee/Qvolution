#include "catalog.h"
#include "test-checks.h"
#include <stdlib.h>
#include <string.h>

void *__real_malloc(size_t);
void __real_free(void *);
static int fail_at, calls, live;
void *__wrap_malloc(size_t n)
{
    if (calls++==fail_at) return NULL;
    void *p=__real_malloc(n);
    if (p) live++;
    return p;
}
void __wrap_free(void *p) { if(p) live--; __real_free(p); }

int main(void)
{
    uint8_t out[4096]; size_t written;
    struct qv_language_input language={"en-gb",2};
    struct qv_voice_input voice={"original/key","Original",1,&language,1};
    for (fail_at=0;fail_at<2;fail_at++) {
        calls=live=0;written=99;memset(out,0xa5,sizeof(out));
        CHECK(qv_catalog_encode(&voice,1,out,sizeof(out),&written)==QV_CATALOG_NO_MEMORY);
        CHECK(written==0 && out[0]==0xa5 && live==0);
    }
    fail_at=0;calls=live=0;written=99;
    CHECK(qv_catalog_error(QV_CATALOG_PROVIDER_ERROR,out,sizeof(out),&written)==QV_CATALOG_NO_MEMORY);
    CHECK(written==0&&live==0);
    fail_at=-1;calls=live=0;
    CHECK(qv_catalog_encode(&voice,1,out,sizeof(out),&written)==QV_CATALOG_OK&&written&&live==0);
    return 0;
}
