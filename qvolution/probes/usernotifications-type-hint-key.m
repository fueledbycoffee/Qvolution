#import <Foundation/Foundation.h>
#import <objc/runtime.h>
#include <dlfcn.h>
#include <stdio.h>
#define CHECK(c,m) do{if(!(c)){fprintf(stderr,"FAIL: %s\n",m);return 1;}}while(0)
int main(void){
 NSAutoreleasePool *pool=[NSAutoreleasePool new];
 CHECK([NSCocoaErrorDomain isKindOfClass:[NSString class]]&&[NSCocoaErrorDomain length]>0,"existing Foundation data control");
 void *library=dlopen("/System/Library/Frameworks/UserNotifications.framework/UserNotifications",RTLD_LAZY|RTLD_LOCAL);
 CHECK(library,"actual UserNotifications framework loads");
 CHECK(objc_getClass("UNNotificationAttachment")!=Nil,"existing attachment class metadata control only");
 NSString *const *key=dlsym(library,"UNNotificationAttachmentOptionsTypeHintKey");
 CHECK(key&&*key,"observed public type-hint data export exists");
 CHECK([*key isKindOfClass:[NSString class]]&&[*key length]>0,"type-hint key is actual nonempty NSString data");
 NSAutoreleasePool *creation=[NSAutoreleasePool new];
 NSMutableDictionary *options=[NSMutableDictionary dictionary];
 CHECK([options objectForKey:*key]==nil,"absent type hint stays absent in generic options dictionary");
 [options setObject:@"public.jpeg" forKey:*key];NSDictionary *snapshot=[[NSDictionary alloc] initWithDictionary:options];
 [options removeObjectForKey:*key];
 CHECK(![options objectForKey:*key]&&[[snapshot objectForKey:*key] isEqual:@"public.jpeg"],"explicit UTI hint survives independent metadata copy");
 CHECK(dlsym(library,"QvolutionAbsentNotificationHintKey")==NULL,"missing data negative control");
 [creation drain];
 CHECK([[snapshot objectForKey:*key] isKindOfClass:[NSString class]]&&[[snapshot objectForKey:*key] isEqual:@"public.jpeg"],"owned generic metadata survives creation pool");
 [snapshot release];[pool drain];dlclose(library);
 puts("PASS: actual attachment type-hint NSString data and generic metadata ownership; no attachment/delivery qualification");return 0;
}
