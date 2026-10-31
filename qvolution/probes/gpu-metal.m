#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
int main(int argc,char **argv) {
    BOOL optIn=NO, negative=NO;
    for(int i=1;i<argc;i++) {
        if(strcmp(argv[i],"--enable-experimental-metal")==0) optIn=YES;
        else if(strcmp(argv[i],"--negative-control")==0) negative=YES;
        else return 2;
    }
    if(optIn) { setenv("DARLING_ENABLE_METAL","1",1); puts("experimental_metal_opt_in=1"); }
    NSAutoreleasePool *pool=[NSAutoreleasePool new];
    void *library=dlopen("/System/Library/Frameworks/Metal.framework/Metal",RTLD_NOW|RTLD_LOCAL);
    if(!library){fprintf(stderr,"FAIL: Metal load: %s\n",dlerror());return 1;}
    id<MTLDevice>(*create)(void)=dlsym(library,"MTLCreateSystemDefaultDevice");
    if(!create){fprintf(stderr,"FAIL: Metal device entry point: %s\n",dlerror());return 1;}
    id<MTLDevice> device=create();
    if(!device){fputs("FAIL: no Metal device\n",stderr);return 1;}
    NSString *name=[device name];printf("device=%s\n",[name UTF8String]);fflush(stdout);
    if(!name){fputs("FAIL: unnamed Metal device\n",stderr);return 1;}
    for(NSString *software in @[@"llvmpipe",@"lavapipe",@"swiftshader",@"software"])
        if([name rangeOfString:software options:NSCaseInsensitiveSearch].location!=NSNotFound){fputs("FAIL: software Metal device\n",stderr);return 1;}
    id<MTLCommandQueue> queue=[device newCommandQueue];
    id<MTLBuffer> buffer=[device newBufferWithLength:4096 options:MTLResourceStorageModeShared];
    if(!queue || !buffer || ![buffer contents]){fputs("FAIL: command queue/shared buffer\n",stderr);return 1;}
    memset([buffer contents],0,4096);
    id<MTLCommandBuffer> command=[queue commandBuffer];id<MTLBlitCommandEncoder> blit=[command blitCommandEncoder];
    if(!command || !blit){fputs("FAIL: command/blit encoder\n",stderr);return 1;}
    [blit fillBuffer:buffer range:NSMakeRange(0,4096) value:0x5a];[blit endEncoding];[command commit];[command waitUntilCompleted];
    if([command error]){fputs("FAIL: Metal command did not complete\n",stderr);return 1;}
    unsigned char expected=negative?0:0x5a;
    const unsigned char *bytes=[buffer contents];for(NSUInteger i=0;i<4096;i++)if(bytes[i]!=expected){fprintf(stderr,"FAIL: Metal buffer readback at %lu: %u\n",(unsigned long)i,bytes[i]);return 1;}
    [buffer release];[queue release];[pool drain];
    puts("PASS: guest Metal device and GPU buffer-fill readback");return 0;
}
