/**
 * @file cgfx_loader.h
 * @brief Load geometry from the LearnWebGPU bespoke text format.
 *
 * Temporary module — remove when no longer following the tutorial.
 *
 * Usage:
 *   CgfxMesh mesh = cgfx_load_tutorial_mesh(&ctx, "pyramid.txt");
 */
#ifndef CGFX_LOADER_H
#define CGFX_LOADER_H

#include <stdbool.h>
#include <stdint.h>
#include "cgfx_mesh.h"
#include "cgfx_export.h"

typedef struct CgfxGeometry {
    float    *point_data;
    uint32_t  point_count;
    uint32_t  floats_per_point;
    uint16_t *index_data;
    uint32_t  index_count;
} CgfxGeometry;

CGFX_API bool cgfx_load_geometry(const char *path, CgfxGeometry *out);
CGFX_API void cgfx_free_geometry(CgfxGeometry *geo);

CGFX_API CgfxMesh cgfx_load_tutorial_mesh(const CgfxCtx *ctx, const char *path);

#endif /* CGFX_LOADER_H */
