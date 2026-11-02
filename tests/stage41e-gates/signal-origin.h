/* Names-only signal boundary record. Raw Linux syscalls: no dyld/libc/TLS locks.
 * The PC is compared only with kernel maps metadata, never dereferenced/stored.
 * The generated allowlist contains exact paths from the reviewed OUR payload. */
#ifndef QV_SIGNAL_ORIGIN_H
#define QV_SIGNAL_ORIGIN_H
#include <stddef.h>
#include <stdint.h>
#include "signal-images.h"

static size_t qvsLength(const char *s) { size_t n = 0; while (s[n]) ++n; return n; }
static int qvsEqual(const char *a, const char *b) {
    while (*a && *a == *b) { ++a; ++b; } return *a == *b;
}
static int qvsPrefix(const char *a, const char *b) {
    while (*b && *a == *b) { ++a; ++b; } return !*b;
}
static const char *qvsHex(const char *p, uintptr_t *value) {
    const char *start = p; *value = 0;
    for (;;) {
        unsigned n;
        if (*p >= '0' && *p <= '9') n = *p - '0';
        else if (*p >= 'a' && *p <= 'f') n = *p - 'a' + 10;
        else break;
        if (*value > (UINTPTR_MAX - n) / 16) return NULL;
        *value = *value * 16 + n; ++p;
    }
    return p == start ? NULL : p;
}
static const char *qvsMapImage(char *line, uintptr_t pc) {
    uintptr_t start, end;
    const char *p = qvsHex(line, &start);
    if (!p || *p++ != '-' || !(p = qvsHex(p, &end)) || pc < start || pc >= end) return NULL;
    /* Skip perms, offset, device, inode. No mapped bytes are read. */
    for (unsigned field = 0; field < 4; ++field) {
        while (*p == ' ') ++p;
        while (*p && *p != ' ') ++p;
    }
    while (*p == ' ') ++p;
    for (unsigned i = 0; i < sizeof(qvsImages) / sizeof(qvsImages[0]); ++i)
        if (qvsEqual(p, qvsImages[i][0])) return qvsImages[i][1];
    return "app";
}
static int qvsEnvironment(char *logpath, size_t capacity) {
    char buffer[1024]; size_t used = 0, total = 0;
    int enabled = 0, have_prefix = 0;
    long fd = QVS_OPEN("/proc/self/environ", QVS_RDONLY);
    if (fd < 0) return 0;
    for (;;) {
        char c;
        if (++total > 65536 || QVS_READ(fd, &c, 1) != 1) break;
        if (c) { if (used < sizeof(buffer) - 1) buffer[used++] = c; else used = sizeof(buffer); continue; }
        if (used < sizeof(buffer)) {
            buffer[used] = 0;
            if (qvsEqual(buffer, "QV_PREVIEW_FATAL=1")) enabled = 1;
            const char *key = "__mldr_sockpath=", *suffix = "/.darlingserver.sock";
            if (qvsPrefix(buffer, key)) {
                const char *path = buffer + qvsLength(key);
                size_t n = qvsLength(path), tail = qvsLength(suffix);
                const char *log = "/private/var/log/qv-terminal-boundaries.log";
                if (*path == '/' && n > tail && qvsEqual(path + n - tail, suffix) &&
                    n - tail + qvsLength(log) < capacity) {
                    size_t j = 0; for (; j < n - tail; ++j) logpath[j] = path[j];
                    for (size_t k = 0; ; ++k) { logpath[j++] = log[k]; if (!log[k]) break; }
                    have_prefix = 1;
                }
            }
        }
        used = 0;
    }
    QVS_CLOSE(fd);
    return enabled && have_prefix;
}
static void qvsSignalOrigin(uintptr_t pc) {
    char logpath[256], line[8192];
    if (!qvsEnvironment(logpath, sizeof(logpath))) return;
    const char *image = "app";
    long fd = QVS_OPEN("/proc/self/maps", QVS_RDONLY);
    if (fd >= 0) {
        char buffer[4096]; size_t used = 0, total = 0; int found = 0;
        long n;
        while (!found && total < 1024 * 1024 && (n = QVS_READ(fd, buffer, sizeof(buffer))) > 0) {
            total += n;
            for (long i = 0; i < n && !found; ++i) {
                if (buffer[i] == '\n') {
                    if (used < sizeof(line)) {
                        line[used] = 0;
                        const char *name = qvsMapImage(line, pc);
                        if (name) { image = name; found = 1; }
                    }
                    used = 0;
                } else if (used < sizeof(line)) line[used++] = buffer[i];
            }
        }
        QVS_CLOSE(fd);
    }
    /* Timestamp 0 denotes unavailable: no clock/TLS services in static dyld. */
    const char *prefix = "QV2 fatal.origin 0 7369676e616c2e53494753454756 ";
    const char *hex = "0123456789abcdef"; size_t used = 0;
    for (const char *p = prefix; *p; ++p) line[used++] = *p;
    for (const unsigned char *p = (const unsigned char *)image; *p; ++p) {
        line[used++] = hex[*p >> 4]; line[used++] = hex[*p & 15];
    }
    line[used++] = '\n';
    fd = QVS_OPEN(logpath, QVS_APPEND);
    if (fd >= 0) { (void)QVS_WRITE(fd, line, used); QVS_CLOSE(fd); }
}
#endif
