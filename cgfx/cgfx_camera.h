/**
 * @file cgfx_camera.h
 * @brief Camera module — projection, view, and GPU uniform management.
 *
 * CgfxCamera owns projection and view matrices plus the GPU uniform
 * buffer and bind group needed to send them to the shader.
 *
 * Usage:
 *   CgfxCamera cam = cgfx_camera_create(&ctx, &shader, &(CgfxCameraDesc){
 *       .fovy   = 45.0f,
 *       .eye    = {1.0f, 1.0f, -2.0f},
 *       .center = {0.0f, 0.0f,  0.0f},
 *   });
 *
 *   // in render loop:
 *   cgfx_camera_write(&ctx, &cam);
 *   cgfx_camera_bind(frame.render_pass, &cam);
 */
#ifndef CGFX_CAMERA_H
#define CGFX_CAMERA_H

#include <cglm/cglm.h>
#include "cgfx_export.h"
#include "cgfx_ctx.h"
#include "cgfx_buffer.h"
#include "cgfx_shader.h"

/**
 * Configuration for creating a camera.
 *
 * Zero-initialize for sensible defaults:
 *   - 45 degree field of view
 *   - Aspect ratio from context window dimensions
 *   - Near plane at 0.01, far plane at 100.0
 *   - Camera at origin, looking at +Z, Y-up
 *   - Bound to @group(0)
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
    uint32_t group_index;  /**< Bind group index.                 0 = @group(0).*/
} CgfxCameraDesc;

typedef struct CgfxCamera {
    mat4           projection;   /**< Projection matrix (perspective or ortho). */
    mat4           view;         /**< View matrix (from look_at).              */
    CgfxBuffer     buffer;       /**< GPU uniform buffer for the matrices.     */
    WGPUBindGroup  bind_group;   /**< Bind group referencing the buffer.       */
    uint32_t       group_index;  /**< The @group(N) this camera binds to.      */
    bool           ok;           /**< True if creation succeeded — check before use. */
} CgfxCamera;

CGFX_API CgfxCamera cgfx_camera_create(const CgfxCtx *ctx,
                               const CgfxShader *shader,
                               const CgfxCameraDesc *desc);

CGFX_API void cgfx_camera_perspective(CgfxCamera *cam, float fovy_deg,
                             float aspect, float near_z, float far_z);

CGFX_API void cgfx_camera_look_at(CgfxCamera *cam, vec3 eye, vec3 center, vec3 up);

CGFX_API void cgfx_camera_write(const CgfxCtx *ctx, const CgfxCamera *cam);

CGFX_API void cgfx_camera_bind(WGPURenderPassEncoder pass, const CgfxCamera *cam);

CGFX_API void cgfx_camera_destroy(CgfxCamera *cam);

#endif /* CGFX_CAMERA_H */
