#define main FrozenRoundtripMain
#include "annotation-roundtrip.m"
#undef main
#import <Onyx2D/O2PDFxref.h>
static NSData *ReviewPDF(void) {
    NSMutableString *text=[NSMutableString stringWithString:[[[NSString alloc] initWithData:InputPDF() encoding:NSASCIIStringEncoding] autorelease]];
    [text replaceOccurrencesOfString:@"/ProcSet" withString:@"/F#31   " options:0 range:NSMakeRange(0,[text length])];
    [text replaceOccurrencesOfString:@"/Size 7 /Root" withString:@"/ID [<00112233445566778899AABBCCDDEEFF> <112233445566778899AABBCCDDEEFF00>] /Size 7 /Root" options:0 range:NSMakeRange(0,[text length])];
    NSRange marker=[text rangeOfString:@"startxref\n" options:NSBackwardsSearch];
    NSRange newline=[text rangeOfString:@"\n" options:0 range:NSMakeRange(NSMaxRange(marker),[text length]-NSMaxRange(marker))];
    [text insertString:@" % comment startxref 0" atIndex:newline.location];
    return [text dataUsingEncoding:NSASCIIStringEncoding];
}
static NSData *RawBytes(O2PDFString *string) { return [NSData dataWithBytes:[string bytes] length:[string length]]; }
int main(void) {
    CHECK(FrozenRoundtripMain(0,NULL)==0);
    NSAutoreleasePool *pool=[NSAutoreleasePool new];
    NSData *original=ReviewPDF();
    PDFDocument *document=[[[PDFDocument alloc] initWithData:original] autorelease];CHECK(document);
    PDFAnnotation *annotation=[[[document pageAtIndex:0] annotations] objectAtIndex:0];
    unichar controls[]={0x18,0x0C,0x7F,0};
    NSString *text=[NSString stringWithCharacters:controls length:4];
    CHECK([annotation setValue:text forAnnotationKey:@"Contents"]);
    CHECK([[annotation valueForAnnotationKey:@"Contents"] isEqual:text]);
    CHECK([annotation setValue:@"" forAnnotationKey:@"DA"]);
    NSData *saved=[document dataRepresentation];CHECK(saved);
    PDFDocument *again=[[[PDFDocument alloc] initWithData:saved] autorelease];CHECK(again);
    PDFAnnotation *reloaded=[[[again pageAtIndex:0] annotations] objectAtIndex:0];
    CHECK([[reloaded valueForAnnotationKey:@"Contents"] isEqual:text]);
    CHECK([[reloaded valueForAnnotationKey:@"DA"] isEqual:@""]);
    O2PDFDocument *raw=[[[O2PDFDocument alloc] initWithData:saved] autorelease];
    O2PDFDictionary *resources=nil;
    CHECK([[[raw pageAtNumber:1] dictionary] getDictionaryForKey:"Resources" value:&resources]);
    O2PDFArray *names=nil;CHECK([resources getArrayForKey:"F1" value:&names]);
    CHECK([[raw xref] trailer] && ![[[raw xref] trailer] objectForKeyNoParsing:"XRefStm"]);
    O2PDFArray *newID=nil,*oldID=nil;O2PDFString *oldFirst=nil,*oldSecond=nil,*newFirst=nil,*newSecond=nil;
    O2PDFDocument *before=[[[O2PDFDocument alloc] initWithData:original] autorelease];
    CHECK([[[raw xref] trailer] getArrayForKey:"ID" value:&newID] && [[[before xref] trailer] getArrayForKey:"ID" value:&oldID]);
    CHECK([oldID getStringAtIndex:0 value:&oldFirst] && [oldID getStringAtIndex:1 value:&oldSecond] &&
          [newID getStringAtIndex:0 value:&newFirst] && [newID getStringAtIndex:1 value:&newSecond]);
    CHECK([RawBytes(oldFirst) isEqual:RawBytes(newFirst)] && ![RawBytes(oldSecond) isEqual:RawBytes(newSecond)]);
    PDFDocument *twice=[[[PDFDocument alloc] initWithData:[again dataRepresentation]] autorelease];CHECK(twice);
    PDFAnnotation *fresh=[[[PDFAnnotation alloc] initWithBounds:NSMakeRect(0,0,10,10) forType:@"FreeText" withProperties:nil] autorelease];
    [[document pageAtIndex:1] addAnnotation:fresh];CHECK(![document dataRepresentation]);
    CHECK([fresh setValue:@"/Helv 12 Tf 0 g" forAnnotationKey:@"DA"] && [document dataRepresentation]);
    puts("PASS PDF text controls, empty strings, escaped names, changing ID and required DA validation");
    [pool drain];return 0;
}
