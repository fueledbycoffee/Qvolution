/* Controlled Foundation/FMDatabase dependencies; the conformance function and
 * registration method under test are included verbatim from production. */
#include <objc/runtime.h>
#include <sqlite3.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
typedef unsigned long NSUInteger;
__attribute__((objc_root_class)) @interface NSObject {Class isa;}
+ (id)alloc; + (id)new;
@end
@implementation NSObject
+ (id)alloc{return class_createInstance(self,0);}
+ (id)new{return [self alloc];}
@end
@interface NSString:NSObject
- (const char *)UTF8String;
@end
@implementation NSString
- (const char *)UTF8String{abort();}
@end
__attribute__((objc_root_class)) @interface NativeConstantString {Class isa;const char *bytes;unsigned length;}
- (const char *)UTF8String;
@end
@implementation NativeConstantString
- (const char *)UTF8String{return bytes;}
@end
@interface NSArray:NSObject {@public id items[8];NSUInteger size;}
- (NSArray *)arrayByAddingObjectsFromArray:(NSArray *)other;
@end
@implementation NSArray
- (NSArray *)arrayByAddingObjectsFromArray:(NSArray *)other{
 NSArray *a=[NSArray new];for(unsigned i=0;i<size;i++)a->items[a->size++]=items[i];
 for(unsigned i=0;i<other->size;i++)a->items[a->size++]=other->items[i];return a;
}
@end
@interface NSDictionary:NSObject {@public NSArray *imported,*exported;}
- (id)objectForKey:(NSString *)key;
@end
@implementation NSDictionary
- (id)objectForKey:(NSString *)key{return !strcmp([key UTF8String],"imports")?imported:exported;}
@end
static NSString *kUTImportedTypeDeclarationsKey=@"imports",*kUTExportedTypeDeclarationsKey=@"exports";
static NSDictionary *CFBundleGetInfoDictionary(id bundle){return bundle;}
static NSArray *processed;
@interface DB:NSObject
- (BOOL)beginTransaction;- (BOOL)commit;- (BOOL)rollback;
@end
@implementation DB
- (BOOL)beginTransaction{return YES;}- (BOOL)commit{return YES;}- (BOOL)rollback{return YES;}
@end
static DB *g_database;
@interface LSBundle:NSObject {@public id _bundle;int _bundleId;}
- (BOOL)process;- (BOOL)setupBundleID:(BOOL *)needed;
- (BOOL)processUTIs:(NSArray *)types;- (BOOL)processFileAssociations;- (BOOL)processURLTypes;
@end
@implementation LSBundle
- (BOOL)setupBundleID:(BOOL *)needed{*needed=YES;return YES;}
- (BOOL)processUTIs:(NSArray *)types{processed=types;return YES;}
- (BOOL)processFileAssociations{return YES;}- (BOOL)processURLTypes{return YES;}
#include "uti-process.inc"
@end
static sqlite3 *database;
@interface FMResultSet:NSObject {@public sqlite3_stmt *statement;}
- (BOOL)next;- (void)close;
@end
@implementation FMResultSet
- (BOOL)next{return sqlite3_step(statement)==SQLITE_ROW;}
- (void)close{sqlite3_finalize(statement);}
@end
@interface FMDatabase:NSObject
- (FMResultSet *)executeQuery:(NSString *)query,...;
@end
@implementation FMDatabase
- (FMResultSet *)executeQuery:(NSString *)query,...{
 FMResultSet *r=[FMResultSet new];int rc=sqlite3_prepare_v2(database,[query UTF8String],-1,&r->statement,NULL);
 if(rc!=SQLITE_OK){fprintf(stderr,"SQL: %s\n",sqlite3_errmsg(database));exit(2);}
 va_list ap;va_start(ap,query);
 for(int i=1;i<=2;i++){NSString *s=va_arg(ap,id);sqlite3_bind_text(r->statement,i,[s UTF8String],-1,SQLITE_TRANSIENT);}
 va_end(ap);return r;
}
@end
@interface FMDatabaseQueue:NSObject
- (void)inDatabase:(void (^)(FMDatabase *))block;
@end
@implementation FMDatabaseQueue
- (void)inDatabase:(void (^)(FMDatabase *))block{block([FMDatabase new]);}
@end
static FMDatabaseQueue *getDatabaseQueue(void){return [FMDatabaseQueue new];}
typedef NSString *CFStringRef;
typedef bool Boolean;
#define TRUE true
#define FALSE false
static Boolean UTTypeEqual(CFStringRef a,CFStringRef b){return !strcasecmp([a UTF8String],[b UTF8String]);}
#include "uti-conforms.inc"
int main(int argc,char **argv){
 assert(argc==2);assert(sqlite3_open(argv[1],&database)==SQLITE_OK);int failures=0;
 struct {NSString *type,*parent;bool expected;} cases[]={
  {@"public.plain-text",@"public.data",true},
  {@"public.utf8-plain-text",@"public.text",true},
  {@"public.png",@"public.data",true},
  {@"com.adobe.pdf",@"public.data",true},
  {@"public.data",@"public.data",true},
  {@"PUBLIC.PNG",@"PUBLIC.DATA",true},
  {@"public.directory",@"public.data",false},
  {@"public.data",@"public.image",false},
  {@"own.cycle-a",@"own.cycle-b",true},
  {@"own.cycle-a",@"own.unrelated",false},
 };
 for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++){
  bool got=UTTypeConformsTo(cases[i].type,cases[i].parent);
  printf("conformance %u: %d expected %d\n",i,got,cases[i].expected);if(got!=cases[i].expected)failures++;
 }
 g_database=[DB new];LSBundle *bundle=[LSBundle new];NSDictionary *info=[NSDictionary new];bundle->_bundle=info;
 for(unsigned mode=0;mode<4;mode++){
  NSArray *imports=[NSArray new],*exports=[NSArray new];imports->items[imports->size++]=@"own.import";exports->items[exports->size++]=@"own.export";
  info->imported=(mode&1)?imports:nil;info->exported=(mode&2)?exports:nil;
  assert([bundle process]);unsigned expected=!!(mode&1)+!!(mode&2);unsigned count=processed?processed->size:0;
  printf("registration %u: %u expected %u\n",mode,count,expected);if(count!=expected)failures++;
  if(mode==3&&(processed->items[0]!=imports->items[0]||processed->items[1]!=exports->items[0]))failures++;
 }
 sqlite3_close(database);return failures?1:0;
}
