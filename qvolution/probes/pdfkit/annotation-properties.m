#define main FrozenRoundtripMain
#include "annotation-roundtrip.m"
#undef main
int main(void) {
    CHECK(FrozenRoundtripMain(0,NULL)==0);
    NSAutoreleasePool *pool=[NSAutoreleasePool new];
    PDFDocument *document=[[[PDFDocument alloc] initWithData:InputPDF()] autorelease];
    PDFAnnotation *annotation=[[[document pageAtIndex:0] annotations] objectAtIndex:0];
    [annotation setBounds:NSMakeRect(12.125,23.5,101.25,42.75)];
    CHECK(NSEqualRects([annotation bounds],NSMakeRect(12.125,23.5,101.25,42.75)));
    [annotation setType:@"Text"]; CHECK([[annotation type] isEqual:@"Text"]);
    [annotation setPage:[document pageAtIndex:1]];
    CHECK([[[document pageAtIndex:0] annotations] count]==0 && [[[document pageAtIndex:1] annotations] count]==1);
    PDFDocument *again=[[[PDFDocument alloc] initWithData:[document dataRepresentation]] autorelease];
    CHECK(again && [[[again pageAtIndex:1] annotations] count]==1);
    PDFAnnotation *reloaded=[[[again pageAtIndex:1] annotations] objectAtIndex:0];
    CHECK([[reloaded type] isEqual:@"Text"] && NSEqualRects([reloaded bounds],[annotation bounds]));
    [annotation setPage:nil]; CHECK(![annotation page] && [[[document pageAtIndex:1] annotations] count]==0);
    puts("PASS public annotation bounds, type and page property persistence");
    [pool drain]; return 0;
}
