// Original two-level class import contract; no handoff/indexing services invoked.
#import <Foundation/NSUserActivity.h>
#import <Foundation/NSString.h>
#import <objc/runtime.h>
#include <stdio.h>
#define CHECK(c,m) do{if(!(c)){fprintf(stderr,"FAIL: %s\n",m);return 1;}}while(0)
int main(void) {
 @autoreleasepool {
  CHECK([NSUserActivity class]==objc_getClass("NSUserActivity"),"real runtime class identity");
  NSUserActivity *a=[[NSUserActivity alloc] initWithActivityType:@"org.qvolution.OriginalActivity"];
  CHECK(a && [[a activityType] isEqualToString:@"org.qvolution.OriginalActivity"],"real typed activity initialization");
  [a setTitle:@"Original activity"];
  CHECK([[a title] isEqualToString:@"Original activity"],"existing title storage");
  [a invalidate];[a release];
 }
 puts("PASS: framework import reaches existing Foundation user activity implementation; no handoff service qualification");
 return 0;
}
