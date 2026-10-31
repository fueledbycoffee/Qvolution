# Native voice catalog foundation

This standalone host helper enumerates actual eSpeakNG provider records without speech synthesis or audio-device initialization. It is not yet connected to the guest AVFAudio framework, and does not resolve Firefox's missing class by itself.

The provider runs once per helper process. Modern status-returning initialization distinguishes missing data from a successful empty catalog. No InitializeOutput, callback, synthesis or voice-selection function is used. Default eSpeakNG data discovery applies; no helper option introduces a data path or library override. A provider failure emits an error envelope and a matching nonzero exit. A complete catalog is copied before provider termination and no host pointer is serialized. Abnormal process death or output failure may produce no complete envelope; a consumer must reject it. The helper has bounded output and memory, but its local provider calls have no internal wall-clock deadline.

The public eSpeakNG 1.52.0 GPLv3-or-later headers and source specify its API and lifecycle: [speak_lib.h](https://github.com/espeak-ng/espeak-ng/blob/1.52.0/src/include/espeak-ng/speak_lib.h), [espeak_ng.h](https://github.com/espeak-ng/espeak-ng/blob/1.52.0/src/include/espeak-ng/espeak_ng.h), [modern initialization](https://github.com/espeak-ng/espeak-ng/blob/1.52.0/src/libespeak-ng/speech.c), [legacy initialization](https://github.com/espeak-ng/espeak-ng/blob/1.52.0/src/libespeak-ng/espeak_api.c), [enumeration](https://github.com/espeak-ng/espeak-ng/blob/1.52.0/src/libespeak-ng/voices.c). Legacy espeak_Initialize can return a sample rate after failed initialization with DONT_EXIT, so it cannot provide the required error distinction. ListVoices(NULL) excludes MBROLA and variant voices in the cited provider source.

## Build and verify

Build this directory with the host compiler in a separate build directory. Dependencies are ICU's `uc` component and the published eSpeakNG development headers. The provider library is optional at runtime and is loaded nonfatally through its normal host SONAME. Do not supply guessed header declarations.

```sh
cmake -S /path/to/src/native/voice-catalog -B /outside/checkout/voice-build \
  -DESPEAK_NG_INCLUDE_DIR=/path/to/public/header/root \
  -DCMAKE_C_FLAGS='-Wall -Wextra -Werror'
cmake --build /outside/checkout/voice-build -j1
ctest --test-dir /outside/checkout/voice-build --output-on-failure
```

Controlled providers are compiled only into dedicated test helpers through a compile-time path. Production has no environment variable or CLI mechanism for switching libraries. The independent provider client uses the real documented API in a separate process, without the production encoder or normalization code, and compares every actual raw identity/name/gender/language priority. A missing or truly empty actual provider returns skip 77, never a positive populated-provider result. Other controls cover initialization/list/termination errors, duplicate identities, UTF8 rejection, exact capacity, ownership after provider mutation, allocation failures and deterministic identity ordering.

`tests/public-before.m` is a frozen original dynamic AVFoundation caller for the runtime owner. It exits 20 with MISSING_CLASS on the current runtime. An argument-free success establishes only class signatures. Its nonempty-catalog path additionally requires an independently produced expected JSON projection with voices containing identifier/name/language, and currentLanguageCode. Official Apple documentation supplies those typed selectors; quality/gender numeric values are deliberately not guessed. No native helper result qualifies that public path, Firefox UI, speech, GPU or stability.

## Version 1 wire format

All integers are unsigned 32-bit little-endian, with no padding, pointers or terminating NUL bytes. A string is its byte length followed by UTF8 bytes. Envelope: magic `QVVC`, version 1, status, exact total bytes, record count, provider string `espeak-ng`. Status values are the project `qv_catalog_status` enum, never Apple or provider error numbers. Errors carry zero records.

Each record: provider gender, language count, primary-language index, provider identifier string, project identifier string, display-name string, then language entries. Each entry: provider priority, raw language string, canonical language string. Empty canonical string means explicitly unsupported; primary UINT32_MAX means no supported canonical language. A consumer must validate complete lengths/counts/UTF8/identities and reject trailing bytes or partial results. The helper does not implement the future guest decoder.

Limits: 4 MiB total, 4096 records, 64 languages per record, 1024 bytes per provider string. Project identifier is the fixed prefix `org.qvolution.voice.v1/espeak-ng/` followed by uppercase percent encoding of every provider-identifier byte except RFC3986 unreserved ASCII; its derived bound is prefix length plus three times the provider identifier bound. Record order is provider-identifier UTF8 byte order; identity never uses display name/index. Duplicate provider identities are rejected. Publication is transactional, including when the destination is one byte short.

## Language policy and remaining integration

[ICU's public locale API](https://unicode-org.github.io/icu-docs/apidoc/released/icu4c/uloc_8h.html) validates complete input and canonicalizes case/locale aliases. This initial adapter accepts a bounded subset: known ISO language, optional script, optional ISO country. Unsupported dialects, extensions, macroregions or newer language subtags are retained verbatim with an empty canonical value. Legacy eSpeak en-sc/en-wi/en-uk are explicitly unsupported because treating dialect labels as ISO regions would change their meaning. This is a deliberate coverage limit, not a claim that all unsupported labels are invalid BCP47. Primary language is the lowest provider priority, then lexical canonical tag; no hardcoded English fallback exists.

A later guest bridge needs an absolute installed executable path, bounded pipes, deadline/child reap and a validating decoder before public immutable objects can be published. Failed enumeration must not be cached as permanent successful empty. AVSpeechSynthesisVoice scalar enum mapping still requires authoritative published/vendored raw values. Voice lookup/default language must use user language preferences independently of provider ordering. NSSecureCoding, voice traits, audio-file settings and synthesis are not implemented by this milestone.
