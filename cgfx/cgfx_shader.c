/**
 * @file cgfx_shader.c
 * @brief Implementation of shader creation, bind group layout building,
 *        and bind group helpers.
 */
#include "cgfx_shader.h"
#include "cgfx_texture.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <webgpu/webgpu.h>
#ifdef WEBGPU_BACKEND_WGPU
#  include <webgpu/wgpu.h>
#endif


static WGPUShaderModule create_module(const CgfxCtx *ctx,
                                      const char *label,
                                      const char *wgsl) {
    WGPUShaderModuleDescriptor shader_desc = {};

#ifdef WEBGPU_BACKEND_WGPU
    shader_desc.hintCount = 0;
    shader_desc.hints = nullptr;
#endif

    WGPUShaderModuleWGSLDescriptor wgsl_desc = {};
    wgsl_desc.chain.next = nullptr;
    wgsl_desc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
    wgsl_desc.code = wgsl;

    shader_desc.nextInChain = &wgsl_desc.chain;
    shader_desc.label = label;

    return wgpuDeviceCreateShaderModule(ctx->device, &shader_desc);
}


/* ── WGSL compilation diagnostics ────────────────────────────────── */

/*
 * wgpuShaderModuleGetCompilationInfo is not implemented in wgpu-native
 * v0.19.x (panics at runtime). The detailed line/column error reporting
 * is disabled until we update wgpu-native. Shader errors still reach
 * stderr through the device uncaptured-error callback.
 *
 * When wgpu-native is updated, re-enable the block below and remove
 * the simple NULL-check fallback.
 */

#if 0 /* requires wgpu-native > v0.19 */
typedef struct {
    bool        done;
    bool        had_error;
    const char *label;
} CgfxCompileInfo;

static void cgfx__on_compilation_info(WGPUCompilationInfoRequestStatus status,
                                      const WGPUCompilationInfo *info,
                                      void *user_data) {
    CgfxCompileInfo *data = user_data;
    data->done = true;

    if (status != WGPUCompilationInfoRequestStatus_Success || !info)
        return;

    for (size_t i = 0; i < info->messageCount; i++) {
        const WGPUCompilationMessage *m = &info->messages[i];
        if (m->type != WGPUCompilationMessageType_Error)
            continue;

        data->had_error = true;
        fprintf(stderr, "[cgfx_shader] WGSL compile error in '%s' at %llu:%llu: %s\n",
                data->label ? data->label : "(unnamed)",
                (unsigned long long)m->lineNum,
                (unsigned long long)m->linePos,
                m->message ? m->message : "");
    }
}

static bool shader_compile_ok(const CgfxCtx *ctx, WGPUShaderModule module,
                              const char *label) {
    (void)ctx;
    if (!module)
        return false;

    CgfxCompileInfo data = { .done = false, .had_error = false, .label = label };
    wgpuShaderModuleGetCompilationInfo(module, &cgfx__on_compilation_info, &data);

#if defined(__EMSCRIPTEN__)
    while (!data.done)
        emscripten_sleep(100);
#elif defined(WEBGPU_BACKEND_WGPU)
    if (!data.done)
        wgpuDevicePoll(ctx->device, true, nullptr);
#endif

    return !data.had_error;
}
#endif

static bool shader_compile_ok(const CgfxCtx *ctx, WGPUShaderModule module,
                              const char *label) {
    (void)ctx;
    if (!module) {
        fprintf(stderr, "[cgfx_shader] Shader module creation failed: '%s'\n",
                label ? label : "(unnamed)");
        return false;
    }
    return true;
}


static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "[cgfx_shader] Failed to open %s\n", path);
        return nullptr;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (len <= 0) {
        fprintf(stderr, "[cgfx_shader] Empty or invalid file: %s\n", path);
        fclose(f);
        return nullptr;
    }

    char *buf = malloc((size_t)len + 1);
    if (!buf) {
        fclose(f);
        return nullptr;
    }

    if (fread(buf, 1, (size_t)len, f) != (size_t)len) {
        fprintf(stderr, "[cgfx_shader] Failed to read %s\n", path);
        free(buf);
        fclose(f);
        return nullptr;
    }

    buf[len] = '\0';
    fclose(f);
    return buf;
}


CgfxShader cgfx_shader_create(const CgfxCtx *ctx,
                               const char *label,
                               const char *wgsl,
                               const CgfxShaderDesc *desc) {
    CgfxShader shader = {};
    shader.module = create_module(ctx, label, wgsl);

    bool compiled = shader_compile_ok(ctx, shader.module, label);

    if (!desc || desc->group_count == 0) {
        shader.ok = compiled;
        return shader;
    }

    shader.group_count = desc->group_count;
    shader.group_layouts = malloc(desc->group_count * sizeof(WGPUBindGroupLayout));

    for (uint32_t i = 0; i < desc->group_count; i++) {
        const CgfxGroupDesc *group = &desc->groups[i];

        WGPUBindGroupLayoutEntry *entries = calloc(group->binding_count,
            sizeof(WGPUBindGroupLayoutEntry));

        for (uint32_t j = 0; j < group->binding_count; j++) {
            const CgfxBindingDesc *b = &group->bindings[j];

            WGPUShaderStageFlags vis = b->visibility;
            if (!vis) {
                switch (b->kind) {
                case CGFX_BINDING_TEXTURE:
                case CGFX_BINDING_SAMPLER:
                    vis = WGPUShaderStage_Fragment;
                    break;
                case CGFX_BINDING_STORAGE_TEXTURE:
                    vis = WGPUShaderStage_Compute;
                    break;
                default:
                    vis = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
                    break;
                }
            }

            entries[j] = (WGPUBindGroupLayoutEntry){
                .binding    = b->binding,
                .visibility = vis,
            };

            switch (b->kind) {
            case CGFX_BINDING_BUFFER:
                entries[j].buffer = (WGPUBufferBindingLayout){
                    .type = b->type ? b->type : WGPUBufferBindingType_Uniform,
                    .minBindingSize = b->min_binding_size,
                };
                break;
            case CGFX_BINDING_TEXTURE:
                entries[j].texture = (WGPUTextureBindingLayout){
                    .sampleType    = b->sample_type    ? b->sample_type    : WGPUTextureSampleType_Float,
                    .viewDimension = b->view_dimension ? b->view_dimension : WGPUTextureViewDimension_2D,
                };
                break;
            case CGFX_BINDING_SAMPLER:
                entries[j].sampler = (WGPUSamplerBindingLayout){
                    .type = WGPUSamplerBindingType_Filtering,
                };
                break;
            case CGFX_BINDING_STORAGE_TEXTURE:
                entries[j].storageTexture = (WGPUStorageTextureBindingLayout){
                    .access        = b->storage_access ? b->storage_access : WGPUStorageTextureAccess_WriteOnly,
                    .format        = b->storage_format,
                    .viewDimension = b->view_dimension ? b->view_dimension : WGPUTextureViewDimension_2D,
                };
                break;
            }
        }

        WGPUBindGroupLayoutDescriptor layout_desc = {
            .entryCount = group->binding_count,
            .entries = entries,
        };
        shader.group_layouts[i] = wgpuDeviceCreateBindGroupLayout(
            ctx->device, &layout_desc);
        free(entries);
    }

    WGPUPipelineLayoutDescriptor pl_desc = {
        .bindGroupLayoutCount = desc->group_count,
        .bindGroupLayouts = shader.group_layouts,
    };
    shader.pipeline_layout = wgpuDeviceCreatePipelineLayout(ctx->device, &pl_desc);

    bool layouts_ok = (shader.pipeline_layout != nullptr);
    for (uint32_t i = 0; i < shader.group_count; i++) {
        if (!shader.group_layouts[i])
            layouts_ok = false;
    }
    shader.ok = compiled && layouts_ok;

    return shader;
}


CgfxShader cgfx_shader_create_from_file(const CgfxCtx *ctx,
                                         const char *label,
                                         const char *path,
                                         const CgfxShaderDesc *desc) {
    char *wgsl = read_file(path);
    if (!wgsl)
        return (CgfxShader){};

    CgfxShader shader = cgfx_shader_create(ctx, label, wgsl, desc);
    free(wgsl);
    return shader;
}


WGPUBindGroup cgfx_bind_group_create_buffers(const CgfxCtx *ctx,
                                            const CgfxShader *shader,
                                            uint32_t group_index,
                                            const CgfxBuffer *buffers,
                                            uint32_t buffer_count) {
    if (group_index >= shader->group_count) {
        fprintf(stderr, "[cgfx_shader] bind group index %u out of range "
                "(shader has %u groups)\n", group_index, shader->group_count);
        return nullptr;
    }

    WGPUBindGroupEntry *entries = calloc(buffer_count, sizeof(WGPUBindGroupEntry));
    for (uint32_t i = 0; i < buffer_count; i++) {
        entries[i] = (WGPUBindGroupEntry){
            .binding = i,
            .buffer = buffers[i].buffer,
            .offset = 0,
            .size = buffers[i].size,
        };
    }

    WGPUBindGroupDescriptor bg_desc = {
        .layout = shader->group_layouts[group_index],
        .entryCount = buffer_count,
        .entries = entries,
    };

    WGPUBindGroup group = wgpuDeviceCreateBindGroup(ctx->device, &bg_desc);
    free(entries);
    return group;
}


WGPUBindGroup cgfx_bind_group_create(const CgfxCtx *ctx,
                                     const CgfxShader *shader,
                                     uint32_t group_index,
                                     const CgfxBindGroupEntry *entries,
                                     uint32_t entry_count) {
    if (group_index >= shader->group_count) {
        fprintf(stderr, "[cgfx_shader] bind group index %u out of range "
                "(shader has %u groups)\n", group_index, shader->group_count);
        return nullptr;
    }

    WGPUBindGroupEntry *bg_entries = calloc(entry_count, sizeof(WGPUBindGroupEntry));
    for (uint32_t i = 0; i < entry_count; i++) {
        bg_entries[i].binding = entries[i].binding;

        if (entries[i].buffer) {
            bg_entries[i].buffer = entries[i].buffer->buffer;
            bg_entries[i].offset = 0;
            bg_entries[i].size   = entries[i].buffer->size;
        } else if (entries[i].texture) {
            bg_entries[i].textureView = entries[i].texture->view;
        } else if (entries[i].sampler) {
            bg_entries[i].sampler = entries[i].sampler;
        }
    }

    WGPUBindGroupDescriptor bg_desc = {
        .layout     = shader->group_layouts[group_index],
        .entryCount = entry_count,
        .entries    = bg_entries,
    };

    WGPUBindGroup group = wgpuDeviceCreateBindGroup(ctx->device, &bg_desc);
    free(bg_entries);
    return group;
}


void cgfx_bind_group_destroy(WGPUBindGroup group) {
    if (group)
        wgpuBindGroupRelease(group);
}


void cgfx_shader_bind(WGPURenderPassEncoder pass,
                       const WGPUBindGroup *groups,
                       uint32_t group_count) {
    for (uint32_t i = 0; i < group_count; i++) {
        wgpuRenderPassEncoderSetBindGroup(pass, i, groups[i], 0, nullptr);
    }
}


void cgfx_shader_bind_compute(WGPUComputePassEncoder pass,
                               const WGPUBindGroup *groups,
                               uint32_t group_count) {
    for (uint32_t i = 0; i < group_count; i++) {
        wgpuComputePassEncoderSetBindGroup(pass, i, groups[i], 0, nullptr);
    }
}


void cgfx_shader_destroy(CgfxShader *shader) {
    if (shader->module)
        wgpuShaderModuleRelease(shader->module);

    if (shader->pipeline_layout)
        wgpuPipelineLayoutRelease(shader->pipeline_layout);

    for (uint32_t i = 0; i < shader->group_count; i++) {
        if (shader->group_layouts[i])
            wgpuBindGroupLayoutRelease(shader->group_layouts[i]);
    }
    free(shader->group_layouts);

    *shader = (CgfxShader){};
}
