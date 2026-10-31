#define main FrozenRoundtripMain
#include "annotation-roundtrip.m"
#undef main
static NSData *Replaced(NSString *old, NSString *replacement) {
    NSMutableString *text=[NSMutableString stringWithString:[[[NSString alloc] initWithData:InputPDF() encoding:NSASCIIStringEncoding] autorelease]];
    [text replaceOccurrencesOfString:old withString:replacement options:0 range:NSMakeRange(0,[text length])];
    return [text dataUsingEncoding:NSASCIIStringEncoding];
}
int main(void) {
    NSAutoreleasePool *pool=[NSAutoreleasePool new];
    CHECK(![[[PDFDocument alloc] initWithData:Replaced(@"(/Helv 12 Tf 0 g)",@"123              ")] autorelease]);
    CHECK(![[[PDFDocument alloc] initWithData:Replaced(@"/Rect [10 20 110 40]",@"/Rect [10 20 /X  40]")] autorelease]);
    CHECK(![[[PDFDocument alloc] initWithData:Replaced(@"/Subtype /FreeText",@"/Subtype (FreeTxt)")] autorelease]);
    CHECK(![[[PDFDocument alloc] initWithData:Replaced(@"/Size 7 /Root",@"/Encrypt <<>> /Size 7 /Root")] autorelease]);
    CHECK(![[[PDFDocument alloc] initWithData:Replaced(@"/Type /Annot",@"/Type /Wrong")] autorelease]);
    puts("PASS invalid annotation dictionaries and encrypted PDF are rejected");
    [pool drain];return 0;
}
