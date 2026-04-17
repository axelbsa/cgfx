/**
 * @file cgfx_mesh.c
 * @brief Implementation of mesh creation, destruction, and vertex layout.
 *
 * TODO: Fill in cgfx_mesh_create and cgfx_mesh_destroy once cgfx_buffer
 * functions are implemented. cgfx_mesh_vertex_layout is fully implemented
 * since it's pure data description with no WebGPU calls.
 */
#include "cgfx_mesh.h"

#include <stddef.h>


CgfxMesh cgfx_mesh_create(const CgfxCtx *ctx,
                            const CgfxVertex *vertices, uint32_t vertex_count,
                            const uint32_t *indices, uint32_t index_count) {
    CgfxMesh mesh = {};

    mesh.vertex_buffer = cgfx_buffer_create_vertex(
            ctx,
            vertices,
            (uint64_t)vertex_count * sizeof(CgfxVertex),
            vertex_count
    );

    mesh.index_buffer = cgfx_buffer_create_index(ctx, indices, index_count);
    mesh.index_count = index_count;

    /*
     * TODO: Upload vertex and index data to GPU buffers.
     *
     * 1. Create vertex buffer:
     *    mesh.vertex_buffer = cgfx_buffer_create_vertex(
     *        ctx,
     *        vertices,
     *        (uint64_t)vertex_count * sizeof(CgfxVertex),
     *        vertex_count
     *    );
     *
     * 2. Create index buffer:
     *    mesh.index_buffer = cgfx_buffer_create_index(ctx, indices, index_count);
     *
     * 3. Store count:
     *    mesh.index_count = index_count;
     */

    return mesh;
}


void cgfx_mesh_destroy(CgfxMesh *mesh) {
    cgfx_buffer_destroy(&mesh->vertex_buffer);
    cgfx_buffer_destroy(&mesh->index_buffer);
    mesh->index_count = 0;

    /*
     * TODO: Release both GPU buffers.
     *
     * 1. cgfx_buffer_destroy(&mesh->vertex_buffer);
     * 2. cgfx_buffer_destroy(&mesh->index_buffer);
     * 3. mesh->index_count = 0;
     */
}


void cgfx_mesh_draw(WGPURenderPassEncoder pass, const CgfxMesh *mesh) {
    wgpuRenderPassEncoderSetVertexBuffer(pass, 0,
                                         mesh->vertex_buffer.buffer,
                                         0, mesh->vertex_buffer.size);
    wgpuRenderPassEncoderSetIndexBuffer(pass,
                                        mesh->index_buffer.buffer,
                                        WGPUIndexFormat_Uint32,
                                        0, mesh->index_buffer.size);
    wgpuRenderPassEncoderDrawIndexed(pass, mesh->index_count, 1, 0, 0, 0);
}


WGPUVertexBufferLayout cgfx_mesh_vertex_layout(void) {
    /*
     * Vertex attribute layout matching CgfxVertex:
     *
     *   struct CgfxVertex {
     *       float position[3];  // offset  0, 12 bytes
     *       float normal[3];    // offset 12, 12 bytes
     *       float uv[2];        // offset 24,  8 bytes
     *   };                      // total: 32 bytes
     *
     * Each attribute is assigned a shader location for use in WGSL:
     *   @location(0) position: vec3f
     *   @location(1) normal:   vec3f
     *   @location(2) uv:       vec2f
     */
    static WGPUVertexAttribute attributes[] = {
        {
            .format = WGPUVertexFormat_Float32x3,
            .offset = offsetof(CgfxVertex, position),
            .shaderLocation = 0,
        },
        {
            .format = WGPUVertexFormat_Float32x3,
            .offset = offsetof(CgfxVertex, normal),
            .shaderLocation = 1,
        },
        {
            .format = WGPUVertexFormat_Float32x2,
            .offset = offsetof(CgfxVertex, uv),
            .shaderLocation = 2,
        },
    };

    const WGPUVertexBufferLayout layout = {
        .arrayStride = sizeof(CgfxVertex),
        .stepMode = WGPUVertexStepMode_Vertex,
        .attributeCount = 3,
        .attributes = attributes,
    };

    return layout;
}
