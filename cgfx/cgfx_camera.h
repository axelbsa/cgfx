/**
 * @file cgfx_camera.h
 * @brief Camera module — projection and view matrix management.
 *
 * CgfxCamera holds projection and view matrices. Convenience functions
 * wrap cglm to set up perspective projection and look-at view matrices
 * configured for WebGPU's coordinate system (left-handed, depth [0,1]).
 *
 * Usage:
 *   CgfxCamera cam;
 *   cgfx_camera_perspective(&cam, glm_rad(45.0f), 16.0f/9.0f, 0.01f, 100.0f);
 *   cgfx_camera_look_at(&cam,
 *       (vec3){0, 1, -3}, (vec3){0, 0, 0}, (vec3){0, 1, 0});
 */
#ifndef CGFX_CAMERA_H
#define CGFX_CAMERA_H

#include <cglm/cglm.h>

typedef struct CgfxCamera {
    mat4 projection;
    mat4 view;
} CgfxCamera;

void cgfx_camera_perspective(CgfxCamera *cam, float fovy,
                             float aspect, float near_z, float far_z);

void cgfx_camera_look_at(CgfxCamera *cam, vec3 eye, vec3 center, vec3 up);

#endif /* CGFX_CAMERA_H */
