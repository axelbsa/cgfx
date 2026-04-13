/**
 * @file cgfx_shader.h
 * @brief Shader module creation from WGSL source strings.
 *
 * Hides the verbose WebGPU chained-struct descriptor pattern for creating
 * shader modules. Also handles backend-specific fields (e.g., wgpu-native
 * hint fields) so the caller doesn't need conditional compilation.
 */
#ifndef CGFX_SHADER_H
#define CGFX_SHADER_H

#include <webgpu/webgpu.h>
#include "cgfx_ctx.h"

/**
 * Create a WebGPU shader module from a WGSL source string.
 *
 * This wraps the WGPUShaderModuleWGSLDescriptor chained-struct pattern:
 *   1. Creates a WGPUShaderModuleWGSLDescriptor with the WGSL code
 *   2. Links it into the WGPUShaderModuleDescriptor via the nextInChain pointer
 *   3. Handles backend-specific fields (e.g., WGPU hint count/hints)
 *   4. Calls wgpuDeviceCreateShaderModule on the context's device
 *
 * The returned shader module is typically passed to cgfx_pipeline_create()
 * and then released by the caller when no longer needed.
 *
 * Example:
 *   WGPUShaderModule shader = cgfx_shader_create(&ctx, "my shader", wgsl_code);
 *   // ... use shader to create pipeline ...
 *   wgpuShaderModuleRelease(shader);
 *
 * @param ctx    Initialized context (uses ctx->device).
 * @param label  Human-readable label for GPU debugging tools. May be NULL.
 * @param wgsl   Null-terminated WGSL source code string.
 * @return       Shader module handle, or NULL on failure.
 *               Caller must release with wgpuShaderModuleRelease().
 */
WGPUShaderModule cgfx_shader_create(const CgfxCtx *ctx,
                                     const char *label,
                                     const char *wgsl);

#endif /* CGFX_SHADER_H */
