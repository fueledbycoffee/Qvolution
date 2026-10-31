/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Darling's normalized CPU layout. This is not an Apple private ABI. */
#ifndef IOSURFACE_DARLING_LAYOUT_H
#define IOSURFACE_DARLING_LAYOUT_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limits.h>

#define DARLING_SURFACE_LAYOUT_VERSION 1
#define DARLING_SURFACE_BGRA UINT32_C(0x42475241)
#define DARLING_SURFACE_420V UINT32_C(0x34323076)

struct DarlingSurfacePlane {
	uint64_t offset, size;
	uint32_t width, height, bytesPerRow, bytesPerElement;
};
struct DarlingSurfaceLayout {
	uint32_t version, pixelFormat, width, height;
	uint32_t planeCount, global;
	uint64_t allocationSize;
	/* Nonplanar storage uses planes[0] but reports planeCount == 0. */
	struct DarlingSurfacePlane planes[2];
};

static inline bool DarlingSurfaceLayoutValid(const struct DarlingSurfaceLayout *l)
{
	if (!l || l->version != DARLING_SURFACE_LAYOUT_VERSION || !l->width || !l->height ||
		l->global > 1 || !l->allocationSize || l->allocationSize > SIZE_MAX ||
		l->allocationSize > INT64_MAX)
		return false;
	if (l->pixelFormat == DARLING_SURFACE_BGRA) {
		if (l->planeCount || l->planes[0].bytesPerElement != 4)
			return false;
	} else if (l->pixelFormat == DARLING_SURFACE_420V) {
		if (l->planeCount != 2 || (l->width & 1) || (l->height & 1) ||
			l->planes[0].bytesPerElement != 1 || l->planes[1].bytesPerElement != 2 ||
			l->planes[1].width != l->width / 2 || l->planes[1].height != l->height / 2)
			return false;
	} else return false;
	if (l->planes[0].width != l->width || l->planes[0].height != l->height)
		return false;
	for (uint32_t i = 0; i < (l->planeCount ? l->planeCount : 1); ++i) {
		const struct DarlingSurfacePlane *p = &l->planes[i];
		if (!p->width || !p->height || !p->bytesPerElement ||
			(uint64_t)p->width * p->bytesPerElement > p->bytesPerRow ||
			(uint64_t)p->height * p->bytesPerRow > p->size ||
			p->offset > l->allocationSize || p->size > l->allocationSize - p->offset)
			return false;
		/* Conservative CPU backend policy: planes must not overlap. */
		if (i && p->offset < l->planes[i-1].offset + l->planes[i-1].size)
			return false;
	}
	return true;
}
#endif
