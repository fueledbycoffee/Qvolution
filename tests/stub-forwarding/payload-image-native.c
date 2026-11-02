#define _GNU_SOURCE
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include "payload-image.h"
int main(void) {
 char root[] = "/tmp/qv-payload-XXXXXX"; assert(mkdtemp(root));
 char a[256], b[256], alias[256];
 snprintf(a,sizeof(a),"%s/reviewed",root); snprintf(b,sizeof(b),"%s/copied",root); snprintf(alias,sizeof(alias),"%s/alias",root);
 FILE *f=fopen(a,"wb"); assert(f); for(int i=0;i<32769;i++) fputc(i % 251,f); assert(!fclose(f));
 f=fopen(b,"wb"); assert(f); for(int i=0;i<32769;i++) fputc(i % 251,f); assert(!fclose(f));
 assert(symlink(a,alias)==0);
 assert(qvSamePayloadImage(a,b)); assert(qvSamePayloadImage(a,alias));
 f=fopen(b,"r+b"); assert(f); assert(!fseek(f,32768,SEEK_SET)); fputc(255,f); assert(!fclose(f));
 assert(!qvSamePayloadImage(a,b)); assert(!qvSamePayloadImage(a,root));
 assert(!qvSamePayloadImage(a,"/nonexistent/qv-payload")); assert(!qvSamePayloadImage(a,NULL));
 puts("PASS: identical copied payload accepted; changed final byte, missing image and directory rejected");
}
