#pragma once
#include <stddef.h>
typedef void *CFMutableDictionaryRef;
typedef const void *CFDictionaryRef;
CFMutableDictionaryRef CFDictionaryCreateMutable(void *, long, const void *, const void *);
const void *CFDictionaryGetValue(CFDictionaryRef, const void *);
void CFDictionaryAddValue(CFMutableDictionaryRef, const void *, const void *);
