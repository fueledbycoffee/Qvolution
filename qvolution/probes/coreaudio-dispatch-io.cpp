// Original internal event source linked to the actual registry and real libdispatch.
#include "AudioPropertyListeners.h"
#include <Block.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

static void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr,"FAIL real-dispatch IO: %s\n",message);
        std::fflush(stderr);
        std::_Exit(1);
    }
}
static const AudioObjectPropertyAddress address{
    kAudioDevicePropertyDeviceIsRunning,kAudioObjectPropertyScopeGlobal,0};
static int contextKey;
struct EventSource {
    AudioPropertyListeners listeners{314};
    std::atomic<int> state{0};
    OSStatus emit() {
        auto stackAddress=address;
        AudioPropertyListeners::Change change;
        auto status=listeners.prepare(stackAddress,change,AudioPropertyListeners::Context::IO);
        if (status) return status;
        stackAddress.mSelector=0;
        ++state;
        change.deliver();
        return 0;
    }
};
struct Capture {
    std::atomic<int>* destroyed;
    ~Capture() { ++*destroyed; }
};
static AudioObjectPropertyListenerBlock makeListener(std::shared_ptr<Capture> held,
        EventSource* source,std::atomic<int>* calls,
        void* expectedContext,bool global,std::atomic<dispatch_queue_t>* globalBaseline) {
    return Block_copy(^(UInt32 count,const AudioObjectPropertyAddress* payload) {
        require(bool(held),"owned capture absent");
        require(count==1 && payload && payload->mSelector==address.mSelector &&
            payload->mScope==address.mScope && payload->mElement==address.mElement,
            "owned address payload mismatch");
        require(source->state.load()==1,"callback before state publication");
        if (global) require(dispatch_get_current_queue()==globalBaseline->load(),
            "global callback context differs from direct sync baseline");
        else require(dispatch_get_specific(&contextKey)==expectedContext,"serial/descendant context mismatch");
        ++*calls;
    });
}
int main(int argc,char** argv) {
    if (argc!=2) { std::fprintf(stderr,"usage: %s current|descendant|global|reverse-reject|unknown-reject\n",argv[0]); return 2; }
    bool current=!std::strcmp(argv[1],"current");
    bool descendant=!std::strcmp(argv[1],"descendant");
    bool global=!std::strcmp(argv[1],"global");
    bool reverse=!std::strcmp(argv[1],"reverse-reject");
    bool unknown=!std::strcmp(argv[1],"unknown-reject");
    if (!(current||descendant||global||reverse||unknown)) return 2;
    bool reject=reverse||unknown;
    int targetToken=0,emitterToken=0;
    dispatch_queue_t listenerQueue=nullptr,emitter=nullptr,ownedEmitter=nullptr;
    if (global) {
        listenerQueue=dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT,0);
        ownedEmitter=emitter=dispatch_queue_create("original.io.global-emitter",DISPATCH_QUEUE_SERIAL);
    } else if (reverse) {
        ownedEmitter=emitter=dispatch_queue_create("original.io.parent",DISPATCH_QUEUE_SERIAL);
        listenerQueue=dispatch_queue_create("original.io.child-listener",DISPATCH_QUEUE_SERIAL);
        require(emitter && listenerQueue,"queue creation failed");
        dispatch_set_target_queue(listenerQueue,emitter);
    } else {
        listenerQueue=dispatch_queue_create("original.io.listener",DISPATCH_QUEUE_SERIAL);
        if (current) emitter=listenerQueue;
        else {
            ownedEmitter=emitter=dispatch_queue_create("original.io.emitter",DISPATCH_QUEUE_SERIAL);
            require(emitter && listenerQueue,"queue creation failed");
            if (descendant) dispatch_set_target_queue(emitter,listenerQueue);
        }
    }
    require(listenerQueue && emitter,"queue prerequisite unavailable");
    if (!global) dispatch_queue_set_specific(listenerQueue,&contextKey,&targetToken,nullptr);
    if (emitter!=listenerQueue) dispatch_queue_set_specific(emitter,&contextKey,&emitterToken,nullptr);
    auto expectedContext=current?&targetToken:&emitterToken;
    EventSource source;
    std::atomic<int> calls{0},destroyed{0},baselineCalls{0};
    std::atomic<dispatch_queue_t> globalBaseline{nullptr};
    auto capture=std::make_shared<Capture>(); capture->destroyed=&destroyed;
    auto listener=makeListener(capture,&source,&calls,expectedContext,global,&globalBaseline);
    require(listener!=nullptr,"block copy failed");
    auto registration=address;
    require(!source.listeners.add(registration,listenerQueue,listener),"registration failed");
    registration.mSelector=0;
    capture.reset(); Block_release(listener);
    // Current mode now has no caller queue owner; registry ownership is required.
    if (!global) dispatch_release(listenerQueue);
    require(destroyed.load()==0,"registration failed to own block capture");
    auto completed=dispatch_semaphore_create(0);
    require(completed!=nullptr,"semaphore creation failed");
    std::atomic<OSStatus> status{0};
    auto sp=&source; auto cp=&calls; auto result=&status;
    auto bp=&globalBaseline; auto bcp=&baselineCalls;
    dispatch_async(emitter,^{
        if (global) {
            dispatch_sync(listenerQueue,^{ bp->store(dispatch_get_current_queue()); ++*bcp; });
            require(bcp->load()==1 && bp->load()!=nullptr,"direct global sync baseline incomplete");
        }
        auto value=sp->emit(); result->store(value);
        require(value==(reject?kAudioHardwareUnsupportedOperationError:0),"unexpected IO preparation result");
        require(cp->load()==(reject?0:1),"IO callback did not finish before emit returned");
        dispatch_semaphore_signal(completed);
    });
    require(!dispatch_semaphore_wait(completed,dispatch_time(DISPATCH_TIME_NOW,3*NSEC_PER_SEC)),
        "bounded IO completion timeout");
    // Drain the emitter before destroying any stack context or semaphore.
    dispatch_sync(emitter,^{});
    require(source.state.load()==(reject?0:1),"rejected event mutated state");
    require(destroyed.load()==0,"capture released before matching removal");
    require(!source.listeners.remove(address,listenerQueue,listener),"matching removal failed");
    require(destroyed.load()==1,"matching removal did not release owned capture");
    require(!source.emit(),"empty registry emission failed");
    require(calls.load()==(reject?0:1),"callback after matching removal");
    dispatch_release(completed);
    if (ownedEmitter) dispatch_release(ownedEmitter);
    const char* baselineIdentity=!global?"not-applicable":
        globalBaseline.load()==listenerQueue?"supplied-global":
        globalBaseline.load()==emitter?"emitter":"other-runtime-queue";
    std::printf("PASS_REAL_DISPATCH_IO mode=%s status=%d callbacks=%d capture_destroyed=%d global_baseline=%s\n",
        argv[1],int(status.load()),calls.load(),destroyed.load(),baselineIdentity);
    return 0;
}
