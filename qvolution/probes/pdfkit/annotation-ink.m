#import <Foundation/Foundation.h>
#import <PDFKit/PDFAnnotation.h>
#import <PDFKit/PDFPage.h>
#import <PDFKit/PDFDocument.h>
#import <Onyx2D/O2PDFDocument.h>
#import <Onyx2D/O2PDFPage.h>
#import <Onyx2D/O2PDFDictionary.h>
#import <Onyx2D/O2PDFArray.h>
#include <stdio.h>
#include <math.h>
extern PDFAnnotationKey const PDFAnnotationKeyInklist;
#define CHECK(c) do { if (!(c)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#c); return 1; } } while (0)
static void Append(NSMutableData *data, NSString *text) { [data appendData:[text dataUsingEncoding:NSASCIIStringEncoding]]; }
static NSData *InputPDF(NSString *annotation) {
    NSArray *objects=[NSArray arrayWithObjects:@"<< /Type /Catalog /Pages 2 0 R >>",@"<< /Type /Pages /Count 1 /Kids [3 0 R] >>",@"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 300 400] /Resources << >> /Contents 5 0 R /Annots [4 0 R] >>",annotation,@"<< /Length 4 >>\nstream\nq Q\n\nendstream",nil];
    NSMutableData *data=[NSMutableData data]; Append(data,@"%PDF-1.4\n"); NSUInteger offsets[6]={0};
    for(NSUInteger i=0;i<[objects count];i++){offsets[i+1]=[data length]; Append(data,[NSString stringWithFormat:@"%lu 0 obj\n%@\nendobj\n",(unsigned long)i+1,[objects objectAtIndex:i]]);}
    NSUInteger xref=[data length]; Append(data,@"xref\n0 6\n0000000000 65535 f \n");
    for(NSUInteger i=1;i<6;i++) Append(data,[NSString stringWithFormat:@"%010lu 00000 n \n",(unsigned long)offsets[i]]);
    Append(data,[NSString stringWithFormat:@"trailer\n<< /Size 6 /Root 1 0 R >>\nstartxref\n%lu\n%%%%EOF\n",(unsigned long)xref]); return data;
}
int main(int argc,char **argv) {
    puts("ENTER ink annotation"); fflush(stdout); NSAutoreleasePool *pool=[NSAutoreleasePool new];
    NSMutableArray *path=[NSMutableArray arrayWithObjects:@1,@2,@3.5,@4,nil];
    NSMutableArray *strokes=[NSMutableArray arrayWithObjects:path,[NSArray arrayWithObjects:@-2,@0,@8,@9,nil],nil];
    PDFAnnotation *ink=[[PDFAnnotation alloc] initWithBounds:NSMakeRect(1,2,40,50) forType:@"Ink" withProperties:[NSDictionary dictionaryWithObject:strokes forKey:PDFAnnotationKeyInklist]];
    CHECK(ink && PDFAnnotationKeyInklist); [path addObject:@99]; [strokes removeLastObject];
    NSArray *expected=[NSArray arrayWithObjects:[NSArray arrayWithObjects:@1,@2,@3.5,@4,nil],[NSArray arrayWithObjects:@-2,@0,@8,@9,nil],nil];
    CHECK([[ink valueForAnnotationKey:PDFAnnotationKeyInklist] isEqual:expected]);
    NSArray *bad=[NSArray arrayWithObjects:@"bad",[NSArray arrayWithObject:@1],[NSArray arrayWithObject:[NSArray arrayWithObject:@1]],[NSArray arrayWithObject:[NSArray arrayWithObjects:@1,@"x",nil]],[NSArray arrayWithObject:[NSArray arrayWithObjects:@1,[NSNumber numberWithDouble:NAN],nil]],[NSArray arrayWithObject:[NSArray arrayWithObjects:@1,[NSNumber numberWithBool:YES],nil]],nil];
    for(id value in bad){ CHECK(![ink setValue:value forAnnotationKey:PDFAnnotationKeyInklist]); CHECK([[ink valueForAnnotationKey:PDFAnnotationKeyInklist] isEqual:expected]); }
    NSArray *valid=[NSArray arrayWithObjects:[NSArray array], [NSArray arrayWithObjects:@0,@0,@5.25,@-7,nil],nil];
    CHECK([ink setValue:[NSArray array] forAnnotationKey:PDFAnnotationKeyInklist]);
    CHECK([[ink valueForAnnotationKey:PDFAnnotationKeyInklist] isEqual:[NSArray array]]);
    CHECK([ink setValue:valid forAnnotationKey:PDFAnnotationKeyInklist]);
    PDFAnnotation *square=[[[PDFAnnotation alloc] initWithBounds:NSMakeRect(1,2,40,50) forType:@"Square" withProperties:nil] autorelease];
    CHECK([square setValue:@"generic" forAnnotationKey:PDFAnnotationKeyInklist]);
    CHECK([[square valueForAnnotationKey:PDFAnnotationKeyInklist] isEqual:@"generic"]);
    NSArray *invalidPDF=[NSArray arrayWithObjects:@"",@"/InkList null",@"/InkList 7",@"/InkList [[1 2 3]]",@"/InkList [[1 (x)]]",@"/InkList [[1 true]]",nil];
    for(NSString *entry in invalidPDF){NSString *a=[NSString stringWithFormat:@"<< /Type /Annot /Subtype /Ink /Rect [1 2 41 52] %@ >>",entry]; CHECK(![[[PDFDocument alloc] initWithData:InputPDF(a)] autorelease]);}
    NSData *input=InputPDF(@"<< /Type /Annot /Subtype /Ink /Rect [1 2 41 52] /InkList [[10 20 30 40] [1.5 2.5]] >>");
    PDFDocument *incompleteDoc=[[[PDFDocument alloc] initWithData:input] autorelease];
    PDFAnnotation *incomplete=[[[PDFAnnotation alloc] initWithBounds:NSMakeRect(1,2,40,50) forType:@"Ink" withProperties:nil] autorelease];
    CHECK(incompleteDoc && incomplete); [[incompleteDoc pageAtIndex:0] addAnnotation:incomplete];
    CHECK(![incompleteDoc dataRepresentation]);
    CHECK([incomplete setValue:valid forAnnotationKey:PDFAnnotationKeyInklist]);
    CHECK([incompleteDoc dataRepresentation]);
    PDFDocument *doc=[[PDFDocument alloc] initWithData:input]; CHECK(doc && [doc pageCount]==1);
    PDFPage *page=[doc pageAtIndex:0]; CHECK([[page annotations] count]==1);
    PDFAnnotation *loaded=[[page annotations] objectAtIndex:0]; CHECK([[[loaded valueForAnnotationKey:PDFAnnotationKeyInklist] objectAtIndex:0] count]==4);
    [page addAnnotation:ink]; NSData *first=[doc dataRepresentation]; CHECK(first && [first length]>[input length]); CHECK([[first subdataWithRange:NSMakeRange(0,[input length])] isEqual:input]);
    PDFDocument *again=[[PDFDocument alloc] initWithData:first]; CHECK(again && [[ [again pageAtIndex:0] annotations] count]==2);
    CHECK([[[[[again pageAtIndex:0] annotations] objectAtIndex:1] valueForAnnotationKey:PDFAnnotationKeyInklist] isEqual:valid]);
    NSData *second=[again dataRepresentation]; CHECK(second && [second length]>[first length] && [[second subdataWithRange:NSMakeRange(0,[first length])] isEqual:first]); PDFDocument *last=[[[PDFDocument alloc] initWithData:second] autorelease];
    CHECK(last && [[[[[last pageAtIndex:0] annotations] objectAtIndex:1] valueForAnnotationKey:PDFAnnotationKeyInklist] isEqual:valid]);
    O2PDFDocument *raw=[[[O2PDFDocument alloc] initWithData:second] autorelease]; O2PDFDictionary *rawPage=[[raw pageAtNumber:1] dictionary],*dict=nil; O2PDFArray *annots=nil,*list=nil,*stroke=nil; O2PDFReal coordinate=0;
    CHECK([rawPage getArrayForKey:"Annots" value:&annots] && [annots count]==2 && [annots getDictionaryAtIndex:1 value:&dict]);
    CHECK([dict getArrayForKey:"InkList" value:&list] && [list count]==2 && [list getArrayAtIndex:1 value:&stroke] && [stroke getNumberAtIndex:2 value:&coordinate] && coordinate==5.25);
    if(argc>1) CHECK([second writeToFile:[NSString stringWithUTF8String:argv[1]] atomically:YES]);
    [ink release]; [again release]; [doc release]; puts("PASS ink annotation two reloads"); [pool drain]; return 0;
}
