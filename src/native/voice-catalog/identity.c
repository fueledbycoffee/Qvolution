/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "catalog.h"
#include <string.h>

enum qv_catalog_status qv_catalog_project_identifier(const char *identifier, char *out, size_t capacity)
{
    static const char prefix[]=QV_CATALOG_IDENTIFIER_PREFIX,hex[]="0123456789ABCDEF";
    if (!identifier||!out) return QV_CATALOG_BAD_DATA;
    size_t length=strnlen(identifier,QV_CATALOG_MAX_STRING+1);
    if (!length||length>QV_CATALOG_MAX_STRING) return QV_CATALOG_BAD_DATA;
    char value[QV_CATALOG_MAX_IDENTIFIER+1];
    size_t n=sizeof(prefix)-1; memcpy(value,prefix,n);
    for (const unsigned char *p=(const unsigned char *)identifier;*p;p++) {
        unsigned char c=*p;
        if ((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='.'||c=='_'||c=='~') value[n++]=c;
        else { value[n++]='%';value[n++]=hex[c>>4];value[n++]=hex[c&15]; }
    }
    if (n>=capacity) return QV_CATALOG_TOO_LARGE;
    value[n]=0;memcpy(out,value,n+1);return QV_CATALOG_OK;
}
