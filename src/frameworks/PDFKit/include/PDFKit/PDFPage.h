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

#ifndef QV_PDF_PAGE_MODEL
#define QV_PDF_PAGE_MODEL 1
#import <Foundation/Foundation.h>
@class PDFAnnotation;
@interface PDFPage : NSObject {
@package
    id _qvDictionary;
    id _qvBackingDocument;
    NSMutableArray *_qvAnnotations;
    NSInteger _qvObjectNumber;
    NSInteger _qvGeneration;
}
@property(readonly, copy) NSArray *annotations;
- (void)addAnnotation:(PDFAnnotation *)annotation;
- (void)removeAnnotation:(PDFAnnotation *)annotation;
@end
#endif
