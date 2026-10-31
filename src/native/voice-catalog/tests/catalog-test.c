#include "catalog.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t u32(const uint8_t *p)
{ return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }

int main(void)
{
    uint8_t *out = malloc(QV_CATALOG_MAX_BYTES);
    size_t written = 123;
    char tag[128];
    assert(qv_canonical_language("en-gb", tag, sizeof(tag)) == QV_LANGUAGE_MAPPED && !strcmp(tag, "en-GB"));
    assert(qv_canonical_language("zh-hant-tw", tag, sizeof(tag)) == QV_LANGUAGE_MAPPED && !strcmp(tag, "zh-Hant-TW"));
    assert(qv_canonical_language("en-uk-north", tag, sizeof(tag)) == QV_LANGUAGE_UNSUPPORTED && !*tag);
    assert(qv_canonical_language("en-sc", tag, sizeof(tag)) == QV_LANGUAGE_UNSUPPORTED && !*tag);
    assert(qv_canonical_language("%%%", tag, sizeof(tag)) == QV_LANGUAGE_UNSUPPORTED && !*tag);
    assert(qv_canonical_language("en_US", tag, sizeof(tag)) == QV_LANGUAGE_UNSUPPORTED && !*tag);
    assert(qv_canonical_language("fr", tag, 1) == QV_LANGUAGE_NO_SPACE);
    struct qv_language_input languages[] = {{"en-gb",2},{"en",3},{"en-uk-north",4}};
    char name[] = "Fixture \xc3\xa9";
    struct qv_voice_input voices[] = {{"gmw/en",name,1,languages,3},{"same name?/%", "Fixture \xc3\xa9",2,languages,3}};
    memset(out, 0xa5, QV_CATALOG_MAX_BYTES);
    assert(qv_catalog_encode(voices,2,out,QV_CATALOG_MAX_BYTES,&written) == QV_CATALOG_OK);
    assert(written > 24 && !memcmp(out,"QVVC",4) && u32(out+4)==1 && u32(out+8)==0 && u32(out+12)==written && u32(out+16)==2);
    assert(memmem(out,written,"org.qvolution.voice.v1/espeak-ng/gmw%2Fen",sizeof("org.qvolution.voice.v1/espeak-ng/gmw%2Fen")-1));
    assert(memmem(out,written,"same%20name%3F%2F%25",sizeof("same%20name%3F%2F%25")-1));
    assert(memmem(out,written,"en-GB",5));
    size_t exact = written;
    uint8_t *snapshot = malloc(exact); memcpy(snapshot,out,exact);
    name[0] = 'X';
    assert(memmem(snapshot,exact,"Fixture \xc3\xa9",10));
    name[0] = 'F';
    memset(out,0xa5,exact);
    assert(qv_catalog_encode(voices,2,out,exact-1,&written)==QV_CATALOG_TOO_LARGE && written==0 && out[0]==0xa5);
    assert(qv_catalog_encode(voices,2,out,exact,&written)==QV_CATALOG_OK && written==exact && !memcmp(snapshot,out,exact));
    struct qv_voice_input reverse[] = {voices[1],voices[0]};
    assert(qv_catalog_encode(reverse,2,out,exact,&written)==QV_CATALOG_OK && !memcmp(snapshot,out,exact));
    voices[1].identifier=voices[0].identifier;
    memset(out,0xa5,exact);
    assert(qv_catalog_encode(voices,2,out,QV_CATALOG_MAX_BYTES,&written)==QV_CATALOG_BAD_DATA && written==0 && out[0]==0xa5);
    voices[1].identifier="distinct";
    voices[0].name="\xc0\xaf";
    assert(qv_catalog_encode(voices,2,out,QV_CATALOG_MAX_BYTES,&written)==QV_CATALOG_BAD_DATA && written==0);
    voices[0].name=name;
    voices[0].language_count=QV_CATALOG_MAX_LANGUAGES+1;
    assert(qv_catalog_encode(voices,2,out,QV_CATALOG_MAX_BYTES,&written)==QV_CATALOG_TOO_LARGE && written==0);
    assert(qv_catalog_encode(voices,QV_CATALOG_MAX_VOICES+1,out,QV_CATALOG_MAX_BYTES,&written)==QV_CATALOG_TOO_LARGE && written==0);
    assert(qv_catalog_encode(NULL,1,out,QV_CATALOG_MAX_BYTES,&written)==QV_CATALOG_BAD_DATA && written==0);
    assert(qv_catalog_encode(NULL,0,out,QV_CATALOG_MAX_BYTES,&written)==QV_CATALOG_OK && u32(out+16)==0);
    assert(qv_catalog_encode(NULL,0,NULL,0,&written)==QV_CATALOG_BAD_DATA && written==0);
    assert(qv_catalog_error(QV_CATALOG_UNAVAILABLE,out,QV_CATALOG_MAX_BYTES,&written)==QV_CATALOG_OK && u32(out+8)==QV_CATALOG_UNAVAILABLE && u32(out+16)==0);
    free(snapshot); free(out);
    puts("PASS: actual native encoder validates, owns and transactionally publishes bounded provider data");
    return 0;
}
