/**
 * @file cgfx_camera.c
 * @brief Implementation of camera with GPU-backed projection and view.
 */
#include "cgfx_camera.h"

#define CGFX_CAMERA_GPU_SIZE (2 * sizeof(mat4))


CgfxCamera cgfx_camera_create(const CgfxCtx *ctx,
                               const CgfxShader *shader,
                               const CgfxCameraDesc *desc) {
    float fovy   = desc->fovy   ? desc->fovy   : 45.0f;
    float near_z = desc->near_z ? desc->near_z : 0.01f;
    float far_z  = desc->far_z  ? desc->far_z  : 100.0f;
    float aspect = (float)ctx->width / (float)ctx->height;

    vec3 eye, center, up;
    glm_vec3_copy((float *)desc->eye, eye);
    glm_vec3_copy((float *)desc->center, center);
    if (desc->up[0] == 0.0f && desc->up[1] == 0.0f && desc->up[2] == 0.0f)
        glm_vec3_copy((vec3){0.0f, 1.0f, 0.0f}, up);
    else
        glm_vec3_copy((float *)desc->up, up);

    CgfxCamera cam = {};
    cam.group_index = desc->group_index;

    glm_perspective(glm_rad(fovy), aspect, near_z, far_z, cam.projection);
    glm_lookat(eye, center, up, cam.view);

    cam.buffer = cgfx_buffer_create_uniform(ctx, cam.projection,
                                            CGFX_CAMERA_GPU_SIZE);
    cam.bind_group = cgfx_shader_create_bind_group(ctx, shader,
                                                   desc->group_index,
                                                   &cam.buffer, 1);
    return cam;
}

void cgfx_camera_perspective(CgfxCamera *cam, float fovy_deg,
                             float aspect, float near_z, float far_z) {
    glm_perspective(glm_rad(fovy_deg), aspect, near_z, far_z, cam->projection);
}

void cgfx_camera_look_at(CgfxCamera *cam, vec3 eye, vec3 center, vec3 up) {
    glm_lookat(eye, center, up, cam->view);
}

void cgfx_camera_write(const CgfxCtx *ctx, const CgfxCamera *cam) {
    wgpuQueueWriteBuffer(ctx->queue, cam->buffer.buffer,
                         0, cam->projection, CGFX_CAMERA_GPU_SIZE);
}

void cgfx_camera_bind(WGPURenderPassEncoder pass, const CgfxCamera *cam) {
    wgpuRenderPassEncoderSetBindGroup(pass, cam->group_index,
                                     cam->bind_group, 0, nullptr);
}

void cgfx_camera_destroy(CgfxCamera *cam) {
    if (cam->bind_group)
        wgpuBindGroupRelease(cam->bind_group);
    cgfx_buffer_destroy(&cam->buffer);
    *cam = (CgfxCamera){};
}
