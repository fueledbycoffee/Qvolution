/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 Darling developers */
#include "catalog.h"
#include <string.h>
#include <strings.h>
#include <unicode/uloc.h>
#include <unicode/uscript.h>

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
    if (count>1 && strlen(parts[1])==4) {
        UScriptCode code; UErrorCode script_error=U_ZERO_ERROR;
        int32_t found=uscript_getCode(parts[1],&code,1,&script_error);
        if (U_FAILURE(script_error)||found!=1||code==USCRIPT_UNKNOWN||
            strcasecmp(parts[1],uscript_getShortName(code))) return QV_LANGUAGE_UNSUPPORTED;
        region=2;
    }
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
