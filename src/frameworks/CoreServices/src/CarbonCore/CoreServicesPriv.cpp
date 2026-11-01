#include <CoreServices/CoreServicesPriv.h>

// TODO: Security.framework expects this symbol to be available in
// /System/Library/Frameworks/CoreServices.framework/Frameworks/CarbonCore.framework/Versions/A/CarbonCore

Boolean _CSCheckFix(CFStringRef str)
{
	// Checks whether given issue number is fixed in this release or something
	return 0;
}

extern "C" int _CSGetProcessorArchitecture(void)
{
#if defined(__x86_64__)
	return 0x01000007; // CPU_TYPE_X86_64
#elif defined(__i386__)
	return 7; // CPU_TYPE_I386
#elif defined(__arm64__) || defined(__aarch64__)
	return 0x0100000c; // CPU_TYPE_ARM64
#else
	return 7;
#endif
}


#include <unistd.h>

extern "C" CFStringRef CSCopyMachineName(void)
{
    char name[256];
    if (gethostname(name, sizeof(name)) != 0) return NULL;
    name[sizeof(name) - 1] = 0;
    return CFStringCreateWithCString(NULL, name, kCFStringEncodingUTF8);
}

/* GUESS: private disable operation, name only from Spotify's imports.
 * _CSCheckFix already reports false for every fix in this backend. */
extern "C" int _CSCheckFixDisable(void)
{
    return 0;
}
