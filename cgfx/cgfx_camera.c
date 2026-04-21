/**
 * @file cgfx_camera.c
 * @brief Implementation of camera projection and view helpers.
 */
#include "cgfx_camera.h"


void cgfx_camera_perspective(CgfxCamera *cam, float fovy,
                             float aspect, float near_z, float far_z) {
    glm_perspective(fovy, aspect, near_z, far_z, cam->projection);
}

void cgfx_camera_look_at(CgfxCamera *cam, vec3 eye, vec3 center, vec3 up) {
    glm_lookat(eye, center, up, cam->view);
}
