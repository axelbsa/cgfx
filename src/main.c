#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "utils.h"

#include <webgpu/webgpu.h>
#ifdef WEBGPU_BACKEND_WGPU
#  include <webgpu/wgpu.h>
#endif // WEBGPU_BACKEND_WGPU

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>


#define WIDTH 1920
#define HEIGHT 1080


GLFWwindow *window;
WGPUDevice device;
WGPUQueue queue;
WGPUSurface surface;
WGPUTextureFormat surfaceFormat = WGPUTextureFormat_Undefined;
WGPURenderPipeline pipeline;


const char* shaderSource = 
"@vertex                                                                                    \n"
"fn vs_main(@builtin(vertex_index) in_vertex_index: u32) -> @builtin(position) vec4f {      \n"
"	var p = vec2f(0.0, 0.0);                                                                \n"
"	if (in_vertex_index == 0u) {                                                            \n"
"		p = vec2f(-0.5, -0.5);                                                              \n"
"	} else if (in_vertex_index == 1u) {                                                     \n"
"		p = vec2f(0.5, -0.5);                                                               \n"
"	} else {                                                                                \n"
"		p = vec2f(0.0, 0.5);                                                                \n"
"	}                                                                                       \n"
"	return vec4f(p, 0.0, 1.0);                                                              \n"
"}                                                                                          \n"
""
"@fragment                                                                                  \n"
"fn fs_main() -> @location(0) vec4f {                                                       \n"
"	return vec4f(0.8, 0.4, 1.0, 1.0);                                                       \n"
"}                                                                                          ";

void deviceLostCallback(WGPUDeviceLostReason reason, char const* message, void* /* pUserData */) {
    fprintf(stderr, "Device lost: reason %d", reason);
    if (strlen(message) > 0)
        fprintf(stderr, " (%s)", message);
    fprintf(stderr, "\n");
}


void onDeviceError(WGPUErrorType type, char const* message, void* /* pUserData */) {
    fprintf(stderr, "Uncaptured device error: type %d", type);
    if (message) 
        fprintf(stderr, " (%s)", message);
    fprintf(stderr, "\n");
}


WGPUTextureView GetNextSurfaceTextureView() {
	// Get the surface texture
	WGPUSurfaceTexture surfaceTexture;
	wgpuSurfaceGetCurrentTexture(surface, &surfaceTexture);
	if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_Success) {
		return nullptr;
	}

	// Create a view for this surface texture
	WGPUTextureViewDescriptor viewDescriptor;
	viewDescriptor.nextInChain = nullptr;
	viewDescriptor.label = "Surface texture view";
	viewDescriptor.format = wgpuTextureGetFormat(surfaceTexture.texture);
	viewDescriptor.dimension = WGPUTextureViewDimension_2D;
	viewDescriptor.baseMipLevel = 0;
	viewDescriptor.mipLevelCount = 1;
	viewDescriptor.baseArrayLayer = 0;
	viewDescriptor.arrayLayerCount = 1;
	viewDescriptor.aspect = WGPUTextureAspect_All;
	WGPUTextureView targetView = wgpuTextureCreateView(surfaceTexture.texture, &viewDescriptor);

#ifndef WEBGPU_BACKEND_WGPU
	// We no longer need the texture, only its view
	// (NB: with wgpu-native, surface textures must not be manually released)
	wgpuTextureRelease(surfaceTexture.texture);
#endif // WEBGPU_BACKEND_WGPU

	return targetView;
}


void terminate() {
	// Unconfigure the surface
	wgpuRenderPipelineRelease(pipeline);
	wgpuSurfaceUnconfigure(surface);
	wgpuQueueRelease(queue);
	wgpuSurfaceRelease(surface);
	wgpuDeviceRelease(device);
	glfwDestroyWindow(window);
	glfwTerminate();
}

void main_loop() {
	glfwPollEvents();

	// Get the next target texture view
	WGPUTextureView targetView = GetNextSurfaceTextureView();

	if (!targetView) 
        return;

	// Create a command encoder for the draw call
	WGPUCommandEncoderDescriptor encoderDesc = {};
	encoderDesc.nextInChain = nullptr;
	encoderDesc.label = "My command encoder";
	WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &encoderDesc);

	// Create the render pass that clears the screen with our color
	WGPURenderPassDescriptor renderPassDesc = {};
	renderPassDesc.nextInChain = nullptr;

	// The attachment part of the render pass descriptor describes the target texture of the pass
	WGPURenderPassColorAttachment renderPassColorAttachment = {};
	renderPassColorAttachment.view = targetView;
	renderPassColorAttachment.resolveTarget = nullptr;
	renderPassColorAttachment.loadOp = WGPULoadOp_Clear;
	renderPassColorAttachment.storeOp = WGPUStoreOp_Store;

    WGPUColor c = {0.1, 0.1, 0.2, 1.0};
	renderPassColorAttachment.clearValue = c;

#ifndef WEBGPU_BACKEND_WGPU
	renderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif // NOT WEBGPU_BACKEND_WGPU

	renderPassDesc.colorAttachmentCount = 1;
	renderPassDesc.colorAttachments = &renderPassColorAttachment;
	renderPassDesc.depthStencilAttachment = nullptr;
	renderPassDesc.timestampWrites = nullptr;

	WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);

	// Select which render pipeline to use
	wgpuRenderPassEncoderSetPipeline(renderPass, pipeline);
	// Draw 1 instance of a 3-vertices shape
	wgpuRenderPassEncoderDraw(renderPass, 3, 1, 0, 0);

	wgpuRenderPassEncoderEnd(renderPass);
	wgpuRenderPassEncoderRelease(renderPass);

	// Encode and submit the render pass
	WGPUCommandBufferDescriptor cmdBufferDescriptor = {};
	cmdBufferDescriptor.nextInChain = nullptr;
	cmdBufferDescriptor.label = "Command buffer";
	WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &cmdBufferDescriptor);
	wgpuCommandEncoderRelease(encoder);

    // print submitting command
	wgpuQueueSubmit(queue, 1, &command);
	wgpuCommandBufferRelease(command);
    // print command submitted

	// At the enc of the frame
	wgpuTextureViewRelease(targetView);
#ifndef __EMSCRIPTEN__
	wgpuSurfacePresent(surface);
#endif

#if defined(WEBGPU_BACKEND_DAWN)
	wgpuDeviceTick(device);
#elif defined(WEBGPU_BACKEND_WGPU)
	wgpuDevicePoll(device, false, nullptr);
#endif

}

bool is_running() {
	return !glfwWindowShouldClose(window);
}


void initialize_pipeline() {
	WGPUShaderModuleDescriptor shaderDesc = {};
#ifdef WEBGPU_BACKEND_WGPU
	shaderDesc.hintCount = 0;
	shaderDesc.hints = nullptr;
#endif

	// We use the extension mechanism to specify the WGSL part of the shader module descriptor
	WGPUShaderModuleWGSLDescriptor shaderCodeDesc = {};

	// Set the chained struct's header
	shaderCodeDesc.chain.next = nullptr;
	shaderCodeDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;

	// Connect the chain
	shaderDesc.nextInChain = &shaderCodeDesc.chain;
	shaderCodeDesc.code = shaderSource;
	WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(device, &shaderDesc);

    // Debug to check the shader source
    fprintf(stderr, "%s\n", shaderSource);

	// Create the render pipeline
	WGPURenderPipelineDescriptor pipelineDesc = {};
	pipelineDesc.nextInChain = nullptr;

	// We do not use any vertex buffer for this first simplistic example
	pipelineDesc.vertex.bufferCount = 0;
	pipelineDesc.vertex.buffers = nullptr;

	// NB: We define the 'shaderModule' in the second part of this chapter.
	// Here we tell that the programmable vertex shader stage is described
	// by the function called 'vs_main' in that module.
	pipelineDesc.vertex.module = shaderModule;
	pipelineDesc.vertex.entryPoint = "vs_main";
	pipelineDesc.vertex.constantCount = 0;
	pipelineDesc.vertex.constants = nullptr;

	// Each sequence of 3 vertices is considered as a triangle
	pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
	
	// We'll see later how to specify the order in which vertices should be
	// connected. When not specified, vertices are considered sequentially.
	pipelineDesc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
	
	// The face orientation is defined by assuming that when looking
	// from the front of the face, its corner vertices are enumerated
	// in the counter-clockwise (CCW) order.
	pipelineDesc.primitive.frontFace = WGPUFrontFace_CCW;
	
	// But the face orientation does not matter much because we do not
	// cull (i.e. "hide") the faces pointing away from us (which is often
	// used for optimization).
	pipelineDesc.primitive.cullMode = WGPUCullMode_None;

	// We tell that the programmable fragment shader stage is described
	// by the function called 'fs_main' in the shader module.
	WGPUFragmentState fragmentState = {};
	fragmentState.module = shaderModule;
	fragmentState.entryPoint = "fs_main";
	fragmentState.constantCount = 0;
	fragmentState.constants = nullptr;

	WGPUBlendState blendState = {};
	blendState.color.srcFactor = WGPUBlendFactor_SrcAlpha;
	blendState.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
	blendState.color.operation = WGPUBlendOperation_Add;
	blendState.alpha.srcFactor = WGPUBlendFactor_Zero;
	blendState.alpha.dstFactor = WGPUBlendFactor_One;
	blendState.alpha.operation = WGPUBlendOperation_Add;
	
	WGPUColorTargetState colorTarget = {};
	colorTarget.format = surfaceFormat;
	colorTarget.blend = &blendState;
	colorTarget.writeMask = WGPUColorWriteMask_All; // We could write to only some of the color channels.

	// We have only one target because our render pass has only one output color
	// attachment.
	fragmentState.targetCount = 1;
	fragmentState.targets = &colorTarget;
	pipelineDesc.fragment = &fragmentState;

	// We do not use stencil/depth testing for now
	pipelineDesc.depthStencil = nullptr;

	// Samples per pixel
	pipelineDesc.multisample.count = 1;

	// Default value for the mask, meaning "all bits on"
	pipelineDesc.multisample.mask = ~0u;

	// Default value as well (irrelevant for count = 1 anyways)
	pipelineDesc.multisample.alphaToCoverageEnabled = false;

	pipelineDesc.layout = nullptr;

	pipeline = wgpuDeviceCreateRenderPipeline(device, &pipelineDesc);

	// We no longer need to access the shader module
	wgpuShaderModuleRelease(shaderModule);
}

bool initialize() {
	WGPUInstanceDescriptor desc = {};
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	window = glfwCreateWindow(WIDTH, HEIGHT, "r", nullptr, nullptr);

	WGPUInstance instance = wgpuCreateInstance(&desc);

	fprintf(stderr, "Requesting adapter...\n");
	surface = glfwGetWGPUSurface(instance, window);
	WGPURequestAdapterOptions adapterOpts = {};
	adapterOpts.nextInChain = nullptr;
	adapterOpts.compatibleSurface = surface;
	WGPUAdapter adapter = requestAdapterSync(instance, &adapterOpts);
	fprintf(stderr, "Got adapter: %p\n", (void *)&adapter);

	wgpuInstanceRelease(instance);

	fprintf(stderr, "Requesting device...\n");
	WGPUDeviceDescriptor deviceDesc = {};
	deviceDesc.nextInChain = nullptr;
	deviceDesc.label = "My Device";
	deviceDesc.requiredFeatureCount = 0;
	deviceDesc.requiredLimits = nullptr;
	deviceDesc.defaultQueue.nextInChain = nullptr;
	deviceDesc.defaultQueue.label = "The default queue";
	deviceDesc.deviceLostCallback = &deviceLostCallback;
	device = requestDeviceSync(adapter, &deviceDesc);
	fprintf(stderr, "Got device: %p\n", (void *)device);

	wgpuDeviceSetUncapturedErrorCallback(device, &onDeviceError, nullptr /* pUserData */);

	queue = wgpuDeviceGetQueue(device);

	// Configure the surface
	WGPUSurfaceConfiguration config = {};
	config.nextInChain = nullptr;

	// Configuration of the textures created for the underlying swap chain
	config.width = WIDTH;
	config.height = HEIGHT;
	config.usage = WGPUTextureUsage_RenderAttachment;
	surfaceFormat = wgpuSurfaceGetPreferredFormat(surface, adapter);
	config.format = surfaceFormat;

	// And we do not need any particular view format:
	config.viewFormatCount = 0;
	config.viewFormats = nullptr;
	config.device = device;
	config.presentMode = WGPUPresentMode_Fifo;
	config.alphaMode = WGPUCompositeAlphaMode_Auto;

	wgpuSurfaceConfigure(surface, &config);

	// Release the adapter only after it has been fully utilized
	wgpuAdapterRelease(adapter);

    initialize_pipeline();

    return true;

}

int main() {
    initialize(); 

    while (is_running()) {
        main_loop();
    }

    terminate();

    return 0;
}
