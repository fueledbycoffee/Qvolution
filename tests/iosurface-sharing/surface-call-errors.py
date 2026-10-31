"""Execute actual framework RPC helper against native Mach boundary adapters."""
from pathlib import Path
import subprocess,json,os
root=Path(os.environ['DARLING_IOSURFACE_TEST_ROOT'])
here=Path(os.environ['DARLING_IOSURFACE_TEST_OUT'])
src=(root/'src/frameworks/IOSurface/src/IOSurfaceObjC.m').resolve().read_text()
start=src.index('static kern_return_t surfaceCall');opening=src.index('{',start);depth=1;end=opening+1
while depth:depth+=(src[end]=='{')-(src[end]=='}');end+=1
preamble=r'''
#include <IOSurface/IOSurfaceDarlingIPC.h>

extern void abort(void);extern int printf(const char*,...);
#define check(x) do {if(!(x))abort();}while(0)
typedef const void *CFDataRef;typedef long CFIndex;
static CFIndex CFDataGetLength(CFDataRef x){(void)x;return 0;}
static const uint8_t *CFDataGetBytePtr(CFDataRef x){return x;}
static mach_port_t g_surfaceService=9;
mach_port_t mach_task_self_=1;
static int mode,destroyed_messages,received_rights_dropped,ports_destroyed;
kern_return_t mach_port_allocate(ipc_space_t task,mach_port_right_t right,mach_port_name_t *port){(void)task;(void)right;*port=41;return KERN_SUCCESS;}
kern_return_t mach_port_destroy(ipc_space_t task,mach_port_name_t port){(void)task;(void)port;++ports_destroyed;return KERN_SUCCESS;}
void mach_msg_destroy(mach_msg_header_t *header){
 ++destroyed_messages;check(header->msgh_id==DARLING_SURFACE_MSG_ID+100);
 struct DarlingSurfaceReply *r=(struct DarlingSurfaceReply*)header;
 if(MACH_PORT_VALID(r->capability.name))++received_rights_dropped;
}
mach_msg_return_t mach_msg(mach_msg_header_t *header,mach_msg_option_t option,mach_msg_size_t send,
 mach_msg_size_t receive,mach_port_name_t name,mach_msg_timeout_t timeout,mach_port_name_t notify){
 (void)option;(void)send;(void)receive;(void)name;(void)timeout;(void)notify;
 if(mode==2)return MACH_SEND_TIMED_OUT;
 struct DarlingSurfaceReply *r=(struct DarlingSurfaceReply*)header;memset(r,0,sizeof(*r));
 r->header.msgh_id=DARLING_SURFACE_MSG_ID+100;r->header.msgh_size=sizeof(*r);r->header.msgh_bits=MACH_MSGH_BITS_COMPLEX;
 r->body.msgh_descriptor_count=1;r->capability.type=MACH_MSG_PORT_DESCRIPTOR;r->capability.disposition=MACH_MSG_TYPE_PORT_SEND;
 r->capability.name=77;r->version=DARLING_SURFACE_LAYOUT_VERSION;
 if(mode==1)return MACH_RCV_BODY_ERROR|MACH_MSG_IPC_SPACE;
 if(mode==3)r->version++;
 return MACH_MSG_SUCCESS;
}
'''
test=r'''
int main(void){
 struct DarlingSurfaceReply r;
 mode=1;check(surfaceCall(DarlingSurfaceCreate,0,0,MACH_PORT_NULL,NULL,&r)!=KERN_SUCCESS);
 check(destroyed_messages==1&&received_rights_dropped==1&&ports_destroyed==1);
 mode=2;check(surfaceCall(DarlingSurfaceCreate,0,0,MACH_PORT_NULL,NULL,&r)==MACH_SEND_TIMED_OUT);
 check(destroyed_messages==1&&received_rights_dropped==1&&ports_destroyed==2);
 mode=3;check(surfaceCall(DarlingSurfaceCreate,0,0,MACH_PORT_NULL,NULL,&r)!=KERN_SUCCESS);
 check(destroyed_messages==2&&received_rights_dropped==2&&ports_destroyed==3);
 mode=0;check(surfaceCall(DarlingSurfaceCreate,0,0,MACH_PORT_NULL,NULL,&r)==KERN_SUCCESS&&r.capability.name==77);
 check(destroyed_messages==3&&received_rights_dropped==2&&ports_destroyed==4);
 printf("actual surfaceCall: partial receive body cleanup, outbound send-error preservation, invalid reply destruction and lease ownership transfer passed\n");
}
'''
probe=here/'surface-call-errors-generated.c';probe.write_text(preamble+src[start:end]+test)
args=json.loads((here/'compile-typed-callers.c.json').read_text());args=args[:-1];out=[];i=0
while i<len(args):
 if args[i] in ['-target','-B']:i+=2;continue
 if args[i]=='-fsyntax-only' or args[i].startswith('-mmacosx-version-min'):i+=1;continue
 out.append(args[i]);i+=1
subprocess.run(out+['-Wno-macro-redefined','-c',str(probe),'-o',str(here/'surface-call-errors.o')],check=True)
subprocess.run(['clang',str(here/'surface-call-errors.o'),'-o',str(here/'surface-call-errors')],check=True)
r=subprocess.run([str(here/'surface-call-errors')],check=True,text=True,stdout=subprocess.PIPE)
(here/'surface-call-errors.log').write_text(r.stdout);print(r.stdout,end='')
