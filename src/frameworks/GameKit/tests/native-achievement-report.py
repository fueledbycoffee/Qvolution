#!/usr/bin/env python3
"""Native isolation of GameKit's actual completion helper/method bodies.

Usage: native-achievement-report.py BLOCKS_SOURCE_DIR OUTPUT_DIR [GAMEKIT_SOURCE]
BLOCKS_SOURCE_DIR contains compiler-rt BlocksRuntime's runtime.c, data.c,
Block.h, Block_private.h and a host config.h. No framework build is needed.
The scheduler is a controlled pthread queue, not libdispatch. Objective-C
block copy/release messages become Block_copy/Block_release. NSError creation
is represented by a C value using the source's domain/code; this does not
verify Foundation's NSError implementation or dispatch/run-loop integration.
"""
import pathlib
import re
import subprocess
import sys

blocks, output = map(pathlib.Path, sys.argv[1:3])
framework = pathlib.Path(__file__).resolve().parents[1]
source = (pathlib.Path(sys.argv[3]) if len(sys.argv) == 4 else
          framework / 'src/GameKit.m').read_text()
header = (framework / 'include/GameKit/GameKit.h').read_text()


def body(signature):
    start = source.index('{', source.index(signature))
    depth = 1
    end = start + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def adapt(text):
    return text.replace('[handler copy]', 'Block_copy(handler)').replace(
        '[handler release]', 'Block_release(handler)')


helpers = []
for match in re.finditer(r'static void (\w+)\(void \(\^handler\)\(NSError \*\)\)', source):
    helpers.append(match[0] + '\n' + adapt(body(match[0])))
methods = [
    '- (void)reportAchievementWithCompletionHandler:',
    '+ (void)reportAchievements:',
    '+ (void)resetAchievementsWithCompletionHandler:',
]
wrappers = [f'static void report{i}(void (^completionHandler)(NSError *))\n'
            + body(method) for i, method in enumerate(methods)]
domain = re.search(r'GKErrorDomain = @("[^"]+")', source)[1]
error_body = body('static NSError *gameCenterUnavailableError(')
assert 'errorWithDomain:GKErrorDomain' in error_body
code_name = re.search(r'code:\s*(\w+)', error_body)[1]
code = re.search(r'\b' + code_name + r'\s*=\s*(\d+)', header)[1]
prefix = r'''
#include <Block.h>
#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#define nil NULL
#define DISPATCH_QUEUE_PRIORITY_DEFAULT 0
typedef struct { const char *domain; int code; } NSError;
static NSError *gameCenterUnavailableError(void) { return &unavailable; }
static int dispatch_get_main_queue(void) { return 0; }
static int dispatch_get_global_queue(int priority, int flags) { return 1; }
typedef struct { int queue; void (^block)(void); } Job;
static Job jobs[16];
static int count;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_t mainThread;
static void dispatch_async(int queue, void (^block)(void)) {
    pthread_mutex_lock(&lock);
    assert(count < 16);
    jobs[count++] = (Job){queue, Block_copy(block)};
    pthread_mutex_unlock(&lock);
}
'''
prefix = prefix.replace('static NSError *gameCenterUnavailableError',
                        f'static NSError unavailable = {{{domain}, {code}}};\n'
                        'static NSError *gameCenterUnavailableError')
suffix = r'''
static int calls, wrongThread, wrongError;
static void *runJob(void *arg) { ((Job *)arg)->block(); return NULL; }
static void drain(void) {
    for (int i = 0; i < count; ++i) {
        if (jobs[i].queue == 0) jobs[i].block();
        else {
            pthread_t worker;
            assert(pthread_create(&worker, NULL, runJob, &jobs[i]) == 0);
            assert(pthread_join(worker, NULL) == 0);
        }
        Block_release(jobs[i].block);
    }
    count = 0;
}
static void (*reports[])(void (^)(NSError *)) = { report0, report1, report2 };
static void *invoke(void *arg) {
    int marker = 123; // A stack capture must outlive this caller.
    for (int i = 0; i < 3; ++i) {
        reports[i](nil);
        reports[i](^(NSError *error) {
            pthread_mutex_lock(&lock);
            ++calls;
            wrongThread += !pthread_equal(mainThread, pthread_self());
            wrongError += marker != 123 || error == NULL ||
                strcmp(error->domain, "com.apple.GameKit") || error->code != 16;
            pthread_mutex_unlock(&lock);
        });
    }
    return NULL;
}
int main(void) {
    mainThread = pthread_self();
    int failed = 0;
    assert(isCompleted(100.0));
    assert(!isCompleted(0) && !isCompleted(50) && !isCompleted(101));
    assert(!isCompleted(NAN) && !isCompleted(INFINITY));
    for (int background = 0; background < 2; ++background) {
        calls = wrongThread = wrongError = 0;
        if (background) {
            pthread_t worker;
            assert(pthread_create(&worker, NULL, invoke, NULL) == 0);
            assert(pthread_join(worker, NULL) == 0);
        } else invoke(NULL);
        assert(calls == 0); // No inline delivery; nil handlers enqueue no work.
        assert(count == 3);
        drain();
        printf("caller=%s calls=%d wrongThread=%d wrongError=%d\n",
               background ? "worker" : "main", calls, wrongThread, wrongError);
        failed |= calls != 3 || wrongThread || wrongError;
        drain();
        assert(calls == 3); // Exactly once.
    }
    if (!failed) puts("PASS: main-queue report/reset, errors, nil, lifetime, isCompleted");
    return failed;
}
'''
output.mkdir(parents=True, exist_ok=True)
generated = output / 'achievement-native.c'
generated.write_text(prefix + '\n'.join(helpers + wrappers) +
                     '\nstatic int isCompleted(double _percentComplete)\n' +
                     body('- (BOOL)isCompleted') + '\n' + suffix)
binary = output / 'achievement-native'
subprocess.run(['clang', '-fblocks', '-pthread', '-g', '-fsanitize=address,undefined',
                '-I' + str(blocks), str(generated), str(blocks / 'runtime.c'),
                str(blocks / 'data.c'), '-o', str(binary)], check=True)
subprocess.run([str(binary)], check=True)
