#pragma once
#include <cstdint>
struct TestQueue;
using dispatch_queue_t = TestQueue*;
using dispatch_block_t = void (^)();
using dispatch_function_t = void (*)(void*);
constexpr intptr_t DISPATCH_QUEUE_PRIORITY_HIGH=2, DISPATCH_QUEUE_PRIORITY_DEFAULT=0,
    DISPATCH_QUEUE_PRIORITY_LOW=-2, DISPATCH_QUEUE_PRIORITY_BACKGROUND=-32768;
constexpr intptr_t QOS_CLASS_USER_INTERACTIVE=33, QOS_CLASS_USER_INITIATED=25,
    QOS_CLASS_DEFAULT=21, QOS_CLASS_UTILITY=17, QOS_CLASS_BACKGROUND=9;
extern "C" {
dispatch_queue_t dispatch_get_global_queue(intptr_t, uintptr_t);
dispatch_queue_t dispatch_get_current_queue();
void dispatch_retain(dispatch_queue_t);
void dispatch_release(dispatch_queue_t);
void dispatch_queue_set_specific(dispatch_queue_t,const void*,void*,dispatch_function_t);
void* dispatch_get_specific(const void*);
void dispatch_async(dispatch_queue_t,dispatch_block_t);
void dispatch_sync(dispatch_queue_t,dispatch_block_t);
// Deterministic adapter controls; this is explicitly not a libdispatch runtime.
dispatch_queue_t test_queue(dispatch_queue_t target=nullptr);
void test_drain(dispatch_queue_t);
void test_on_queue(dispatch_queue_t,dispatch_block_t);
int test_live_queues();
}
