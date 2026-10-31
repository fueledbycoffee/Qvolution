/* SPDX-License-Identifier: GPL-3.0-or-later */
#import <AVFAudio/AVSpeechSynthesisVoice.h>
#include "catalog-client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef QV_VOICE_HELPER_PATH
#define QV_VOICE_HELPER_PATH "/usr/libexec/darling/qv-voice-catalog"
#endif

@interface AVSpeechSynthesisVoice ()
- (id)_qvInitWithRecord:(const struct qv_voice_record *)record;
@end

@implementation AVSpeechSynthesisVoice
- (id)init { [self release];return nil; }
- (id)_qvInitWithRecord:(const struct qv_voice_record *)record
{
    self=[super init];
    if(!self)return nil;
    _qvIdentifier=[[NSString alloc]initWithUTF8String:record->identifier];
    _qvName=[[NSString alloc]initWithUTF8String:record->name];
    _qvLanguage=[[NSString alloc]initWithUTF8String:record->languages[record->primary].canonical];
    NSMutableArray *languages=[NSMutableArray array],*priorities=[NSMutableArray array];
    for(uint32_t i=0;i<record->language_count;i++)if(*record->languages[i].canonical) {
        [languages addObject:[NSString stringWithUTF8String:record->languages[i].canonical]];
        [priorities addObject:[NSNumber numberWithUnsignedInt:record->languages[i].priority]];
    }
    _qvLanguages=[languages copy];_qvPriorities=[priorities copy];return self;
}
- (void)dealloc
{
    [_qvIdentifier release];[_qvName release];[_qvLanguage release];[_qvLanguages release];[_qvPriorities release];[super dealloc];
}
- (NSString *)identifier { return _qvIdentifier; }
- (NSString *)name { return _qvName; }
- (NSString *)language { return _qvLanguage; }
+ (NSArray *)speechVoices
{
    uint8_t *bytes=malloc(QV_CATALOG_MAX_BYTES);
    if(!bytes) { fputs("AVFAudio: voice catalog allocation failed\n",stderr);return [NSArray array]; }
    size_t length=0;int exit_status=-1;
    enum qv_transport_status transport=qv_catalog_run(QV_VOICE_HELPER_PATH,2000,bytes,QV_CATALOG_MAX_BYTES,&length,&exit_status);
    struct qv_catalog_snapshot *snapshot=NULL;
    enum qv_catalog_status decoded=transport==QV_TRANSPORT_OK?qv_catalog_decode(bytes,length,&snapshot):QV_CATALOG_BAD_DATA;
    free(bytes);
    if(transport!=QV_TRANSPORT_OK||decoded!=QV_CATALOG_OK) {
        fprintf(stderr,"AVFAudio: voice catalog failed (transport=%u decode=%u exit=%d)\n",transport,decoded,exit_status);
        qv_catalog_snapshot_free(snapshot);return [NSArray array];
    }
    if(snapshot->provider_status!=QV_CATALOG_OK||exit_status!=(int)snapshot->provider_status) {
        fprintf(stderr,"AVFAudio: voice provider unavailable/error (provider=%u exit=%d)\n",snapshot->provider_status,exit_status);
        qv_catalog_snapshot_free(snapshot);return [NSArray array];
    }
    NSMutableArray *voices=[NSMutableArray array];uint32_t unsupported=0;
    for(uint32_t i=0;i<snapshot->count;i++) {
        const struct qv_voice_record *record=&snapshot->voices[i];
        if(record->primary==UINT32_MAX) { unsupported++;continue; }
        AVSpeechSynthesisVoice *voice=[[[self alloc]_qvInitWithRecord:record]autorelease];
        if(!voice) { qv_catalog_snapshot_free(snapshot);fputs("AVFAudio: voice record allocation failed\n",stderr);return [NSArray array]; }
        [voices addObject:voice];
    }
    qv_catalog_snapshot_free(snapshot);
    if(unsupported)fprintf(stderr,"AVFAudio: %u provider voices have no supported language mapping\n",unsupported);
    return [[voices copy]autorelease];
}
+ (AVSpeechSynthesisVoice *)voiceWithIdentifier:(NSString *)identifier
{
    if(![identifier isKindOfClass:[NSString class]])return nil;
    for(AVSpeechSynthesisVoice *voice in [self speechVoices])if([voice.identifier isEqual:identifier])return voice;
    return nil;
}
+ (NSString *)currentLanguageCode
{
    NSString *code=[[NSLocale currentLocale]languageIdentifier];
    if(![code isKindOfClass:[NSString class]]||![code length])
        [NSException raise:NSInternalInconsistencyException format:@"Current user locale has no language identifier"];
    return code;
}
+ (AVSpeechSynthesisVoice *)voiceWithLanguage:(NSString *)languageCode
{
    if(!languageCode)languageCode=[self currentLanguageCode];
    if(![languageCode isKindOfClass:[NSString class]])return nil;
    NSUInteger units=[languageCode length],used=0;
    if(!units||units>QV_CATALOG_MAX_STRING)return nil;
    char raw[QV_CATALOG_MAX_STRING+1],canonical[QV_CATALOG_MAX_STRING+1];
    NSRange remaining=NSMakeRange(0,units);
    if(![languageCode getBytes:raw maxLength:QV_CATALOG_MAX_STRING usedLength:&used
        encoding:NSUTF8StringEncoding options:0 range:NSMakeRange(0,units) remainingRange:&remaining]||
        remaining.length||!used||used>QV_CATALOG_MAX_STRING||memchr(raw,0,used))return nil;
    raw[used]=0;
    if(qv_canonical_language(raw,canonical,sizeof(canonical))!=QV_LANGUAGE_MAPPED)return nil;
    NSString *requested=[NSString stringWithUTF8String:canonical];
    NSString *base=[[requested componentsSeparatedByString:@"-"]objectAtIndex:0];
    AVSpeechSynthesisVoice *best=nil;BOOL best_exact=NO;unsigned int best_priority=0;
    for(AVSpeechSynthesisVoice *voice in [self speechVoices]) {
        for(NSUInteger i=0;i<[voice->_qvLanguages count];i++) {
            NSString *tag=[voice->_qvLanguages objectAtIndex:i];BOOL exact=[tag isEqual:requested];
            if(!exact&&![[[tag componentsSeparatedByString:@"-"]objectAtIndex:0]isEqual:base])continue;
            unsigned int priority=[[voice->_qvPriorities objectAtIndex:i]unsignedIntValue];
            if(!best||(exact&&!best_exact)||(exact==best_exact&&(priority<best_priority||
                (priority==best_priority&&[voice.identifier compare:best.identifier]==NSOrderedAscending)))) {
                best=voice;best_exact=exact;best_priority=priority;
            }
        }
    }
    return best;
}
@end
