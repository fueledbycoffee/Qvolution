void CoreDisplayDummy(void) {}

#include <CoreFoundation/CoreFoundation.h>
#include <stdint.h>

/* GUESS: rung 5, IINA import supplies only the private name. No schema is
 * available in the vendored headers, so report unavailable information. */
CFDictionaryRef CoreDisplay_DisplayCreateInfoDictionary(uint32_t display)
{
    return NULL;
}
