/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 Darling developers */
#include "catalog.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unicode/uloc.h>
#include <unicode/utf8.h>

struct writer { uint8_t *bytes; size_t length; enum qv_catalog_status status; };

static void append(struct writer *w, const void *bytes, size_t length)
{
    if (w->status != QV_CATALOG_OK) return;
    if (length > QV_CATALOG_MAX_BYTES - w->length) { w->status=QV_CATALOG_TOO_LARGE; return; }
    memcpy(w->bytes+w->length, bytes, length); w->length+=length;
}

static void number(struct writer *w, uint32_t n)
{
    uint8_t bytes[]={(uint8_t)n,(uint8_t)(n>>8),(uint8_t)(n>>16),(uint8_t)(n>>24)};
    append(w,bytes,sizeof(bytes));
}

static int valid_string(const char *s)
{
    if (!s) return 0;
    size_t n=strnlen(s,QV_CATALOG_MAX_STRING+1);
    if (!n || n>QV_CATALOG_MAX_STRING) return 0;
    int32_t at=0;
    while (at<(int32_t)n) { UChar32 cp; U8_NEXT(s,at,(int32_t)n,cp); if (cp<0) return 0; }
    return 1;
}

static void string(struct writer *w, const char *s)
{
    size_t n=strlen(s); number(w,(uint32_t)n); append(w,s,n);
}

/* Accept a bounded subset. Provider dialect labels are never guessed to be regions. */
enum qv_language_status qv_canonical_language(const char *raw, char *out, size_t capacity)
{
    if (!out || !capacity) return QV_LANGUAGE_NO_SPACE;
    out[0]=0;
    if (!raw) return QV_LANGUAGE_UNSUPPORTED;
    size_t n=strnlen(raw,QV_CATALOG_MAX_STRING+1);
    if (!n || n>QV_CATALOG_MAX_STRING) return QV_LANGUAGE_UNSUPPORTED;
    char copy[QV_CATALOG_MAX_STRING+1]; memcpy(copy,raw,n+1);
    char *parts[3]; size_t count=0;
    char *start=copy;
    for (size_t i=0;i<=n;i++) {
        unsigned char c=copy[i];
        if (c && c!='-' && !((c>='A'&&c<='Z')||(c>='a'&&c<='z'))) return QV_LANGUAGE_UNSUPPORTED;
        if (!c || c=='-') {
            if (count==3 || copy+i==start) return QV_LANGUAGE_UNSUPPORTED;
            copy[i]=0; parts[count++]=start; start=copy+i+1;
        }
    }
    if (strlen(parts[0])<2 || strlen(parts[0])>3) return QV_LANGUAGE_UNSUPPORTED;
    size_t region=1;
    if (count>1 && strlen(parts[1])==4) region=2;
    if (count>region+1 || (count>region && strlen(parts[region])!=2)) return QV_LANGUAGE_UNSUPPORTED;
    /* Historical eSpeak labels en-sc/en-wi mean dialects, not ISO countries. */
    if (!strcasecmp(raw,"en-sc") || !strcasecmp(raw,"en-wi") || !strcasecmp(raw,"en-uk")) return QV_LANGUAGE_UNSUPPORTED;
    char locale[ULOC_FULLNAME_CAPACITY],canonical[ULOC_FULLNAME_CAPACITY],tag[ULOC_FULLNAME_CAPACITY];
    UErrorCode error=U_ZERO_ERROR; int32_t parsed=0;
    uloc_forLanguageTag(raw,locale,sizeof(locale),&parsed,&error);
    if (U_FAILURE(error) || parsed!=(int32_t)n) return QV_LANGUAGE_UNSUPPORTED;
    if (!*uloc_getISO3Language(locale)) return QV_LANGUAGE_UNSUPPORTED;
    if (count>region) {
        char country[ULOC_COUNTRY_CAPACITY]; error=U_ZERO_ERROR;
        uloc_getCountry(locale,country,sizeof(country),&error);
        if (U_FAILURE(error)) return QV_LANGUAGE_UNSUPPORTED;
        const char *const *countries=uloc_getISOCountries(); size_t i=0;
        for (;countries[i] && strcmp(country,countries[i]);i++) {}
        if (!countries[i]) return QV_LANGUAGE_UNSUPPORTED;
    }
    error=U_ZERO_ERROR; uloc_canonicalize(locale,canonical,sizeof(canonical),&error);
    if (U_FAILURE(error)) return QV_LANGUAGE_UNSUPPORTED;
    error=U_ZERO_ERROR; int32_t length=uloc_toLanguageTag(canonical,tag,sizeof(tag),1,&error);
    if (U_FAILURE(error) || length<=0) return QV_LANGUAGE_UNSUPPORTED;
    if ((size_t)length>=capacity) return QV_LANGUAGE_NO_SPACE;
    memcpy(out,tag,(size_t)length+1); return QV_LANGUAGE_MAPPED;
}

static void project_identifier(struct writer *w, const char *identifier)
{
    static const char prefix[]="org.qvolution.voice.v1/espeak-ng/",hex[]="0123456789ABCDEF";
    char out[sizeof(prefix)+3*QV_CATALOG_MAX_STRING];
    size_t n=sizeof(prefix)-1; memcpy(out,prefix,n);
    for (const unsigned char *p=(const unsigned char *)identifier;*p;p++) {
        unsigned char c=*p;
        if ((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='.'||c=='_'||c=='~') out[n++]=c;
        else { out[n++]='%'; out[n++]=hex[c>>4]; out[n++]=hex[c&15]; }
    }
    out[n]=0; string(w,out);
}

static void header(struct writer *w, enum qv_catalog_status status, uint32_t count)
{
    append(w,"QVVC",4); number(w,1); number(w,status); number(w,0); number(w,count); string(w,"espeak-ng");
}

static enum qv_catalog_status finish(struct writer *w, uint8_t *out, size_t capacity, size_t *written)
{
    enum qv_catalog_status status=w->status;
    if (status==QV_CATALOG_OK && w->length>capacity) status=QV_CATALOG_TOO_LARGE;
    if (status==QV_CATALOG_OK) {
        uint32_t n=(uint32_t)w->length;
        for (size_t i=0;i<4;i++) w->bytes[12+i]=(uint8_t)(n>>(8*i));
        memcpy(out,w->bytes,w->length); *written=w->length;
    }
    free(w->bytes); return status;
}

enum qv_catalog_status qv_catalog_error(enum qv_catalog_status error, uint8_t *out, size_t capacity, size_t *written)
{
    if (written) *written=0;
    if (!written || !out || error<=QV_CATALOG_OK || error>QV_CATALOG_NO_MEMORY) return QV_CATALOG_BAD_DATA;
    struct writer w={malloc(QV_CATALOG_MAX_BYTES),0,QV_CATALOG_OK};
    if (!w.bytes) return QV_CATALOG_NO_MEMORY;
    header(&w,error,0); return finish(&w,out,capacity,written);
}

static int compare_voice(const void *a, const void *b)
{ return strcmp((*(const struct qv_voice_input *const *)a)->identifier,(*(const struct qv_voice_input *const *)b)->identifier); }

enum qv_catalog_status qv_catalog_encode(const struct qv_voice_input *voices, size_t count,
                                       uint8_t *out, size_t capacity, size_t *written)
{
    if (written) *written=0;
    if (!written || !out || (!voices&&count)) return QV_CATALOG_BAD_DATA;
    if (count>QV_CATALOG_MAX_VOICES) return QV_CATALOG_TOO_LARGE;
    const struct qv_voice_input **order=malloc((count?count:1)*sizeof(*order));
    if (!order) return QV_CATALOG_NO_MEMORY;
    enum qv_catalog_status status=QV_CATALOG_OK;
    for (size_t i=0;i<count;i++) {
        const struct qv_voice_input *v=&voices[i]; order[i]=v;
        if (v->language_count>QV_CATALOG_MAX_LANGUAGES) { status=QV_CATALOG_TOO_LARGE; break; }
        if (!valid_string(v->identifier)||!valid_string(v->name)||!v->languages||!v->language_count||v->provider_gender>3) { status=QV_CATALOG_BAD_DATA; break; }
        for (size_t j=0;j<v->language_count;j++) {
            if (!valid_string(v->languages[j].tag)||v->languages[j].priority==0||v->languages[j].priority>255) { status=QV_CATALOG_BAD_DATA; break; }
            for (size_t k=0;k<j;k++) if (!strcmp(v->languages[j].tag,v->languages[k].tag)) status=QV_CATALOG_BAD_DATA;
        }
        if (status!=QV_CATALOG_OK) break;
    }
    if (status!=QV_CATALOG_OK) { free(order); return status; }
    qsort(order,count,sizeof(*order),compare_voice);
    for (size_t i=1;i<count;i++) if (!strcmp(order[i-1]->identifier,order[i]->identifier)) { free(order); return QV_CATALOG_BAD_DATA; }
    struct writer w={malloc(QV_CATALOG_MAX_BYTES),0,QV_CATALOG_OK};
    if (!w.bytes) { free(order); return QV_CATALOG_NO_MEMORY; }
    header(&w,QV_CATALOG_OK,(uint32_t)count);
    for (size_t i=0;i<count;i++) {
        const struct qv_voice_input *v=order[i];
        char tags[QV_CATALOG_MAX_LANGUAGES][ULOC_FULLNAME_CAPACITY]; uint32_t primary=UINT32_MAX;
        for (size_t j=0;j<v->language_count;j++) {
            qv_canonical_language(v->languages[j].tag,tags[j],sizeof(tags[j]));
            if (*tags[j] && (primary==UINT32_MAX || v->languages[j].priority<v->languages[primary].priority ||
                (v->languages[j].priority==v->languages[primary].priority && strcmp(tags[j],tags[primary])<0))) primary=(uint32_t)j;
        }
        number(&w,v->provider_gender); number(&w,(uint32_t)v->language_count); number(&w,primary);
        string(&w,v->identifier); project_identifier(&w,v->identifier); string(&w,v->name);
        for (size_t j=0;j<v->language_count;j++) { number(&w,v->languages[j].priority); string(&w,v->languages[j].tag); string(&w,tags[j]); }
    }
    free(order); return finish(&w,out,capacity,written);
}
