#include <espeak-ng/espeak_ng.h>
#include "test-checks.h"
#include <stdlib.h>
#include <string.h>

static int state;
static char name[]="Fixture \xc3\xa9";
static char languages[]="\002en-gb\000\003en\000\004en-uk-north\000";
static espeak_VOICE voice={.identifier="fixture/key",.name=name,.gender=2,.languages=languages};
static espeak_VOICE duplicate={.identifier="fixture/key",.name="Distinct name",.gender=1,.languages=languages};
static const espeak_VOICE *voices[]={&voice,NULL,NULL};

void espeak_ng_InitializePath(const char *path) { CHECK(state==0&&!path);state=1; }
espeak_ng_STATUS espeak_ng_Initialize(espeak_ng_ERROR_CONTEXT *context)
{
    CHECK(state==1&&context&&!*context);state=2;
    const char *mode=getenv("QV_TEST_PROVIDER_CASE");
    return mode&&!strcmp(mode,"init-error")?ENS_NOT_INITIALIZED:ENS_OK;
}
void espeak_ng_ClearErrorContext(espeak_ng_ERROR_CONTEXT *context)
{ CHECK(state==2&&context);state=3;*context=NULL; }
const espeak_VOICE **espeak_ListVoices(espeak_VOICE *spec)
{
    CHECK(state==3&&!spec);state=4;
    const char *mode=getenv("QV_TEST_PROVIDER_CASE");
    if (mode&&!strcmp(mode,"list-error")) return NULL;
    if (mode&&!strcmp(mode,"empty")) voices[0]=NULL;
    if (mode&&!strcmp(mode,"duplicate")) voices[1]=&duplicate;
    if (mode&&!strcmp(mode,"malformed")) voice.name="\xc0\xaf";
    return voices;
}
espeak_ng_STATUS espeak_ng_Terminate(void)
{
    CHECK(state==4);state=5;memset(name,'X',sizeof(name)-1);memset(languages,0,sizeof(languages));
    const char *mode=getenv("QV_TEST_PROVIDER_CASE");
    return mode&&!strcmp(mode,"terminate-error")?ENS_NOT_INITIALIZED:ENS_OK;
}
espeak_ng_STATUS espeak_ng_InitializeOutput(espeak_ng_OUTPUT_MODE mode,int length,const char *device)
{ (void)mode;(void)length;(void)device;abort(); }
