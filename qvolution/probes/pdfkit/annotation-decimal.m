#define main FrozenRoundtripMain
#include "annotation-roundtrip.m"
#undef main
#include <math.h>
int main(void) {
    CHECK(FrozenRoundtripMain(0,NULL)==0);
    NSAutoreleasePool *pool=[NSAutoreleasePool new];
    PDFDocument *document=[[[PDFDocument alloc] initWithData:InputPDF()] autorelease];
    PDFAnnotation *annotation=[[[document pageAtIndex:0] annotations] objectAtIndex:0];
    NSArray *values=[NSArray arrayWithObjects:@(0.12345678901234567),@(1e-30),@(1e30),@(-1.2345678901234567),nil];
    CHECK([annotation setValue:values forAnnotationKey:@"CustomDecimals"]);
    PDFDocument *again=[[[PDFDocument alloc] initWithData:[document dataRepresentation]] autorelease];
    CHECK(again);
    NSArray *actual=[[[[again pageAtIndex:0] annotations] objectAtIndex:0] valueForAnnotationKey:@"CustomDecimals"];
    CHECK([actual count]==[values count]);
    for (NSUInteger i=0;i<[values count];i++) {
        double wanted=[[values objectAtIndex:i] doubleValue],got=[[actual objectAtIndex:i] doubleValue];
        CHECK(fabs((wanted-got)/wanted)<1e-14);
    }
    puts("PASS annotation PDF decimal precision without exponent syntax");
    [pool drain]; return 0;
}
