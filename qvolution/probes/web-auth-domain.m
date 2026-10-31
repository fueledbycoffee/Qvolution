#import <Foundation/Foundation.h>
#import <objc/runtime.h>
#include <dlfcn.h>
#include <stdio.h>
#define CHECK(c,m) do {if(!(c)){fprintf(stderr,"FAIL: %s\n",m);return 1;}}while(0)
int main(void){
 NSAutoreleasePool *pool=[NSAutoreleasePool new];
 void *lib=dlopen("/System/Library/Frameworks/AuthenticationServices.framework/AuthenticationServices",RTLD_LAZY|RTLD_LOCAL);
 CHECK(lib,"real AuthenticationServices loads");
 CHECK(objc_getClass("ASWebAuthenticationSession")!=Nil,"existing session class positive control");
 NSErrorDomain const *authorization=dlsym(lib,"ASAuthorizationErrorDomain");
 CHECK(authorization && [*authorization isKindOfClass:[NSString class]],"existing authorization-domain export preserved");
 CHECK([*authorization isEqualToString:@"com.apple.AuthenticationServices.AuthorizationError"],"existing authorization-domain value preserved");
 NSErrorDomain const *domain=dlsym(lib,"ASWebAuthenticationSessionErrorDomain");
 CHECK(domain!=NULL,"web-authentication error-domain data export");
 CHECK([*domain isKindOfClass:[NSString class]],"web-authentication domain is NSString object");
 CHECK([*domain isEqualToString:@"com.apple.AuthenticationServices.WebAuthenticationSession"],"official upstream API-client domain spelling");
 CHECK(![*domain isEqualToString:*authorization],"web and authorization domains remain distinct");
 NSError *error=[[NSError alloc] initWithDomain:*domain code:1 userInfo:nil];
 CHECK([[error domain] isEqual:*domain] && [error code]==1,"NSError roundtrip preserves domain/code");
 CHECK(dlsym(lib,"QvolutionAbsentWebAuthenticationDomain")==NULL,"missing export negative control");
 [pool drain];
 CHECK([[error domain] isEqualToString:@"com.apple.AuthenticationServices.WebAuthenticationSession"],"retained NSError survives pool");
 [error release];dlclose(lib);
 puts("PASS: web-authentication domain public data, existing authorization and ownership");return 0;
}
