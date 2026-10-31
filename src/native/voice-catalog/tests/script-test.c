#include "catalog.h"
#include <stdio.h>
#include <string.h>
int main(void) {
    const char *raw[]={"en-Abcd","en-XXXX","en-Qaaa","en-Zzzz","zh-hant-tw"};
    int failed=0;
    for (unsigned i=0;i<sizeof(raw)/sizeof(raw[0]);i++) {
        char tag[128]; enum qv_language_status status=qv_canonical_language(raw[i],tag,sizeof(tag));
        printf("%s status=%u canonical=%s\n",raw[i],status,tag);
        if (i<4 && (status!=QV_LANGUAGE_UNSUPPORTED || *tag)) failed=1;
        if (i==4 && (status!=QV_LANGUAGE_MAPPED || strcmp(tag,"zh-Hant-TW"))) failed=1;
    }
    return failed;
}
