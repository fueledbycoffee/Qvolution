"""Run exact broker lifecycle methods with explicit native boundary adapters.
No Mach rights, public surface creation or interprocess claim is simulated as a pass.
"""
from pathlib import Path
import subprocess,os
root=Path(os.environ['DARLING_IOSURFACE_TEST_ROOT'])
here=Path(os.environ['DARLING_IOSURFACE_TEST_OUT'])
source=(root/'src/external/iokitd/src/IOSurfaceRoot.mm').resolve().read_text()
def method(marker):
 start=source.index(marker); opening=source.index('{',start); depth=1; end=opening+1
 while depth:
  depth+=(source[end]=='{')-(source[end]=='}');end+=1
 return source[start:end]
preamble=r'''
#include <memory>
#include <unordered_map>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <utility>
#include <new>
#include <cstdlib>
static bool failNextAllocation=false;
void* operator new(std::size_t n){if(failNextAllocation){failNextAllocation=false;throw std::bad_alloc();}void *p=std::malloc(n);if(!p)throw std::bad_alloc();return p;}
void operator delete(void *p) noexcept{std::free(p);}
void operator delete(void *p,std::size_t) noexcept{std::free(p);}
#include <IOSurface/IOSurfaceDarlingLayout.h>
using mach_port_t = uint32_t; using mach_vm_address_t = uint64_t; using mach_vm_size_t = uint64_t;
using kern_return_t = int; using vm_prot_t = int;
constexpr int KERN_SUCCESS=0,KERN_FAILURE=5,KERN_INVALID_ARGUMENT=4,KERN_RESOURCE_SHORTAGE=3,KERN_PROTECTION_FAILURE=2;
constexpr int VM_PROT_READ=1,VM_PROT_WRITE=2,VM_FLAGS_ANYWHERE=1,VM_INHERIT_NONE=2;
struct TestSource { int suspended=1,cancelled=0,released=0; };
using dispatch_source_t=TestSource*;
static void dispatch_source_cancel(TestSource *s){ s->cancelled=1; }
static void dispatch_resume(TestSource *s){ assert(s->suspended==1);s->suspended=0; }
static void dispatch_release(TestSource *s){ assert(!s->suspended && s->cancelled);s->released++; }
static int unmap_failure=0,unmaps=0,task_deallocations=0,remaps=0,remap_failure=0,remap_protection=3;
static kern_return_t mach_vm_deallocate(mach_port_t,uint64_t,uint64_t){ ++unmaps;return unmap_failure?KERN_FAILURE:KERN_SUCCESS; }
static mach_port_t mach_task_self(){return 1;}
static kern_return_t mach_port_deallocate(mach_port_t,mach_port_t){++task_deallocations;return 0;}
static kern_return_t mach_vm_remap(mach_port_t,uint64_t *address,uint64_t,uint64_t,int,
 mach_port_t,uint64_t,bool,int *cur,int *max,int){++remaps;if(remap_failure)return KERN_FAILURE;*address=0x9000;*cur=*max=remap_protection;return 0;}
struct IOSurface {
 DarlingSurfaceLayout metadata{};uint32_t seed=0,exports=0;uint64_t manualUses=0;
 uint64_t mappedSize()const{return 4096;}uint64_t address()const{return 0x8000;}
 const DarlingSurfaceLayout& layout()const{return metadata;}
};
struct DarlingSurfaceReply {uint32_t surfaceID=0,seed=0,useCount=0,inUse=0;uint64_t address=0,mappedSize=0;DarlingSurfaceLayout layout{};};
struct IOSurfaceRoot {
 struct Mapping {std::shared_ptr<IOSurface> surface;uint64_t address=0;uint32_t references=0,uses=0,readLocks=0,writeLocks=0;};
 struct Process {mach_port_t task=0;uint64_t generation=0;TestSource *source=nullptr;std::unordered_map<uint32_t,Mapping> mappings;};
 struct Export {std::shared_ptr<IOSurface> surface;TestSource *source;mach_port_t task;uint32_t surfaceID;uint64_t generation;bool publicExport;};
 std::unordered_map<mach_port_t,Process> m_processes;std::unordered_map<uint32_t,std::weak_ptr<IOSurface>> m_surfaces;
 std::unordered_map<mach_port_t,Export> m_exports;
 void fillReply(const Mapping&,uint32_t,DarlingSurfaceReply&);
 kern_return_t mapSurface(Process&,uint32_t,std::shared_ptr<IOSurface>,DarlingSurfaceReply&);
 kern_return_t releaseMapping(Process&,uint32_t);
 void processDied(mach_port_t);void exportDied(mach_port_t);
};
'''
test=r'''
int main(){
 TestSource suspended;disposeUnresumedSource(&suspended);assert(suspended.released==1);
 IOSurfaceRoot root;
 IOSurfaceRoot::Process unpublished;unpublished.task=99;auto storage=std::make_shared<IOSurface>();DarlingSurfaceReply unpublishedReply;
 const int firstRemaps=remaps;failNextAllocation=true;
 try{root.mapSurface(unpublished,99,storage,unpublishedReply);assert(false);}catch(const std::bad_alloc&){}
 assert(remaps==firstRemaps&&unpublished.mappings.empty());
 remap_protection=1;unmap_failure=1;
 assert(root.mapSurface(unpublished,99,storage,unpublishedReply)==KERN_PROTECTION_FAILURE);
 assert(unpublished.mappings.count(99)&&unpublished.mappings.at(99).references==0&&unpublished.mappings.at(99).address==0x9000);
 remap_protection=3;unmap_failure=0;
 assert(root.mapSurface(unpublished,99,storage,unpublishedReply)==KERN_SUCCESS);
 assert(root.releaseMapping(unpublished,99)==KERN_SUCCESS&&unpublished.mappings.empty());
 remap_failure=1;const int beforeFailureUnmaps=unmaps;
 assert(root.mapSurface(unpublished,99,storage,unpublishedReply)==KERN_FAILURE&&unpublished.mappings.empty()&&unmaps==beforeFailureUnmaps);
 remap_failure=0;
 TestSource processSource;processSource.suspended=0;
 auto s=std::make_shared<IOSurface>();std::weak_ptr<IOSurface> weak=s;
 IOSurfaceRoot::Process process;process.task=11;process.generation=7;process.source=&processSource;
 process.mappings.emplace(3,IOSurfaceRoot::Mapping{s,0x4000,1});root.m_surfaces[3]=s;
 unmap_failure=1;assert(root.releaseMapping(process,3)==KERN_FAILURE);
 assert(process.mappings.count(3)&&process.mappings.at(3).references==0&&!weak.expired());
 DarlingSurfaceReply reply;assert(root.mapSurface(process,3,s,reply)==KERN_FAILURE);
 assert(process.mappings.at(3).references==0);
 unmap_failure=0;assert(root.mapSurface(process,3,s,reply)==KERN_SUCCESS);
 assert(process.mappings.at(3).references==1&&reply.address==0x9000);
 root.m_processes.emplace(11,std::move(process));
 TestSource lease;lease.suspended=0;root.m_exports.emplace(20,IOSurfaceRoot::Export{s,&lease,11,3,7,false});
 s.reset(); // a reply was enqueued; receiver destroys its unread descriptor
 root.exportDied(20); // actual no-senders transition method
 assert(weak.expired()&&root.m_surfaces.empty()&&root.m_processes.at(11).mappings.empty());
 // Death before an old lease callback; reuse the same scalar task name.
 s=std::make_shared<IOSurface>();root.m_processes.at(11).mappings.emplace(4,IOSurfaceRoot::Mapping{s,0x5000,1});
 root.m_surfaces[4]=s;TestSource oldLease;oldLease.suspended=0;
 root.m_exports.emplace(21,IOSurfaceRoot::Export{s,&oldLease,11,4,7,false});s.reset();
 root.processDied(11);
 TestSource newSource;newSource.suspended=0;IOSurfaceRoot::Process replacement;replacement.task=11;replacement.generation=8;replacement.source=&newSource;
 auto fresh=std::make_shared<IOSurface>();replacement.mappings.emplace(4,IOSurfaceRoot::Mapping{fresh,0x6000,1});
 root.m_processes.emplace(11,std::move(replacement));
 const int before=unmaps;root.exportDied(21);
 assert(root.m_processes.at(11).mappings.at(4).references==1&&unmaps==before);
 // Last public export prunes its weak ID without requiring process death.
 auto portOnly=std::make_shared<IOSurface>();portOnly->exports=1;std::weak_ptr<IOSurface> portWeak=portOnly;
 TestSource publicSource;publicSource.suspended=0;root.m_surfaces[5]=portOnly;
 root.m_exports.emplace(22,IOSurfaceRoot::Export{portOnly,&publicSource,0,5,0,true});portOnly.reset();
 root.exportDied(22);assert(portWeak.expired()&&!root.m_surfaces.count(5));
 std::puts("actual broker methods: suspended-source cleanup, pre-map OOM, protection/failed-unmap ownership/retry, lost-reply lease cleanup, death/name reuse, final-export ID pruning passed");
}
'''
markers=['static void disposeUnresumedSource','void IOSurfaceRoot::fillReply','kern_return_t IOSurfaceRoot::mapSurface','kern_return_t IOSurfaceRoot::releaseMapping','void IOSurfaceRoot::processDied','void IOSurfaceRoot::exportDied']
probe=here/'broker-lifetime-generated.cpp';probe.write_text(preamble+'\n'.join(method(x) for x in markers)+test)
subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-UNDEBUG','-I'+str(root/'src/frameworks/IOSurface/include'),str(probe),'-o',str(here/'broker-lifetime')],check=True)
r=subprocess.run([str(here/'broker-lifetime')],check=True,text=True,stdout=subprocess.PIPE)
(here/'broker-lifetime.log').write_text(r.stdout);print(r.stdout,end='')
