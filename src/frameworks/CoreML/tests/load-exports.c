#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

static int check(const char *path, const char *const *names) {
    void *h = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!h) { fprintf(stderr, "dlopen %s: %s\n", path, dlerror()); return 1; }
    int failed = 0;
    for (; *names; ++names) {
        if (!dlsym(h, *names)) { fprintf(stderr, "%s: missing %s\n", path, *names); failed = 1; }
    }
    /* Keep Objective-C images loaded until process exit. */
    return failed;
}
int main(int argc, char **argv) {
    if (argc != 4 || strcmp(argv[1], "--vision-coreml-subset")) {
        fprintf(stderr, "usage: load-exports --vision-coreml-subset VISION COREML\nVisionKit Swift imports remain OPEN; this is not the full lane gate.\n"); return 2;
    }
    static const char *const vision[] = {
        "VNBarcodeSymbologyAztec",
        "VNBarcodeSymbologyCode128",
        "VNBarcodeSymbologyCode39",
        "VNBarcodeSymbologyCode39Checksum",
        "VNBarcodeSymbologyCode39FullASCII",
        "VNBarcodeSymbologyCode39FullASCIIChecksum",
        "VNBarcodeSymbologyCode93",
        "VNBarcodeSymbologyCode93i",
        "VNBarcodeSymbologyDataMatrix",
        "VNBarcodeSymbologyEAN13",
        "VNBarcodeSymbologyEAN8",
        "VNBarcodeSymbologyI2of5",
        "VNBarcodeSymbologyI2of5Checksum",
        "VNBarcodeSymbologyITF14",
        "VNBarcodeSymbologyPDF417",
        "VNBarcodeSymbologyQR",
        "VNBarcodeSymbologyUPCE",
        0
    };
    static const char *const coreml[] = {
        "OBJC_CLASS_$_MLFeatureValue",
        "OBJC_CLASS_$_MLModel",
        "OBJC_CLASS_$_MLModelConfiguration",
        "OBJC_CLASS_$_MLMultiArray",
        "OBJC_CLASS_$_MLPredictionOptions",
        0
    };
    int a = check(argv[2], vision);
    int b = check(argv[3], coreml);
    if (!a && !b) puts("PASS: Vision/CoreML export subset only; 19 VisionKit Swift imports OPEN");
    return a || b;
}
