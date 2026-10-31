#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/un.h>
#include "../../src/shellspawn/shellspawn.h"
#include "../../src/startup/profile-prefix.h"

static void expect(const char *explicitPrefix, const char *profile,
                   const char *home, int accepted)
{
    enum darling_prefix_error error;
    char *result = darlingSelectPrefix(explicitPrefix, profile, home, &error);
    if (accepted) {
        assert(result && error == DARLING_PREFIX_OK);
        char socketPath[512];
        int size = snprintf(socketPath, sizeof(socketPath), "%s%s", result, SHELLSPAWN_SOCKPATH);
        assert(size >= 0 && (size_t)size < sizeof(((struct sockaddr_un *)0)->sun_path));
    } else {
        assert(!result && error == DARLING_PREFIX_TOO_LONG);
    }
    free(result);
}

int main(void)
{
    size_t capacity = sizeof(((struct sockaddr_un *)0)->sun_path);
    size_t socketSuffix = strlen(SHELLSPAWN_SOCKPATH);
    size_t serverSuffix = strlen("/.darlingserver.sock");
    size_t longestSuffix = socketSuffix > serverSuffix ? socketSuffix : serverSuffix;
    size_t maximum = capacity - 1 - longestSuffix;
    assert(maximum > strlen("/.darling.x"));
    char prefix[512];
    memset(prefix, 'p', maximum); prefix[0] = '/'; prefix[maximum] = 0;
    expect(prefix, NULL, NULL, 1);
    prefix[maximum] = 'p'; prefix[maximum + 1] = 0;
    expect(prefix, NULL, NULL, 0);
    prefix[1] = (char)0xc3; prefix[2] = (char)0xa9;
    expect(prefix, NULL, NULL, 0);
    prefix[maximum] = 0;
    expect(prefix, NULL, NULL, 1);

    char home[512];
    size_t homeLength = maximum - strlen("/.darling");
    memset(home, 'h', homeLength); home[0] = '/'; home[homeLength] = 0;
    expect(NULL, NULL, home, 1);
    home[homeLength] = 'h'; home[homeLength + 1] = 0;
    expect(NULL, NULL, home, 0);
    homeLength = maximum - strlen("/.darling.x");
    memset(home, 'h', homeLength); home[0] = '/'; home[homeLength] = 0;
    expect(NULL, "x", home, 1);
    home[homeLength] = 'h'; home[homeLength + 1] = 0;
    expect(NULL, "x", home, 0);
    puts("PASS: explicit/default/profile prefix socket limits, 107/108-byte boundary and UTF-8 byte lengths");
    return 0;
}
