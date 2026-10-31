#include "catalog-client.h"
#include "test-checks.h"
#include <stdlib.h>
void *__real_malloc(size_t);
void *__real_calloc(size_t,size_t);
void __real_free(void *);
static int fail_at=-1,calls,live;
void *__wrap_malloc(size_t n) { if(calls++==fail_at)return NULL;void *p=__real_malloc(n);if(p)live++;return p; }
void *__wrap_calloc(size_t n,size_t s) { if(calls++==fail_at)return NULL;void *p=__real_calloc(n,s);if(p)live++;return p; }
void __wrap_free(void *p) { if(p)live--;__real_free(p); }
int main(void)
{
    uint8_t bytes[4096];size_t n;struct qv_language_input language={"en-gb",2};
    struct qv_voice_input voice={"real/key","Original",1,&language,1};
    CHECK(qv_catalog_encode(&voice,1,bytes,sizeof(bytes),&n)==QV_CATALOG_OK&&live==0);
    struct qv_catalog_snapshot *s=NULL;calls=0;
    CHECK(qv_catalog_decode(bytes,n,&s)==QV_CATALOG_OK);
    int allocations=calls;qv_catalog_snapshot_free(s);CHECK(live==0&&allocations>0);
    for(int i=0;i<allocations;i++) {
        fail_at=i;calls=0;s=(void *)1;
        CHECK(qv_catalog_decode(bytes,n,&s)==QV_CATALOG_NO_MEMORY&&s==NULL&&live==0);
    }
    return 0;
}
