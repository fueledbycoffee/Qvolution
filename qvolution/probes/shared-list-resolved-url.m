#include <CoreFoundation/CoreFoundation.h>
#include <SharedFileList/LSSharedFileList.h>
#include <dlfcn.h>
#include <stdio.h>
#define CHECK(c,m) do{if(!(c)){fprintf(stderr,"FAIL: %s\n",m);return 1;}}while(0)
int main(void){@autoreleasepool {
 void *lib=dlopen("/System/Library/Frameworks/CoreServices.framework/Frameworks/SharedFileList.framework/SharedFileList",RTLD_LAZY|RTLD_LOCAL);
 CHECK(lib,"actual SharedFileList loads");
 LSSharedFileListRef list=LSSharedFileListCreate(NULL,kLSSharedFileListFavoriteItems,NULL);
 CHECK(list!=NULL,"existing list positive control");
 typedef CFURLRef (*CopyURL)(LSSharedFileListItemRef,UInt32,CFErrorRef *);
 CopyURL copy=dlsym(lib,"LSSharedFileListItemCopyResolvedURL");CHECK(copy!=NULL,"documented URL-copy symbol available");
 CFURLRef input=CFURLCreateWithString(NULL,CFSTR("file:///qvolution-original-no-file-created.txt"),NULL);
 LSSharedFileListItemRef item=LSSharedFileListInsertItemURL(list,kLSSharedFileListItemLast,CFSTR("Original fixture"),NULL,input,NULL,NULL);
 CHECK(item!=NULL,"existing URL item positive control");
 CFErrorRef error=NULL;CFURLRef resolved=copy(item,kLSSharedFileListNoUserInteraction|kLSSharedFileListDoNotMountVolumes,&error);
 CHECK(resolved!=NULL && error==NULL && CFEqual(input,resolved),"copy preserves stored URL and success error state");
 CFURLRef second=copy(item,0,NULL);CHECK(second && CFEqual(second,resolved),"optional error output");CFRelease(second);
 CFRelease(input);CHECK(LSSharedFileListItemRemove(list,item)==noErr,"remove item");CFRelease(item);CFRelease(list);
 CHECK(CFEqual(CFURLGetString(resolved),CFSTR("file:///qvolution-original-no-file-created.txt")),"owned URL survives source item/list release");CFRelease(resolved);
 CHECK(copy(NULL,0,&error)==NULL && error!=NULL,"null item reports failure");
 CHECK(CFEqual(CFErrorGetDomain(error),kCFErrorDomainOSStatus) && CFErrorGetCode(error)==paramErr,"legacy status bridged to explicit CFError");CFRelease(error);
 CHECK(copy(NULL,0,NULL)==NULL,"null item without error output");dlclose(lib);
 puts("PASS: shared-list copied stored URL ownership and error bridge");return 0;
}}
