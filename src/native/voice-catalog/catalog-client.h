/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef QV_CATALOG_CLIENT_H
#define QV_CATALOG_CLIENT_H
#include "catalog.h"

struct qv_language_record { const char *raw,*canonical; uint32_t priority; };
struct qv_voice_record {
    const char *provider_identifier,*identifier,*name;
    uint32_t provider_gender,language_count,primary;
    struct qv_language_record *languages;
};
struct qv_catalog_snapshot {
    enum qv_catalog_status provider_status;
    uint32_t count;
    struct qv_voice_record *voices;
    char *storage;
};
enum qv_catalog_status qv_catalog_decode(const uint8_t *, size_t, struct qv_catalog_snapshot **);
void qv_catalog_snapshot_free(struct qv_catalog_snapshot *);

enum qv_transport_status {
    QV_TRANSPORT_OK,
    QV_TRANSPORT_BAD_ARGUMENT,
    QV_TRANSPORT_SPAWN_ERROR,
    QV_TRANSPORT_IO_ERROR,
    QV_TRANSPORT_TIMEOUT,
    QV_TRANSPORT_TOO_LARGE,
    QV_TRANSPORT_CHILD_SIGNAL
};
enum qv_transport_status qv_catalog_run(const char *, unsigned int, uint8_t *, size_t, size_t *, int *);
#endif
