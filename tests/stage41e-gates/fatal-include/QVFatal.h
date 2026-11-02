/* Static dyld runs before dynamic TLS/clock services are available. Its abort
 * behavior stays unchanged; only the installed dynamic libraries emit records. */
#if defined(VARIANT_DYLD)
#define QV_FATAL(kind, message, sourceMessage) ((void)0)
#define qvFatalRecord(kind, caller, site, message, sourceMessage) ((void)0)
#else
#include_next <QVFatal.h>
#endif
