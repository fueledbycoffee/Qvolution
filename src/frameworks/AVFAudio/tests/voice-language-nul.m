#import <Foundation/Foundation.h>
#include <dlfcn.h>
#include <stdio.h>

@protocol QVNulVoice <NSObject>
- (NSString *)identifier;
- (NSString *)name;
- (NSString *)language;
@end
@protocol QVNulVoiceClass <NSObject>
+ (id<QVNulVoice>)voiceWithLanguage:(NSString *)language;
@end
#define CHECK(c,m) do { if(!(c)) { fprintf(stderr,"FAIL: %s\n",m);return 1; } } while(0)

int main(void)
{
    NSAutoreleasePool *pool=[NSAutoreleasePool new];
    CHECK(dlopen("/System/Library/Frameworks/AVFoundation.framework/AVFoundation",RTLD_NOW|RTLD_LOCAL),"AVFoundation loads");
    Class cls=NSClassFromString(@"AVSpeechSynthesisVoice");
    if(!cls) { fputs("MISSING_CLASS: AVSpeechSynthesisVoice\n",stderr);return 20; }
    id<QVNulVoice> positive=[[(id<QVNulVoiceClass>)cls voiceWithLanguage:@"en"]retain];
    if(!positive) { fputs("UNAVAILABLE: real positive English voice required; negative nils alone cannot qualify\n",stderr);return 77; }
    CHECK([[positive identifier]length]&&[[positive name]length]&&[[positive language]length],"real positive provider metadata");
    unichar suffix[]={'e','n',0,'%','%','%'};
    unichar ending[]={'e','n',0};
    unichar initial[]={0,'e','n'};
    unichar region[]={'e','n','-','U','S',0,'x'};
    NSString *withSuffix=[NSString stringWithCharacters:suffix length:sizeof(suffix)/sizeof(suffix[0])];
    NSString *withEnding=[NSString stringWithCharacters:ending length:sizeof(ending)/sizeof(ending[0])];
    NSString *withInitial=[NSString stringWithCharacters:initial length:sizeof(initial)/sizeof(initial[0])];
    NSString *withRegion=[NSString stringWithCharacters:region length:sizeof(region)/sizeof(region[0])];
    CHECK([withSuffix length]==6&&[withSuffix characterAtIndex:2]==0&&[withSuffix characterAtIndex:5]=='%',"NSString preserves invalid bytes beyond embedded NUL");
    CHECK([withEnding length]==3&&[withInitial length]==3&&[withRegion length]==7,"entire NUL-bearing input strings exist");
    CHECK(nil==[(id<QVNulVoiceClass>)cls voiceWithLanguage:withSuffix],"valid language prefix plus NUL and garbage must return nil");
    CHECK(nil==[(id<QVNulVoiceClass>)cls voiceWithLanguage:withEnding],"trailing U+0000 is not a valid language tag");
    CHECK(nil==[(id<QVNulVoiceClass>)cls voiceWithLanguage:withInitial],"initial U+0000 must return nil");
    CHECK(nil==[(id<QVNulVoiceClass>)cls voiceWithLanguage:withRegion],"region prefix cannot hide invalid suffix behind NUL");
    NSMutableString *oversized=[NSMutableString stringWithString:withEnding];
    while([oversized length]<=1024)[oversized appendString:@"x"];
    CHECK(nil==[(id<QVNulVoiceClass>)cls voiceWithLanguage:oversized],"oversized complete string cannot bypass bound through valid prefix");
    CHECK(nil==[(id<QVNulVoiceClass>)cls voiceWithLanguage:@"é"],"lossless non-ASCII encoding cannot become an ASCII language tag");
    CHECK(nil==[(id<QVNulVoiceClass>)cls voiceWithLanguage:@"%%%"],"invalid ASCII remains rejected");
    id<QVNulVoice> after=[(id<QVNulVoiceClass>)cls voiceWithLanguage:@"EN"];
    CHECK(after&&[[after identifier]isEqual:[positive identifier]],"ordinary supported normalization survives rejected inputs");
    [(id)positive release];[pool drain];
    puts("PASS: actual public factory rejects complete NUL-bearing/oversized/invalid NSString inputs; positive provider normalization retained");
    return 0;
}
