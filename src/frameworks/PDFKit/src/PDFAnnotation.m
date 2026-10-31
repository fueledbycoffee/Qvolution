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
#import <Onyx2D/O2PDFString.h>
#import <Onyx2D/O2PDFObject_Name.h>
#import <Onyx2D/O2PDFObject_Integer.h>
#import <Onyx2D/O2PDFObject_Real.h>
#import <Onyx2D/O2PDFObject_Boolean.h>
#import <Onyx2D/O2PDFObject_const.h>
#import <CoreGraphics/CGPDFString.h>
#include <math.h>
#include <limits.h>
#include <string.h>

// GUESS: public docs specify the semantic, not the native NSString raw value.
PDFAnnotationKey const PDFAnnotationKeyDefaultAppearance = @"DA";
PDFAnnotationKey const PDFAnnotationKeyIconName = @"Name";

static NSString *QVKey(id key) {
    if (![key isKindOfClass:[NSString class]]) return nil;
    if ([key hasPrefix:@"/"]) key = [key substringFromIndex:1];
    if (![key length]) return nil;
    const char *bytes = [key UTF8String];
    for (NSUInteger i=0; bytes[i]; i++)
        if (!((bytes[i]>='A' && bytes[i]<='Z') || (bytes[i]>='a' && bytes[i]<='z') ||
                (bytes[i]>='0' && bytes[i]<='9') || strchr("._-",bytes[i]))) return nil;
    return strlen(bytes)==[key length] ? key : nil;
}

static O2PDFArray *QVBounds(NSRect bounds) {
    if (!isfinite(bounds.origin.x) || !isfinite(bounds.origin.y) || !isfinite(bounds.size.width) ||
            !isfinite(bounds.size.height) || bounds.size.width<0 || bounds.size.height<0 ||
            !isfinite(bounds.origin.x+bounds.size.width) || !isfinite(bounds.origin.y+bounds.size.height)) return nil;
    O2PDFReal rect[]={bounds.origin.x,bounds.origin.y,bounds.origin.x+bounds.size.width,bounds.origin.y+bounds.size.height};
    return [O2PDFArray pdfArrayWithNumbers:rect count:4];
}

static O2PDFObject *QVToPDF(id value, BOOL appearance, NSUInteger depth) {
    if (depth>32) return nil;
    if ([value isKindOfClass:[NSString class]]) {
        if ([value length]>(appearance ? UINT_MAX : UINT_MAX/2-1)) return nil;
        NSData *data;
        if (appearance) {
            data=[value dataUsingEncoding:NSASCIIStringEncoding];
            if (!data) return nil;
        } else {
            NSData *encoded=[value dataUsingEncoding:NSUTF16BigEndianStringEncoding];
            if (!encoded) return nil;
            NSMutableData *text=[NSMutableData dataWithBytes:"\xfe\xff" length:2];
            [text appendData:encoded]; data=text;
        }
        if ([data length]>UINT_MAX) return nil;
        return [O2PDFString pdfObjectWithBytes:[data bytes] length:[data length]];
    }
    if ([value isKindOfClass:[NSNumber class]]) {
        double number = [value doubleValue];
        if (!isfinite(number)) return nil;
        const char *type = [value objCType];
        if (CFGetTypeID((CFTypeRef)value)==CFBooleanGetTypeID()) return [O2PDFObject_Boolean pdfObjectWithBoolean:[value boolValue]];
        if (strchr("cCsSiIlLqQ",type[0])) {
            if (strchr("CSILQ",type[0]) && [value unsignedLongLongValue]>(unsigned long long)NSIntegerMax) return nil;
            long long integer=[value longLongValue];
            if (integer<NSIntegerMin || integer>NSIntegerMax) return nil;
            return [O2PDFObject_Integer pdfObjectWithInteger:integer];
        }
        return [O2PDFObject_Real pdfObjectWithReal:number];
    }
    if ([value isKindOfClass:[NSArray class]]) {
        O2PDFArray *array = [O2PDFArray pdfArray];
        for (id element in value) {
            O2PDFObject *object = QVToPDF(element,NO,depth+1);
            if (!object) return nil;
            [array addObject:object];
        }
        return array;
    }
    if (value==[NSNull null]) return [O2PDFObject_const pdfObjectWithNull];
    return nil;
}

static id QVFromPDF(O2PDFObject *object, BOOL appearance, NSUInteger depth) {
    if (!object || depth>32) return nil;
    object = [object realObject];
    switch ([object objectType]) {
        case kO2PDFObjectTypeString: {
            O2PDFString *string=(O2PDFString *)object;
            if (appearance) return [[[NSString alloc] initWithBytes:[string bytes] length:[string length]
                    encoding:NSISOLatin1StringEncoding] autorelease];
            return [(id)CGPDFStringCopyTextString((CGPDFStringRef)string) autorelease];
        }
        case kO2PDFObjectTypeName: {
            const char *name=NULL;
            return [object checkForType:kO2PDFObjectTypeName value:&name] ? [NSString stringWithUTF8String:name] : nil;
        }
        case kO2PDFObjectTypeInteger: {
            O2PDFInteger number; return [object checkForType:kO2PDFObjectTypeInteger value:&number] ? [NSNumber numberWithInteger:number] : nil;
        }
        case kO2PDFObjectTypeReal: {
            O2PDFReal number; return [object checkForType:kO2PDFObjectTypeReal value:&number] ? [NSNumber numberWithDouble:number] : nil;
        }
        case kO2PDFObjectTypeBoolean: {
            O2PDFBoolean flag; return [object checkForType:kO2PDFObjectTypeBoolean value:&flag] ? [NSNumber numberWithBool:flag] : nil;
        }
        case kO2PDFObjectTypeNull: return [NSNull null];
        case kO2PDFObjectTypeArray: {
            NSMutableArray *result=[NSMutableArray array];
            O2PDFArray *array=(O2PDFArray *)object;
            for (NSUInteger i=0;i<[array count];i++) {
                id value=QVFromPDF([array objectAtIndex:i],NO,depth+1);
                if (!value) return nil;
                [result addObject:value];
            }
            return [[result copy] autorelease];
        }
        default: return nil;
    }
}

BOOL QVAnnotationDictionaryIsValid(O2PDFDictionary *dictionary) {
    const char *type=NULL;
    O2PDFArray *rect=nil; O2PDFString *appearance=nil; O2PDFObject *entry=nil;
    if ([dictionary getObjectForKey:"Type" value:&entry] && [entry objectType]!=kO2PDFObjectTypeNull) {
        const char *objectType=NULL;
        if (![entry checkForType:kO2PDFObjectTypeName value:&objectType] || strcmp(objectType,"Annot")) return NO;
    }
    if (![dictionary getNameForKey:"Subtype" value:&type] ||
            ![dictionary getArrayForKey:"Rect" value:&rect] || [rect count]!=4) return NO;
    O2PDFReal numbers[4];
    for (NSUInteger i=0;i<4;i++)
        if (![rect getNumberAtIndex:i value:&numbers[i]] || !isfinite(numbers[i])) return NO;
    if (!isfinite(numbers[2]-numbers[0]) || !isfinite(numbers[3]-numbers[1])) return NO;
    BOOL hasAppearance=[dictionary getObjectForKey:"DA" value:&entry] && [entry objectType]!=kO2PDFObjectTypeNull;
    if ((hasAppearance || !strcmp(type,"FreeText")) &&
            ![dictionary getStringForKey:"DA" value:&appearance]) return NO;
    if ((!strcmp(type,"Text") || !strcmp(type,"Stamp")) &&
            [dictionary getObjectForKey:"Name" value:&entry] && [entry objectType]!=kO2PDFObjectTypeNull) {
        const char *icon=NULL;
        if (![entry checkForType:kO2PDFObjectTypeName value:&icon]) return NO;
    }
    return YES;
}

PDFAnnotation *QVAnnotationFromDictionary(O2PDFDictionary *dictionary, O2PDFDocument *document) {
    if (!QVAnnotationDictionaryIsValid(dictionary)) return nil;
    PDFAnnotation *annotation = [[PDFAnnotation alloc] init];
    annotation->_qvDictionary = [dictionary retain];
    annotation->_qvBackingDocument = [document retain];
    return [annotation autorelease];
}

@implementation PDFAnnotation
- (instancetype)initWithBounds:(NSRect)bounds forType:(PDFAnnotationSubtype)type withProperties:(NSDictionary *)properties {
    if (!(self=[super init])) return nil;
    NSString *name=QVKey(type);
    O2PDFArray *rectangle=QVBounds(bounds);
    if (!name || !rectangle || (properties && ![properties isKindOfClass:[NSDictionary class]])) {
        [self release]; return nil;
    }
    _qvDictionary = [[O2PDFDictionary pdfDictionary] retain];
    [_qvDictionary setNameForKey:"Type" value:"Annot"];
    [_qvDictionary setNameForKey:"Subtype" value:[name UTF8String]];
    [_qvDictionary setObjectForKey:"Rect" value:rectangle];
    for (id key in properties) {
        if (![self setValue:[properties objectForKey:key] forAnnotationKey:key]) {
            [self release]; return nil;
        }
    }
    return self;
}
- (void)dealloc {
    [_qvDictionary release]; [_qvReference release]; [_qvBackingDocument release]; [super dealloc];
}
- (id)valueForAnnotationKey:(PDFAnnotationKey)key {
    key=QVKey(key); if (!key) return nil;
    O2PDFObject *object=nil;
    if (![_qvDictionary getObjectForKey:[key UTF8String] value:&object]) return nil;
    return QVFromPDF(object,[key isEqual:@"DA"],0);
}
- (BOOL)setValue:(id)value forAnnotationKey:(PDFAnnotationKey)key {
    key=QVKey(key); if (!key || !_qvDictionary || !value) return NO;
    BOOL appearance=[key isEqual:@"DA"];
    if (appearance && ![value isKindOfClass:[NSString class]]) return NO;
    // Structural entries are managed by the initializer/page model.
    if ([key isEqual:@"Type"] || [key isEqual:@"Subtype"] || [key isEqual:@"Rect"] || [key isEqual:@"P"]) return NO;
    const char *subtype=NULL;
    if ([key isEqual:@"Name"] && [_qvDictionary getNameForKey:"Subtype" value:&subtype] &&
            (!strcmp(subtype,"Text") || !strcmp(subtype,"Stamp"))) {
        if (![value isKindOfClass:[NSString class]]) return NO;
        NSData *data=[value dataUsingEncoding:NSUTF8StringEncoding];
        if (!data || [data length]>=UINT_MAX) return NO;
        const char *bytes=[data length] ? [data bytes] : "";
        if (memchr(bytes,0,[data length])) return NO;
        O2PDFObject *name=[O2PDFObject_Name pdfObjectWithBytes:bytes length:(unsigned)[data length]];
        [_qvDictionary setObjectForKey:"Name" value:name]; return YES;
    }
    O2PDFObject *object=QVToPDF(value,appearance,0);
    if (!object) return NO;
    [_qvDictionary setObjectForKey:[key UTF8String] value:object]; return YES;
}
- (NSString *)type { return [self valueForAnnotationKey:@"Subtype"]; }
- (NSRect)bounds {
    NSArray *rect=[self valueForAnnotationKey:@"Rect"];
    if ([rect count]!=4) return NSZeroRect;
    for (id number in rect) if (![number isKindOfClass:[NSNumber class]]) return NSZeroRect;
    double x1=[[rect objectAtIndex:0] doubleValue],y1=[[rect objectAtIndex:1] doubleValue];
    double x2=[[rect objectAtIndex:2] doubleValue],y2=[[rect objectAtIndex:3] doubleValue];
    return NSMakeRect(fmin(x1,x2),fmin(y1,y2),fabs(x2-x1),fabs(y2-y1));
}
- (void)setBounds:(NSRect)bounds {
    O2PDFArray *rectangle=QVBounds(bounds);
    if (!rectangle) [NSException raise:NSInvalidArgumentException format:@"Invalid PDF annotation bounds"];
    [_qvDictionary setObjectForKey:"Rect" value:rectangle];
}
- (void)setType:(NSString *)type {
    NSString *name=QVKey(type);
    if (!name) [NSException raise:NSInvalidArgumentException format:@"Invalid PDF annotation subtype"];
    [_qvDictionary setNameForKey:"Subtype" value:[name UTF8String]];
}
- (PDFPage *)page { return _qvPage; }
- (void)setPage:(PDFPage *)page {
    if (page) [page addAnnotation:self];
    else [_qvPage removeAnnotation:self];
}
@end
