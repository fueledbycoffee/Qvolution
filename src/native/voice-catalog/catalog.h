/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 Darling developers */
#ifndef QV_VOICE_CATALOG_H
#define QV_VOICE_CATALOG_H

#include <stddef.h>
#include <stdint.h>

#define QV_CATALOG_MAX_BYTES (4u * 1024u * 1024u)
#define QV_CATALOG_MAX_VOICES 4096u
#define QV_CATALOG_MAX_LANGUAGES 64u
#define QV_CATALOG_MAX_STRING 1024u
#define QV_CATALOG_IDENTIFIER_PREFIX "org.qvolution.voice.v1/espeak-ng/"
#define QV_CATALOG_MAX_IDENTIFIER ((sizeof(QV_CATALOG_IDENTIFIER_PREFIX)-1)+3*QV_CATALOG_MAX_STRING)

enum qv_catalog_status {
    QV_CATALOG_OK,
    QV_CATALOG_UNAVAILABLE,
    QV_CATALOG_PROVIDER_ERROR,
    QV_CATALOG_BAD_DATA,
    QV_CATALOG_TOO_LARGE,
    QV_CATALOG_NO_MEMORY
};

enum qv_language_status {
    QV_LANGUAGE_MAPPED,
    QV_LANGUAGE_UNSUPPORTED,
    QV_LANGUAGE_NO_SPACE
};

/* Inputs are borrowed only during encode; the successful byte snapshot owns every field. */
struct qv_language_input {
    const char *tag;
    uint32_t priority;
};

struct qv_voice_input {
    const char *identifier;
    const char *name;
    uint32_t provider_gender;
    const struct qv_language_input *languages;
    size_t language_count;
};

enum qv_language_status qv_canonical_language(const char *, char *, size_t);
enum qv_catalog_status qv_catalog_project_identifier(const char *, char *, size_t);
enum qv_catalog_status qv_catalog_encode(const struct qv_voice_input *, size_t,
                                       uint8_t *, size_t, size_t *);
enum qv_catalog_status qv_catalog_error(enum qv_catalog_status, uint8_t *, size_t, size_t *);

#endif
