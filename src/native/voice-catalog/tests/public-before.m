#import <Foundation/Foundation.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

/* Original caller types from Apple's public Objective-C documentation. */
@protocol QVVoiceRecord <NSObject>
- (NSString *)identifier;
- (NSString *)name;
- (NSString *)language;
- (NSInteger)quality;
- (NSInteger)gender;
@end
@protocol QVVoiceClass <NSObject>
+ (NSArray *)speechVoices;
+ (id<QVVoiceRecord>)voiceWithIdentifier:(NSString *)identifier;
+ (id<QVVoiceRecord>)voiceWithLanguage:(NSString *)language;
+ (NSString *)currentLanguageCode;
@end

#define REQUIRE(c, message) do { if (!(c)) { fprintf(stderr, "FAIL: %s\n", message); return 1; } } while (0)

static BOOL signature(id receiver, SEL selector, const char *result, NSUInteger arguments)
{
    NSMethodSignature *sig = [receiver methodSignatureForSelector:selector];
    if (!sig || strcmp([sig methodReturnType], result) || [sig numberOfArguments] != arguments + 2)
        return NO;
    for (NSUInteger i = 0; i < arguments; i++)
        if (strcmp([sig getArgumentTypeAtIndex:i + 2], @encode(id))) return NO;
    return YES;
}

int main(int argc, char **argv)
{
    NSAutoreleasePool *pool = [NSAutoreleasePool new];
    REQUIRE(dlopen("/System/Library/Frameworks/AVFoundation.framework/AVFoundation", RTLD_NOW | RTLD_LOCAL), "AVFoundation loads");
    Class cls = NSClassFromString(@"AVSpeechSynthesisVoice");
    if (!cls) { fputs("MISSING_CLASS: AVSpeechSynthesisVoice through AVFoundation\n", stderr); return 20; }
    REQUIRE(signature(cls, @selector(speechVoices), @encode(id), 0), "typed speechVoices method");
    REQUIRE(signature(cls, @selector(currentLanguageCode), @encode(id), 0), "typed currentLanguageCode method");
    REQUIRE(signature(cls, @selector(voiceWithIdentifier:), @encode(id), 1), "typed identifier factory");
    REQUIRE(signature(cls, @selector(voiceWithLanguage:), @encode(id), 1), "typed language factory");
    REQUIRE(![cls respondsToSelector:NSSelectorFromString(@"qvUnknownVoiceSelector:")], "unknown selector remains unsupported");
    if (argc == 1) {
        puts("CLASS_SURFACE_ONLY: catalog/provider/audio/browser behavior untested");
        [pool drain]; return 0;
    }
    REQUIRE(argc == 2, "one independently recorded expected-catalog JSON file argument");
    NSData *data = [NSData dataWithContentsOfFile:[NSString stringWithUTF8String:argv[1]]];
    NSError *error = nil;
    NSDictionary *expected = data ? [NSJSONSerialization JSONObjectWithData:data options:0 error:&error] : nil;
    REQUIRE([expected isKindOfClass:[NSDictionary class]] && !error, "independent expected catalog input");
    NSArray *rows = [expected objectForKey:@"voices"];
    REQUIRE([rows isKindOfClass:[NSArray class]] && [rows count] > 0, "positive provider proof requires independently observed nonempty catalog");
    NSAutoreleasePool *creation = [NSAutoreleasePool new];
    NSArray *voices = [[(id<QVVoiceClass>)cls speechVoices] retain];
    [creation drain];
    REQUIRE([voices isKindOfClass:[NSArray class]] && [voices count] == [rows count], "entire actual provider projection matches count");
    NSMutableSet *identifiers = [NSMutableSet set];
    for (id<QVVoiceRecord> voice in voices) {
        REQUIRE([(id)voice isKindOfClass:cls], "real typed record class");
        REQUIRE(signature(voice, @selector(identifier), @encode(id), 0) && signature(voice, @selector(name), @encode(id), 0) && signature(voice, @selector(language), @encode(id), 0), "object getter signatures");
        REQUIRE(signature(voice, @selector(quality), @encode(NSInteger), 0) && signature(voice, @selector(gender), @encode(NSInteger), 0), "NSInteger getter signatures; numeric mapping tested separately once sourced");
        NSString *identifier = [voice identifier];
        REQUIRE([identifier isKindOfClass:[NSString class]] && [identifier length] && ![identifiers containsObject:identifier], "unique nonempty identifier");
        [identifiers addObject:identifier];
        NSDictionary *row = nil;
        for (NSDictionary *candidate in rows)
            if ([[candidate objectForKey:@"identifier"] isEqual:identifier]) { row = candidate; break; }
        REQUIRE(row && [[voice name] isEqual:[row objectForKey:@"name"]] && [[voice language] isEqual:[row objectForKey:@"language"]], "actual provider metadata agrees with independent projection");
        NSMutableString *query = [identifier mutableCopy];
        id<QVVoiceRecord> found = [[(id<QVVoiceClass>)cls voiceWithIdentifier:query] retain];
        [query appendString:@" changed"]; [query release];
        REQUIRE(found && [[found identifier] isEqual:identifier] && [[found name] isEqual:[voice name]] && [[found language] isEqual:[voice language]], "identifier roundtrip copies lookup input and preserves metadata");
        [(id)found release];
    }
    REQUIRE([(id<QVVoiceClass>)cls voiceWithIdentifier:@"org.qvolution.voice.v1/missing/%00"] == nil, "unknown identifier does not fabricate a voice");
    REQUIRE(nil == [(id<QVVoiceClass>)cls voiceWithLanguage:@"%%%invalid%%%"], "invalid language returns nil");
    NSString *locale = [(id<QVVoiceClass>)cls currentLanguageCode];
    REQUIRE([locale isKindOfClass:[NSString class]] && [locale isEqual:[expected objectForKey:@"currentLanguageCode"]], "user locale is independent of provider ordering");
    (void)[(id<QVVoiceClass>)cls speechVoices];
    for (id<QVVoiceRecord> voice in voices)
        REQUIRE([[voice identifier] length] && [[voice name] length] && [[voice language] length], "retained original snapshot survives another enumeration");
    [voices release]; [pool drain];
    puts("PASS: provider catalog metadata/lookup/lifetime; no audio/browser qualification");
    return 0;
}
