/**
 * @file cgfx_camera.h
 * @brief Camera module - projection, view, and GPU uniform management.
 *
 * CgfxCamera owns projection and view matrices plus the GPU uniform
 * buffer needed to send them to the shader. The caller creates and
 * owns the bind group, placing the camera buffer wherever they need
 * it (composable with lights, time, etc. in the same group).
 *
 * Usage:
 *   CgfxCamera cam = cgfx_camera_create(&ctx, &(CgfxCameraDesc){
 *       .fovy   = 45.0f,
 *       .eye    = {1.0f, 1.0f, -2.0f},
 *       .center = {0.0f, 0.0f,  0.0f},
 *   });
 *
 *   WGPUBindGroup cam_bg = cgfx_bind_group_create_buffers(&ctx, &shader,
 *                                                          0, &cam.buffer, 1);
 *
 *   // in render loop:
 *   cgfx_camera_write(&ctx, &cam);
 *   cgfx_camera_bind(frame.render_pass, cam_bg, 0);
 */
#ifndef CGFX_CAMERA_H
#define CGFX_CAMERA_H

#include <cglm/cglm.h>
#include "cgfx_export.h"
#include "cgfx_ctx.h"
#include "cgfx_buffer.h"

/**
 * Configuration for creating a camera.
 *
 * Zero-initialize for sensible defaults:
 *   - 45 degree field of view
 *   - Aspect ratio from context window dimensions
 *   - Near plane at 0.01, far plane at 100.0
 *   - Camera at origin, looking at +Z, Y-up
 *
 * fovy is in degrees for readability (converted to radians internally).
 */
typedef struct CgfxCameraDesc {
    float    fovy;         /**< Vertical field of view in degrees. 0 = 45.     */
    float    near_z;       /**< Near clipping plane.              0 = 0.01.    */
    float    far_z;        /**< Far clipping plane.               0 = 100.0.   */
    vec3     eye;          /**< Camera position.                                */
    vec3     center;       /**< Look-at target.                                 */
    vec3     up;           /**< Up direction.        {0,0,0} = {0, 1, 0}.       */
} CgfxCameraDesc;

typedef struct CgfxCamera {
    mat4           projection;   /**< Projection matrix (perspective or ortho). */
    mat4           view;         /**< View matrix (from look_at).              */
    CgfxBuffer     buffer;       /**< GPU uniform buffer for the matrices.     */
    bool           ok;           /**< True if creation succeeded - check before use. */
} CgfxCamera;

/**
 * Create a camera with projection and view matrices and a GPU buffer.
 *
 * The caller is responsible for creating a bind group from cam.buffer
 * using cgfx_bind_group_create_buffers() or cgfx_bind_group_create().
 *
 * @param ctx   Initialized context.
 * @param desc  Camera configuration.
 * @return      A CgfxCamera. Call cgfx_camera_destroy() to release.
 */
CGFX_API CgfxCamera cgfx_camera_create(const CgfxCtx *ctx,
                               const CgfxCameraDesc *desc);

CGFX_API void cgfx_camera_perspective(CgfxCamera *cam, float fovy_deg,
                             float aspect, float near_z, float far_z);

CGFX_API void cgfx_camera_look_at(CgfxCamera *cam, vec3 eye, vec3 center, vec3 up);

CGFX_API void cgfx_camera_write(const CgfxCtx *ctx, const CgfxCamera *cam);

/**
 * Set a camera's bind group on a render pass.
 *
 * @param pass         Active render pass encoder.
 * @param bind_group   Bind group containing the camera buffer.
 * @param group_index  The @group(N) index.
 */
CGFX_API void cgfx_camera_bind(WGPURenderPassEncoder pass,
                       WGPUBindGroup bind_group, uint32_t group_index);

CGFX_API void cgfx_camera_destroy(CgfxCamera *cam);

#endif /* CGFX_CAMERA_H */
