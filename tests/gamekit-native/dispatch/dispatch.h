#ifndef GAMEKIT_TEST_DISPATCH_H
#define GAMEKIT_TEST_DISPATCH_H
// A controlled queue seam: drain explicitly after the API returns. This checks
// GameKit's queue selection and ownership, not libdispatch's implementation.
typedef void *dispatch_queue_t;
typedef long dispatch_once_t;
#define DISPATCH_QUEUE_PRIORITY_DEFAULT 0
dispatch_queue_t dispatch_get_main_queue(void);
dispatch_queue_t dispatch_get_global_queue(long priority, unsigned long flags);
void dispatch_async(dispatch_queue_t queue, void (^block)(void));
void dispatch_once(dispatch_once_t *once, void (^block)(void));
#endif
