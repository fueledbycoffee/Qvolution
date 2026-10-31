#define main FrozenRoundtripMain
#include "annotation-roundtrip.m"
#undef main
#include <math.h>
int main(void) {
    CHECK(FrozenRoundtripMain(0,NULL)==0);
    NSAutoreleasePool *pool=[NSAutoreleasePool new];
    PDFDocument *document=[[[PDFDocument alloc] initWithData:InputPDF()] autorelease];
    PDFAnnotation *annotation=[[[document pageAtIndex:0] annotations] objectAtIndex:0];
    NSString *text=@"Résumé — 日本語 😀";
    CHECK([annotation setValue:text forAnnotationKey:@"Contents"]);
    CHECK([[annotation valueForAnnotationKey:@"Contents"] isEqual:text]);
    CHECK([annotation setValue:[NSNull null] forAnnotationKey:@"CustomNull"]);
    CHECK([annotation setValue:[NSNumber numberWithLongLong:0x123456789LL] forAnnotationKey:@"CustomWide"]);
    CHECK([annotation setValue:[NSNumber numberWithBool:YES] forAnnotationKey:@"CustomBool"]);
    NSMutableArray *cycle=[NSMutableArray array]; [cycle addObject:cycle];
    CHECK(![annotation setValue:cycle forAnnotationKey:@"CustomCycle"]); [cycle removeAllObjects];
    CHECK(![annotation setValue:@1 forAnnotationKey:@"/Bad Key"]);
    CHECK(![annotation setValue:@"/Helv 12 Tf café" forAnnotationKey:@"DA"]);
    CHECK(![[[PDFAnnotation alloc] initWithBounds:NSMakeRect(0,0,NAN,3) forType:@"FreeText" withProperties:nil] autorelease]);
    NSData *saved=[document dataRepresentation];
    PDFDocument *again=[[[PDFDocument alloc] initWithData:saved] autorelease];
    CHECK(again);
    PDFAnnotation *reloaded=[[[again pageAtIndex:0] annotations] objectAtIndex:0];
    CHECK([[reloaded valueForAnnotationKey:@"Contents"] isEqual:text]);
    CHECK([reloaded valueForAnnotationKey:@"CustomNull"]==[NSNull null]);
    CHECK([[reloaded valueForAnnotationKey:@"CustomWide"] longLongValue]==0x123456789LL);
    CHECK([[reloaded valueForAnnotationKey:@"CustomBool"] boolValue]);
    // Same-width edits preserve all original offsets while giving the page generation two.
    NSMutableString *generation=[NSMutableString stringWithString:[[[NSString alloc] initWithData:InputPDF() encoding:NSASCIIStringEncoding] autorelease]];
    [generation replaceOccurrencesOfString:@"3 0 R" withString:@"3 2 R" options:0 range:NSMakeRange(0,[generation length])];
    [generation replaceOccurrencesOfString:@"3 0 obj" withString:@"3 2 obj" options:0 range:NSMakeRange(0,[generation length])];
    NSRange table=[generation rangeOfString:@"xref\n0 7\n"];
    NSUInteger pageLine=table.location+[@"xref\n0 7\n" length]+20*3;
    [generation replaceCharactersInRange:NSMakeRange(pageLine+11,5) withString:@"00002"];
    PDFDocument *nonzero=[[[PDFDocument alloc] initWithData:[generation dataUsingEncoding:NSASCIIStringEncoding]] autorelease];
    CHECK(nonzero && [nonzero pageCount]==2);
    PDFAnnotation *nonzeroAnnotation=[[[nonzero pageAtIndex:0] annotations] objectAtIndex:0];
    CHECK([nonzeroAnnotation setValue:@"/Helv 22 Tf 0 g" forAnnotationKey:@"DA"]);
    PDFDocument *nonzeroAgain=[[[PDFDocument alloc] initWithData:[nonzero dataRepresentation]] autorelease];
    CHECK(nonzeroAgain && [[[[[nonzeroAgain pageAtIndex:0] annotations] objectAtIndex:0] valueForAnnotationKey:@"DA"] isEqual:@"/Helv 22 Tf 0 g"]);
    puts("PASS annotation unicode, null, wide integer, boolean, cyclic input, nonzero page generation");
    [pool drain]; return 0;
}
