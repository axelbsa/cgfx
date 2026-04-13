/**
 * @file cgfx_shader.c
 * @brief Implementation of shader module creation (WGSL and SPIR-V).
 */
#include "cgfx_shader.h"

#include <stdio.h>

#include <webgpu/webgpu.h>


WGPUShaderModule cgfx_shader_create(const CgfxCtx *ctx,
                                     const char *label,
                                     const char *wgsl) {
    WGPUShaderModuleDescriptor shader_desc = {};

    /*
     * WebGPU uses a chained-struct extension mechanism to attach the WGSL
     * source code to the shader module descriptor. The ShaderSourceWGSL is
     * linked via the nextInChain pointer with an sType tag identifying it.
     */
    WGPUShaderSourceWGSL wgsl_desc = {};
    wgsl_desc.chain.next = nullptr;
    wgsl_desc.chain.sType = WGPUSType_ShaderSourceWGSL;
    wgsl_desc.code = (WGPUStringView){ .data = wgsl, .length = WGPU_STRLEN };

    shader_desc.nextInChain = &wgsl_desc.chain;
    shader_desc.label = (WGPUStringView){ .data = label, .length = WGPU_STRLEN };

    return wgpuDeviceCreateShaderModule(ctx->device, &shader_desc);
}


WGPUShaderModule cgfx_shader_create_spirv(const CgfxCtx *ctx,
                                           const char *label,
                                           const uint32_t *spirv,
                                           size_t size_bytes) {
    if (size_bytes % 4 != 0) {
        fprintf(stderr, "[cgfx] SPIR-V size must be a multiple of 4 bytes (got %zu)\n",
                size_bytes);
        return nullptr;
    }

    /*
     * Use the standard WebGPU SPIR-V shader source path. The SPIR-V binary
     * is parsed by naga's SPIR-V frontend and converted to the backend's
     * native shader format. This works on all backends (Vulkan, Metal, DX12).
     *
     * The WGPUShaderSourceSPIRV struct is chained onto the shader module
     * descriptor via nextInChain, same pattern as WGPUShaderSourceWGSL.
     */
    WGPUShaderSourceSPIRV spirv_source = {};
    spirv_source.chain.next = nullptr;
    spirv_source.chain.sType = WGPUSType_ShaderSourceSPIRV;
    spirv_source.codeSize = (uint32_t)(size_bytes / 4);
    spirv_source.code = spirv;

    WGPUShaderModuleDescriptor shader_desc = {};
    shader_desc.nextInChain = &spirv_source.chain;
    shader_desc.label = (WGPUStringView){ .data = label, .length = WGPU_STRLEN };

    return wgpuDeviceCreateShaderModule(ctx->device, &shader_desc);
}
