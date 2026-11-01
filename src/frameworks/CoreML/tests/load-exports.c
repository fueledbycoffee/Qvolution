#include <dlfcn.h>
#include <stdio.h>

static int check(const char *path, const char *const *names) {
    void *h = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!h) { fprintf(stderr, "dlopen %s: %s\n", path, dlerror()); return 1; }
    int failed = 0;
    for (; *names; ++names) {
        if (!dlsym(h, *names)) { fprintf(stderr, "%s: missing %s\n", path, *names); failed = 1; }
    }
    dlclose(h);
    return failed;
}
int main(int argc, char **argv) {
    if (argc != 3) { fprintf(stderr, "usage: load-exports VISION COREML\n"); return 2; }
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
    int a = check(argv[1], vision);
    int b = check(argv[2], coreml);
    return a || b;
}
