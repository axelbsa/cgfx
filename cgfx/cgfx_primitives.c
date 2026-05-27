/**
 * @file cgfx_primitives.c
 * @brief Implementation of primitive geometry generators.
 *
 * TODO: Fill in the geometry generation for each primitive.
 * Each function should:
 *   1. Allocate temporary CPU arrays for vertices and indices
 *   2. Generate the geometry (positions, normals, UVs, indices)
 *   3. Call cgfx_mesh_create() to upload to GPU
 *   4. Free the temporary CPU arrays
 *   5. Return the mesh
 *
 * All primitives use the CgfxVertex format and uint32_t indices.
 */
#include "cgfx_primitives.h"

#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


CgfxMesh cgfx_primitives_plane(const CgfxCtx *ctx,
                                float width, float depth,
                                uint32_t subdivisions) {
    CgfxMesh mesh = {};

    /*
     * TODO: Generate a subdivided XZ plane.
     *
     * Grid dimensions:
     *   uint32_t cols = subdivisions;  // cells along X
     *   uint32_t rows = subdivisions;  // cells along Z
     *   uint32_t vertex_count = (cols + 1) * (rows + 1);
     *   uint32_t index_count  = 6 * cols * rows;
     *
     * Allocate:
     *   CgfxVertex *vertices = malloc(vertex_count * sizeof(CgfxVertex));
     *   uint32_t   *indices  = malloc(index_count * sizeof(uint32_t));
     *
     * Generate vertices:
     *   for (uint32_t i = 0; i <= rows; i++) {
     *       for (uint32_t j = 0; j <= cols; j++) {
     *           float u = (float)j / (float)cols;
     *           float v = (float)i / (float)rows;
     *           vertices[i * (cols + 1) + j] = (CgfxVertex){
     *               .position = { -width/2 + u * width, 0.0f, -depth/2 + v * depth },
     *               .normal   = { 0.0f, 1.0f, 0.0f },
     *               .texcoord0       = { u, v },
     *           };
     *       }
     *   }
     *
     * Generate indices (2 triangles per cell):
     *   uint32_t idx = 0;
     *   for (uint32_t i = 0; i < rows; i++) {
     *       for (uint32_t j = 0; j < cols; j++) {
     *           uint32_t tl = i * (cols + 1) + j;
     *           uint32_t tr = tl + 1;
     *           uint32_t bl = (i + 1) * (cols + 1) + j;
     *           uint32_t br = bl + 1;
     *           indices[idx++] = tl; indices[idx++] = bl; indices[idx++] = tr;
     *           indices[idx++] = tr; indices[idx++] = bl; indices[idx++] = br;
     *       }
     *   }
     *
     * Upload and cleanup:
     *   mesh = cgfx_mesh_create(ctx, vertices, vertex_count, indices, index_count);
     *   free(vertices);
     *   free(indices);
     */

    (void)ctx;
    (void)width;
    (void)depth;
    (void)subdivisions;

    return mesh;
}


CgfxMesh cgfx_primitives_triangle(const CgfxCtx *ctx, float size) {
    CgfxMesh mesh = {};

    /*
     * TODO: Generate an equilateral triangle in the XY plane.
     *
     * Geometry (3 vertices, 3 indices):
     *   float h = size * sqrtf(3.0f) / 2.0f;  // height of equilateral triangle
     *
     *   CgfxVertex vertices[3] = {
     *       // Top vertex
     *       { .position = { 0.0f, h * 2.0f/3.0f, 0.0f },
     *         .normal = { 0.0f, 0.0f, 1.0f },
     *         .texcoord0 = { 0.5f, 1.0f } },
     *       // Bottom-left vertex
     *       { .position = { -size/2.0f, -h * 1.0f/3.0f, 0.0f },
     *         .normal = { 0.0f, 0.0f, 1.0f },
     *         .texcoord0 = { 0.0f, 0.0f } },
     *       // Bottom-right vertex
     *       { .position = { size/2.0f, -h * 1.0f/3.0f, 0.0f },
     *         .normal = { 0.0f, 0.0f, 1.0f },
     *         .texcoord0 = { 1.0f, 0.0f } },
     *   };
     *
     *   uint32_t indices[3] = { 0, 1, 2 };
     *
     *   mesh = cgfx_mesh_create(ctx, vertices, 3, indices, 3);
     */

    (void)ctx;
    (void)size;

    return mesh;
}


CgfxMesh cgfx_primitives_sphere(const CgfxCtx *ctx,
                                 float radius,
                                 uint32_t slices, uint32_t stacks) {
    CgfxMesh mesh = {};

    /*
     * TODO: Generate a UV sphere.
     *
     * Vertex generation:
     *   uint32_t vertex_count = (slices + 1) * (stacks + 1);
     *   uint32_t index_count  = 6 * slices * stacks;
     *   CgfxVertex *vertices = malloc(vertex_count * sizeof(CgfxVertex));
     *   uint32_t   *indices  = malloc(index_count * sizeof(uint32_t));
     *
     *   for (uint32_t stack = 0; stack <= stacks; stack++) {
     *       float theta = (float)stack * M_PI / (float)stacks;  // 0 at north pole, PI at south
     *       float sin_theta = sinf(theta);
     *       float cos_theta = cosf(theta);
     *
     *       for (uint32_t slice = 0; slice <= slices; slice++) {
     *           float phi = (float)slice * 2.0f * M_PI / (float)slices;
     *           float sin_phi = sinf(phi);
     *           float cos_phi = cosf(phi);
     *
     *           // Position on unit sphere, scaled by radius
     *           float x = sin_theta * cos_phi;
     *           float y = cos_theta;
     *           float z = sin_theta * sin_phi;
     *
     *           uint32_t idx = stack * (slices + 1) + slice;
     *           vertices[idx] = (CgfxVertex){
     *               .position = { radius * x, radius * y, radius * z },
     *               .normal   = { x, y, z },  // normalized position = normal
     *               .texcoord0       = { (float)slice / slices, (float)stack / stacks },
     *           };
     *       }
     *   }
     *
     * Index generation (2 triangles per cell):
     *   uint32_t idx = 0;
     *   for (uint32_t stack = 0; stack < stacks; stack++) {
     *       for (uint32_t slice = 0; slice < slices; slice++) {
     *           uint32_t tl = stack * (slices + 1) + slice;
     *           uint32_t tr = tl + 1;
     *           uint32_t bl = (stack + 1) * (slices + 1) + slice;
     *           uint32_t br = bl + 1;
     *           indices[idx++] = tl; indices[idx++] = bl; indices[idx++] = tr;
     *           indices[idx++] = tr; indices[idx++] = bl; indices[idx++] = br;
     *       }
     *   }
     *
     *   mesh = cgfx_mesh_create(ctx, vertices, vertex_count, indices, index_count);
     *   free(vertices);
     *   free(indices);
     */

    (void)ctx;
    (void)radius;
    (void)slices;
    (void)stacks;

    return mesh;
}


CgfxMesh cgfx_primitives_cube(const CgfxCtx *ctx, float size) {
    CgfxMesh mesh = {};

    /*
     * TODO: Generate an axis-aligned cube with per-face normals.
     *
     * 24 vertices (4 per face), 36 indices (6 per face).
     * Each face has its own vertices so normals are correct (hard edges).
     *
     *   float s = size / 2.0f;
     *
     *   CgfxVertex vertices[24] = {
     *       // Front face (+Z), normal (0, 0, +1)
     *       { .position = {-s, -s, +s}, .normal = {0,0,1}, .texcoord0 = {0,0} },
     *       { .position = {+s, -s, +s}, .normal = {0,0,1}, .texcoord0 = {1,0} },
     *       { .position = {+s, +s, +s}, .normal = {0,0,1}, .texcoord0 = {1,1} },
     *       { .position = {-s, +s, +s}, .normal = {0,0,1}, .texcoord0 = {0,1} },
     *
     *       // Back face (-Z), normal (0, 0, -1)
     *       { .position = {+s, -s, -s}, .normal = {0,0,-1}, .texcoord0 = {0,0} },
     *       { .position = {-s, -s, -s}, .normal = {0,0,-1}, .texcoord0 = {1,0} },
     *       { .position = {-s, +s, -s}, .normal = {0,0,-1}, .texcoord0 = {1,1} },
     *       { .position = {+s, +s, -s}, .normal = {0,0,-1}, .texcoord0 = {0,1} },
     *
     *       // Top face (+Y), normal (0, +1, 0)
     *       { .position = {-s, +s, +s}, .normal = {0,1,0}, .texcoord0 = {0,0} },
     *       { .position = {+s, +s, +s}, .normal = {0,1,0}, .texcoord0 = {1,0} },
     *       { .position = {+s, +s, -s}, .normal = {0,1,0}, .texcoord0 = {1,1} },
     *       { .position = {-s, +s, -s}, .normal = {0,1,0}, .texcoord0 = {0,1} },
     *
     *       // Bottom face (-Y), normal (0, -1, 0)
     *       { .position = {-s, -s, -s}, .normal = {0,-1,0}, .texcoord0 = {0,0} },
     *       { .position = {+s, -s, -s}, .normal = {0,-1,0}, .texcoord0 = {1,0} },
     *       { .position = {+s, -s, +s}, .normal = {0,-1,0}, .texcoord0 = {1,1} },
     *       { .position = {-s, -s, +s}, .normal = {0,-1,0}, .texcoord0 = {0,1} },
     *
     *       // Right face (+X), normal (+1, 0, 0)
     *       { .position = {+s, -s, +s}, .normal = {1,0,0}, .texcoord0 = {0,0} },
     *       { .position = {+s, -s, -s}, .normal = {1,0,0}, .texcoord0 = {1,0} },
     *       { .position = {+s, +s, -s}, .normal = {1,0,0}, .texcoord0 = {1,1} },
     *       { .position = {+s, +s, +s}, .normal = {1,0,0}, .texcoord0 = {0,1} },
     *
     *       // Left face (-X), normal (-1, 0, 0)
     *       { .position = {-s, -s, -s}, .normal = {-1,0,0}, .texcoord0 = {0,0} },
     *       { .position = {-s, -s, +s}, .normal = {-1,0,0}, .texcoord0 = {1,0} },
     *       { .position = {-s, +s, +s}, .normal = {-1,0,0}, .texcoord0 = {1,1} },
     *       { .position = {-s, +s, -s}, .normal = {-1,0,0}, .texcoord0 = {0,1} },
     *   };
     *
     *   // Each face: 2 triangles = 6 indices, pattern: (0,1,2), (0,2,3) offset by face*4
     *   uint32_t indices[36];
     *   for (uint32_t face = 0; face < 6; face++) {
     *       uint32_t base = face * 4;
     *       uint32_t i = face * 6;
     *       indices[i+0] = base + 0; indices[i+1] = base + 1; indices[i+2] = base + 2;
     *       indices[i+3] = base + 0; indices[i+4] = base + 2; indices[i+5] = base + 3;
     *   }
     *
     *   mesh = cgfx_mesh_create(ctx, vertices, 24, indices, 36);
     */

    (void)ctx;
    (void)size;

    return mesh;
}
