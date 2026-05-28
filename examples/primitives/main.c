#include "cgfx.h"

#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
    float model[4][4];
} ObjectUniforms;


static CgfxMesh create_triangle(const CgfxCtx *ctx, float size) {
    float h = size * sqrtf(3.0f) / 2.0f;

    CgfxVertex vertices[3] = {
        { .position = { 0.0f, h * 2.0f/3.0f, 0.0f },
          .normal = { 0.0f, 0.0f, 1.0f },
          .texcoord0 = { 0.5f, 1.0f } },
        { .position = { -size/2.0f, -h * 1.0f/3.0f, 0.0f },
          .normal = { 0.0f, 0.0f, 1.0f },
          .texcoord0 = { 0.0f, 0.0f } },
        { .position = { size/2.0f, -h * 1.0f/3.0f, 0.0f },
          .normal = { 0.0f, 0.0f, 1.0f },
          .texcoord0 = { 1.0f, 0.0f } },
    };
    uint32_t indices[3] = { 0, 1, 2 };

    return cgfx_mesh_create(ctx, vertices, 3, indices, 3);
}


static CgfxMesh create_cube(const CgfxCtx *ctx, float size) {
    float s = size / 2.0f;

    CgfxVertex vertices[24] = {
        // Front face (+Z)
        { .position = {-s, -s, +s}, .normal = {0,0,1}, .texcoord0 = {0,0} },
        { .position = {+s, -s, +s}, .normal = {0,0,1}, .texcoord0 = {1,0} },
        { .position = {+s, +s, +s}, .normal = {0,0,1}, .texcoord0 = {1,1} },
        { .position = {-s, +s, +s}, .normal = {0,0,1}, .texcoord0 = {0,1} },
        // Back face (-Z)
        { .position = {+s, -s, -s}, .normal = {0,0,-1}, .texcoord0 = {0,0} },
        { .position = {-s, -s, -s}, .normal = {0,0,-1}, .texcoord0 = {1,0} },
        { .position = {-s, +s, -s}, .normal = {0,0,-1}, .texcoord0 = {1,1} },
        { .position = {+s, +s, -s}, .normal = {0,0,-1}, .texcoord0 = {0,1} },
        // Top face (+Y)
        { .position = {-s, +s, +s}, .normal = {0,1,0}, .texcoord0 = {0,0} },
        { .position = {+s, +s, +s}, .normal = {0,1,0}, .texcoord0 = {1,0} },
        { .position = {+s, +s, -s}, .normal = {0,1,0}, .texcoord0 = {1,1} },
        { .position = {-s, +s, -s}, .normal = {0,1,0}, .texcoord0 = {0,1} },
        // Bottom face (-Y)
        { .position = {-s, -s, -s}, .normal = {0,-1,0}, .texcoord0 = {0,0} },
        { .position = {+s, -s, -s}, .normal = {0,-1,0}, .texcoord0 = {1,0} },
        { .position = {+s, -s, +s}, .normal = {0,-1,0}, .texcoord0 = {1,1} },
        { .position = {-s, -s, +s}, .normal = {0,-1,0}, .texcoord0 = {0,1} },
        // Right face (+X)
        { .position = {+s, -s, +s}, .normal = {1,0,0}, .texcoord0 = {0,0} },
        { .position = {+s, -s, -s}, .normal = {1,0,0}, .texcoord0 = {1,0} },
        { .position = {+s, +s, -s}, .normal = {1,0,0}, .texcoord0 = {1,1} },
        { .position = {+s, +s, +s}, .normal = {1,0,0}, .texcoord0 = {0,1} },
        // Left face (-X)
        { .position = {-s, -s, -s}, .normal = {-1,0,0}, .texcoord0 = {0,0} },
        { .position = {-s, -s, +s}, .normal = {-1,0,0}, .texcoord0 = {1,0} },
        { .position = {-s, +s, +s}, .normal = {-1,0,0}, .texcoord0 = {1,1} },
        { .position = {-s, +s, -s}, .normal = {-1,0,0}, .texcoord0 = {0,1} },
    };

    uint32_t indices[36];
    for (uint32_t face = 0; face < 6; face++) {
        uint32_t base = face * 4;
        uint32_t i = face * 6;
        indices[i+0] = base + 0; indices[i+1] = base + 1; indices[i+2] = base + 2;
        indices[i+3] = base + 0; indices[i+4] = base + 2; indices[i+5] = base + 3;
    }

    return cgfx_mesh_create(ctx, vertices, 24, indices, 36);
}


static CgfxMesh create_plane(const CgfxCtx *ctx,
                              float width, float depth,
                              uint32_t subdivisions) {
    uint32_t cols = subdivisions;
    uint32_t rows = subdivisions;
    uint32_t vertex_count = (cols + 1) * (rows + 1);
    uint32_t index_count  = 6 * cols * rows;

    CgfxVertex *vertices = malloc(vertex_count * sizeof(CgfxVertex));
    uint32_t   *indices  = malloc(index_count * sizeof(uint32_t));

    for (uint32_t i = 0; i <= rows; i++) {
        for (uint32_t j = 0; j <= cols; j++) {
            float u = (float)j / (float)cols;
            float v = (float)i / (float)rows;
            vertices[i * (cols + 1) + j] = (CgfxVertex){
                .position = { -width/2 + u * width, 0.0f, -depth/2 + v * depth },
                .normal   = { 0.0f, 1.0f, 0.0f },
                .texcoord0 = { u, v },
            };
        }
    }

    uint32_t idx = 0;
    for (uint32_t i = 0; i < rows; i++) {
        for (uint32_t j = 0; j < cols; j++) {
            uint32_t tl = i * (cols + 1) + j;
            uint32_t tr = tl + 1;
            uint32_t bl = (i + 1) * (cols + 1) + j;
            uint32_t br = bl + 1;
            indices[idx++] = tl; indices[idx++] = bl; indices[idx++] = tr;
            indices[idx++] = tr; indices[idx++] = bl; indices[idx++] = br;
        }
    }

    CgfxMesh mesh = cgfx_mesh_create(ctx, vertices, vertex_count, indices, index_count);
    free(vertices);
    free(indices);
    return mesh;
}


static CgfxMesh create_sphere(const CgfxCtx *ctx,
                               float radius,
                               uint32_t slices, uint32_t stacks) {
    uint32_t vertex_count = (slices + 1) * (stacks + 1);
    uint32_t index_count  = 6 * slices * stacks;

    CgfxVertex *vertices = malloc(vertex_count * sizeof(CgfxVertex));
    uint32_t   *indices  = malloc(index_count * sizeof(uint32_t));

    for (uint32_t stack = 0; stack <= stacks; stack++) {
        float theta = (float)stack * (float)M_PI / (float)stacks;
        float sin_theta = sinf(theta);
        float cos_theta = cosf(theta);

        for (uint32_t slice = 0; slice <= slices; slice++) {
            float phi = (float)slice * 2.0f * (float)M_PI / (float)slices;
            float sin_phi = sinf(phi);
            float cos_phi = cosf(phi);

            float x = sin_theta * cos_phi;
            float y = cos_theta;
            float z = sin_theta * sin_phi;

            uint32_t vi = stack * (slices + 1) + slice;
            vertices[vi] = (CgfxVertex){
                .position = { radius * x, radius * y, radius * z },
                .normal   = { x, y, z },
                .texcoord0 = { (float)slice / (float)slices,
                               (float)stack / (float)stacks },
            };
        }
    }

    uint32_t idx = 0;
    for (uint32_t stack = 0; stack < stacks; stack++) {
        for (uint32_t slice = 0; slice < slices; slice++) {
            uint32_t tl = stack * (slices + 1) + slice;
            uint32_t tr = tl + 1;
            uint32_t bl = (stack + 1) * (slices + 1) + slice;
            uint32_t br = bl + 1;
            indices[idx++] = tl; indices[idx++] = bl; indices[idx++] = tr;
            indices[idx++] = tr; indices[idx++] = bl; indices[idx++] = br;
        }
    }

    CgfxMesh mesh = cgfx_mesh_create(ctx, vertices, vertex_count, indices, index_count);
    free(vertices);
    free(indices);
    return mesh;
}


static void make_identity(float m[4][4]) {
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            m[i][j] = (i == j) ? 1.0f : 0.0f;
}

static void make_rotation_y(float m[4][4], float angle) {
    make_identity(m);
    float c = cosf(angle);
    float s = sinf(angle);
    m[0][0] =  c; m[0][2] = s;
    m[2][0] = -s; m[2][2] = c;
}

static void set_translation(float m[4][4], float x, float y, float z) {
    m[3][0] = x; m[3][1] = y; m[3][2] = z;
}


int main(void) {
    CgfxCtx ctx;
    if (!cgfx_ctx_init(&ctx, &(CgfxCtxDesc){
        .width = 1280,
        .height = 720,
        .title = "cgfx — procedural geometry",
        .depth_buffer = true,
        .limits = cgfx_default_limits(),
    })) {
        return 1;
    }

    CgfxShader shader = cgfx_shader_create_from_file(&ctx, "primitives", "shaders/primitives.wgsl",
        &(CgfxShaderDesc){
            .group_count = 2,
            .groups = (CgfxGroupDesc[]){{
                // @group(0): camera (projection + view)
                .binding_count = 1,
                .bindings = (CgfxBindingDesc[]){{ .binding = 0 }},
            }, {
                // @group(1): per-object model matrix
                .binding_count = 1,
                .bindings = (CgfxBindingDesc[]){{ .binding = 0 }},
            }},
        });

    WGPUVertexBufferLayout layout = cgfx_mesh_vertex_layout();
    WGPURenderPipeline pipeline = cgfx_pipeline_create(&ctx, &(CgfxPipelineDesc){
        .shader = &shader,
        .depth_test = true,
        .vertex_buffer_count = 1,
        .vertex_layouts = &layout,
    });

    CgfxCamera camera = cgfx_camera_create(&ctx, &shader, &(CgfxCameraDesc){
        .fovy = 45.0f,
        .eye    = { 0.0f, 3.0f, -8.0f },
        .center = { 0.0f, 0.0f,  0.0f },
    });

    CgfxMesh meshes[4] = {
        create_triangle(&ctx, 1.5f),
        create_cube(&ctx, 1.5f),
        create_plane(&ctx, 2.0f, 2.0f, 4),
        create_sphere(&ctx, 0.8f, 24, 16),
    };

    float positions[4][3] = {
        { -2.0f,  0.5f,  2.0f },  // triangle — back left
        {  2.0f,  0.5f,  2.0f },  // cube — back right
        { -2.0f, -0.5f, -2.0f },  // plane — front left
        {  2.0f,  0.0f, -2.0f },  // sphere — front right
    };

    ObjectUniforms obj_data[4] = {};
    CgfxUniform obj_uniforms[4];
    for (int i = 0; i < 4; i++) {
        make_identity(obj_data[i].model);
        obj_uniforms[i] = cgfx_uniform_create(&ctx, &shader, 1,
                                               &obj_data[i], sizeof(ObjectUniforms));
    }

    float time = 0.0f;

    while (cgfx_ctx_is_running(&ctx)) {
        glfwPollEvents();
        time += 0.016f;

        for (int i = 0; i < 4; i++) {
            make_rotation_y(obj_data[i].model, time + (float)i * 1.5f);
            set_translation(obj_data[i].model,
                            positions[i][0], positions[i][1], positions[i][2]);
            cgfx_uniform_write(&ctx, &obj_uniforms[i]);
        }
        cgfx_camera_write(&ctx, &camera);

        CgfxFrame frame;
        if (cgfx_frame_begin(&ctx, &frame, (WGPUColor){0.1, 0.1, 0.15, 1.0})) {
            wgpuRenderPassEncoderSetPipeline(frame.render_pass, pipeline);
            cgfx_camera_bind(frame.render_pass, &camera);

            for (int i = 0; i < 4; i++) {
                wgpuRenderPassEncoderSetBindGroup(frame.render_pass, 1,
                    obj_uniforms[i].bind_group, 0, nullptr);
                cgfx_mesh_draw(frame.render_pass, &meshes[i]);
            }

            cgfx_frame_end(&ctx, &frame);
        }
    }

    for (int i = 0; i < 4; i++) {
        cgfx_uniform_destroy(&obj_uniforms[i]);
        cgfx_mesh_destroy(&meshes[i]);
    }
    cgfx_camera_destroy(&camera);
    wgpuRenderPipelineRelease(pipeline);
    cgfx_shader_destroy(&shader);
    cgfx_ctx_destroy(&ctx);

    return 0;
}
