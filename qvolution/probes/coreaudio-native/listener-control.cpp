// Compiles the actual production registry and AudioHardwareImpl unchanged.
#include "AudioHardwareImpl.h"
#include <CarbonCore/MacErrors.h>
#include <Block.h>
#include <cassert>
#include <atomic>
#include <cstdlib>
#include <cstdio>
#include <new>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <type_traits>
#include <chrono>
static_assert(!std::is_reference<decltype(std::declval<const AudioHardwareImpl&>().asbd())>::value,
    "ASBD readers must own a snapshot, never an unlocked reference");
struct AllocationGate {
    std::mutex mutex; std::condition_variable changed; bool paused=false,resume=false;
    void pause() {
        std::unique_lock<std::mutex> lock(mutex); paused=true; changed.notify_all();
        assert(changed.wait_for(lock,std::chrono::seconds(5),[&]{return resume;}));
    }
};
static thread_local AllocationGate* allocationGate=nullptr;
static thread_local int pauseAllocationAfter=-1;
static thread_local int failAfter=-1;
static thread_local bool failCopy=false;
extern "C" void* __real__Block_copy(const void*);
extern "C" void* __wrap__Block_copy(const void* block) {
    if (failCopy) return nullptr;
    return __real__Block_copy(block);
}
void* operator new(size_t size) {
    if(pauseAllocationAfter==0) { pauseAllocationAfter=-1; allocationGate->pause(); }
    else if(pauseAllocationAfter>0) --pauseAllocationAfter;
    if(failAfter==0) throw std::bad_alloc();
    if(failAfter>0) --failAfter;
    if(auto p=std::malloc(size)) return p; throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p,size_t) noexcept { std::free(p); }
struct Device : AudioHardwareImpl {
    Device() : AudioHardwareImpl(42) {}
    AudioHardwareStream* createStream(AudioDeviceIOProc,void*) override { return nullptr; }
    OSStatus emit(AudioObjectPropertyAddress a, bool io=false) {
        AudioPropertyListeners::Change change;
        auto status=preparePropertyChange(a,change,io?AudioPropertyListeners::Context::IO:AudioPropertyListeners::Context::Normal);
        if (!status) change.deliver(); return status;
    }
};
static AudioObjectPropertyAddress format{kAudioDevicePropertyStreamFormat,kAudioObjectPropertyScopeOutput,0};
static AudioStreamBasicDescription get(Device& d) {
    AudioStreamBasicDescription a{}; UInt32 size=sizeof(a);
    assert(!d.getPropertyData(&format,0,nullptr,&size,&a)); return a;
}
static OSStatus set(Device& d, AudioStreamBasicDescription a) { return d.setPropertyData(&format,0,nullptr,sizeof(a),&a); }
struct Count { int n=0; Device* device=nullptr; };
static OSStatus count(AudioObjectID id,UInt32 n,const AudioObjectPropertyAddress* a,void* opaque) {
    auto c=static_cast<Count*>(opaque); assert(id==42 && n==1 && a->mSelector==format.mSelector);
    ++c->n; if(c->device) assert(get(*c->device).mSampleRate==48000); return 0;
}
struct Capture { std::atomic<int>* destroyed; ~Capture() { ++*destroyed; } };
static AudioObjectPropertyListenerBlock capturing(std::shared_ptr<Capture> capture, int* count) {
    return Block_copy(^(UInt32 n,const AudioObjectPropertyAddress* payload) {
        assert(n==1 && payload->mSelector==format.mSelector && payload->mElement==0);
        assert(capture); ++*count;
    });
}
static void aliasControls() {
    Device target;
    AudioObjectPropertyAddress addresses[] = {
        {kAudioDevicePropertyStreamFormat,kAudioObjectPropertyScopeGlobal,0},
        {kAudioDevicePropertyStreamFormat,kAudioObjectPropertyScopeInput,0},
        {kAudioDevicePropertyStreamFormat,kAudioObjectPropertyScopeOutput,1}
    };
    auto queue=test_queue(); int calls[3]={};
    struct Alias { AudioObjectPropertyAddress address; int* calls; };
    Alias c{addresses[0],&calls[0]};
    auto proc=+[](AudioObjectID id,UInt32 n,const AudioObjectPropertyAddress* a,void* context)->OSStatus {
        auto expected=static_cast<Alias*>(context);
        assert(id==42 && n==1 && a->mSelector==expected->address.mSelector &&
            a->mScope==expected->address.mScope && a->mElement==expected->address.mElement);
        ++*expected->calls; return 0;
    };
    auto input=addresses[1], output=addresses[2]; auto cp=calls;
    AudioObjectPropertyListenerBlock direct=^(UInt32 n,const AudioObjectPropertyAddress* a) {
        assert(n==1 && a->mScope==input.mScope && a->mElement==input.mElement); ++cp[1];
    };
    AudioObjectPropertyListenerBlock queued=^(UInt32 n,const AudioObjectPropertyAddress* a) {
        assert(n==1 && a->mScope==output.mScope && a->mElement==output.mElement); ++cp[2];
    };
    assert(!target.addPropertyListener(&addresses[0],proc,&c));
    assert(!target.addPropertyListenerBlock(&addresses[1],nullptr,direct));
    assert(!target.addPropertyListenerBlock(&addresses[2],queue,queued));
    // Every accepted alias exposes the shared format and must observe its change.
    auto next=get(target); next.mSampleRate=48000;
    assert(!target.setPropertyData(&addresses[0],0,nullptr,sizeof(next),&next));
    test_drain(queue); assert(calls[0]==1 && calls[1]==1 && calls[2]==1);
    for(const auto& address : addresses) {
        AudioStreamBasicDescription read{}; UInt32 size=sizeof(read);
        assert(!target.getPropertyData(&address,0,nullptr,&size,&read) && read.mSampleRate==48000);
    }
    bool reachedSuccess=false;
    for(int failure=0;failure<32 && !reachedSuccess;++failure) {
        next.mSampleRate=44100;
        failAfter=failure;
        auto status=target.setPropertyData(&addresses[0],0,nullptr,sizeof(next),&next);
        failAfter=-1; test_drain(queue);
        if(status) assert(status==kAudioHardwareUnspecifiedError && get(target).mSampleRate==48000 &&
            calls[0]==1 && calls[1]==1 && calls[2]==1);
        else { assert(calls[0]==2 && calls[1]==2 && calls[2]==2); reachedSuccess=true; }
    }
    assert(reachedSuccess);
    assert(!target.removePropertyListener(&addresses[0],proc,&c));
    assert(!target.removePropertyListenerBlock(&addresses[1],nullptr,direct));
    assert(!target.removePropertyListenerBlock(&addresses[2],queue,queued)); dispatch_release(queue);
}
static void registrationPublicationControls() {
    Device target; Count original,added; AllocationGate gate;
    assert(!target.addPropertyListener(&format,count,&original));
    auto next=get(target); next.mSampleRate=48000;
    std::thread writer([&] {
        allocationGate=&gate;
        // One vector allocation snapshots the existing registration under lock;
        // the next allocation prepares its owned payload after unlocking.
        pauseAllocationAfter=1;
        assert(!set(target,next));
    });
    {
        std::unique_lock<std::mutex> lock(gate.mutex);
        assert(gate.changed.wait_for(lock,std::chrono::seconds(5),[&]{return gate.paused;}));
    }
    assert(!target.addPropertyListener(&format,count,&added));
    assert(get(target).mSampleRate==44100);
    { std::lock_guard<std::mutex> lock(gate.mutex); gate.resume=true; } gate.changed.notify_all();
    writer.join();
    assert(get(target).mSampleRate==48000 && original.n==1 && added.n==1);
}
static void snapshotControls() {
    Device target;
    const auto saved=target.asbd();
    auto next=saved; next.mSampleRate=48000;
    assert(!set(target,next) && saved.mSampleRate==44100);
    std::atomic<bool> started{false},finished{false};
    std::thread writer([&] {
        started=true;
        for(int i=0;i<10000;++i) {
            auto changing=next;
            changing.mChannelsPerFrame=i%2+1;
            changing.mBitsPerChannel=i%2?32:16;
            assert(!set(target,changing));
        }
        finished=true;
    });
    while(!started.load()) std::this_thread::yield();
    int reads=0;
    do {
        const auto current=target.asbd();
        assert(current.mBytesPerFrame==current.mChannelsPerFrame*(current.mBitsPerChannel/8) &&
            current.mBytesPerPacket==current.mBytesPerFrame && current.mFramesPerPacket==1);
        ++reads;
    } while(!finished.load());
    writer.join(); assert(reads>0);
}
static void cachedStreamControls() {
    struct IdleStream : AudioHardwareStream {
        IdleStream(AudioHardwareImpl* owner) : AudioHardwareStream(owner,false) {}
        void check() {
            auto next=m_hw->asbd(); next.mSampleRate=48000;
            assert(m_hw->setPropertyData(&format,0,nullptr,sizeof(next),&next)==kAudioHardwareUnsupportedOperationError);
        }
        void start() override { check(); }
        void stop() override { check(); }
        ~IdleStream() override { check(); }
    };
    struct StreamDevice : Device {
        AudioHardwareStream* createStream(AudioDeviceIOProc,void*) override {
            auto next=asbd(); next.mSampleRate=48000;
            assert(set(*this,next)==kAudioHardwareUnsupportedOperationError);
            return new IdleStream(this);
        }
    } target;
    AudioDeviceIOProc proc=+[](AudioObjectID,const AudioTimeStamp*,const AudioBufferList*,
        const AudioTimeStamp*,AudioBufferList*,const AudioTimeStamp*,void*)->OSStatus {return 0;};
    AudioDeviceIOProcID id=nullptr;
    Count events;
    assert(!target.addPropertyListener(&format,count,&events));
    assert(!target.createIOProcID(proc,nullptr,&id));
    assert(!target.start(id,nullptr,0));
    auto before=get(target), next=before; next.mSampleRate=48000;
    assert(!set(target,before) && events.n==0);
    assert(set(target,next)==kAudioHardwareUnsupportedOperationError &&
        get(target).mSampleRate==before.mSampleRate && events.n==0);
    assert(!target.stop(id));
    assert(set(target,next)==kAudioHardwareUnsupportedOperationError &&
        get(target).mSampleRate==before.mSampleRate && events.n==0);
    assert(!target.destroyIOProcID(id));
    assert(!set(target,next) && get(target).mSampleRate==48000 && events.n==1);
    // Failed/null construction and every allocation failure release the format
    // reservation after destroying any partially constructed idle stream.
    {
        Device failed; AudioDeviceIOProcID failedID=nullptr;
        assert(!failed.createIOProcID(proc,nullptr,&failedID));
        assert(failed.start(failedID,nullptr,0)==kAudioHardwareBadStreamError);
        auto next=get(failed); next.mSampleRate=48000; assert(!set(failed,next));
        assert(!failed.destroyIOProcID(failedID));
    }
    bool reachedSuccess=false;
    for(int failure=0;failure<32 && !reachedSuccess;++failure) {
        StreamDevice failed; AudioDeviceIOProcID failedID=nullptr;
        assert(!failed.createIOProcID(proc,nullptr,&failedID));
        failAfter=failure;
        try { assert(!failed.start(failedID,nullptr,0)); reachedSuccess=true; }
        catch(const std::bad_alloc&) {}
        failAfter=-1;
        assert(!failed.destroyIOProcID(failedID));
        auto next=get(failed); next.mSampleRate=48000; assert(!set(failed,next));
    }
    assert(reachedSuccess);
}
static void concurrentCControls() {
    Device target;
    struct Gate { std::mutex mutex; std::condition_variable changed; bool started=false,finish=false; int calls=0; } gate;
    auto proc=+[](AudioObjectID,UInt32,const AudioObjectPropertyAddress*,void* opaque)->OSStatus {
        auto& gate=*static_cast<Gate*>(opaque); std::unique_lock<std::mutex> lock(gate.mutex);
        ++gate.calls; gate.started=true; gate.changed.notify_all();
        gate.changed.wait(lock,[&]{return gate.finish;}); return 0;
    };
    assert(!target.addPropertyListener(&format,proc,&gate));
    std::thread caller([&]{ assert(!target.emit(format)); });
    { std::unique_lock<std::mutex> lock(gate.mutex); gate.changed.wait(lock,[&]{return gate.started;}); }
    assert(!target.removePropertyListener(&format,proc,&gate));
    assert(!target.emit(format));
    { std::lock_guard<std::mutex> lock(gate.mutex); gate.finish=true; } gate.changed.notify_all();
    caller.join(); assert(gate.calls==1);
}
int main() {
    aliasControls(); registrationPublicationControls(); snapshotControls(); concurrentCControls(); cachedStreamControls();
    Device d; Count c{0,&d}; Boolean writable=false;
    assert(!d.isPropertySettable(&format,&writable) && writable);
    assert(!d.addPropertyListener(&format,count,&c));
    assert(!d.addPropertyListener(&format,count,&c));
    auto a=get(d); a.mSampleRate=48000;
    assert(!set(d,a) && c.n==1 && get(d).mSampleRate==48000);
    a.mReserved=99; assert(!set(d,a) && c.n==1);
    auto wrong=format; ++wrong.mElement;
    assert(!d.removePropertyListener(&wrong,count,&c));
    Count other; assert(!d.removePropertyListener(&format,count,&other));
    assert(!d.removePropertyListener(&format,count,&c)); assert(!d.removePropertyListener(&format,count,&c));
    assert(d.addPropertyListener(nullptr,count,&c)==paramErr);
    assert(d.addPropertyListener(&format,nullptr,&c)==paramErr);
    wrong.mSelector=0; assert(d.addPropertyListener(&wrong,count,&c)==kAudioHardwareUnknownPropertyError);
    assert(d.setPropertyData(&format,0,nullptr,sizeof(a),nullptr)==paramErr);
    assert(d.setPropertyData(&format,0,nullptr,1,&a)==kAudioHardwareBadPropertySizeError);

    // Real format callback reenters getter/removal/add; additions start next event.
    __block int first=0,second=0;
    AudioObjectPropertyListenerBlock later=^(UInt32,const AudioObjectPropertyAddress*){ ++second; };
    auto dp=&d; auto lp=later;
    __block AudioObjectPropertyListenerBlock self;
    self=^(UInt32,const AudioObjectPropertyAddress*) {
        ++first; get(*dp); assert(!dp->removePropertyListenerBlock(&format,nullptr,self));
        assert(!dp->addPropertyListenerBlock(&format,nullptr,lp));
    };
    assert(!d.addPropertyListenerBlock(&format,nullptr,self));
    a.mSampleRate=44100; assert(!set(d,a) && first==1 && second==0);
    a.mSampleRate=48000; assert(!set(d,a) && first==1 && second==1);
    assert(!d.removePropertyListenerBlock(&format,nullptr,later));

    auto queue=test_queue(); auto wrongQueue=test_queue();
    std::atomic<int> destroyed{0}; int queued=0;
    auto capture=std::make_shared<Capture>(); capture->destroyed=&destroyed;
    auto listener=capturing(capture,&queued);
    assert(!d.addPropertyListenerBlock(&format,queue,listener));
    assert(!d.addPropertyListenerBlock(&format,queue,listener)); capture.reset(); Block_release(listener);
    auto registration=format;
    assert(!d.removePropertyListenerBlock(&registration,wrongQueue,listener));
    // Prepare with a stack payload, then overwrite that payload before queued delivery.
    d.emit(registration); registration.mSelector=0; test_drain(queue); assert(queued==1 && destroyed==0);
    d.emit(format); assert(!d.removePropertyListenerBlock(&format,queue,listener));
    test_drain(queue); assert(queued==1 && destroyed==1);
    dispatch_release(wrongQueue); dispatch_release(queue);

    // IO source dispatches synchronously, including already executing target/descendant.
    queue=test_queue(); auto child=test_queue(queue);
    AudioObjectPropertyAddress running{kAudioDevicePropertyDeviceIsRunning,kAudioObjectPropertyScopeGlobal,0};
    __block int synchronous=0;
    AudioObjectPropertyListenerBlock io=^(UInt32,const AudioObjectPropertyAddress*){ ++synchronous; };
    assert(!d.addPropertyListenerBlock(&running,queue,io));
    d.emit(running,true); assert(synchronous==1);
    test_on_queue(queue,^{ dp->emit(running,true); assert(synchronous==2); });
    test_on_queue(child,^{ dp->emit(running,true); assert(synchronous==3); });
    auto unrelated=test_queue();
    test_on_queue(unrelated,^{ assert(dp->emit(running,true)==kAudioHardwareUnsupportedOperationError); });
    assert(synchronous==3); dispatch_release(unrelated);
    // The reverse ancestry case (listener queue targets current parent) is rejected.
    assert(!d.removePropertyListenerBlock(&running,queue,io));
    assert(!d.addPropertyListenerBlock(&running,child,io));
    test_on_queue(queue,^{ assert(dp->emit(running,true)==kAudioHardwareUnsupportedOperationError); });
    assert(synchronous==3); assert(!d.removePropertyListenerBlock(&running,child,io));
    auto global=dispatch_get_global_queue(0,0);
    assert(!d.addPropertyListenerBlock(&running,global,io)); d.emit(running,true); assert(synchronous==4);
    assert(!d.removePropertyListenerBlock(&running,global,io));
    dispatch_release(child); dispatch_release(queue);

    // Pending records survive object teardown, cancel delivery, then release captures.
    queue=test_queue(); auto dying=new Device;
    capture=std::make_shared<Capture>(); capture->destroyed=&destroyed;
    auto pending=capturing(capture,&queued);
    assert(!dying->addPropertyListenerBlock(&format,queue,pending)); capture.reset(); Block_release(pending);
    dying->emit(format); delete dying; assert(destroyed==1); test_drain(queue);
    assert(destroyed==2 && queued==1); dispatch_release(queue);

    // Fail each own allocation in registration and pre-commit notification preparation.
    for(int failure=0;failure<6;++failure) {
        Device target; Count events; failAfter=failure;
        auto status=target.addPropertyListener(&format,count,&events); failAfter=-1;
        if(status) { assert(status==kAudioHardwareUnspecifiedError); target.emit(format); assert(events.n==0); }
    }
    for(int failure=0;failure<6;++failure) {
        Device target; Count events; assert(!target.addPropertyListener(&format,count,&events));
        auto before=get(target),next=before; next.mSampleRate=48000;
        failAfter=failure; auto status=set(target,next); failAfter=-1;
        if(status) assert(status==kAudioHardwareUnspecifiedError && get(target).mSampleRate==before.mSampleRate && events.n==0);
        else assert(get(target).mSampleRate==48000 && events.n==1);
    }
    // A running callback owns its capture across concurrent removal. Removal
    // cancels pending work checked after removal; it does not wait for a running invocation.
    {
        Device target; queue=test_queue();
        std::mutex gate; std::condition_variable changed;
        bool started=false, finish=false; int calls=0;
        capture=std::make_shared<Capture>(); capture->destroyed=&destroyed;
        auto gp=&gate; auto cv=&changed;
        auto sp=&started; auto fp=&finish; auto cp=&calls;
        auto makeBlocking=[&](std::shared_ptr<Capture> held) { return Block_copy(^(UInt32,const AudioObjectPropertyAddress*) {
            assert(held); std::unique_lock<std::mutex> lock(*gp);
            ++*cp; *sp=true; cv->notify_all();
            cv->wait(lock,[&]{ return *fp; });
        }); };
        auto blocking=makeBlocking(capture);
        int before=destroyed.load();
        assert(!target.addPropertyListenerBlock(&format,queue,blocking));
        capture.reset(); Block_release(blocking);
        assert(!target.emit(format));
        std::thread worker([&]{ test_drain(queue); });
        { std::unique_lock<std::mutex> lock(gate); changed.wait(lock,[&]{return started;}); }
        assert(!target.removePropertyListenerBlock(&format,queue,blocking));
        assert(destroyed==before);
        assert(!target.emit(format));
        { std::lock_guard<std::mutex> lock(gate); finish=true; } changed.notify_all();
        worker.join(); test_drain(queue);
        assert(calls==1 && destroyed==before+1); dispatch_release(queue);
    }
    // Enumerate each C++ allocation until the first successful block add.
    // The adapter's queue-specific map allocation is included in this sweep.
    for (bool queuedRegistration : {false,true}) {
        bool reachedSuccess=false;
        for (int failure=0; failure<32 && !reachedSuccess; ++failure) {
            Device target; queue=queuedRegistration?test_queue():nullptr;
            int calls=0; auto callsPtr=&calls;
            AudioObjectPropertyListenerBlock block=^(UInt32,const AudioObjectPropertyAddress*){++*callsPtr;};
            failAfter=failure;
            auto status=target.addPropertyListenerBlock(&format,queue,block); failAfter=-1;
            assert(!status || status==kAudioHardwareUnspecifiedError);
            assert(!target.emit(format)); if(queue) test_drain(queue);
            if(status) assert(calls==0);
            else { assert(calls==1); reachedSuccess=true; }
            assert(!target.removePropertyListenerBlock(&format,queue,block));
            if(queue) dispatch_release(queue);
        }
        assert(reachedSuccess);
    }
    {
        bool reachedSuccess=false;
        for(int failure=0; failure<32 && !reachedSuccess; ++failure) {
            Device target; queue=test_queue(); int calls=0; auto cp=&calls;
            AudioObjectPropertyListenerBlock block=^(UInt32,const AudioObjectPropertyAddress*){++*cp;};
            assert(!target.addPropertyListenerBlock(&format,queue,block));
            auto before=get(target), next=before; next.mSampleRate=48000;
            failAfter=failure; auto status=set(target,next); failAfter=-1;
            test_drain(queue);
            if(status) assert(status==kAudioHardwareUnspecifiedError &&
                get(target).mSampleRate==before.mSampleRate && calls==0);
            else { assert(get(target).mSampleRate==48000 && calls==1); reachedSuccess=true; }
            assert(!target.removePropertyListenerBlock(&format,queue,block)); dispatch_release(queue);
        }
        assert(reachedSuccess);
    }
    // BlocksRuntime returns NULL on copy allocation failure (not bad_alloc).
    {
        Device target; queue=test_queue(); int calls=0; auto cp=&calls;
        AudioObjectPropertyListenerBlock block=^(UInt32,const AudioObjectPropertyAddress*){++*cp;};
        failCopy=true;
        assert(target.addPropertyListenerBlock(&format,queue,block)==kAudioHardwareUnspecifiedError);
        failCopy=false; assert(!target.emit(format)); test_drain(queue); assert(calls==0);
        assert(!target.addPropertyListenerBlock(&format,queue,block));
        auto before=get(target), next=before; next.mSampleRate=48000;
        failCopy=true; assert(set(target,next)==kAudioHardwareUnspecifiedError); failCopy=false;
        assert(get(target).mSampleRate==before.mSampleRate && calls==0);
        assert(!target.removePropertyListenerBlock(&format,queue,block)); dispatch_release(queue);
    }
    assert(test_live_queues()==0);
    std::puts("PASS actual-source format/registry controls; deterministic dispatch adapter only");
}
