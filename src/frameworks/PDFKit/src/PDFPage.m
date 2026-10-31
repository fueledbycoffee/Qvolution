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

PDFPage *QVPageFromDictionary(O2PDFDictionary *dictionary, O2PDFDocument *document,
        NSInteger number, NSInteger generation) {
    PDFPage *page=[[PDFPage alloc] init];
    page->_qvDictionary=[dictionary retain];
    page->_qvBackingDocument=[document retain];
    page->_qvObjectNumber=number; page->_qvGeneration=generation;
    O2PDFArray *annots=nil;
    O2PDFObject *entry=nil;
    if ([dictionary getObjectForKey:"Annots" value:&entry] && [entry objectType]!=kO2PDFObjectTypeNull &&
            ![entry checkForType:kO2PDFObjectTypeArray value:&annots]) { [page release]; return nil; }
    for (NSUInteger i=0;i<[annots count];i++) {
        O2PDFDictionary *annotationDictionary=nil;
        if (![annots getDictionaryAtIndex:i value:&annotationDictionary]) { [page release]; return nil; }
        O2PDFObject *reference=[annots objectAtIndex:i];
        if ([reference objectTypeNoParsing]!=O2PDFObjectType_R) { [page release]; return nil; }
        PDFAnnotation *annotation=QVAnnotationFromDictionary(annotationDictionary,document);
        if (!annotation) { [page release]; return nil; }
        annotation->_qvReference=[reference retain];
        annotation->_qvPage=page;
        [page->_qvAnnotations addObject:annotation];
    }
    return [page autorelease];
}

@implementation PDFPage
- (instancetype)init {
    if ((self=[super init])) _qvAnnotations=[NSMutableArray new];
    return self;
}
- (void)dealloc {
    for (PDFAnnotation *annotation in _qvAnnotations) annotation->_qvPage=nil;
    [_qvAnnotations release]; [_qvDictionary release]; [_qvBackingDocument release]; [super dealloc];
}
- (NSArray *)annotations { return [[_qvAnnotations copy] autorelease]; }
- (void)addAnnotation:(PDFAnnotation *)annotation {
    if (![annotation isKindOfClass:[PDFAnnotation class]] || !annotation->_qvDictionary)
        [NSException raise:NSInvalidArgumentException format:@"Invalid PDF annotation"];
    if (annotation->_qvBackingDocument && annotation->_qvBackingDocument!=_qvBackingDocument)
        [NSException raise:NSInvalidArgumentException format:@"Cross-document annotation transfer is unsupported"];
    if (annotation->_qvPage==self) return;
    [annotation retain];
    [annotation->_qvPage removeAnnotation:annotation];
    [_qvAnnotations addObject:annotation]; annotation->_qvPage=self;
    [annotation release];
}
- (void)removeAnnotation:(PDFAnnotation *)annotation {
    if ([annotation isKindOfClass:[PDFAnnotation class]] && annotation->_qvPage==self) {
        annotation->_qvPage=nil; [_qvAnnotations removeObjectIdenticalTo:annotation];
    }
}
@end
