# Vision/CoreML integration subset

The export probe requires `--vision-coreml-subset`. It covers the 17 Vision
constants and five CoreML class objects (plus their five metaclasses).
CoreML behavior is checked separately by `imp-vision-coreml-behavior`.
Neither probe closes the original all-non-weak-import gate.

OPEN: all 19 non-weak VisionKit Swift imports used by CotEditor, enumerated
in `visionkit-open.txt` from imports-sweep-b/MISSING.md. Integrate this subset
only as partial coverage; do not mark imp-vision done or claim CotEditor support.
The local Swift 6.3.3 toolchain targets Linux. A supported Darwin Swift/ObjC
toolchain, AppKit Swift module, matching runtime metadata and async throwing ABI
are prerequisites for VisionKit's ImageAnalyzer/ImageAnalysisOverlayView APIs.
No C catch-all or fabricated metadata substitutes for that work.

Rung 3 type provenance: local cached Apple public
`documentation__vision__vnbarcodesymbology.json` defines barcode symbologies.
Raw constant bytes remain GUESS; that page does not specify their string bytes.
CoreML selectors/types come from the local public MLFeatureValue, MLMultiArray,
MLModelConfiguration, MLPredictionOptions and MLModel pages (including their
Objective-C variants). Unsupported constructors deliberately return nil;
inference is unavailable. Zero/nil option defaults and the Cocoa error choice
remain GUESS. No model execution or GPU support is claimed.
