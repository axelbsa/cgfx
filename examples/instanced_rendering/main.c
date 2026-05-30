#include "cgfx.h"
#include <GLFW/glfw3.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>

#define GRID_SIZE       500
#define INSTANCE_COUNT  (GRID_SIZE * GRID_SIZE)
#define CUBE_SIZE       0.8f
#define GRID_SPACING    1.0f

typedef struct {
    float model_col0[4];
    float model_col1[4];
    float model_col2[4];
    float model_col3[4];
    float color[4];
} InstanceData;

static CgfxMesh create_cube(const CgfxCtx *ctx, float size) {
    float s = size / 2.0f;

    CgfxVertex vertices[24] = {
        { .position = {-s, -s, +s}, .normal = {0,0,1} },
        { .position = {+s, -s, +s}, .normal = {0,0,1} },
        { .position = {+s, +s, +s}, .normal = {0,0,1} },
        { .position = {-s, +s, +s}, .normal = {0,0,1} },
        { .position = {+s, -s, -s}, .normal = {0,0,-1} },
        { .position = {-s, -s, -s}, .normal = {0,0,-1} },
        { .position = {-s, +s, -s}, .normal = {0,0,-1} },
        { .position = {+s, +s, -s}, .normal = {0,0,-1} },
        { .position = {-s, +s, +s}, .normal = {0,1,0} },
        { .position = {+s, +s, +s}, .normal = {0,1,0} },
        { .position = {+s, +s, -s}, .normal = {0,1,0} },
        { .position = {-s, +s, -s}, .normal = {0,1,0} },
        { .position = {-s, -s, -s}, .normal = {0,-1,0} },
        { .position = {+s, -s, -s}, .normal = {0,-1,0} },
        { .position = {+s, -s, +s}, .normal = {0,-1,0} },
        { .position = {-s, -s, +s}, .normal = {0,-1,0} },
        { .position = {+s, -s, +s}, .normal = {1,0,0} },
        { .position = {+s, -s, -s}, .normal = {1,0,0} },
        { .position = {+s, +s, -s}, .normal = {1,0,0} },
        { .position = {+s, +s, +s}, .normal = {1,0,0} },
        { .position = {-s, -s, -s}, .normal = {-1,0,0} },
        { .position = {-s, -s, +s}, .normal = {-1,0,0} },
        { .position = {-s, +s, +s}, .normal = {-1,0,0} },
        { .position = {-s, +s, -s}, .normal = {-1,0,0} },
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

static void hsv_to_rgb(float h, float s, float v, float *r, float *g, float *b) {
    int i = (int)(h * 6.0f);
    float f = h * 6.0f - i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - f * s);
    float t = v * (1.0f - (1.0f - f) * s);
    switch (i % 6) {
    case 0: *r = v; *g = t; *b = p; break;
    case 1: *r = q; *g = v; *b = p; break;
    case 2: *r = p; *g = v; *b = t; break;
    case 3: *r = p; *g = q; *b = v; break;
    case 4: *r = t; *g = p; *b = v; break;
    case 5: *r = v; *g = p; *b = q; break;
    }
}

static void on_resize(GLFWwindow *window, int width, int height) {
    CgfxCtx *ctx = glfwGetWindowUserPointer(window);
    cgfx_ctx_resize(ctx, (uint32_t)width, (uint32_t)height);
}

int main(void) {
    CgfxCtx ctx;
    char title[50];
    sprintf(title, "cgfx - instanced rendering (%dk cubes)", INSTANCE_COUNT/1000);
    if (!cgfx_ctx_init(&ctx, &(CgfxCtxDesc){
        .width  = 1280,
        .height = 720,
        .title  = title,
        .depth_buffer = true,
        .limits = cgfx_default_limits(),
    })) {
        return 1;
    }
    glfwSetWindowUserPointer(ctx.window, &ctx);
    glfwSetFramebufferSizeCallback(ctx.window, on_resize);

    CgfxShader shader = cgfx_shader_create_from_file(&ctx,
        "instanced shader", "shaders/instanced_rendering.wgsl",
        &(CgfxShaderDesc){
            .group_count = 1,
            .groups = (CgfxGroupDesc[]){{
                .binding_count = 1,
                .bindings = (CgfxBindingDesc[]){{
                    .binding = 0,
                    .min_binding_size = 2 * sizeof(float) * 16,
                }},
            }},
        });
    if (!shader.ok) {
        fprintf(stderr, "shader creation failed\n");
        cgfx_ctx_destroy(&ctx);
        return 1;
    }

    WGPUVertexAttribute instance_attrs[] = {
        { .format = WGPUVertexFormat_Float32x4, .offset = offsetof(InstanceData, model_col0), .shaderLocation = 8 },
        { .format = WGPUVertexFormat_Float32x4, .offset = offsetof(InstanceData, model_col1), .shaderLocation = 9 },
        { .format = WGPUVertexFormat_Float32x4, .offset = offsetof(InstanceData, model_col2), .shaderLocation = 10 },
        { .format = WGPUVertexFormat_Float32x4, .offset = offsetof(InstanceData, model_col3), .shaderLocation = 11 },
        { .format = WGPUVertexFormat_Float32x4, .offset = offsetof(InstanceData, color),      .shaderLocation = 12 },
    };

    WGPUVertexBufferLayout layouts[2] = {
        cgfx_mesh_vertex_layout(),
        {
            .arrayStride = sizeof(InstanceData),
            .stepMode = WGPUVertexStepMode_Instance,
            .attributeCount = 5,
            .attributes = instance_attrs,
        },
    };

    WGPURenderPipeline pipeline = cgfx_pipeline_create(&ctx, &(CgfxPipelineDesc){
        .shader = &shader,
        .depth_test = true,
        .cull_mode = WGPUCullMode_Back,
        .vertex_buffer_count = 2,
        .vertex_layouts = layouts,
    });

    CgfxCamera camera = cgfx_camera_create(&ctx, &(CgfxCameraDesc){
        .fovy   = 82.0f,
        .near_z = 0.1f,
        .far_z  = 500.0f,
        .eye    = {-200, -200, -60},
        .center = {0, 0, 0},
    });
    WGPUBindGroup camera_bg = cgfx_bind_group_create_buffers(
        &ctx, &shader, 0, &camera.buffer, 1);

    CgfxMesh cube = create_cube(&ctx, CUBE_SIZE);

    InstanceData *instances = calloc(INSTANCE_COUNT, sizeof(InstanceData));
    float grid_offset = (GRID_SIZE - 1) * GRID_SPACING / 2.0f;

    for (int z = 0; z < GRID_SIZE; z++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            int idx = z * GRID_SIZE + x;
            float px = x * GRID_SPACING - grid_offset;
            float pz = z * GRID_SPACING - grid_offset;

            instances[idx].model_col0[0] = 1; instances[idx].model_col0[1] = 0;
            instances[idx].model_col0[2] = 0; instances[idx].model_col0[3] = 0;
            instances[idx].model_col1[0] = 0; instances[idx].model_col1[1] = 1;
            instances[idx].model_col1[2] = 0; instances[idx].model_col1[3] = 0;
            instances[idx].model_col2[0] = 0; instances[idx].model_col2[1] = 0;
            instances[idx].model_col2[2] = 1; instances[idx].model_col2[3] = 0;
            instances[idx].model_col3[0] = px; instances[idx].model_col3[1] = 0;
            instances[idx].model_col3[2] = pz; instances[idx].model_col3[3] = 1;

            float hue = (float)(x + z) / (GRID_SIZE * 2);
            float sat = 0.6f + 0.4f * ((float)x / GRID_SIZE);
            float val = 0.7f + 0.3f * ((float)z / GRID_SIZE);
            hsv_to_rgb(hue, sat, val,
                       &instances[idx].color[0],
                       &instances[idx].color[1],
                       &instances[idx].color[2]);
            instances[idx].color[3] = 1.0f;
        }
    }

    CgfxBuffer instance_buffer = cgfx_buffer_create_vertex(&ctx,
        instances, INSTANCE_COUNT * sizeof(InstanceData), INSTANCE_COUNT);
    free(instances);

    printf("Rendering %d cubes in a single draw call\n", INSTANCE_COUNT);

    float angle = 0.0f;
    while (cgfx_ctx_is_running(&ctx)) {
        glfwPollEvents();
        angle += 0.005f;

        float radius = 60.0f;
        float cam_x = cosf(angle) * radius;
        float cam_z = sinf(angle) * radius;
        cgfx_camera_look_at(&camera,
            (vec3){cam_x, 90.0f, cam_z},
            (vec3){0, 0, 0},
            (vec3){0, 1, 0});
        cgfx_camera_perspective(&camera, 80.0f,
            (float)ctx.width / (float)ctx.height, 0.1f, 500.0f);
        cgfx_camera_write(&ctx, &camera);

        CgfxFrame frame;
        if (cgfx_frame_begin(&ctx, &frame, (WGPUColor){0.05, 0.05, 0.1, 1.0})) {
            wgpuRenderPassEncoderSetPipeline(frame.render_pass, pipeline);
            cgfx_camera_bind(frame.render_pass, camera_bg, 0);
            wgpuRenderPassEncoderSetVertexBuffer(frame.render_pass, 1,
                instance_buffer.buffer, 0, instance_buffer.size);
            cgfx_mesh_draw_instanced(frame.render_pass, &cube, INSTANCE_COUNT);
            cgfx_frame_end(&ctx, &frame);
        }
    }

    cgfx_buffer_destroy(&instance_buffer);
    cgfx_mesh_destroy(&cube);
    cgfx_bind_group_destroy(camera_bg);
    cgfx_camera_destroy(&camera);
    cgfx_pipeline_destroy(pipeline);
    cgfx_shader_destroy(&shader);
    cgfx_ctx_destroy(&ctx);

    return 0;
}
