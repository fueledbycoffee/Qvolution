/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 Darling developers */
#include "catalog.h"
#include <dlfcn.h>
#include <espeak-ng/espeak_ng.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef QV_ESPEAK_LIBRARY
#define QV_ESPEAK_LIBRARY "libespeak-ng.so.1"
#endif

struct espeak_api {
    __typeof__(&espeak_ng_InitializePath) initialize_path;
    __typeof__(&espeak_ng_Initialize) initialize;
    __typeof__(&espeak_ng_ClearErrorContext) clear_error;
    __typeof__(&espeak_ng_Terminate) terminate;
    __typeof__(&espeak_ListVoices) list;
};

static enum qv_catalog_status collect(uint8_t *out, size_t *written)
{
    struct espeak_api api;
    void *library=dlopen(QV_ESPEAK_LIBRARY,RTLD_NOW|RTLD_LOCAL);
    if (!library) return QV_CATALOG_UNAVAILABLE;
#define BIND(field,symbol) do { *(void **)(&api.field)=dlsym(library,#symbol); if (!api.field) { dlclose(library); return QV_CATALOG_UNAVAILABLE; } } while(0)
    BIND(initialize_path,espeak_ng_InitializePath);
    BIND(initialize,espeak_ng_Initialize);
    BIND(clear_error,espeak_ng_ClearErrorContext);
    BIND(terminate,espeak_ng_Terminate);
    BIND(list,espeak_ListVoices);
#undef BIND
    espeak_ng_ERROR_CONTEXT context=NULL;
    api.initialize_path(NULL);
    espeak_ng_STATUS initialized=api.initialize(&context);
    api.clear_error(&context);
    if (initialized!=ENS_OK) {
        fprintf(stderr,"voice catalog: eSpeakNG initialization failed (%d)\n",initialized);
        /* A failed provider owns any partial state until this one-shot process exits. */
        dlclose(library); return QV_CATALOG_PROVIDER_ERROR;
    }
    const espeak_VOICE **available=api.list(NULL);
    enum qv_catalog_status status=QV_CATALOG_OK;
    struct qv_voice_input *voices=calloc(QV_CATALOG_MAX_VOICES,sizeof(*voices));
    struct qv_language_input *languages=calloc(QV_CATALOG_MAX_VOICES*QV_CATALOG_MAX_LANGUAGES,sizeof(*languages));
    size_t count=0;
    if (!voices||!languages) status=QV_CATALOG_NO_MEMORY;
    else if (!available) status=QV_CATALOG_PROVIDER_ERROR;
    else {
        while (count<QV_CATALOG_MAX_VOICES && available[count]) count++;
        if (count==QV_CATALOG_MAX_VOICES && available[count]) status=QV_CATALOG_TOO_LARGE;
        for (size_t i=0;i<count && status==QV_CATALOG_OK;i++) {
            const espeak_VOICE *v=available[i];
            struct qv_language_input *entries=languages+i*QV_CATALOG_MAX_LANGUAGES;
            voices[i]=(struct qv_voice_input){v->identifier,v->name,v->gender,entries,0};
            const unsigned char *p=(const unsigned char *)v->languages;
            if (!p) { status=QV_CATALOG_BAD_DATA; break; }
            while (*p) {
                if (voices[i].language_count==QV_CATALOG_MAX_LANGUAGES) { status=QV_CATALOG_TOO_LARGE; break; }
                uint32_t priority=*p++;
                size_t length=strnlen((const char *)p,QV_CATALOG_MAX_STRING+1);
                if (!length||length>QV_CATALOG_MAX_STRING) { status=QV_CATALOG_BAD_DATA; break; }
                entries[voices[i].language_count++]=(struct qv_language_input){(const char *)p,priority};
                p+=length+1;
            }
        }
        if (status==QV_CATALOG_OK) status=qv_catalog_encode(voices,count,out,QV_CATALOG_MAX_BYTES,written);
    }
    free(languages); free(voices);
    espeak_ng_STATUS ended=api.terminate();
    dlclose(library);
    if (ended!=ENS_OK) { *written=0; status=QV_CATALOG_PROVIDER_ERROR; }
    return status;
}

int main(int argc, char **argv)
{
    (void)argv;
    if (argc!=1) { fputs("usage: qv-voice-catalog (binary catalog on stdout)\n",stderr); return 64; }
    uint8_t *bytes=malloc(QV_CATALOG_MAX_BYTES);
    if (!bytes) return QV_CATALOG_NO_MEMORY;
    size_t written=0;
    enum qv_catalog_status status=collect(bytes,&written);
    if (status!=QV_CATALOG_OK) {
        enum qv_catalog_status encoded=qv_catalog_error(status,bytes,QV_CATALOG_MAX_BYTES,&written);
        if (encoded!=QV_CATALOG_OK) { free(bytes); return encoded; }
    }
    int failed=fwrite(bytes,1,written,stdout)!=written || fflush(stdout)!=0;
    free(bytes);
    return failed?74:(int)status;
}
