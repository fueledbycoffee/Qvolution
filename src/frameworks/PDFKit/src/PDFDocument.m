/*
 This file is part of Darling.

 Copyright (C) 2019 Lubos Dolezel

 Darling is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 Darling is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with Darling.  If not, see <http://www.gnu.org/licenses/>.
*/

#import "PDFModelPrivate.h"
#import <Onyx2D/O2PDFObject_R.h>
#import <Onyx2D/O2PDFScanner.h>
#import <Onyx2D/O2PDFObject_identifier.h>
#include <limits.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <xlocale.h>
#import <Onyx2D/O2PDFObject_const.h>
#import <Onyx2D/O2PDFString.h>
#import <CommonCrypto/CommonDigest.h>
#include <stdarg.h>
#include <string.h>

PDFDocumentAttribute const PDFDocumentAuthorAttribute = @"Author";
PDFDocumentAttribute const PDFDocumentCreationDateAttribute = @"CreationDate";
PDFDocumentAttribute const PDFDocumentKeywordsAttribute = @"Keywords";
PDFDocumentAttribute const PDFDocumentTitleAttribute = @"Title";

// PDF numbers forbid exponent notation. Expand a round-trip decimal token before
// writing it, so the existing six-decimal O2 encoder cannot round annotation data.
static NSString *QVDecimal(double value) {
    if (!isfinite(value)) return nil;
    char token[64], digits[64];
    int length=snprintf_l(token,sizeof(token),LC_C_LOCALE,"%.17g",value);
    if (length<=0 || length>=sizeof(token)) return nil;
    const char *cursor=token;
    BOOL negative=(*cursor=='-'); if (negative) cursor++;
    NSInteger point=0,count=0;
    BOOL fractional=NO;
    while (*cursor && *cursor!='e' && *cursor!='E') {
        if (*cursor=='.') fractional=YES;
        else if (*cursor>='0' && *cursor<='9') {
            digits[count++]=*cursor; if (!fractional) point++;
        } else return nil;
        cursor++;
    }
    if (*cursor) point+=strtol(cursor+1,NULL,10);
    NSMutableString *result=[NSMutableString stringWithString:negative ? @"-" : @""];
    if (point<=0) {
        [result appendString:@"0."];
        for (NSInteger i=point;i<0;i++) [result appendString:@"0"];
    }
    for (NSInteger i=0;i<count;i++) {
        if (i==point && i>0) [result appendString:@"."];
        [result appendFormat:@"%c",digits[i]];
    }
    for (NSInteger i=count;i<point;i++) [result appendString:@"0"];
    if ([result rangeOfString:@"."].location==NSNotFound) [result appendString:@".0"];
    return result;
}

// Internal adapter for the existing O2PDFObject encoding protocol. References keep
// their original numbers; strings and names are escaped as PDF syntax, not text.
@interface QVPDFEncoder : NSObject {
@public
    NSMutableData *data;
    BOOL valid;
    NSUInteger depth;
}
- (void)appendString:(NSString *)string;
- (void)appendCString:(const char *)string;
- (void)appendData:(NSData *)bytes;
- (void)appendBytes:(const void *)bytes length:(NSUInteger)length;
- (void)appendFormat:(NSString *)format, ...;
- (void)appendPDFStringWithBytes:(const void *)bytes length:(NSUInteger)length;
- (void)encodePDFObject:(id)object;
@end
@implementation QVPDFEncoder
- (instancetype)init { if ((self=[super init])) { data=[NSMutableData new]; valid=YES; } return self; }
- (void)dealloc { [data release]; [super dealloc]; }
- (void)appendString:(NSString *)string {
    NSData *bytes=[string dataUsingEncoding:NSASCIIStringEncoding];
    if (!bytes) valid=NO; else [data appendData:bytes];
}
- (void)appendCString:(const char *)string { [data appendBytes:string length:strlen(string)]; }
- (void)appendData:(NSData *)bytes { [data appendData:bytes]; }
- (void)appendBytes:(const void *)bytes length:(NSUInteger)length { [data appendBytes:bytes length:length]; }
- (void)appendFormat:(NSString *)format, ... {
    va_list args; va_start(args,format);
    if ([format isEqual:@"/%s "]) {
        const unsigned char *name=(const unsigned char *)va_arg(args,const char *);
        [self appendCString:"/"];
        for (;*name;name++) {
            if (*name<=32 || *name>=127 || strchr("()<>[]{}/%#",*name))
                [self appendString:[NSString stringWithFormat:@"#%02X",*name]];
            else [data appendBytes:name length:1];
        }
        [self appendCString:" "];
    } else if ([format isEqual:@"%f "]) {
        [self appendString:QVDecimal(va_arg(args,double))]; [self appendCString:" "];
    } else if ([format isEqual:@"%d "]) {
        [self appendString:[NSString stringWithFormat:@"%ld ",(long)va_arg(args,O2PDFInteger)]];
    } else if ([format isEqual:@"%d %d R "]) {
        O2PDFInteger number=va_arg(args,O2PDFInteger), generation=va_arg(args,O2PDFInteger);
        [self appendString:[NSString stringWithFormat:@"%ld %ld R ",(long)number,(long)generation]];
    } else {
        NSString *string=[[NSString alloc] initWithFormat:format arguments:args];
        [self appendString:string]; [string release];
    }
    va_end(args);
}
- (void)appendPDFStringWithBytes:(const void *)bytes length:(NSUInteger)length {
    const unsigned char *source=bytes;
    static const char hex[]="0123456789ABCDEF";
    [self appendCString:"<"];
    for (NSUInteger i=0;i<length;i++) {
        char pair[]={hex[source[i]>>4],hex[source[i]&15]};
        [data appendBytes:pair length:2];
    }
    [self appendCString:"> "];
}
- (void)encodePDFObject:(id)object {
    if (depth>=64) { valid=NO; return; }
    depth++;
    if ([object objectTypeNoParsing]==kO2PDFObjectTypeNull) [self appendCString:"null "];
    else [object encodeWithPDFContext:(id)self];
    depth--;
}
@end

static BOOL QVCollectPages(O2PDFObject *reference, NSMutableArray *references, NSMutableSet *visited) {
    if (!reference) return NO;
    NSMutableArray *pending=[NSMutableArray arrayWithObject:reference];
    while ([pending count]) {
        reference=[[[pending lastObject] retain] autorelease];
        [pending removeLastObject];
        O2PDFDictionary *dictionary=nil;
        if (![reference checkForType:kO2PDFObjectTypeDictionary value:&dictionary]) return NO;
        NSValue *identity=[NSValue valueWithPointer:dictionary];
        if ([visited containsObject:identity]) return NO;
        [visited addObject:identity];
        const char *type=NULL;
        if (![dictionary getNameForKey:"Type" value:&type]) return NO;
        if (!strcmp(type,"Page")) {
            if ([reference objectTypeNoParsing]!=O2PDFObjectType_R) return NO;
            [references addObject:reference];
        } else {
            if (strcmp(type,"Pages")) return NO;
            O2PDFArray *kids=nil;
            if (![dictionary getArrayForKey:"Kids" value:&kids]) return NO;
            for (NSUInteger i=[kids count];i>0;i--) [pending addObject:[kids objectAtIndex:i-1]];
        }
    }
    return YES;
}

static BOOL QVReferenceNumbers(O2PDFObject *reference, NSInteger *number, NSInteger *generation) {
    QVPDFEncoder *encoder=[[[QVPDFEncoder alloc] init] autorelease];
    [encoder encodePDFObject:reference];
    O2PDFInteger cursor=0;
    return O2PDFScanInteger([encoder->data bytes],[encoder->data length],0,&cursor,number) &&
        O2PDFScanInteger([encoder->data bytes],[encoder->data length],cursor,&cursor,generation) &&
        *number>0 && *generation>=0 && *generation<=65535;
}

@implementation PDFDocument
- (instancetype)initWithData:(NSData *)source {
    if (!(self=[super init])) return nil;
    if (![source isKindOfClass:[NSData class]] || [source length]>UINT_MAX) { [self release]; return nil; }
    _qvData=[source copy];
    _qvBackingDocument=[[O2PDFDocument alloc] initWithData:_qvData];
    O2PDFDocument *document=_qvBackingDocument;
    O2PDFInteger size=0;
    O2PDFObject *encryption=nil;
    BOOL encrypted=[[[document xref] trailer] getObjectForKey:"Encrypt" value:&encryption] &&
            [encryption objectType]!=kO2PDFObjectTypeNull;
    if (!document || ![document catalog] || encrypted ||
            ![[[document xref] trailer] getIntegerForKey:"Size" value:&size] || size<=0 ||
            [document pageCount]<0) { [self release]; return nil; }
    // The parser has already validated the EOF structure; use its integer scanner
    // to recover the final cross-reference offset for the incremental Prev entry.
    const char *bytes=[_qvData bytes]; NSUInteger length=[_qvData length];
    O2PDFInteger cursor=0,ignore=0;
    O2PDFObject_identifier *identifier=nil;
    _qvStartXref=-1;
    if (!O2PDFScanBackwardsByLines(bytes,length,length,&cursor,3) ||
            !O2PDFScanIdentifier(bytes,length,cursor,&cursor,&identifier) ||
            [identifier identifier]!=O2PDFIdentifier_startxref ||
            !O2PDFScanInteger(bytes,length,cursor,&ignore,&_qvStartXref)) { [self release]; return nil; }
    NSMutableArray *references=[NSMutableArray array];
    O2PDFObject *pages=[[document catalog] objectForKeyNoParsing:"Pages"];
    if (_qvStartXref<0 || (NSUInteger)_qvStartXref>=length ||
            !QVCollectPages(pages,references,[NSMutableSet set]) ||
            [references count]!=(NSUInteger)[document pageCount]) { [self release]; return nil; }
    NSMutableArray *result=[NSMutableArray array];
    NSMutableSet *annotationIdentities=[NSMutableSet set];
    NSMutableSet *objectNumbers=[NSMutableSet set];
    for (O2PDFObject *reference in references) {
        O2PDFDictionary *dictionary=nil;
        NSInteger number=0,generation=0;
        if (![reference checkForType:kO2PDFObjectTypeDictionary value:&dictionary] ||
                !QVReferenceNumbers(reference,&number,&generation) || number>=size) { [self release]; return nil; }
        NSNumber *objectNumber=[NSNumber numberWithInteger:number];
        if ([objectNumbers containsObject:objectNumber]) { [self release]; return nil; }
        [objectNumbers addObject:objectNumber];
        PDFPage *page=QVPageFromDictionary(dictionary,document,number,generation);
        if (!page) { [self release]; return nil; }
        for (PDFAnnotation *annotation in page->_qvAnnotations) {
            NSInteger annotationNumber,annotationGeneration;
            if (!QVReferenceNumbers(annotation->_qvReference,&annotationNumber,&annotationGeneration) || annotationNumber>=size) {
                [self release]; return nil;
            }
            NSNumber *annotationObjectNumber=[NSNumber numberWithInteger:annotationNumber];
            if ([objectNumbers containsObject:annotationObjectNumber]) { [self release]; return nil; }
            [objectNumbers addObject:annotationObjectNumber];
            NSValue *identity=[NSValue valueWithPointer:annotation->_qvDictionary];
            if ([annotationIdentities containsObject:identity]) { [self release]; return nil; }
            [annotationIdentities addObject:identity];
        }
        [result addObject:page];
    }
    _qvPages=[result copy]; return self;
}
- (instancetype)initWithURL:(NSURL *)url { return [self initWithData:[NSData dataWithContentsOfURL:url]]; }
- (void)dealloc { [_qvPages release]; [_qvBackingDocument release]; [_qvData release]; [super dealloc]; }
- (NSUInteger)pageCount { return [_qvPages count]; }
- (PDFPage *)pageAtIndex:(NSUInteger)index {
    if (index>=[_qvPages count]) [NSException raise:NSRangeException format:@"PDF page index out of bounds"];
    return [_qvPages objectAtIndex:index];
}
- (NSData *)dataRepresentation {
    if (!_qvBackingDocument || !_qvData) return nil;
    O2PDFxref *xref=[_qvBackingDocument xref];
    O2PDFDictionary *trailer=[xref trailer];
    O2PDFInteger next=0;
    O2PDFObject *root=[trailer objectForKeyNoParsing:"Root"];
    if (![trailer getIntegerForKey:"Size" value:&next] || next<=0 ||
            [root objectTypeNoParsing]!=O2PDFObjectType_R) return nil;
    NSUInteger count=0;
    for (PDFPage *page in _qvPages) {
        if (count>NSIntegerMax-[page->_qvAnnotations count]) return nil;
        count += [page->_qvAnnotations count];
    }
    if (count>NSIntegerMax-next) return nil;
    QVPDFEncoder *encoder=[[[QVPDFEncoder alloc] init] autorelease];
    [encoder appendData:_qvData]; [encoder appendCString:"\n"];
    NSMutableArray *entries=[NSMutableArray array];
    for (PDFPage *page in _qvPages) {
        O2PDFArray *annots=[O2PDFArray pdfArray];
        for (PDFAnnotation *annotation in page->_qvAnnotations) {
            if (!QVAnnotationDictionaryIsValid(annotation->_qvDictionary)) return nil;
            NSInteger number=next,generation=0;
            O2PDFObject *reference=annotation->_qvReference;
            if (reference) {
                if (!QVReferenceNumbers(reference,&number,&generation)) return nil;
            } else {
                reference=[O2PDFObject_R pdfObjectWithNumber:next++ generation:0 xref:xref];
            }
            [annots addObject:reference];
            O2PDFObject *pageReference=[O2PDFObject_R pdfObjectWithNumber:page->_qvObjectNumber generation:page->_qvGeneration xref:xref];
            [annotation->_qvDictionary setObjectForKey:"P" value:pageReference];
            [entries addObject:[NSArray arrayWithObjects:[NSNumber numberWithInteger:number],[NSNumber numberWithInteger:generation],
                    [NSNumber numberWithUnsignedInteger:[encoder->data length]],nil]];
            [encoder appendFormat:@"%ld %ld obj\n",(long)number,(long)generation];
            [encoder encodePDFObject:annotation->_qvDictionary]; [encoder appendCString:"endobj\n"];
        }
        [page->_qvDictionary setObjectForKey:"Annots" value:annots];
        [entries addObject:[NSArray arrayWithObjects:[NSNumber numberWithInteger:page->_qvObjectNumber],
                [NSNumber numberWithInteger:page->_qvGeneration],
                [NSNumber numberWithUnsignedInteger:[encoder->data length]],nil]];
        [encoder appendFormat:@"%ld %ld obj\n",(long)page->_qvObjectNumber,(long)page->_qvGeneration];
        [encoder encodePDFObject:page->_qvDictionary]; [encoder appendCString:"endobj\n"];
    }
    NSUInteger offset=[encoder->data length];
    if (offset>UINT_MAX) return nil;
    [encoder appendCString:"xref\n"];
    if (![entries count]) [encoder appendCString:"0 1\n0000000000 65535 f \n"];
    for (NSArray *entry in entries) {
        [encoder appendFormat:@"%ld 1\n%010lu %05ld n \n",(long)[[entry objectAtIndex:0] integerValue],
                (unsigned long)[[entry objectAtIndex:2] unsignedIntegerValue],(long)[[entry objectAtIndex:1] integerValue]];
    }
    if ([encoder->data length]>UINT_MAX) return nil;
    QVPDFEncoder *copy=[[[QVPDFEncoder alloc] init] autorelease];
    [copy encodePDFObject:trailer];
    O2PDFObject *cloned=nil; O2PDFDictionary *updated=nil; O2PDFInteger end=0;
    if (!copy->valid || [copy->data length]>UINT_MAX ||
            !O2PDFParseObject([copy->data bytes],[copy->data length],0,&end,&cloned,xref) ||
            ![cloned checkForType:kO2PDFObjectTypeDictionary value:&updated]) return nil;
    [updated setIntegerForKey:"Size" value:next]; [updated setIntegerForKey:"Prev" value:_qvStartXref];
    [updated removeObjectForKey:"XRefStm"];
    const char *trailerType=NULL;
    if ([trailer getNameForKey:"Type" value:&trailerType] && !strcmp(trailerType,"XRef")) {
        for (NSString *key in [NSArray arrayWithObjects:@"Type",@"W",@"Index",@"Length",@"Filter",@"DecodeParms",nil])
            [updated removeObjectForKey:[key UTF8String]];
    }
    O2PDFObject *identifier=nil;
    if ([trailer getObjectForKey:"ID" value:&identifier] && [identifier objectType]!=kO2PDFObjectTypeNull) {
        O2PDFArray *oldID=nil; O2PDFString *permanent=nil,*previous=nil;
        if (![identifier checkForType:kO2PDFObjectTypeArray value:&oldID] || [oldID count]!=2 ||
                ![oldID getStringAtIndex:0 value:&permanent] || ![oldID getStringAtIndex:1 value:&previous]) return nil;
        unsigned char digest[CC_MD5_DIGEST_LENGTH];
        if (!CC_MD5([encoder->data bytes],[encoder->data length],digest)) return nil;
        O2PDFArray *newID=[O2PDFArray pdfArray];
        [newID addObject:permanent]; [newID addObject:[O2PDFString pdfObjectWithBytes:digest length:sizeof(digest)]];
        [updated setObjectForKey:"ID" value:newID];
    }
    [encoder appendCString:"trailer\n"]; [encoder encodePDFObject:updated];
    [encoder appendFormat:@"startxref\n%lu\n%%%%EOF\n",(unsigned long)offset];
    return encoder->valid && [encoder->data length]<=UINT_MAX ? [[encoder->data copy] autorelease] : nil;
}
- (BOOL)writeToFile:(NSString *)path { return [[self dataRepresentation] writeToFile:path atomically:YES]; }
- (BOOL)writeToURL:(NSURL *)url { return [[self dataRepresentation] writeToURL:url atomically:YES]; }
@end
