/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "catalog-client.h"
#include <stdlib.h>
#include <string.h>
#include <unicode/uloc.h>
#include <unicode/utf8.h>

struct reader { const uint8_t *bytes;size_t length,at;char *next,*end;int bad; };
static uint32_t number(struct reader *r)
{
    if (r->bad||r->length-r->at<4) { r->bad=1;return 0; }
    const uint8_t *p=r->bytes+r->at;r->at+=4;
    return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
static const char *string(struct reader *r,size_t maximum,int allow_empty)
{
    uint32_t n=number(r);
    if (r->bad||n>maximum||n>r->length-r->at||(!n&&!allow_empty)||(size_t)(r->end-r->next)<(size_t)n+1) { r->bad=1;return NULL; }
    const uint8_t *p=r->bytes+r->at;int32_t at=0;
    while (at<(int32_t)n) { UChar32 cp;U8_NEXT(p,at,(int32_t)n,cp);if(cp<=0) { r->bad=1;return NULL; } }
    char *out=r->next;memcpy(out,p,n);out[n]=0;r->next+=n+1;r->at+=n;return out;
}
void qv_catalog_snapshot_free(struct qv_catalog_snapshot *s)
{
    if (!s)return;
    if (s->voices) for (uint32_t i=0;i<s->count;i++) free(s->voices[i].languages);
    free(s->voices);free(s->storage);free(s);
}
enum qv_catalog_status qv_catalog_decode(const uint8_t *bytes,size_t length,struct qv_catalog_snapshot **out)
{
    if (out)*out=NULL;
    if (!out||!bytes||length<32||length>QV_CATALOG_MAX_BYTES||memcmp(bytes,"QVVC",4))return QV_CATALOG_BAD_DATA;
    struct qv_catalog_snapshot *s=calloc(1,sizeof(*s));
    if (!s)return QV_CATALOG_NO_MEMORY;
    s->storage=malloc(length);
    if (!s->storage) { qv_catalog_snapshot_free(s);return QV_CATALOG_NO_MEMORY; }
    struct reader r={bytes,length,4,s->storage,s->storage+length,0};
    uint32_t version=number(&r),status=number(&r),total=number(&r),count=number(&r);
    const char *provider=string(&r,QV_CATALOG_MAX_STRING,0);
    if (r.bad||version!=1||total!=length||status>QV_CATALOG_NO_MEMORY||count>QV_CATALOG_MAX_VOICES||
        (status&&count)||!provider||strcmp(provider,"espeak-ng")||count>(length-r.at)/12) goto bad;
    s->provider_status=(enum qv_catalog_status)status;s->count=count;
    s->voices=calloc(count?count:1,sizeof(*s->voices));
    if (!s->voices) { qv_catalog_snapshot_free(s);return QV_CATALOG_NO_MEMORY; }
    for (uint32_t i=0;i<count;i++) {
        struct qv_voice_record *v=&s->voices[i];
        v->provider_gender=number(&r);v->language_count=number(&r);v->primary=number(&r);
        if (r.bad||v->provider_gender>3||!v->language_count||v->language_count>QV_CATALOG_MAX_LANGUAGES) goto bad;
        v->provider_identifier=string(&r,QV_CATALOG_MAX_STRING,0);
        v->identifier=string(&r,QV_CATALOG_MAX_IDENTIFIER,0);v->name=string(&r,QV_CATALOG_MAX_STRING,0);
        if (r.bad)goto bad;
        char expected[QV_CATALOG_MAX_IDENTIFIER+1];
        if (qv_catalog_project_identifier(v->provider_identifier,expected,sizeof(expected))!=QV_CATALOG_OK||strcmp(expected,v->identifier))goto bad;
        if (i&&strcmp(s->voices[i-1].provider_identifier,v->provider_identifier)>=0)goto bad;
        v->languages=calloc(v->language_count,sizeof(*v->languages));
        if (!v->languages) { qv_catalog_snapshot_free(s);return QV_CATALOG_NO_MEMORY; }
        uint32_t primary=UINT32_MAX;
        for (uint32_t j=0;j<v->language_count;j++) {
            struct qv_language_record *l=&v->languages[j];
            l->priority=number(&r);l->raw=string(&r,QV_CATALOG_MAX_STRING,0);l->canonical=string(&r,QV_CATALOG_MAX_STRING,1);
            if (r.bad||!l->priority||l->priority>255)goto bad;
            char tag[ULOC_FULLNAME_CAPACITY];qv_canonical_language(l->raw,tag,sizeof(tag));
            if (strcmp(tag,l->canonical))goto bad;
            for (uint32_t k=0;k<j;k++)if(!strcmp(v->languages[k].raw,l->raw))goto bad;
            if (*tag&&(primary==UINT32_MAX||l->priority<v->languages[primary].priority||
                (l->priority==v->languages[primary].priority&&strcmp(l->canonical,v->languages[primary].canonical)<0)))primary=j;
        }
        if (v->primary!=primary)goto bad;
    }
    if (r.bad||r.at!=length)goto bad;
    *out=s;return QV_CATALOG_OK;
bad:
    qv_catalog_snapshot_free(s);return QV_CATALOG_BAD_DATA;
}
