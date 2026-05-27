/**
 * @file cgfx_mesh.h
 * @brief Mesh abstraction — vertex data + index data + GPU buffers.
 *
 * A mesh combines vertex data (position, normal, tangent, texcoords, color, joints, weights) with index data
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
#include "cgfx_export.h"

/**
 * Standard vertex format used by all cgfx meshes.
 *
 * Layout (96 bytes total, tightly packed):
 *   Offset  0: position   float[3]     (12 bytes)  — world-space position
 *   Offset 12: normal     float[3]     (12 bytes)  — surface normal (unit length)
 *   Offset 24: tangent    float[4]     (16 bytes)  — tangent (xyz) + handedness (w)
 *   Offset 40: texcoord0  float[2]     ( 8 bytes)  — primary texture coordinates
 *   Offset 48: texcoord1  float[2]     ( 8 bytes)  — secondary texture coordinates
 *   Offset 56: color      float[4]     (16 bytes)  — vertex color (RGBA)
 *   Offset 72: joints     uint16_t[4]  ( 8 bytes)  — skeletal joint indices
 *   Offset 80: weights    float[4]     (16 bytes)  — skeletal blend weights
 *
 * This matches the vertex buffer layout returned by cgfx_mesh_vertex_layout().
 * When creating a pipeline for mesh rendering, pass that layout to
 * CgfxPipelineDesc.vertex_layouts.
 */
typedef struct CgfxVertex {
    float       position[3];   /**< XYZ position.                                   */
    float       normal[3];     /**< Surface normal (should be normalized).           */
    float       tangent[4];    /**< Tangent vector (xyz) + handedness sign (w).      */
    float       texcoord0[2];  /**< Primary texture coordinates (0.0-1.0).          */
    float       texcoord1[2];  /**< Secondary texture coordinates (lightmaps, etc). */
    float       color[4];      /**< Vertex color (RGBA, 0.0-1.0).                   */
    uint16_t    joints[4];     /**< Skeletal joint/bone indices.                     */
    float       weights[4];    /**< Skeletal blend weights (should sum to 1.0).      */
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
CGFX_API CgfxMesh cgfx_mesh_create(const CgfxCtx *ctx,
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
CGFX_API void cgfx_mesh_destroy(CgfxMesh *mesh);

/**
 * Record draw commands for a mesh on an active render pass.
 *
 * Binds the vertex buffer at slot 0, sets the index buffer, and issues
 * an indexed draw call for mesh->index_count indices.
 *
 * The pipeline must already be set on the render pass before calling
 * this function.
 *
 * For instanced rendering or non-indexed draws, use the raw WebGPU API
 * with mesh->vertex_buffer.buffer and mesh->index_buffer.buffer.
 *
 * @param pass  Active render pass encoder (from cgfx_frame_begin()).
 * @param mesh  Mesh to draw. Must have valid vertex and index buffers.
 */
CGFX_API void cgfx_mesh_draw(WGPURenderPassEncoder pass, const CgfxMesh *mesh);

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
 *       .vertex_layouts = &layout,
 *   };
 *
 * The layout describes 8 attributes at shader locations 0-7:
 *   - Location 0: position  (Float32x3, offset  0)
 *   - Location 1: normal    (Float32x3, offset 12)
 *   - Location 2: tangent   (Float32x4, offset 24)
 *   - Location 3: texcoord0 (Float32x2, offset 40)
 *   - Location 4: texcoord1 (Float32x2, offset 48)
 *   - Location 5: color     (Float32x4, offset 56)
 *   - Location 6: joints    (Uint16x4,  offset 72)
 *   - Location 7: weights   (Float32x4, offset 80)
 *
 * Stride is sizeof(CgfxVertex) = 96 bytes, step mode is Vertex.
 *
 * Note: The returned layout references static internal storage.
 * It is valid for the lifetime of the program but should not be modified.
 *
 * @return  Vertex buffer layout matching CgfxVertex.
 */
CGFX_API WGPUVertexBufferLayout cgfx_mesh_vertex_layout(void);

#endif /* CGFX_MESH_H */
