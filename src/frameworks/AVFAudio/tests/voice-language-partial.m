#import <Foundation/Foundation.h>
#include <dlfcn.h>
#include <stdio.h>
@protocol QVLanguageVoice <NSObject>
- (NSString *)identifier;
- (NSString *)name;
- (NSString *)language;
@end
@protocol QVLanguageVoiceClass <NSObject>
+ (id<QVLanguageVoice>)voiceWithLanguage:(NSString *)language;
+ (NSString *)currentLanguageCode;
@end
#define CHECK(c,m) do { if(!(c)) { fprintf(stderr,"FAIL: %s\n",m);return 1; } } while(0)
static BOOL matches(id<QVLanguageVoice> voice,id identifier)
{ return identifier==[NSNull null]?voice==nil:[identifier isKindOfClass:[NSString class]]&&[[voice identifier]isEqual:identifier]; }
int main(int argc,char **argv)
{
    NSAutoreleasePool *pool=[NSAutoreleasePool new];
    CHECK(argc==2,"independently recorded language-lookups JSON argument required");
    CHECK(dlopen("/System/Library/Frameworks/AVFoundation.framework/AVFoundation",RTLD_NOW|RTLD_LOCAL),"AVFoundation loads");
    Class cls=NSClassFromString(@"AVSpeechSynthesisVoice");
    if(!cls) { fputs("MISSING_CLASS: AVSpeechSynthesisVoice\n",stderr);return 20; }
    NSData *data=[NSData dataWithContentsOfFile:[NSString stringWithUTF8String:argv[1]]];
    NSDictionary *manifest=data?[NSJSONSerialization JSONObjectWithData:data options:0 error:NULL]:nil;
    CHECK([manifest isKindOfClass:[NSDictionary class]],"expected manifest object");
    NSArray *lookups=[manifest objectForKey:@"languageLookups"];
    CHECK([lookups isKindOfClass:[NSArray class]]&&[lookups count],"independent lookup expectations required");
    NSUInteger positive=0;
    for(NSDictionary *row in lookups) {
        CHECK([row isKindOfClass:[NSDictionary class]],"typed lookup row");
        NSString *language=[row objectForKey:@"language"];id expected=[row objectForKey:@"identifier"];
        CHECK([language isKindOfClass:[NSString class]]&&(expected==[NSNull null]||[expected isKindOfClass:[NSString class]]),"explicit language and identifier-or-null expectation");
        NSMutableString *query=[language mutableCopy];
        NSAutoreleasePool *creation=[NSAutoreleasePool new];
        id<QVLanguageVoice> voice=[[(id<QVLanguageVoiceClass>)cls voiceWithLanguage:query]retain];
        [creation drain];[query appendString:@" mutated"];[query release];
        CHECK(matches(voice,expected),"actual language selection matches independent expected identity after input mutation/pool drain");
        if(expected!=[NSNull null]) { CHECK([[voice name]length]&&[[voice language]length],"selected voice retains genuine metadata");positive++; }
        [(id)voice release];
    }
    CHECK(positive>0,"nil-only outcomes do not qualify populated provider language lookup");
    NSString *current=[(id<QVLanguageVoiceClass>)cls currentLanguageCode];
    CHECK([current isEqual:[manifest objectForKey:@"currentLanguageCode"]],"recorded user locale remains independent of provider");
    id expectedDefault=[manifest objectForKey:@"defaultIdentifier"];
    CHECK(expectedDefault==[NSNull null]||[expectedDefault isKindOfClass:[NSString class]],"explicit independently chosen default identity-or-null required");
    CHECK(matches([(id<QVLanguageVoiceClass>)cls voiceWithLanguage:nil],expectedDefault),"nil uses recorded system language/region policy");
    CHECK(matches([(id<QVLanguageVoiceClass>)cls voiceWithLanguage:current],expectedDefault),"explicit user language agrees with default");
    CHECK(nil==[(id<QVLanguageVoiceClass>)cls voiceWithLanguage:@"%%%invalid%%%"],"invalid input cannot fabricate voice");
    [pool drain];puts("PASS: partial language/default/ownership behavior; no scalar or synthesis qualification");return 0;
}
