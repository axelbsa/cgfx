/**
 * @file cgfx_shader.h
 * @brief Shader module creation from WGSL source strings and SPIR-V binaries.
 *
 * Hides the verbose WebGPU chained-struct descriptor pattern for creating
 * shader modules. SPIR-V binaries are loaded through the standard WebGPU
 * API via WGPUShaderSourceSPIRV (naga SPIR-V frontend).
 */
#ifndef CGFX_SHADER_H
#define CGFX_SHADER_H

#include <webgpu/webgpu.h>
#include <stddef.h>
#include "cgfx_ctx.h"

/**
 * Create a WebGPU shader module from a WGSL source string.
 *
 * This wraps the WGPUShaderSourceWGSL chained-struct pattern:
 *   1. Creates a WGPUShaderSourceWGSL with the WGSL code
 *   2. Links it into the WGPUShaderModuleDescriptor via the nextInChain pointer
 *   3. Calls wgpuDeviceCreateShaderModule on the context's device
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

/**
 * Create a WebGPU shader module from pre-compiled SPIR-V binary.
 *
 * Uses the standard WebGPU WGPUShaderSourceSPIRV chained struct to load
 * SPIR-V through naga's SPIR-V frontend. The SPIR-V is parsed and
 * converted to the backend's native shader format. Works on all backends
 * (Vulkan, Metal, DX12). No special device feature required.
 *
 * Typical workflow with Slang:
 *   1. Compile .slang to SPIR-V with slangc (one module per entry point)
 *   2. Load the .spv file into memory
 *   3. Call cgfx_shader_create_spirv() for each module
 *   4. Pass to cgfx_pipeline_create() using vertex_shader/fragment_shader
 *
 * Example:
 *   // Load a .spv file into a buffer
 *   uint32_t *spv_data = ...;
 *   size_t spv_size = ...;  // size in bytes, must be multiple of 4
 *   WGPUShaderModule vs = cgfx_shader_create_spirv(&ctx, "vertex", spv_data, spv_size);
 *   wgpuShaderModuleRelease(vs);
 *
 * @param ctx        Initialized context (uses ctx->device).
 * @param label      Human-readable label for GPU debugging tools. May be NULL.
 * @param spirv      Pointer to SPIR-V binary data (array of uint32_t words).
 * @param size_bytes Size of the SPIR-V data in bytes. Must be a multiple of 4.
 * @return           Shader module handle, or NULL on failure.
 *                   Caller must release with wgpuShaderModuleRelease().
 */
WGPUShaderModule cgfx_shader_create_spirv(const CgfxCtx *ctx,
                                           const char *label,
                                           const uint32_t *spirv,
                                           size_t size_bytes);

#endif /* CGFX_SHADER_H */
