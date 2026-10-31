# Bounded PDF annotation dictionary model

The typed public clients `annotation-roundtrip.m` and `annotation-boundaries.m`
exercise genuine PDFAnnotation/PDFPage/PDFDocument methods and the existing
O2PDFDocument parser. They do not manufacture a forwarding class or a private
selector. The initial fixture was frozen before production edits. Further public-doc
verification corrected its original out-of-range nil assumption: pageAtIndex:
must raise an exception. The original source/object stay frozen as v1 evidence;
a separately frozen v2 replaces only that assertion. Strict old-header v2
compilation passes and old PDFKit linkage fails solely on
`_PDFAnnotationKeyDefaultAppearance`. Strict error flags must follow the build's
inherited `-Wno-error` flags. Root retains the exact original object and commands
in `Qvolution-work/parallel/pdf-model-next/probe`.

`qvolution_pdf_annotation_roundtrip` loads an authored two-page PDF, reads its
FreeText DA, changes it, adds/moves/removes an annotation, serializes twice and
reloads twice. It independently checks real indirect annotation dictionaries and
DA string type through O2PDFDocument, checks unknown numeric properties, and
verifies the original PDF byte prefix and page content stream survive. Mutable
input isolation and failed assignments are included. The optional first argument
writes the final serialized fixture PDF for independent readers.

`qvolution_pdf_annotation_boundaries` first runs that unchanged roundtrip, then
checks Unicode Contents, null, wide integers, booleans, invalid keys/values,
cyclic array rejection, NaN bounds rejection, and an original page object whose
generation is two. Both require their explicit PASS lines and exit zero.

`qvolution_pdf_annotation_properties` checks the documented bounds/type/page
setters, including moved-page persistence. `qvolution_pdf_annotation_decimal`
checks fractional, tiny and large real dictionary values survive saving without
PDF-forbidden exponent notation. These also run the base v2 roundtrip first.
`qvolution_pdf_annotation_invalid` rejects malformed annotation types, rectangles
and DA entries, and rejects encrypted input. Missing required annotation entries
also make serialization fail; no default DA is invented.
`qvolution_pdf_annotation_review` verifies PDF text control characters, empty DA,
escaped resource-key preservation, permanent/changing file identifiers, a comment
containing a misleading startxref substring, and adding a required DA before save.
`qvolution_pdf_annotation_optional_nulls` checks optional null Annots/DA values
and rejection of a required FreeText DA that is null.

`qvolution_pdf_annotation_icon` exercises typed icon-key access for Text and
Stamp annotations, invalid assignments and subtype-specific encoding, and reloads the
saved document while checking the PDF object is a name, including an empty name
and an escaped custom name. It checks wrong-type, null and absent names for both
subtypes and preserves generic conversion for other subtypes. An independent reader
must also verify the saved PDF. It does not test rendering.

`qvolution_pdf_annotation_ink` checks typed InkList nested coordinate arrays,
invalid inputs, required loaded entries, mutable input isolation, and two saves
and reloads. Empty outer and path arrays are accepted as a permissive project
policy; neither native PDFKit behavior nor ISO conformance for them is claimed.
Other subtypes retain generic conversion. An independent PDF reader must verify
the resulting file. This does
not test path rendering or NSBezierPath convenience APIs.

## Implementation boundary

Annotations retain actual O2PDFDictionary objects. Foundation scalar/string/array
inputs become genuine typed PDF objects; getter results are independent values.
DA accepts ASCII content syntax and is decoded as an opaque PDF byte string.
Text strings reuse CGPDFStringCopyTextString; non-ASCII new text uses UTF-16BE.
Objects unsupported by this bounded converter, malformed page trees and encrypted
PDFs fail initialization/assignment or serialization. Structural annotation fields
are initialized by initWithBounds:forType:withProperties: and changed through the
documented bounds/type/page properties. The generic setter rejects Type/Subtype/
Rect/P rather than silently corrupting them.
Dictionary-valued assignment/retrieval, NSColor conveniences, dates, PDFView,
appearance streams, text rendering, form inheritance and page insertion remain
unimplemented. This does not qualify Skim's document/page/view workflow.

Saving appends an incremental update to the original bytes. Page-tree Kids
references establish original page object numbers/generations. Existing annotation
object identities are preserved; new annotations receive fresh indirect objects.
Original content/resources and untouched references stay in the original revision.
The encoder adapter reuses O2PDFObject serialization with explicit PDF name and
hex-string escaping, pointer-width integer formatting, precision-preserving plain decimal tokens,
and null syntax. The matching Cocotron parser/name encoder correction is required
for exact fractional/large-real values, canonical escaped names and empty hex
strings; its standalone numbers/names/hex fixture has root-recorded before-fail/
after-pass execution. Models are capped at 32 input array levels, 64 serialized
object levels, and the existing parser's unsigned 32-bit file/string length. It clones the prior trailer, updates Size/Prev, physically removes obsolete
cross-reference stream keys, preserves the permanent file ID and
changes the second ID using an MD5 digest of the updated document bytes. It writes
classic xref subsections and a Prev trailer; it does not flatten or rasterize pages.
No renderer or appearance regeneration is implied by saving dictionary values.

## Clean-room provenance

- Rung 2: this tree's O2PDFDocument/O2PDFDictionary/O2PDFPage/O2PDFObject_R,
  O2PDFContext encoding protocol and CGPDFStringCopyTextString. Existing source
  implements parsing, typed storage, reference serialization and text decoding.
- Rung 3: Apple's [initializer](https://developer.apple.com/documentation/pdfkit/pdfannotation/init(bounds:fortype:withproperties:)),
  [getter](https://developer.apple.com/documentation/pdfkit/pdfannotation/value(forannotationkey:)),
  [setter](https://developer.apple.com/documentation/pdfkit/pdfannotation/setvalue(_:forannotationkey:)),
  [defaultAppearance](https://developer.apple.com/documentation/pdfkit/pdfannotationkey/defaultappearance),
  [annotations](https://developer.apple.com/documentation/pdfkit/pdfpage/annotations),
  [addAnnotation](https://developer.apple.com/documentation/pdfkit/pdfpage/addannotation(_:)),
  [PDFDocument init](https://developer.apple.com/documentation/pdfkit/pdfdocument/init(data:))
  and [dataRepresentation](https://developer.apple.com/documentation/pdfkit/pdfdocument/datarepresentation())
  specify the public API and dictionary semantics. Other declarations follow the
  corresponding public pageCount/pageAtIndex, bounds/type/page, removeAnnotation,
  [initWithURL](https://developer.apple.com/documentation/pdfkit/pdfdocument/init(url:)-98jte)
  and writeToFile/writeToURL documentation.
- Apple's [iconName key](https://developer.apple.com/documentation/pdfkit/pdfannotationkeyiconname?language=objc)
  describes an icon name for text and stamp annotations. ISO 32000-1:2008 Tables
  172 and 181 specify the corresponding /Name value as a PDF name.
- Rung 6/GUESS: `PDFAnnotationKeyIconName = @"Name"` is a project mapping;
  the public documentation does not publish the native NSString raw spelling.
  For Text and Stamp /Name values the model encodes NSString as UTF-8 bytes, a
  project policy: ISO defines name objects as byte sequences and does not specify
  native PDFKit NSString encoding or imply parity. Other subtypes retain the
  generic converter.
- Apple's [inklist key](https://developer.apple.com/documentation/pdfkit/pdfannotationkey/inklist?language=objc)
  describes arrays of stroked paths. ISO 32000-1:2008 Table 182 specifies the
  required /InkList array of arrays of alternating coordinates for Ink annotations.
- Rung 6/GUESS: `PDFAnnotationKeyInklist = @"InkList"` is a project mapping; the
  public documentation does not publish the native NSString raw spelling.
- PDF format: Adobe's published [ISO 32000-1:2008](https://opensource.adobe.com/dc-acrobat-sdk-docs/pdfstandards/PDF32000_2008.pdf),
  Tables 30/164/174, specifies Annots indirect references, annotation fields and DA
  string semantics; sections 7.3 and 7.5.6 specify object syntax/incremental updates.
- Rung 6/GUESS: `PDFAnnotationKeyDefaultAppearance = @"DA"` is an explicit project
  mapping from the documented semantic to PDF's DA key. Public Apple documentation
  does not publish the native NSString raw spelling; native raw equality is not
  claimed. DA and /DA normalize to the same dictionary entry. No private enum or
  selector, proprietary binary, SDK scrape or replacement implementation supplied
  this value or model.

A direct source-control executable may link the original frozen fixture object
with the three owned model objects and genuine AppKit/Foundation/CoreGraphics/
Onyx2D dependencies. It must not link the old forwarding PDFKit simultaneously.
This is a separate gate from building/normal-linking the installed PDFKit and
running these targets through the actual framework. Keep both evidence sets.

The original source-control model proved basic dictionary/ownership roundtrips,
then failed unchanged public bounds and large-decimal assertions in actual root
execution. Those failures motivated the independently reviewed Cocotron scanner
correction; assertions and ordinary finite real support were retained. The pure
C-entry scanner gate does not establish installed PDFKit qualification. Root Stage35 ran all seven final source-control model executables: explicit PASS,
exit zero, no timeout and exact prefix shutdown zero. Those executables link the
owned model objects plus the exact production dictionary-removal method category,
using the genuine installed corrected Onyx parser. The actual two-reload output
also passes the unchanged independent pypdf 6.10.0 strict reader, checking page,
annotation, root identities and preserved contents/resources; its non-zero-xref-
subsection advisory is retained. The output SHA256 is
`422a608f87e058ceeaa689bb57646ab837409cb9625eebbc9f7472eed209be73`.

Normal installed dictionary-method/PDFKit linkage remains separate: build these
CMake targets against the actual rebuilt frameworks, without model objects or a
removal category, and repeat runtime/strict-reader gates. Keep exact source,
object, executable and runtime-manifest hashes for each gate. Full Skim remains
pending until real document/page/view/annotation workflows work.

The icon fixture also requires Cocotron empty-name scanner support per ISO 32000-1
section 7.3.5; it accepts empty names in values and keys, including at EOF.

Prerequisite component commits are Cocotron e55af08d (token values and canonical
name encoding) and c9bbe4ed (physical dictionary removal). The latter fixes an
actual independent-reader failure caused by adding XRefStm null; obsolete entries
are now absent in the serialized trailer. The private model source-control
category compiles the exact owned production method and adds no duplicate class.
Normal targets use the real Onyx2D method without that category.
