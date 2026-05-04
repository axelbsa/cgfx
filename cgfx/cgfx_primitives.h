/**
 * @file cgfx_primitives.h
 * @brief Primitive geometry generators — plane, triangle, sphere, cube.
 *
 * Each function generates CPU-side vertex and index data for a geometric
 * primitive, uploads it to GPU buffers, and returns a ready-to-render
 * CgfxMesh. All primitives use the CgfxVertex format (position + normal + UV).
 *
 * The returned mesh owns its GPU buffers. Destroy with cgfx_mesh_destroy().
 *
 * Coordinate conventions:
 *   - Right-handed coordinate system
 *   - Y is up
 *   - Normals point outward
 *   - UVs range from 0.0 to 1.0
 */
#ifndef CGFX_PRIMITIVES_H
#define CGFX_PRIMITIVES_H

#include "cgfx_ctx.h"
#include "cgfx_mesh.h"
#include "cgfx_export.h"

/**
 * Generate a flat plane in the XZ plane.
 *
 * Creates a subdivided quad centered at the origin, lying flat in the
 * XZ plane with the normal pointing up (+Y). Useful for floors, terrain
 * base, water surfaces, etc.
 *
 * Geometry:
 *   - Center: origin (0, 0, 0)
 *   - Extends from (-width/2, 0, -depth/2) to (+width/2, 0, +depth/2)
 *   - Normal: (0, 1, 0) for all vertices
 *   - UVs: (0,0) at (-width/2, -depth/2), (1,1) at (+width/2, +depth/2)
 *
 * Subdivision:
 *   - subdivisions=1 produces 4 vertices and 2 triangles (a single quad)
 *   - subdivisions=N produces (N+1)*(N+1) vertices and 2*N*N triangles
 *   - Higher subdivision allows per-vertex displacement (e.g., heightmaps)
 *
 * Implementation should:
 *   1. Allocate (subdivisions+1)^2 CgfxVertex array
 *   2. For each grid point (i, j) in [0..subdivisions]:
 *      - position = (lerp(-w/2, w/2, j/N), 0, lerp(-d/2, d/2, i/N))
 *      - normal = (0, 1, 0)
 *      - uv = (j/N, i/N)
 *   3. Allocate 6 * subdivisions^2 uint32_t index array
 *   4. For each cell (i, j), emit 2 triangles (6 indices)
 *   5. Call cgfx_mesh_create() with the generated data
 *   6. Free the CPU arrays and return the mesh
 *
 * @param ctx          Initialized context (for GPU buffer upload).
 * @param width        Width of the plane along X axis.
 * @param depth        Depth of the plane along Z axis.
 * @param subdivisions Number of subdivisions per axis (minimum 1).
 * @return             A CgfxMesh with GPU buffers. Destroy with cgfx_mesh_destroy().
 */
CGFX_API CgfxMesh cgfx_primitives_plane(const CgfxCtx *ctx,
                                float width, float depth,
                                uint32_t subdivisions);

/**
 * Generate an equilateral triangle in the XY plane.
 *
 * Creates a single equilateral triangle centered at the origin in the
 * XY plane, with the normal pointing toward the camera (+Z).
 * The simplest possible mesh — useful for testing pipelines.
 *
 * Geometry:
 *   - 3 vertices, 1 triangle (3 indices)
 *   - Center: origin (0, 0, 0)
 *   - Top vertex at (0, size * 2/3, 0), bottom vertices spread to +-size/2
 *   - Normal: (0, 0, 1) for all vertices
 *   - UVs: mapped to cover [0,1] range
 *
 * Implementation should:
 *   1. Compute the 3 vertices of an equilateral triangle:
 *      - v0 = top    = (0, h*2/3, 0)        where h = size * sqrt(3)/2
 *      - v1 = bottom-left  = (-size/2, -h/3, 0)
 *      - v2 = bottom-right = (+size/2, -h/3, 0)
 *   2. All normals = (0, 0, 1)
 *   3. UVs: v0=(0.5, 1.0), v1=(0.0, 0.0), v2=(1.0, 0.0)
 *   4. Indices: {0, 1, 2}
 *   5. Call cgfx_mesh_create() and return
 *
 * @param ctx   Initialized context.
 * @param size  Distance from center to each vertex (approximate radius).
 * @return      A CgfxMesh with GPU buffers. Destroy with cgfx_mesh_destroy().
 */
CGFX_API CgfxMesh cgfx_primitives_triangle(const CgfxCtx *ctx, float size);

/**
 * Generate a UV sphere.
 *
 * Creates a sphere using the standard UV (latitude/longitude) subdivision
 * method. The sphere is centered at the origin with the poles on the Y axis.
 *
 * Geometry:
 *   - Center: origin (0, 0, 0)
 *   - Radius: as specified
 *   - Normals: point radially outward (equal to normalized position)
 *   - UVs: longitude maps to U (0.0-1.0), latitude maps to V (0.0-1.0)
 *   - North pole at (0, +radius, 0), south pole at (0, -radius, 0)
 *
 * Tessellation:
 *   - slices = number of vertical divisions (longitude lines, like orange segments)
 *   - stacks = number of horizontal divisions (latitude lines)
 *   - Vertex count: (slices + 1) * (stacks + 1)
 *   - Triangle count: 2 * slices * stacks (degenerate triangles at poles are fine)
 *
 * Implementation should:
 *   1. Allocate (slices+1) * (stacks+1) vertices
 *   2. For each (stack, slice) pair:
 *      - theta = stack * PI / stacks  (polar angle, 0 at north pole)
 *      - phi = slice * 2*PI / slices  (azimuthal angle)
 *      - position = radius * (sin(theta)*cos(phi), cos(theta), sin(theta)*sin(phi))
 *      - normal = normalized position
 *      - uv = (slice/slices, stack/stacks)
 *   3. Allocate 6 * slices * stacks indices
 *   4. For each cell, emit 2 triangles connecting adjacent stack/slice vertices
 *   5. Call cgfx_mesh_create() and return
 *
 * @param ctx     Initialized context.
 * @param radius  Sphere radius.
 * @param slices  Number of longitudinal divisions (minimum 3).
 * @param stacks  Number of latitudinal divisions (minimum 2).
 * @return        A CgfxMesh with GPU buffers. Destroy with cgfx_mesh_destroy().
 */
CGFX_API CgfxMesh cgfx_primitives_sphere(const CgfxCtx *ctx,
                                 float radius,
                                 uint32_t slices, uint32_t stacks);

/**
 * Generate an axis-aligned cube.
 *
 * Creates a cube centered at the origin with 6 faces. Each face has its
 * own set of 4 vertices with a unique face normal (not shared between
 * faces), so lighting looks correct with hard edges.
 *
 * Geometry:
 *   - Center: origin (0, 0, 0)
 *   - Extends from (-size/2, -size/2, -size/2) to (+size/2, +size/2, +size/2)
 *   - 24 vertices (4 per face, 6 faces) — not shared, for correct normals
 *   - 36 indices (2 triangles per face, 6 faces)
 *   - Normals: axis-aligned per face (+X, -X, +Y, -Y, +Z, -Z)
 *   - UVs: each face maps (0,0)-(1,1) independently
 *
 * Implementation should:
 *   1. Define 6 faces, each with 4 corner vertices:
 *      Face +Z (front):  normals (0,0,+1), corners at z=+size/2
 *      Face -Z (back):   normals (0,0,-1), corners at z=-size/2
 *      Face +Y (top):    normals (0,+1,0), corners at y=+size/2
 *      Face -Y (bottom): normals (0,-1,0), corners at y=-size/2
 *      Face +X (right):  normals (+1,0,0), corners at x=+size/2
 *      Face -X (left):   normals (-1,0,0), corners at x=-size/2
 *   2. Each face: 4 vertices with UVs (0,0), (1,0), (1,1), (0,1)
 *   3. Each face: 6 indices forming 2 triangles (0,1,2) and (0,2,3)
 *      offset by face_index * 4
 *   4. Call cgfx_mesh_create() and return
 *
 * @param ctx   Initialized context.
 * @param size  Side length of the cube.
 * @return      A CgfxMesh with GPU buffers. Destroy with cgfx_mesh_destroy().
 */
CGFX_API CgfxMesh cgfx_primitives_cube(const CgfxCtx *ctx, float size);

#endif /* CGFX_PRIMITIVES_H */
