/**
 * @file cgfx_shader.c
 * @brief Implementation of shader module creation.
 */
#include "cgfx_shader.h"

#include <webgpu/webgpu.h>
#ifdef WEBGPU_BACKEND_WGPU
#  include <webgpu/wgpu.h>
#endif


WGPUShaderModule cgfx_shader_create(const CgfxCtx *ctx,
                                     const char *label,
                                     const char *wgsl) {
    WGPUShaderModuleDescriptor shader_desc = {};

#ifdef WEBGPU_BACKEND_WGPU
    /* wgpu-native requires hint fields to be explicitly set */
    shader_desc.hintCount = 0;
    shader_desc.hints = nullptr;
#endif

    /*
     * WebGPU uses a chained-struct extension mechanism to attach the WGSL
     * source code to the shader module descriptor. The WGSLDescriptor is
     * linked via the nextInChain pointer with an sType tag identifying it.
     */
    WGPUShaderModuleWGSLDescriptor wgsl_desc = {};
    wgsl_desc.chain.next = nullptr;
    wgsl_desc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
    wgsl_desc.code = wgsl;

    shader_desc.nextInChain = &wgsl_desc.chain;
    shader_desc.label = label;

    return wgpuDeviceCreateShaderModule(ctx->device, &shader_desc);
}
