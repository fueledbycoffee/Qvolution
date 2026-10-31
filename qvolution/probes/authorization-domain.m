#import <Foundation/Foundation.h>
#import <objc/runtime.h>
#include <dlfcn.h>
#include <stdio.h>
#define CHECK(c,m) do {if(!(c)){fprintf(stderr,"FAIL: %s\n",m);return 1;}}while(0)
int main(void){
 NSAutoreleasePool *pool=[NSAutoreleasePool new];
 void *lib=dlopen("/System/Library/Frameworks/AuthenticationServices.framework/AuthenticationServices",RTLD_LAZY|RTLD_LOCAL);
 CHECK(lib,"real AuthenticationServices loads");
 CHECK(objc_getClass("ASAuthorization")!=Nil,"existing class positive control");
 NSString *const *domain=dlsym(lib,"ASAuthorizationErrorDomain");
 CHECK(domain!=NULL,"documented error-domain symbol");
 CHECK([*domain isKindOfClass:[NSString class]],"domain is NSString object, not C bytes");
 CHECK([*domain isEqualToString:@"com.apple.AuthenticationServices.AuthorizationError"],"documented vendor error-domain value");
 NSError *error=[[NSError alloc] initWithDomain:*domain code:1000 userInfo:nil];
 CHECK([[error domain] isEqual:*domain] && [error code]==1000,"Foundation error-domain roundtrip");
 CHECK(dlsym(lib,"QvolutionAbsentAuthorizationDomain")==NULL,"missing-symbol negative control");
 [pool drain];CHECK([[error domain] length]>0,"retained NSError domain survives pool");[error release];dlclose(lib);
 puts("PASS: authorization error-domain public data and ownership");return 0;
}
