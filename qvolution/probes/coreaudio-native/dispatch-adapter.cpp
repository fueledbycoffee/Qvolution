// Original deterministic queue adapter. Real BlocksRuntime owns copied work.
// Runtime queue semantics still require the unchanged guest fixture.
#include <dispatch/dispatch.h>
#include <Block.h>
#include <cassert>
#include <deque>
#include <map>
#include <mutex>
#include <atomic>
struct TestQueue {
    std::atomic<int> refs{1}; bool global=false;
    dispatch_queue_t target=nullptr;
    std::map<const void*,void*> specifics;
    std::deque<dispatch_block_t> pending;
    std::mutex mutex;
};
static TestQueue globals[5];
static thread_local dispatch_queue_t current=nullptr;
static std::atomic<int> live{0};
dispatch_queue_t dispatch_get_global_queue(intptr_t p,uintptr_t flags) {
    if (flags) return nullptr;
    int i=p==33?0:(p==25||p==2)?1:(p==21||p==0)?2:(p==17||p==-2)?3:(p==9||p==-32768)?4:-1;
    if (i<0) return nullptr;
    globals[i].global=true; return &globals[i];
}
dispatch_queue_t dispatch_get_current_queue() { return current ? current : dispatch_get_global_queue(0,0); }
void dispatch_retain(dispatch_queue_t q) { if (!q->global) ++q->refs; }
void dispatch_release(dispatch_queue_t q) {
    if (!q->global && --q->refs==0) {
        assert(q->pending.empty()); if(q->target) dispatch_release(q->target); --live; delete q;
    }
}
dispatch_queue_t test_queue(dispatch_queue_t target) {
    auto q=new TestQueue; q->target=target; if(target) dispatch_retain(target); ++live; return q;
}
void dispatch_queue_set_specific(dispatch_queue_t q,const void* k,void* v,dispatch_function_t d) {
    assert(!q->global && !d); std::lock_guard<std::mutex> lock(q->mutex);
    if(v) q->specifics[k]=v; else q->specifics.erase(k);
}
void* dispatch_get_specific(const void* k) {
    for(auto q=current;q;q=q->target) {
        std::lock_guard<std::mutex> lock(q->mutex);
        auto it=q->specifics.find(k); if(it!=q->specifics.end()) return it->second;
    } return nullptr;
}
void test_on_queue(dispatch_queue_t q,dispatch_block_t work) { auto previous=current; current=q; work(); current=previous; }
void dispatch_async(dispatch_queue_t q,dispatch_block_t work) {
    auto copied=Block_copy(work); dispatch_retain(q);
    std::lock_guard<std::mutex> lock(q->mutex); q->pending.push_back(copied);
}
void test_drain(dispatch_queue_t q) {
    for(;;) {
        dispatch_block_t work;
        { std::lock_guard<std::mutex> lock(q->mutex); if(q->pending.empty()) break; work=q->pending.front(); q->pending.pop_front(); }
        test_on_queue(q,work); Block_release(work); dispatch_release(q);
    }
}
void dispatch_sync(dispatch_queue_t q,dispatch_block_t work) {
    for(auto t=current;t;t=t->target) assert(t!=q || q->global);
    test_drain(q); test_on_queue(q,work);
}
int test_live_queues() { return live; }
