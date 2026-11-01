#!/usr/bin/env python3
"""Exercise actual attachment/rectangle routines with host CoreFoundation.
The host adapter supplies only buffer storage and geometry ABI, not API behavior.
Darwin compilation of the complete translation unit is a separate build check.
"""
import pathlib, subprocess, sys
root=pathlib.Path(__file__).resolve().parents[2]
out=pathlib.Path(sys.argv[1]).resolve();out.mkdir(parents=True,exist_ok=True)
cf=pathlib.Path(sys.argv[2]).resolve()
source=pathlib.Path(sys.argv[3]).read_text() if len(sys.argv)>3 else (root/'src/frameworks/CoreVideo/src/CVPixelBuffer.c').read_text()
def function(name):
 pos=source.index(name+'('); start=source.rfind('\n',0,pos)+1; opening=source.index('{',pos);level=1;end=opening+1
 while level:
  level += (source[end]=='{')-(source[end]=='}');end+=1
 return source[start:end]+'\n'
names=['CVBufferSetAttachment','CVBufferGetAttachment','CVBufferHasAttachment','CVBufferCopyAttachment','CVBufferRemoveAttachment','CVBufferRemoveAllAttachments','CVImageBufferGetEncodedSize','cleanApertureNumber','CVImageBufferGetCleanRect','CVIsCompressedPixelFormatAvailable']
header='''#include <CoreFoundation/CoreFoundation.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
typedef double CGFloat;
typedef struct { CGFloat width, height; } CGSize;
typedef struct { CGFloat x, y; } CGPoint;
typedef struct { CGPoint origin; CGSize size; } CGRect;
static CGRect CGRectMake(double x,double y,double w,double h) { return (CGRect){{x,y},{w,h}}; }
typedef unsigned int OSType;
typedef unsigned int CVAttachmentMode;
enum { kCVAttachmentMode_ShouldPropagate=1, kCVAttachmentMode_ShouldNotPropagate=0 };
struct __CVBuffer { size_t width, height; CFMutableDictionaryRef attachments; };
typedef struct __CVBuffer *CVBufferRef, *CVImageBufferRef;
#define kCVImageBufferCleanApertureKey CFSTR("CleanAperture")
#define kCVImageBufferCleanApertureWidthKey CFSTR("CleanApertureWidth")
#define kCVImageBufferCleanApertureHeightKey CFSTR("CleanApertureHeight")
#define kCVImageBufferCleanApertureHorizontalOffsetKey CFSTR("CleanApertureHorizontalOffset")
#define kCVImageBufferCleanApertureVerticalOffsetKey CFSTR("CleanApertureVerticalOffset")
'''
# Before the fix this intentionally fails during source extraction: the required
# exported routine does not exist. Do not supply a fake implementation.
fixture=header+'\n'.join(function(name) for name in names)+(pathlib.Path(__file__).with_name('attachments.c')).read_text()
(out/'test.c').write_text(fixture)
subprocess.run(['clang','-fblocks','-g','-O1','-fsanitize=address,undefined','-I'+str(cf),str(out/'test.c'),'-L'+str(cf/'linux'),'-Wl,-rpath,'+str(cf/'linux'),'-lFoundation','-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],check=True)
