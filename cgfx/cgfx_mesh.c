/**
 * @file cgfx_mesh.c
 * @brief Implementation of mesh creation, destruction, and vertex layout.
 */
#include "cgfx_mesh.h"

#include <stddef.h>

static_assert(sizeof(CgfxVertex) == 96, "CgfxVertex must be 96 bytes (tightly packed)");


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
    mesh.ok = mesh.vertex_buffer.ok && mesh.index_buffer.ok;

    return mesh;
}


void cgfx_mesh_destroy(CgfxMesh *mesh) {
    cgfx_buffer_destroy(&mesh->vertex_buffer);
    cgfx_buffer_destroy(&mesh->index_buffer);
    mesh->index_count = 0;
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


void cgfx_mesh_draw_instanced(WGPURenderPassEncoder pass,
                               const CgfxMesh *mesh,
                               uint32_t instance_count) {
    wgpuRenderPassEncoderSetVertexBuffer(pass, 0,
                                         mesh->vertex_buffer.buffer,
                                         0, mesh->vertex_buffer.size);
    wgpuRenderPassEncoderSetIndexBuffer(pass,
                                        mesh->index_buffer.buffer,
                                        WGPUIndexFormat_Uint32,
                                        0, mesh->index_buffer.size);
    wgpuRenderPassEncoderDrawIndexed(pass, mesh->index_count,
                                     instance_count, 0, 0, 0);
}


WGPUVertexBufferLayout cgfx_mesh_vertex_layout(void) {
    static WGPUVertexAttribute attributes[] = {
        { .format = WGPUVertexFormat_Float32x3, .offset = offsetof(CgfxVertex, position),  .shaderLocation = 0 },
        { .format = WGPUVertexFormat_Float32x3, .offset = offsetof(CgfxVertex, normal),    .shaderLocation = 1 },
        { .format = WGPUVertexFormat_Float32x4, .offset = offsetof(CgfxVertex, tangent),   .shaderLocation = 2 },
        { .format = WGPUVertexFormat_Float32x2, .offset = offsetof(CgfxVertex, texcoord0), .shaderLocation = 3 },
        { .format = WGPUVertexFormat_Float32x2, .offset = offsetof(CgfxVertex, texcoord1), .shaderLocation = 4 },
        { .format = WGPUVertexFormat_Float32x4, .offset = offsetof(CgfxVertex, color),     .shaderLocation = 5 },
        { .format = WGPUVertexFormat_Uint16x4,  .offset = offsetof(CgfxVertex, joints),    .shaderLocation = 6 },
        { .format = WGPUVertexFormat_Float32x4, .offset = offsetof(CgfxVertex, weights),   .shaderLocation = 7 },
    };

    const WGPUVertexBufferLayout layout = {
        .arrayStride = sizeof(CgfxVertex),
        .stepMode = WGPUVertexStepMode_Vertex,
        .attributeCount = 8,
        .attributes = attributes,
    };

    return layout;
}
