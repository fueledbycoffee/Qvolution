/*
This file is part of Darling.

Copyright (C) 2020 Lubos Dolezel

Darling is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Darling is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Darling.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <LaunchServices/LaunchServices.h>
#include <CarbonCore/MacErrors.h>
#include <sqlite3.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

static Boolean asciiLetter(UniChar c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static Boolean bundleCharacter(UniChar c) {
    return asciiLetter(c) || (c >= '0' && c <= '9') || c == '-' || c == '.';
}

// Public URI/bundle character sets; reverse-DNS bundle naming is recommended,
// not an extra required label-count policy. Scheme comparison is ASCII-insensitive.
static OSStatus copyValidatedASCII(CFStringRef value, Boolean scheme, char **result) {
    *result = NULL;
    if (!value || CFGetTypeID(value) != CFStringGetTypeID()) return paramErr;
    CFIndex length = CFStringGetLength(value);
    if (length <= 0 || length > INT_MAX) return paramErr;
    for (CFIndex i = 0; i < length; ++i) {
        UniChar c = CFStringGetCharacterAtIndex(value, i);
        if (scheme ? ((i == 0 && !asciiLetter(c)) || (!bundleCharacter(c) && c != '+'))
                   : !bundleCharacter(c)) return paramErr;
    }
    char *bytes = malloc((size_t)length + 1);
    if (!bytes) return memFullErr;
    for (CFIndex i = 0; i < length; ++i) {
        UniChar c = CFStringGetCharacterAtIndex(value, i);
        bytes[i] = scheme && c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
    }
    bytes[length] = 0;
    *result = bytes;
    return noErr;
}

static Boolean preferencePaths(char *directory, char *path, size_t capacity) {
    int length = snprintf(directory, capacity, "/private/var/db/qvolution-url-handlers-%lu", (unsigned long)getuid());
    if (length <= 0 || (size_t)length >= capacity) return false;
    length = snprintf(path, capacity, "%s/preferences.db", directory);
    return length > 0 && (size_t)length < capacity;
}

static Boolean privatePath(const char *path, Boolean directory) {
    struct stat info;
    return lstat(path, &info) == 0 && info.st_uid == getuid() &&
        (directory ? S_ISDIR(info.st_mode) : S_ISREG(info.st_mode)) &&
        (info.st_mode & 0777) == (directory ? 0700 : 0600);
}

static Boolean privateSidecars(const char *path) {
    static const char *suffixes[] = {"-journal", "-wal", "-shm"};
    for (unsigned i = 0; i < sizeof(suffixes) / sizeof(*suffixes); ++i) {
        char sidecar[PATH_MAX];
        int length = snprintf(sidecar, sizeof(sidecar), "%s%s", path, suffixes[i]);
        if (length <= 0 || (size_t)length >= sizeof(sidecar)) return false;
        struct stat info;
        if (lstat(sidecar, &info) != 0) {
            if (errno == ENOENT) continue;
            return false;
        }
        if (!S_ISREG(info.st_mode) || info.st_uid != getuid() || (info.st_mode & 0777) != 0600)
            return false;
    }
    return true;
}

static Boolean preparePreferenceStore(const char *directory, const char *path) {
    if (mkdir(directory, 0700) != 0 && errno != EEXIST) return false;
    if (!privatePath(directory, true)) return false;
    struct stat existing;
    if (lstat(path, &existing) == 0)
        return privatePath(path, false) && privateSidecars(path);
    if (errno != ENOENT) return false;

    // Closing ANY raw fd to a published SQLite inode drops this process's POSIX
    // locks. Close our unpublished inode before atomically linking its final name.
    char temporary[PATH_MAX];
    int length = snprintf(temporary, sizeof(temporary), "%s/.create-XXXXXX", directory);
    if (length <= 0 || (size_t)length >= sizeof(temporary)) return false;
    int fd = mkstemp(temporary);
    if (fd < 0) return false;
    struct stat info;
    Boolean valid = fcntl(fd, F_SETFD, FD_CLOEXEC) == 0 &&
        fstat(fd, &info) == 0 && S_ISREG(info.st_mode) &&
        info.st_uid == getuid() && (info.st_mode & 0777) == 0600;
    if (close(fd) != 0) valid = false;
    int published = valid ? link(temporary, path) : -1;
    int publishError = errno;
    if (unlink(temporary) != 0) return false;
    if (published != 0 && (!valid || publishError != EEXIST)) return false;
    return privatePath(path, false) && privateSidecars(path);
}

static CFStringRef copyBundleColumn(sqlite3_stmt *statement) {
    if (sqlite3_column_type(statement, 0) != SQLITE_TEXT) return NULL;
    const unsigned char *text = sqlite3_column_text(statement, 0);
    int length = sqlite3_column_bytes(statement, 0);
    if (!text || length <= 0) return NULL;
    for (int i = 0; i < length; ++i)
        if (!bundleCharacter(text[i])) return NULL;
    return CFStringCreateWithBytes(kCFAllocatorDefault, text, length, kCFStringEncodingASCII, false);
}

static CFStringRef copyRegisteredHandler(const char *scheme) {
    sqlite3 *db = NULL;
    sqlite3_stmt *statement = NULL;
    CFStringRef result = NULL;
    if (sqlite3_open_v2("/private/var/db/launchservices.db", &db,
            SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX | SQLITE_OPEN_NOFOLLOW, NULL) == SQLITE_OK &&
        sqlite3_prepare_v2(db,
            "SELECT DISTINCT B.bundle_id FROM bundle B "
            "JOIN bundle_url_type T ON T.bundle=B.id "
            "JOIN bundle_url_type_scheme S ON S.type=T.id "
            "WHERE B.package_type='APPL' AND S.scheme=?1 COLLATE NOCASE "
            "ORDER BY B.bundle_id COLLATE NOCASE, B.bundle_id LIMIT 1", -1, &statement, NULL) == SQLITE_OK &&
        sqlite3_bind_text(statement, 1, scheme, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
        sqlite3_step(statement) == SQLITE_ROW)
        result = copyBundleColumn(statement);
    if (statement) sqlite3_finalize(statement);
    if (db) sqlite3_close(db);
    return result;
}

OSStatus LSSetDefaultHandlerForURLScheme(CFStringRef inURLScheme, CFStringRef inHandlerBundleID) {
    char *scheme = NULL, *handler = NULL;
    OSStatus status = copyValidatedASCII(inURLScheme, true, &scheme);
    if (status == noErr) status = copyValidatedASCII(inHandlerBundleID, false, &handler);
    if (status != noErr) { free(scheme); return status; }
    char directory[PATH_MAX], path[PATH_MAX];
    sqlite3 *db = NULL;
    sqlite3_stmt *statement = NULL;
    status = kLSDataUnavailableErr;
    if (!preferencePaths(directory, path, sizeof(path)) || !preparePreferenceStore(directory, path)) goto out;
    if (sqlite3_open_v2(path, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX | SQLITE_OPEN_NOFOLLOW, NULL) != SQLITE_OK ||
        sqlite3_busy_timeout(db, 5000) != SQLITE_OK ||
        sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS url_handler (scheme TEXT PRIMARY KEY COLLATE NOCASE NOT NULL, bundle_id TEXT NOT NULL)", NULL, NULL, NULL) != SQLITE_OK ||
        sqlite3_prepare_v2(db, "INSERT OR REPLACE INTO url_handler (scheme,bundle_id) VALUES (?1,?2)", -1, &statement, NULL) != SQLITE_OK ||
        sqlite3_bind_text(statement, 1, scheme, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(statement, 2, handler, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_step(statement) != SQLITE_DONE) goto out;
    status = noErr;
out:
    if (statement) sqlite3_finalize(statement);
    if (db && sqlite3_close(db) != SQLITE_OK) status = kLSDataUnavailableErr;
    free(scheme); free(handler);
    return status;
}

CFStringRef LSCopyDefaultHandlerForURLScheme(CFStringRef inURLScheme) {
    char *scheme = NULL;
    if (copyValidatedASCII(inURLScheme, true, &scheme) != noErr) return NULL;
    char directory[PATH_MAX], path[PATH_MAX];
    sqlite3 *db = NULL;
    sqlite3_stmt *statement = NULL;
    CFStringRef result = NULL;
    Boolean fallback = false;
    if (!preferencePaths(directory, path, sizeof(path))) goto out;
    struct stat info;
    if (lstat(directory, &info) != 0) {
        fallback = errno == ENOENT;
        goto out;
    }
    if (!privatePath(directory, true)) goto out;
    if (lstat(path, &info) != 0) {
        fallback = errno == ENOENT;
        goto out;
    }
    if (!privatePath(path, false) || !privateSidecars(path)) goto out;
    if (sqlite3_open_v2(path, &db, SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX | SQLITE_OPEN_NOFOLLOW, NULL) != SQLITE_OK ||
        sqlite3_busy_timeout(db, 5000) != SQLITE_OK ||
        sqlite3_prepare_v2(db, "SELECT bundle_id FROM url_handler WHERE scheme=?1", -1, &statement, NULL) != SQLITE_OK ||
        sqlite3_bind_text(statement, 1, scheme, -1, SQLITE_TRANSIENT) != SQLITE_OK) goto out;
    int step = sqlite3_step(statement);
    if (step == SQLITE_ROW) result = copyBundleColumn(statement);
    else if (step == SQLITE_DONE) fallback = true;
out:
    if (statement) sqlite3_finalize(statement);
    if (db) sqlite3_close(db);
    if (fallback) result = copyRegisteredHandler(scheme);
    else if (!result) fprintf(stderr, "LaunchServices: URL handler preference store unavailable\n");
    free(scheme);
    return result;
}
