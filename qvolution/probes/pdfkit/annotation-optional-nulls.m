#define main FrozenRoundtripMain
#include "annotation-roundtrip.m"
#undef main
static NSMutableString *Source(void) { return [NSMutableString stringWithString:[[[NSString alloc] initWithData:InputPDF() encoding:NSASCIIStringEncoding] autorelease]]; }
int main(void) {
    NSAutoreleasePool *pool=[NSAutoreleasePool new];
    NSMutableString *text=Source();
    [text replaceOccurrencesOfString:@"/Annots [5 0 R]" withString:@"/Annots null   " options:0 range:NSMakeRange(0,[text length])];
    PDFDocument *empty=[[[PDFDocument alloc] initWithData:[text dataUsingEncoding:NSASCIIStringEncoding]] autorelease];
    CHECK(empty && [[[empty pageAtIndex:0] annotations] count]==0 && [empty dataRepresentation]);
    text=Source();
    [text replaceOccurrencesOfString:@"/FreeText" withString:@"/Text    " options:0 range:NSMakeRange(0,[text length])];
    [text replaceOccurrencesOfString:@"(/Helv 12 Tf 0 g)" withString:@"null             " options:0 range:NSMakeRange(0,[text length])];
    PDFDocument *note=[[[PDFDocument alloc] initWithData:[text dataUsingEncoding:NSASCIIStringEncoding]] autorelease];
    CHECK(note && [[[note pageAtIndex:0] annotations] count]==1 && [note dataRepresentation]);
    text=Source();
    [text replaceOccurrencesOfString:@"(/Helv 12 Tf 0 g)" withString:@"null             " options:0 range:NSMakeRange(0,[text length])];
    CHECK(![[[PDFDocument alloc] initWithData:[text dataUsingEncoding:NSASCIIStringEncoding]] autorelease]);
    puts("PASS optional PDF null Annots/DA and required FreeText DA semantics");
    [pool drain];return 0;
}
