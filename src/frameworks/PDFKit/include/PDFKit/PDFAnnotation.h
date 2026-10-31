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

#ifndef QV_PDF_ANNOTATION_MODEL
#define QV_PDF_ANNOTATION_MODEL 1
#import <Foundation/Foundation.h>
#import <PDFKit/PDFKitExport.h>

@class PDFPage;
typedef NSString *PDFAnnotationKey;
typedef NSString *PDFAnnotationSubtype;
PDFKIT_EXPORT PDFAnnotationKey const PDFAnnotationKeyDefaultAppearance;
PDFKIT_EXPORT PDFAnnotationKey const PDFAnnotationKeyIconName;

@interface PDFAnnotation : NSObject {
@package
    id _qvDictionary;
    id _qvReference;
    id _qvBackingDocument;
    PDFPage *_qvPage;
}
- (instancetype)initWithBounds:(NSRect)bounds forType:(PDFAnnotationSubtype)type withProperties:(NSDictionary *)properties;
- (id)valueForAnnotationKey:(PDFAnnotationKey)key;
- (BOOL)setValue:(id)value forAnnotationKey:(PDFAnnotationKey)key;
@property NSRect bounds;
@property(copy) NSString *type;
@property(assign) PDFPage *page;
@end
#endif
