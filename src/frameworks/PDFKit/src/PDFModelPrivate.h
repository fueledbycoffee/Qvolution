#import <PDFKit/PDFAnnotation.h>
#import <PDFKit/PDFPage.h>
#import <PDFKit/PDFDocument.h>
#import <Onyx2D/O2PDFDictionary.h>
#import <Onyx2D/O2PDFDocument.h>
#import <Onyx2D/O2PDFArray.h>
#import <Onyx2D/O2PDFPage.h>
#import <Onyx2D/O2PDFxref.h>
#import <Onyx2D/O2PDFxrefEntry.h>

BOOL QVAnnotationDictionaryIsValid(O2PDFDictionary *dictionary);
PDFAnnotation *QVAnnotationFromDictionary(O2PDFDictionary *dictionary, O2PDFDocument *document);
PDFPage *QVPageFromDictionary(O2PDFDictionary *dictionary, O2PDFDocument *document,
        NSInteger number, NSInteger generation);
