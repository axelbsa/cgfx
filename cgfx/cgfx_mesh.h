/**
 * @file cgfx_mesh.h
 * @brief Mesh abstraction — vertex data + index data + GPU buffers.
 *
 * A mesh combines vertex data (position, normal, UV) with index data
 * into GPU-resident buffers ready for rendering. The CgfxVertex layout
 * is standardized so all meshes share the same vertex buffer format,
 * which simplifies pipeline creation.
 *
 * Lifecycle:
 *   CgfxMesh mesh = cgfx_mesh_create(&ctx, verts, nv, indices, ni);
 *   // ... render with mesh ...
 *   cgfx_mesh_destroy(&mesh);
 */
#ifndef CGFX_MESH_H
#define CGFX_MESH_H

#include <webgpu/webgpu.h>
#include <stdint.h>
#include "cgfx_ctx.h"
#include "cgfx_buffer.h"

/**
 * Standard vertex format used by all cgfx meshes.
 *
 * Layout (32 bytes total, tightly packed):
 *   Offset  0: position  float[3]  (12 bytes)  — world-space position
 *   Offset 12: normal    float[3]  (12 bytes)  — surface normal (unit length)
 *   Offset 24: uv        float[2]  (8 bytes)   — texture coordinates
 *
 * This matches the vertex buffer layout returned by cgfx_mesh_vertex_layout().
 * When creating a pipeline for mesh rendering, pass that layout to
 * CgfxPipelineDesc.vertex_buffers.
 */
typedef struct CgfxVertex {
    float       position[3];  /**< XYZ position.                                    */
    float       normal[3];    /**< Surface normal (should be normalized).           */
    float       uv[2];        /**< Texture coordinates (0.0-1.0 range typically).   */
} CgfxVertex;

/**
 * A renderable mesh with GPU-resident vertex and index buffers.
 *
 * Created by cgfx_mesh_create() or by primitive generators.
 * Contains everything needed to bind and draw the mesh.
 */
typedef struct CgfxMesh {
    CgfxBuffer  vertex_buffer;  /**< GPU vertex buffer (CgfxVertex array).    */
    CgfxBuffer  index_buffer;   /**< GPU index buffer (uint32_t array).       */
    uint32_t    index_count;    /**< Number of indices (= number of draw elements). */
} CgfxMesh;

/**
 * Create a mesh from vertex and index data.
 *
 * Uploads the provided vertex and index arrays to GPU buffers using
 * cgfx_buffer_create_vertex() and cgfx_buffer_create_index().
 *
 * Implementation should:
 *   1. Create a vertex buffer:
 *      mesh.vertex_buffer = cgfx_buffer_create_vertex(ctx, vertices,
 *          vertex_count * sizeof(CgfxVertex), vertex_count);
 *   2. Create an index buffer:
 *      mesh.index_buffer = cgfx_buffer_create_index(ctx, indices, index_count);
 *   3. Store index_count for draw calls.
 *
 * The caller is responsible for freeing the CPU-side vertex/index arrays
 * if they were dynamically allocated — the mesh only holds GPU copies.
 *
 * @param ctx           Initialized context.
 * @param vertices      Array of CgfxVertex structs.
 * @param vertex_count  Number of vertices in the array.
 * @param indices       Array of uint32_t triangle indices.
 * @param index_count   Number of indices (should be a multiple of 3 for triangles).
 * @return              A CgfxMesh with GPU buffers. Call cgfx_mesh_destroy() to free.
 */
CgfxMesh cgfx_mesh_create(const CgfxCtx *ctx,
                            const CgfxVertex *vertices, uint32_t vertex_count,
                            const uint32_t *indices, uint32_t index_count);

/**
 * Destroy a mesh and release its GPU buffers.
 *
 * Releases both the vertex buffer and index buffer via cgfx_buffer_destroy().
 * After this call, the mesh must not be used.
 *
 * @param mesh  Mesh to destroy.
 */
void cgfx_mesh_destroy(CgfxMesh *mesh);

/**
 * Get the vertex buffer layout descriptor for CgfxVertex.
 *
 * Returns a WGPUVertexBufferLayout that describes the memory layout of
 * CgfxVertex to the render pipeline. Pass this to CgfxPipelineDesc:
 *
 *   WGPUVertexBufferLayout layout = cgfx_mesh_vertex_layout();
 *   CgfxPipelineDesc desc = {
 *       .shader = shader,
 *       .vertex_buffer_count = 1,
 *       .vertex_buffers = &layout,
 *   };
 *
 * The layout describes 3 attributes at shader locations 0, 1, 2:
 *   - Location 0: position (Float32x3, offset 0)
 *   - Location 1: normal   (Float32x3, offset 12)
 *   - Location 2: uv       (Float32x2, offset 24)
 *
 * Stride is sizeof(CgfxVertex) = 32 bytes, step mode is Vertex.
 *
 * Implementation should:
 *   1. Define a static array of 3 WGPUVertexAttribute entries
 *   2. Return a WGPUVertexBufferLayout pointing to that array
 *   3. The static array persists because it's file-scope static
 *
 * Note: The returned layout references static internal storage.
 * It is valid for the lifetime of the program but should not be modified.
 *
 * @return  Vertex buffer layout matching CgfxVertex.
 */
WGPUVertexBufferLayout cgfx_mesh_vertex_layout(void);

#endif /* CGFX_MESH_H */
