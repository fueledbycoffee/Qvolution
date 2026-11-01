/* Clean-room compatibility for the Stage41b Carbon import audits.
 * Rung 2: MacErrors.h. Rung 5 supplies entry point names only.
 * GUESS: signatures unavailable in the vendored headers; services are
 * explicitly unsupported, and no caller data is read or persisted.
 */
#include <Carbon/Carbon.h>

OSStatus AEProcessEvent(const AppleEvent *appleEvent)
{
    return appleEvent ? errAEEventNotHandled : paramErr;
}

UInt8 LMGetKbdLast(void)
{
    // GUESS: no classic low-memory keyboard state; report no last key.
    return 0;
}

OSStatus CopySymbolicHotKeys(CFArrayRef *hotKeys)
{
    if (!hotKeys)
        return paramErr;
    *hotKeys = NULL;
    return unimpErr;
}

UInt32 GetCurrentEventButtonState(void)
{
    // GUESS: Carbon has no event queue; match the current NSEvent fallback.
    return 0;
}

OSStatus TSMSetDocumentProperty(TSMDocumentID document, OSType tag,
    UInt32 size, const void *data)
{
    // There is no TSM document registry; never claim to have stored data.
    return tsmInvalidDocIDErr;
}

OSStatus TSMRemoveDocumentProperty(TSMDocumentID document, OSType tag)
{
    return tsmInvalidDocIDErr;
}
