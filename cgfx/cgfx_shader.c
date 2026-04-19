/**
 * @file cgfx_shader.c
 * @brief Implementation of shader creation, bind group layout building,
 *        and bind group helpers.
 */
#include "cgfx_shader.h"

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

    if (!desc || desc->group_count == 0)
        return shader;

    shader.group_count = desc->group_count;
    shader.group_layouts = malloc(desc->group_count * sizeof(WGPUBindGroupLayout));

    for (uint32_t i = 0; i < desc->group_count; i++) {
        const CgfxGroupDesc *group = &desc->groups[i];

        WGPUBindGroupLayoutEntry *entries = calloc(group->binding_count,
            sizeof(WGPUBindGroupLayoutEntry));

        for (uint32_t j = 0; j < group->binding_count; j++) {
            const CgfxBindingDesc *b = &group->bindings[j];
            entries[j] = (WGPUBindGroupLayoutEntry){
                .binding = b->binding,
                .visibility = b->visibility
                    ? b->visibility
                    : (WGPUShaderStage_Vertex | WGPUShaderStage_Fragment),
                .buffer = {
                    .type = b->type ? b->type : WGPUBufferBindingType_Uniform,
                    .minBindingSize = b->min_binding_size,
                },
            };
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


WGPUBindGroup cgfx_shader_create_bind_group(const CgfxCtx *ctx,
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


void cgfx_shader_bind(WGPURenderPassEncoder pass,
                       const WGPUBindGroup *groups,
                       uint32_t group_count) {
    for (uint32_t i = 0; i < group_count; i++) {
        wgpuRenderPassEncoderSetBindGroup(pass, i, groups[i], 0, nullptr);
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
