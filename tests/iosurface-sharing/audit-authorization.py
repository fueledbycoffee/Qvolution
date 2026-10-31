"""Execute exact broker authorization prefix with task_info boundary adapters.
This cannot prove actual Mach audit trailers or foreign task-right enforcement.
"""
from pathlib import Path
import json,subprocess,os
root=Path(os.environ['DARLING_IOSURFACE_TEST_ROOT'])
here=Path(os.environ['DARLING_IOSURFACE_TEST_OUT'])
src=(root/'src/external/iokitd/src/IOSurfaceRoot.mm').resolve().read_text()
start=src.index('    if (!DarlingSurfaceRequestValid(&r))',src.index('kern_return_t IOSurfaceRoot::execute'))
end=src.index('    auto process = m_processes.find',start)
preamble=r'''
#include <IOSurface/IOSurfaceDarlingIPC.h>
extern "C" {void abort(void);int printf(const char*,...); }
#define check(x) do {if(!(x))abort();}while(0)
static kern_return_t infoResult=KERN_SUCCESS;
static mach_msg_type_number_t infoCount=TASK_AUDIT_TOKEN_COUNT;
static audit_token_t returnedAudit;
static unsigned infoCalls;
kern_return_t task_info(task_name_t task,task_flavor_t flavor,task_info_t info,mach_msg_type_number_t *count){
 (void)task;check(flavor==TASK_AUDIT_TOKEN);++infoCalls;
 memcpy(info,&returnedAudit,sizeof(returnedAudit));*count=infoCount;return infoResult;
}
static kern_return_t authorize(const DarlingSurfaceRequest& r,const audit_token_t& audit){
'''
test=r'''
 return KERN_SUCCESS;
}
int main(){
 DarlingSurfaceRequest r{};r.header.msgh_size=sizeof(r);r.header.msgh_bits=MACH_MSGH_BITS_COMPLEX;
 r.body.msgh_descriptor_count=2;r.task.type=r.capability.type=MACH_MSG_PORT_DESCRIPTOR;
 r.task.name=1;r.task.disposition=MACH_MSG_TYPE_PORT_SEND;r.version=DARLING_SURFACE_LAYOUT_VERSION;r.operation=DarlingSurfaceCreate;
 audit_token_t audit{};for(unsigned i=0;i<sizeof(audit.val)/sizeof(audit.val[0]);++i)audit.val[i]=i+1;
 returnedAudit=audit;check(authorize(r,audit)==KERN_SUCCESS);
 infoResult=KERN_FAILURE;check(authorize(r,audit)==KERN_PROTECTION_FAILURE);infoResult=KERN_SUCCESS;
 infoCount=TASK_AUDIT_TOKEN_COUNT-1;check(authorize(r,audit)==KERN_PROTECTION_FAILURE);
 infoCount=TASK_AUDIT_TOKEN_COUNT+1;check(authorize(r,audit)==KERN_PROTECTION_FAILURE);infoCount=TASK_AUDIT_TOKEN_COUNT;
 for(unsigned i=0;i<sizeof(audit.val)/sizeof(audit.val[0]);++i){returnedAudit=audit;returnedAudit.val[i]^=1;check(authorize(r,audit)==KERN_PROTECTION_FAILURE);}
 returnedAudit=audit;check(authorize(r,audit)==KERN_SUCCESS);
 unsigned before=infoCalls;r.header.msgh_size--;check(authorize(r,audit)==KERN_INVALID_ARGUMENT&&infoCalls==before);
 printf("actual authorization prefix: task_info failure/count mismatch, every audit word mismatch, exact match and malformed-before-task-info passed\n");
}
'''
probe=here/'audit-authorization-generated.cpp';probe.write_text(preamble+src[start:end]+test)
args=json.loads((here/'compile-typed-callers.c.json').read_text())[:-1];out=[];i=0
while i<len(args):
 if args[i] in ['-target','-B']:i+=2;continue
 if args[i]=='-fsyntax-only' or args[i].startswith('-mmacosx-version-min'):i+=1;continue
 out.append(args[i]);i+=1
subprocess.run(out+['-Wno-macro-redefined','-c',str(probe),'-o',str(here/'audit-authorization.o')],check=True)
subprocess.run(['clang++',str(here/'audit-authorization.o'),'-o',str(here/'audit-authorization')],check=True)
r=subprocess.run([str(here/'audit-authorization')],check=True,text=True,stdout=subprocess.PIPE)
(here/'audit-authorization.log').write_text(r.stdout);print(r.stdout,end='')
