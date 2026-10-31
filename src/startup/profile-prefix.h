/* Prefix selection helpers shared by the launcher and its host-only tests. */
#ifndef DARLING_PROFILE_PREFIX_H
#define DARLING_PROFILE_PREFIX_H

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/un.h>
#include "../shellspawn/shellspawn.h"

static inline size_t darlingMaximumPrefixLength(void)
{
	const size_t capacity = sizeof(((struct sockaddr_un*)0)->sun_path);
	const size_t serverSuffix = sizeof("/.darlingserver.sock") - 1;
	const size_t shellSuffix = sizeof(SHELLSPAWN_SOCKPATH) - 1;
	const size_t longestSuffix = serverSuffix > shellSuffix ? serverSuffix : shellSuffix;
	return capacity - 1 - longestSuffix;
}

enum darling_prefix_error {
	DARLING_PREFIX_OK = 0,
	DARLING_PREFIX_CONFLICT,
	DARLING_PREFIX_EMPTY,
	DARLING_PREFIX_INVALID_PROFILE,
	DARLING_PREFIX_NO_HOME,
	DARLING_PREFIX_TOO_LONG,
	DARLING_PREFIX_NO_MEMORY,
};

static inline const char* darlingPrefixError(enum darling_prefix_error error)
{
	switch (error) {
		case DARLING_PREFIX_CONFLICT:
			return "DPREFIX and DARLING_PROFILE cannot be used together";
		case DARLING_PREFIX_EMPTY:
			return "DPREFIX cannot be empty";
		case DARLING_PREFIX_INVALID_PROFILE:
			return "DARLING_PROFILE must contain 1-64 ASCII letters, digits, dots, underscores, or hyphens, and cannot be '.' or '..'";
		case DARLING_PREFIX_NO_HOME:
			return "Cannot detect your home directory";
		case DARLING_PREFIX_TOO_LONG:
			return "Prefix path too long for Unix-domain sockets";
		case DARLING_PREFIX_NO_MEMORY:
			return "Cannot allocate prefix path";
		case DARLING_PREFIX_OK:
			return "";
	}
	return "Cannot select prefix";
}

static inline int darlingValidProfileName(const char* profile)
{
	if (!profile || !profile[0])
		return 0;
	size_t length = strlen(profile);
	if (length > 64 || !strcmp(profile, ".") || !strcmp(profile, ".."))
		return 0;
	for (size_t i = 0; i < length; ++i) {
		unsigned char c = (unsigned char)profile[i];
		if (c > 0x7f || (!isalnum(c) && c != '.' && c != '_' && c != '-'))
			return 0;
	}
	return 1;
}

static inline char* darlingSelectPrefix(const char* explicitPrefix, const char* profile,
	const char* home, enum darling_prefix_error* error)
{
	*error = DARLING_PREFIX_OK;
	if (explicitPrefix && profile) {
		*error = DARLING_PREFIX_CONFLICT;
		return NULL;
	}
	if (explicitPrefix) {
		if (!explicitPrefix[0]) {
			*error = DARLING_PREFIX_EMPTY;
			return NULL;
		}
		if (strlen(explicitPrefix) > darlingMaximumPrefixLength()) {
			*error = DARLING_PREFIX_TOO_LONG;
			return NULL;
		}
		char* result = strdup(explicitPrefix);
		if (!result)
			*error = DARLING_PREFIX_NO_MEMORY;
		return result;
	}
	if (profile && !darlingValidProfileName(profile)) {
		*error = DARLING_PREFIX_INVALID_PROFILE;
		return NULL;
	}
	if (!home || !home[0]) {
		*error = DARLING_PREFIX_NO_HOME;
		return NULL;
	}
	const char* suffix = profile ? "/.darling." : "/.darling";
	size_t size = strlen(home) + strlen(suffix) + (profile ? strlen(profile) : 0) + 1;
	if (size - 1 > darlingMaximumPrefixLength()) {
		*error = DARLING_PREFIX_TOO_LONG;
		return NULL;
	}
	char* result = malloc(size);
	if (!result) {
		*error = DARLING_PREFIX_NO_MEMORY;
		return NULL;
	}
	snprintf(result, size, "%s%s%s", home, suffix, profile ? profile : "");
	return result;
}

#endif
