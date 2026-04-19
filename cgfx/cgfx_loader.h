/**
 * @file cgfx_loader.h
 * @brief Load geometry from the LearnWebGPU bespoke text format.
 *
 * Temporary module — remove when no longer following the tutorial.
 *
 * Usage with CgfxMesh:
 *
 *   CgfxGeometry geo;
 *   cgfx_load_geometry("model.txt", &geo);
 *
 *   uint32_t vertex_count = geo.point_count / 5;
 *   CgfxVertex *vertices = malloc(vertex_count * sizeof(CgfxVertex));
 *   for (uint32_t i = 0; i < vertex_count; i++) {
 *       float *p = &geo.point_data[i * 5];
 *       vertices[i] = (CgfxVertex){
 *           .position = {p[0], p[1], 0.0f},
 *           .normal   = {0.0f, 0.0f, 1.0f},
 *           .color    = {p[2], p[3], p[4]},
 *           .uv       = {0.0f, 0.0f},
 *       };
 *   }
 *
 *   uint32_t *indices = malloc(geo.index_count * sizeof(uint32_t));
 *   for (uint32_t i = 0; i < geo.index_count; i++)
 *       indices[i] = geo.index_data[i];
 *
 *   CgfxMesh mesh = cgfx_mesh_create(&ctx, vertices, vertex_count,
 *                                     indices, geo.index_count);
 *   free(vertices);
 *   free(indices);
 *   cgfx_free_geometry(&geo);
 */
#ifndef CGFX_LOADER_H
#define CGFX_LOADER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct CgfxGeometry {
    float    *point_data;
    uint32_t  point_count;
    uint16_t *index_data;
    uint32_t  index_count;
} CgfxGeometry;

bool cgfx_load_geometry(const char *path, CgfxGeometry *out);
void cgfx_free_geometry(CgfxGeometry *geo);

#endif /* CGFX_LOADER_H */
