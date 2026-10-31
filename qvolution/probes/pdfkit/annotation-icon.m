#import <Foundation/Foundation.h>
#import <PDFKit/PDFAnnotation.h>
#import <PDFKit/PDFPage.h>
#import <PDFKit/PDFDocument.h>
#import <Onyx2D/O2PDFDocument.h>
#import <Onyx2D/O2PDFPage.h>
#import <Onyx2D/O2PDFDictionary.h>
#import <Onyx2D/O2PDFArray.h>
#import <Onyx2D/O2PDFString.h>
#include <stdio.h>
#include <string.h>
extern NSString * const PDFAnnotationKeyIconName;
// Public documentation declarations let this same fixture compile at the old stub baseline.
#ifndef QV_PDF_ANNOTATION_MODEL
extern NSString * const PDFAnnotationKeyDefaultAppearance;
@interface PDFAnnotation (FrozenPublicAPI)
- (instancetype)initWithBounds:(NSRect)bounds forType:(NSString *)type withProperties:(NSDictionary *)properties;
- (id)valueForAnnotationKey:(NSString *)key;
- (BOOL)setValue:(id)value forAnnotationKey:(NSString *)key;
- (NSRect)bounds;
- (NSString *)type;
- (PDFPage *)page;
@end
@interface PDFDocument (FrozenPublicAPI)
- (instancetype)initWithData:(NSData *)data;
- (NSUInteger)pageCount;
- (PDFPage *)pageAtIndex:(NSUInteger)index;
- (NSData *)dataRepresentation;
@end
@interface PDFPage (FrozenPublicAPI)
- (NSArray *)annotations;
- (void)addAnnotation:(PDFAnnotation *)annotation;
- (void)removeAnnotation:(PDFAnnotation *)annotation;
@end
#endif
#define CHECK(c) do { if (!(c)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#c); return 1; } } while (0)
static void Append(NSMutableData *data, NSString *text) {
    [data appendData:[text dataUsingEncoding:NSASCIIStringEncoding]];
}
static NSData *InputPDF(NSString *annotation) {
    NSArray *objects = [NSArray arrayWithObjects:
        @"<< /Type /Catalog /Pages 2 0 R >>",
        @"<< /Type /Pages /Count 2 /Kids [3 0 R 4 0 R] >>",
        @"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 300 400] /Resources << /ProcSet [/PDF] >> /Contents 6 0 R /Annots [5 0 R] >>",
        @"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 200 300] >>",
        annotation,
        @"<< /Length 4 >>\nstream\nq Q\n\nendstream", nil];
    NSMutableData *data=[NSMutableData data]; Append(data,@"%PDF-1.4\n");
    NSUInteger offsets[7]={0};
    for (NSUInteger i=0;i<[objects count];i++) {
        offsets[i+1]=[data length];
        Append(data,[NSString stringWithFormat:@"%lu 0 obj\n%@\nendobj\n",(unsigned long)i+1,[objects objectAtIndex:i]]);
    }
    NSUInteger xref=[data length];
    Append(data,@"xref\n0 7\n0000000000 65535 f \n");
    for (NSUInteger i=1;i<7;i++) Append(data,[NSString stringWithFormat:@"%010lu 00000 n \n",(unsigned long)offsets[i]]);
    Append(data,[NSString stringWithFormat:@"trailer\n<< /Size 7 /Root 1 0 R >>\nstartxref\n%lu\n%%%%EOF\n",(unsigned long)xref]);
    return data;
}
int main(int argc, char **argv) {
    puts("ENTER icon roundtrip"); fflush(stdout);
    NSAutoreleasePool *pool=[NSAutoreleasePool new];
    NSMutableString *initial=[NSMutableString stringWithString:@"/Helv 11 Tf 0 g"];
    PDFAnnotation *fresh=[[PDFAnnotation alloc] initWithBounds:NSMakeRect(3,4,100,20) forType:@"FreeText"
        withProperties:[NSDictionary dictionaryWithObject:initial forKey:@"DA"]];
    CHECK(fresh && PDFAnnotationKeyDefaultAppearance);
    [initial appendString:@" MUTATED"];
    CHECK([[fresh valueForAnnotationKey:PDFAnnotationKeyDefaultAppearance] isEqual:@"/Helv 11 Tf 0 g"]);
    CHECK(NSEqualRects([fresh bounds],NSMakeRect(3,4,100,20)) && [[fresh type] isEqual:@"FreeText"]);
    CHECK(![fresh setValue:[NSNumber numberWithInt:4] forAnnotationKey:PDFAnnotationKeyDefaultAppearance]);
    CHECK(![fresh setValue:[NSDate date] forAnnotationKey:@"Unknown"]);
    CHECK(![fresh setValue:@"x" forAnnotationKey:nil]);
    CHECK([[fresh valueForAnnotationKey:@"/DA"] isEqual:@"/Helv 11 Tf 0 g"]);
    NSMutableArray *numbers=[NSMutableArray arrayWithObjects:@1,@2,@3,nil];
    CHECK([fresh setValue:numbers forAnnotationKey:@"C"]);
    [numbers addObject:@4];
    CHECK([[fresh valueForAnnotationKey:@"C"] count]==3);
    PDFAnnotation *icon=[[PDFAnnotation alloc] initWithBounds:NSMakeRect(1,2,20,30) forType:@"Text" withProperties:[NSDictionary dictionaryWithObject:@"Comment" forKey:PDFAnnotationKeyIconName]];
    CHECK(icon && [[icon valueForAnnotationKey:PDFAnnotationKeyIconName] isEqual:@"Comment"]);
    CHECK(![icon setValue:@42 forAnnotationKey:PDFAnnotationKeyIconName]);
    CHECK([icon setValue:@"" forAnnotationKey:PDFAnnotationKeyIconName]);
    CHECK([[icon valueForAnnotationKey:PDFAnnotationKeyIconName] isEqual:@""]);
    CHECK([icon setValue:@"Custom Name/#" forAnnotationKey:PDFAnnotationKeyIconName]);
    CHECK([[icon valueForAnnotationKey:PDFAnnotationKeyIconName] isEqual:@"Custom Name/#"]);
    PDFAnnotation *stamp=[[PDFAnnotation alloc] initWithBounds:NSMakeRect(1,2,20,30) forType:@"Stamp" withProperties:[NSDictionary dictionaryWithObject:@"Draft" forKey:PDFAnnotationKeyIconName]];
    CHECK(stamp && [[stamp valueForAnnotationKey:PDFAnnotationKeyIconName] isEqual:@"Draft"]);
    PDFAnnotation *square=[[[PDFAnnotation alloc] initWithBounds:NSMakeRect(1,2,20,30) forType:@"Square" withProperties:[NSDictionary dictionaryWithObject:@"Comment" forKey:PDFAnnotationKeyIconName]] autorelease];
    CHECK(square && [[square valueForAnnotationKey:PDFAnnotationKeyIconName] isEqual:@"Comment"]);
    CHECK(![icon setValue:@42 forAnnotationKey:PDFAnnotationKeyIconName]);
    CHECK([[icon valueForAnnotationKey:PDFAnnotationKeyIconName] isEqual:@"Custom Name/#"]);
    CHECK([stamp setValue:@"" forAnnotationKey:PDFAnnotationKeyIconName]);
    for (NSString *subtype in [NSArray arrayWithObjects:@"Text",@"Stamp",nil]) {
        for (NSString *entry in [NSArray arrayWithObjects:@"/Name (wrong)",@"/Name null",@"",nil]) {
            NSString *annotation=[NSString stringWithFormat:@"<< /Type /Annot /Subtype /%@ /Rect [10 20 110 40] %@ >>",subtype,entry];
            PDFDocument *sample=[[[PDFDocument alloc] initWithData:InputPDF(annotation)] autorelease];
            if ([entry isEqual:@"/Name (wrong)"]) CHECK(!sample);
            else CHECK(sample && [[[sample pageAtIndex:0] annotations] count]==1);
        }
    }
    NSData *input=InputPDF(@"<< /Type /Annot /Subtype /FreeText /Rect [10 20 110 40] /DA (/Helv 12 Tf 0 g) /Contents (before) /CustomFlag 27 >>");
    PDFDocument *doc=[[PDFDocument alloc] initWithData:input];
    CHECK(doc && [doc pageCount]==2);
    BOOL outOfRange=NO;
    @try { [doc pageAtIndex:2]; } @catch (NSException *exception) { outOfRange=YES; }
    CHECK(outOfRange);
    CHECK(![[[PDFDocument alloc] initWithData:[NSData dataWithBytes:"bad" length:3]] autorelease]);
    PDFPage *page=[doc pageAtIndex:0];
    CHECK([[page annotations] count]==1 && page==[doc pageAtIndex:0]);
    PDFAnnotation *loaded=[[page annotations] objectAtIndex:0];
    CHECK([[loaded valueForAnnotationKey:PDFAnnotationKeyDefaultAppearance] isEqual:@"/Helv 12 Tf 0 g"]);
    NSString *changed=@"/Helv 19 Tf 1 0 0 rg % slash\\ and (parentheses)\n";
    CHECK([loaded setValue:changed forAnnotationKey:PDFAnnotationKeyDefaultAppearance]);
    CHECK([[loaded valueForAnnotationKey:@"CustomFlag"] intValue]==27);
    [page addAnnotation:fresh]; [page addAnnotation:fresh]; [page addAnnotation:icon]; [page addAnnotation:stamp];
    CHECK([[page annotations] count]==4 && [fresh page]==page);
    NSData *saved=[doc dataRepresentation];
    CHECK(saved && [saved length]>[input length]);
    CHECK([[saved subdataWithRange:NSMakeRange(0,[input length])] isEqual:input]);
    PDFDocument *again=[[PDFDocument alloc] initWithData:saved];
    CHECK(again && [again pageCount]==2);
    NSArray *reloaded=[[again pageAtIndex:0] annotations];
    CHECK([reloaded count]==4);
    CHECK([[[reloaded objectAtIndex:0] valueForAnnotationKey:PDFAnnotationKeyDefaultAppearance] isEqual:changed]);
    CHECK([[[reloaded objectAtIndex:1] valueForAnnotationKey:@"DA"] isEqual:@"/Helv 11 Tf 0 g"]);
    CHECK([[[reloaded objectAtIndex:0] valueForAnnotationKey:@"CustomFlag"] intValue]==27);
    CHECK([[[reloaded objectAtIndex:2] valueForAnnotationKey:PDFAnnotationKeyIconName] isEqual:@"Custom Name/#"]);
    CHECK([[[reloaded objectAtIndex:3] valueForAnnotationKey:PDFAnnotationKeyIconName] isEqual:@""]);
    O2PDFDocument *raw=[[[O2PDFDocument alloc] initWithData:saved] autorelease];
    O2PDFDictionary *rawPage=[[raw pageAtNumber:1] dictionary]; O2PDFArray *annots=nil;
    CHECK([rawPage getArrayForKey:"Annots" value:&annots] && [annots count]==4);
    CHECK([[annots objectAtIndex:0] objectTypeNoParsing]==O2PDFObjectType_R);
    O2PDFDictionary *dict=nil; O2PDFString *da=nil;
    CHECK([annots getDictionaryAtIndex:0 value:&dict] && [dict getStringForKey:"DA" value:&da]);
    CHECK([[[[NSString alloc] initWithBytes:[da bytes] length:[da length] encoding:NSISOLatin1StringEncoding] autorelease] isEqual:changed]);
    const char *rawName=NULL; O2PDFDictionary *iconDict=nil;
    CHECK([annots getDictionaryAtIndex:2 value:&iconDict] && [iconDict getNameForKey:"Name" value:&rawName] && !strcmp(rawName,"Custom Name/#"));
    CHECK([annots getDictionaryAtIndex:3 value:&iconDict] && [iconDict getNameForKey:"Name" value:&rawName] && !strcmp(rawName,""));
    O2PDFObject *contents=nil; CHECK([rawPage getObjectForKey:"Contents" value:&contents] && [contents objectType]==kO2PDFObjectTypeStream);
    PDFPage *next=[again pageAtIndex:1]; PDFAnnotation *moved=[reloaded objectAtIndex:1];
    [next addAnnotation:moved];
    CHECK([[[again pageAtIndex:0] annotations] count]==3 && [[next annotations] count]==1 && [moved page]==next);
    [next removeAnnotation:moved]; CHECK(![moved page] && [[next annotations] count]==0);
    NSData *twice=[again dataRepresentation];
    PDFDocument *last=[[[PDFDocument alloc] initWithData:twice] autorelease];
    CHECK(last && [[[last pageAtIndex:0] annotations] count]==3 && [[[last pageAtIndex:1] annotations] count]==0);
    CHECK([[[[[last pageAtIndex:0] annotations] objectAtIndex:0] valueForAnnotationKey:@"DA"] isEqual:changed]);
    CHECK([[[[[last pageAtIndex:0] annotations] objectAtIndex:1] valueForAnnotationKey:PDFAnnotationKeyIconName] isEqual:@"Custom Name/#"]);
    CHECK([[[[[last pageAtIndex:0] annotations] objectAtIndex:2] valueForAnnotationKey:PDFAnnotationKeyIconName] isEqual:@""]);
    O2PDFDocument *finalRaw=[[[O2PDFDocument alloc] initWithData:twice] autorelease];
    O2PDFArray *finalAnnots=nil; O2PDFDictionary *finalPage=[[finalRaw pageAtNumber:1] dictionary];
    CHECK([finalPage getArrayForKey:"Annots" value:&finalAnnots] && [finalAnnots count]==3);
    CHECK([finalAnnots getDictionaryAtIndex:1 value:&iconDict] && [iconDict getNameForKey:"Name" value:&rawName] && !strcmp(rawName,"Custom Name/#"));
    CHECK([finalAnnots getDictionaryAtIndex:2 value:&iconDict] && [iconDict getNameForKey:"Name" value:&rawName] && !strcmp(rawName,""));
    if (argc>1) CHECK([twice writeToFile:[NSString stringWithUTF8String:argv[1]] atomically:YES]);
    [fresh release]; [again release]; [doc release];
    puts("PASS annotation dictionary edit, indirect persistence, two reloads, ownership, preserved contents");
    [pool drain]; return 0;
}
