# WebGPU API Translation Guide

**Elie Michel's tutorial (wgpu v0.19.4.1) → wgpu-native v29.0.0.0**

This document maps every API difference you will encounter while following
[Elie Michel's Learn WebGPU for C++](https://eliemichel.github.io/LearnWebGPU)
tutorial against the wgpu-native v29 headers shipping with this project.

The tutorial is excellent and free — this guide just bridges the version gap so
you can follow along without guessing what changed.

---

## Quick-Reference Cheat Sheet

If you just want to get code compiling, these are the most common things that
changed. Details and rationale for each are in the sections below.

```c
// ── 1. Strings are now WGPUStringView, not char* ──────────────────
// OLD:
desc.label = "my label";
// NEW:
desc.label = (WGPUStringView){ .data = "my label", .length = WGPU_STRLEN };

// ── 2. Shader source struct renamed ────────────────────────────────
// OLD:
WGPUShaderModuleWGSLDescriptor wgsl_desc;
wgsl_desc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
wgsl_desc.code = "...";
// NEW:
WGPUShaderSourceWGSL wgsl_desc;
wgsl_desc.chain.sType = WGPUSType_ShaderSourceWGSL;
wgsl_desc.code = (WGPUStringView){ .data = "...", .length = WGPU_STRLEN };

// ── 3. Async callbacks use CallbackInfo structs ────────────────────
// OLD:
wgpuInstanceRequestAdapter(instance, &opts, myCallback, userData);
// NEW:
WGPURequestAdapterCallbackInfo cb = {
    .mode = WGPUCallbackMode_AllowSpontaneous,
    .callback = myCallback,   // signature changed too — see below
    .userdata1 = userData,
};
wgpuInstanceRequestAdapter(instance, &opts, cb);

// ── 4. Surface preferred format → capabilities ────────────────────
// OLD:
format = wgpuSurfaceGetPreferredFormat(surface, adapter);
// NEW:
WGPUSurfaceCapabilities caps = {};
wgpuSurfaceGetCapabilities(surface, adapter, &caps);
format = caps.formats[0];   // first format = preferred

// ── 5. Error/device-lost callbacks moved into DeviceDescriptor ─────
// OLD:
device_desc.deviceLostCallback = myLostCb;
wgpuDeviceSetUncapturedErrorCallback(device, myErrCb, NULL);
// NEW:
device_desc.deviceLostCallbackInfo = (WGPUDeviceLostCallbackInfo){
    .mode = WGPUCallbackMode_AllowSpontaneous,
    .callback = myLostCb,     // signature changed — see below
};
device_desc.uncapturedErrorCallbackInfo = (WGPUUncapturedErrorCallbackInfo){
    .callback = myErrCb,      // signature changed — see below
};
```

---

## Table of Contents

1. [Strings: `char*` → `WGPUStringView`](#1-strings)
2. [Callback Signatures](#2-callbacks)
3. [Async Request Pattern (CallbackInfo + Future)](#3-async-requests)
4. [Shader Module Creation](#4-shader-modules)
5. [Surface & Preferred Format](#5-surface)
6. [Device Descriptor Changes](#6-device-descriptor)
7. [Frame & Render Pass Changes](#7-frame)
8. [Buffer API Changes](#8-buffers)
9. [Pipeline Changes](#9-pipelines)
10. [Adapter Inspection](#10-adapter)
11. [Reference Counting](#11-refcounting)
12. [Surface Creation (glfw3webgpu)](#12-surface-creation)
13. [Removed / Renamed Types](#13-renames)
14. [wgpu-native Extension (wgpu.h) Changes](#14-wgpu-ext)
15. [New Constants & Macros](#15-constants)

---

## 1. Strings: `char*` → `WGPUStringView` {#1-strings}

The single most pervasive change. Every `char const *` in the old API is now
`WGPUStringView`:

```c
typedef struct WGPUStringView {
    char const *data;
    size_t      length;
} WGPUStringView;
```

`WGPU_STRLEN` (`SIZE_MAX`) is a sentinel that means *"data is null-terminated,
compute the length yourself"*, so for string literals the conversion is
mechanical:

```c
// Tutorial code:
desc.label = "hello";

// What you write instead:
desc.label = (WGPUStringView){ .data = "hello", .length = WGPU_STRLEN };
```

This applies to **every** descriptor's `.label` field, entry point names,
shader source code, and any other string parameter.

**Tip:** If you want a small helper to reduce verbosity:

```c
#define SV(s) ((WGPUStringView){ .data = (s), .length = WGPU_STRLEN })
// then:
desc.label = SV("hello");
```

---

## 2. Callback Signatures {#2-callbacks}

Every callback in the API changed in two ways:

1. **`char const *message`** → **`WGPUStringView message`**
2. **Single `void *userdata`** → **`void *userdata1, void *userdata2`**

Some callbacks also gained a leading `WGPUDevice const *device` parameter.

### Adapter request callback

```c
// OLD
void onAdapter(WGPURequestAdapterStatus status,
               WGPUAdapter adapter,
               char const *message,
               void *userdata);

// NEW
void onAdapter(WGPURequestAdapterStatus status,
               WGPUAdapter adapter,
               WGPUStringView message,
               void *userdata1,
               void *userdata2);
```

### Device request callback

```c
// OLD
void onDevice(WGPURequestDeviceStatus status,
              WGPUDevice device,
              char const *message,
              void *userdata);

// NEW
void onDevice(WGPURequestDeviceStatus status,
              WGPUDevice device,
              WGPUStringView message,
              void *userdata1,
              void *userdata2);
```

### Device lost callback

```c
// OLD
void onDeviceLost(WGPUDeviceLostReason reason,
                  char const *message,
                  void *userdata);

// NEW
void onDeviceLost(WGPUDevice const *device,
                  WGPUDeviceLostReason reason,
                  WGPUStringView message,
                  void *userdata1,
                  void *userdata2);
```

### Uncaptured error callback

```c
// OLD
void onError(WGPUErrorType type,
             char const *message,
             void *userdata);

// NEW
void onError(WGPUDevice const *device,
             WGPUErrorType type,
             WGPUStringView message,
             void *userdata1,
             void *userdata2);
```

### Buffer map callback

```c
// OLD
void onBufferMapped(WGPUBufferMapAsyncStatus status,
                    void *userdata);

// NEW
void onBufferMapped(WGPUMapAsyncStatus status,
                    WGPUStringView message,
                    void *userdata1,
                    void *userdata2);
```

Note: the status enum was also renamed from `WGPUBufferMapAsyncStatus` to
`WGPUMapAsyncStatus` (values changed — see [section 13](#13-renames)).

### Printing messages from callbacks

Since messages are now `WGPUStringView` (not necessarily null-terminated), use
`%.*s` instead of `%s`:

```c
// OLD
fprintf(stderr, "Error: %s\n", message);

// NEW
fprintf(stderr, "Error: %.*s\n", (int)message.length, message.data);
```

---

## 3. Async Request Pattern (CallbackInfo + Future) {#3-async-requests}

The tutorial teaches this pattern for requesting an adapter:

```c
// OLD — callback + userdata passed as separate arguments
wgpuInstanceRequestAdapter(instance, &options, onAdapterReady, &userData);
```

The new API wraps the callback into a **CallbackInfo struct** and returns a
**WGPUFuture**:

```c
// NEW — CallbackInfo struct
WGPURequestAdapterCallbackInfo callbackInfo = {
    .nextInChain = NULL,
    .mode        = WGPUCallbackMode_AllowSpontaneous,
    .callback    = onAdapterReady,
    .userdata1   = &userData,
    .userdata2   = NULL,
};
WGPUFuture future = wgpuInstanceRequestAdapter(instance, &options, callbackInfo);
```

The same pattern applies to all async operations:

| Operation | Old call | New CallbackInfo type |
|-----------|----------|-----------------------|
| Request adapter | `wgpuInstanceRequestAdapter(inst, opts, cb, ud)` | `WGPURequestAdapterCallbackInfo` |
| Request device | `wgpuAdapterRequestDevice(adapter, desc, cb, ud)` | `WGPURequestDeviceCallbackInfo` |
| Map buffer | `wgpuBufferMapAsync(buf, mode, off, sz, cb, ud)` | `WGPUBufferMapCallbackInfo` |
| Queue work done | `wgpuQueueOnSubmittedWorkDone(q, cb, ud)` | `WGPUQueueWorkDoneCallbackInfo` |
| Pop error scope | `wgpuDevicePopErrorScope(dev, cb, ud)` | `WGPUPopErrorScopeCallbackInfo` |
| Shader compile info | `wgpuShaderModuleGetCompilationInfo(m, cb, ud)` | `WGPUCompilationInfoCallbackInfo` |

### WGPUCallbackMode

Each CallbackInfo needs a `mode`. For the tutorial's synchronous wrappers
(where the callback fires immediately on native), use:

```c
.mode = WGPUCallbackMode_AllowSpontaneous
```

The three modes are:

| Mode | When the callback fires |
|------|------------------------|
| `WGPUCallbackMode_WaitAnyOnly` | Only when you explicitly call `wgpuInstanceWaitAny` |
| `WGPUCallbackMode_AllowProcessEvents` | During `WaitAny` or `ProcessEvents` |
| `WGPUCallbackMode_AllowSpontaneous` | Any time, including immediately during the request call |

For the tutorial's pattern of "request adapter, callback fires inline", use
`AllowSpontaneous`.

---

## 4. Shader Module Creation {#4-shader-modules}

### Struct rename

```c
// OLD
WGPUShaderModuleWGSLDescriptor wgsl_desc = {};
wgsl_desc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
wgsl_desc.code = shaderSource;  // char*

// NEW
WGPUShaderSourceWGSL wgsl_desc = {};
wgsl_desc.chain.sType = WGPUSType_ShaderSourceWGSL;
wgsl_desc.code = (WGPUStringView){ .data = shaderSource, .length = WGPU_STRLEN };
```

### ShaderModuleDescriptor simplified

The `hintCount` and `hints` fields were removed from
`WGPUShaderModuleDescriptor`. If the tutorial sets them, just delete those
lines:

```c
// OLD — delete these two lines
shader_desc.hintCount = 0;
shader_desc.hints = NULL;
```

### SPIR-V (if used)

```
WGPUShaderModuleSPIRVDescriptor  →  WGPUShaderSourceSPIRV
WGPUSType_ShaderModuleSPIRVDescriptor  →  WGPUSType_ShaderSourceSPIRV
```

---

## 5. Surface & Preferred Format {#5-surface}

### `wgpuSurfaceGetPreferredFormat` — REMOVED

The tutorial uses this one-liner to get the ideal format:

```c
// OLD
WGPUTextureFormat format = wgpuSurfaceGetPreferredFormat(surface, adapter);
```

This function no longer exists. Replace with:

```c
// NEW
WGPUSurfaceCapabilities caps = {};
wgpuSurfaceGetCapabilities(surface, adapter, &caps);
WGPUTextureFormat format = caps.formats[0];  // first = preferred
```

`WGPUSurfaceCapabilities` also gives you the list of supported present modes
and alpha modes, which can be useful.

### Surface texture status

```c
// OLD
if (surface_texture.status != WGPUSurfaceGetCurrentTextureStatus_Success)

// NEW — Success was split into two values
if (surface_texture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal
 && surface_texture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal)
```

`SuccessSuboptimal` means it still works but the surface should ideally be
reconfigured (e.g., after a window resize).

---

## 6. Device Descriptor Changes {#6-device-descriptor}

### Device lost callback — moved into descriptor

```c
// OLD
device_desc.deviceLostCallback = myDeviceLostCallback;

// NEW
device_desc.deviceLostCallbackInfo = (WGPUDeviceLostCallbackInfo){
    .nextInChain = NULL,
    .mode = WGPUCallbackMode_AllowSpontaneous,
    .callback = myDeviceLostCallback,  // new signature — see section 2
    .userdata1 = NULL,
    .userdata2 = NULL,
};
```

### Uncaptured error callback — moved into descriptor

```c
// OLD — separate function call after device creation
wgpuDeviceSetUncapturedErrorCallback(device, myErrorCallback, NULL);

// NEW — set in the device descriptor before creation
device_desc.uncapturedErrorCallbackInfo = (WGPUUncapturedErrorCallbackInfo){
    .nextInChain = NULL,
    .callback = myErrorCallback,  // new signature — see section 2
    .userdata1 = NULL,
    .userdata2 = NULL,
};
```

The setter function `wgpuDeviceSetUncapturedErrorCallback` still exists but
now takes a `WGPUUncapturedErrorCallbackInfo` struct instead of separate
callback + userdata args.

### Label field

```c
// OLD
device_desc.label = "My Device";
device_desc.defaultQueue.label = "Default Queue";

// NEW
device_desc.label = (WGPUStringView){ .data = "My Device", .length = WGPU_STRLEN };
device_desc.defaultQueue.label = (WGPUStringView){ .data = "Default Queue", .length = WGPU_STRLEN };
```

---

## 7. Frame & Render Pass Changes {#7-frame}

### `depthSlice` is now always present

The tutorial may conditionally set `depthSlice` only for non-wgpu backends.
In v29 it is a standard field on `WGPURenderPassColorAttachment` for all
backends. Always set it:

```c
// OLD (conditional)
#ifndef WEBGPU_BACKEND_WGPU
    color_attachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif

// NEW (always)
color_attachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
```

### Label fields

All descriptor labels (encoder, command buffer, texture view) need the
`WGPUStringView` treatment — see [section 1](#1-strings).

### Surface texture release

In the old API, wgpu-native required you to NOT release the surface texture
after creating a view, while Dawn required you to release it. This
backend-conditional code can be removed — just don't manually release surface
textures:

```c
// OLD
#ifndef WEBGPU_BACKEND_WGPU
    wgpuTextureRelease(surface_texture.texture);
#endif

// NEW — just remove this block entirely
```

---

## 8. Buffer API Changes {#8-buffers}

### Type rename

```c
// OLD
WGPUBufferUsageFlags usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;

// NEW
WGPUBufferUsage usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
```

The individual flag values (`WGPUBufferUsage_Vertex`, etc.) are unchanged.

### Buffer descriptor label

```c
// OLD
bufferDesc.label = "my buffer";
// NEW
bufferDesc.label = (WGPUStringView){ .data = "my buffer", .length = WGPU_STRLEN };
```

### `wgpuBufferMapAsync` — CallbackInfo pattern

```c
// OLD
wgpuBufferMapAsync(buffer, WGPUMapMode_Read, 0, size, onMapped, userData);

// NEW
WGPUBufferMapCallbackInfo mapCb = {
    .nextInChain = NULL,
    .mode = WGPUCallbackMode_AllowSpontaneous,
    .callback = onMapped,   // new signature — see section 2
    .userdata1 = userData,
    .userdata2 = NULL,
};
wgpuBufferMapAsync(buffer, WGPUMapMode_Read, 0, size, mapCb);
```

### Map status enum

```c
// OLD
if (status == WGPUBufferMapAsyncStatus_Success)

// NEW
if (status == WGPUMapAsyncStatus_Success)
```

Full mapping:

| Old | New |
|-----|-----|
| `WGPUBufferMapAsyncStatus_Success` | `WGPUMapAsyncStatus_Success` |
| `WGPUBufferMapAsyncStatus_ValidationError` | `WGPUMapAsyncStatus_Error` |
| `WGPUBufferMapAsyncStatus_Unknown` | `WGPUMapAsyncStatus_Error` |
| `WGPUBufferMapAsyncStatus_DeviceLost` | `WGPUMapAsyncStatus_Aborted` |
| `WGPUBufferMapAsyncStatus_DestroyedBeforeCallback` | `WGPUMapAsyncStatus_Aborted` |
| `WGPUBufferMapAsyncStatus_UnmappedBeforeCallback` | `WGPUMapAsyncStatus_Aborted` |
| `WGPUBufferMapAsyncStatus_MappingAlreadyPending` | `WGPUMapAsyncStatus_Error` |

---

## 9. Pipeline Changes {#9-pipelines}

### Entry point names

```c
// OLD
vertex_state.entryPoint = "vs_main";
fragment_state.entryPoint = "fs_main";

// NEW
vertex_state.entryPoint = (WGPUStringView){ .data = "vs_main", .length = WGPU_STRLEN };
fragment_state.entryPoint = (WGPUStringView){ .data = "fs_main", .length = WGPU_STRLEN };
```

Everything else in the pipeline descriptor (topology, blend state, depth
stencil, multisample, color targets) is unchanged.

---

## 10. Adapter Inspection {#10-adapter}

If the tutorial queries adapter properties or features:

### Properties → Info

```c
// OLD
WGPUAdapterProperties props = {};
wgpuAdapterGetProperties(adapter, &props);
printf("Vendor: %s\n", props.vendorName);

// NEW
WGPUAdapterInfo info = {};
wgpuAdapterGetInfo(adapter, &info);
printf("Vendor: %.*s\n", (int)info.vendor.length, info.vendor.data);
```

### Feature enumeration

```c
// OLD
size_t count = wgpuAdapterEnumerateFeatures(adapter, NULL);
WGPUFeatureName *features = malloc(count * sizeof(WGPUFeatureName));
wgpuAdapterEnumerateFeatures(adapter, features);

// NEW
WGPUSupportedFeatures features = {};
wgpuAdapterGetFeatures(adapter, &features);
// features.featureCount and features.features are populated
```

### Limits

```c
// OLD
WGPUSupportedLimits limits = {};
wgpuAdapterGetLimits(adapter, &limits);
uint32_t maxTex = limits.limits.maxTextureDimension2D;

// NEW
WGPULimits limits = {};
wgpuAdapterGetLimits(adapter, &limits);
uint32_t maxTex = limits.maxTextureDimension2D;  // no nested .limits
```

---

## 11. Reference Counting {#11-refcounting}

All `wgpu*Reference()` functions were renamed to `wgpu*AddRef()`:

```c
// OLD
wgpuDeviceReference(device);
wgpuBufferReference(buffer);
wgpuTextureReference(texture);

// NEW
wgpuDeviceAddRef(device);
wgpuBufferAddRef(buffer);
wgpuTextureAddRef(texture);
```

`wgpu*Release()` is unchanged.

---

## 12. Surface Creation (glfw3webgpu) {#12-surface-creation}

If the tutorial shows the platform-specific surface creation code
(usually hidden inside `glfwGetWGPUSurface`), the structs were renamed:

| Old | New |
|-----|-----|
| `WGPUSurfaceDescriptorFromMetalLayer` | `WGPUSurfaceSourceMetalLayer` |
| `WGPUSurfaceDescriptorFromWindowsHWND` | `WGPUSurfaceSourceWindowsHWND` |
| `WGPUSurfaceDescriptorFromXlibWindow` | `WGPUSurfaceSourceXlibWindow` |
| `WGPUSurfaceDescriptorFromWaylandSurface` | `WGPUSurfaceSourceWaylandSurface` |

And their `sType` values:

| Old | New |
|-----|-----|
| `WGPUSType_SurfaceDescriptorFromMetalLayer` | `WGPUSType_SurfaceSourceMetalLayer` |
| `WGPUSType_SurfaceDescriptorFromWindowsHWND` | `WGPUSType_SurfaceSourceWindowsHWND` |
| `WGPUSType_SurfaceDescriptorFromXlibWindow` | `WGPUSType_SurfaceSourceXlibWindow` |
| `WGPUSType_SurfaceDescriptorFromWaylandSurface` | `WGPUSType_SurfaceSourceWaylandSurface` |

The `WGPUSurfaceSourceXlibWindow.window` field changed from X11's `Window`
type to `uint64_t`, so cast it:

```c
fromXlibWindow.window = (uint64_t)glfwGetX11Window(window);
```

---

## 13. Removed / Renamed Types {#13-renames}

### Struct renames

| Old name | New name |
|----------|----------|
| `WGPUShaderModuleWGSLDescriptor` | `WGPUShaderSourceWGSL` |
| `WGPUShaderModuleSPIRVDescriptor` | `WGPUShaderSourceSPIRV` |
| `WGPUAdapterProperties` | `WGPUAdapterInfo` |
| `WGPUSurfaceDescriptorFromMetalLayer` | `WGPUSurfaceSourceMetalLayer` |
| `WGPUSurfaceDescriptorFromWindowsHWND` | `WGPUSurfaceSourceWindowsHWND` |
| `WGPUSurfaceDescriptorFromXlibWindow` | `WGPUSurfaceSourceXlibWindow` |
| `WGPUSurfaceDescriptorFromWaylandSurface` | `WGPUSurfaceSourceWaylandSurface` |
| `WGPUTextureDataLayout` | `WGPUTexelCopyBufferLayout` |
| `WGPUImageCopyBuffer` | `WGPUTexelCopyBufferInfo` |
| `WGPUImageCopyTexture` | `WGPUTexelCopyTextureInfo` |
| `WGPURenderPassDescriptorMaxDrawCount` | `WGPURenderPassMaxDrawCount` |
| `WGPUComputePassTimestampWrites` | `WGPUPassTimestampWrites` (unified) |
| `WGPURenderPassTimestampWrites` | `WGPUPassTimestampWrites` (unified) |
| `WGPUSupportedLimits` | `WGPULimits` (flattened, no nested `.limits`) |

### Enum renames

| Old name | New name |
|----------|----------|
| `WGPUBufferMapAsyncStatus` | `WGPUMapAsyncStatus` |
| `WGPUBufferUsageFlags` (typedef) | `WGPUBufferUsage` |
| `WGPUTextureUsageFlags` (typedef) | `WGPUTextureUsage` |
| `WGPUColorWriteMaskFlags` (typedef) | `WGPUColorWriteMask` |
| `WGPUMapModeFlags` (typedef) | `WGPUMapMode` |
| `WGPUShaderStageFlags` (typedef) | `WGPUShaderStage` |

### sType value renames

| Old | New |
|-----|-----|
| `WGPUSType_ShaderModuleWGSLDescriptor` | `WGPUSType_ShaderSourceWGSL` |
| `WGPUSType_ShaderModuleSPIRVDescriptor` | `WGPUSType_ShaderSourceSPIRV` |
| `WGPUSType_SurfaceDescriptorFromMetalLayer` | `WGPUSType_SurfaceSourceMetalLayer` |
| `WGPUSType_SurfaceDescriptorFromWindowsHWND` | `WGPUSType_SurfaceSourceWindowsHWND` |
| `WGPUSType_SurfaceDescriptorFromXlibWindow` | `WGPUSType_SurfaceSourceXlibWindow` |
| `WGPUSType_SurfaceDescriptorFromWaylandSurface` | `WGPUSType_SurfaceSourceWaylandSurface` |
| `WGPUSType_RenderPassDescriptorMaxDrawCount` | `WGPUSType_RenderPassMaxDrawCount` |

### Enum value renames

| Old | New |
|-----|-----|
| `WGPUSurfaceGetCurrentTextureStatus_Success` | `WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal` |
| `WGPUBufferMapAsyncStatus_Success` | `WGPUMapAsyncStatus_Success` |
| `WGPUBufferMapAsyncStatus_ValidationError` | `WGPUMapAsyncStatus_Error` |
| `WGPUBufferMapAsyncStatus_DeviceLost` | `WGPUMapAsyncStatus_Aborted` |

### Removed functions

| Removed | Replacement |
|---------|-------------|
| `wgpuSurfaceGetPreferredFormat()` | `wgpuSurfaceGetCapabilities()` → `.formats[0]` |
| `wgpuAdapterGetProperties()` | `wgpuAdapterGetInfo()` |
| `wgpuAdapterEnumerateFeatures()` | `wgpuAdapterGetFeatures()` |
| `wgpuDeviceEnumerateFeatures()` | `wgpuDeviceGetFeatures()` |
| All `wgpu*Reference()` | `wgpu*AddRef()` |

---

## 14. wgpu-native Extension (`wgpu.h`) Changes {#14-wgpu-ext}

The `#include <webgpu/wgpu.h>` header provides wgpu-native-specific
extensions. Key changes:

### `wgpuDevicePoll` — still available, signature unchanged

```c
wgpuDevicePoll(device, false, nullptr);  // works the same
```

### GLSL shader source (if used)

```c
// OLD
WGPUShaderModuleGLSLDescriptor glsl;
// NEW
WGPUShaderSourceGLSL glsl;
```

### Instance extras

`WGPUInstanceExtras` gained many new fields. The existing fields
(`backends`, `flags`, `dx12ShaderCompiler`, `gles3MinorVersion`) still
work. String fields changed from `char*` to `WGPUStringView`.

### Native features

The `WGPUNativeFeature` enum was expanded significantly. Old feature
names still work, and many new features were added (subgroups, ray query,
shader f64, etc.).

---

## 15. New Constants & Macros {#15-constants}

| Name | Value | Purpose |
|------|-------|---------|
| `WGPU_STRLEN` | `SIZE_MAX` | Use as `.length` in `WGPUStringView` for null-terminated strings |
| `WGPU_DEPTH_SLICE_UNDEFINED` | `UINT32_MAX` | Set on color attachments for non-3D textures |
| `WGPU_TRUE` / `WGPU_FALSE` | 1 / 0 | Explicit boolean values |
| `WGPU_QUERY_SET_INDEX_UNDEFINED` | `UINT32_MAX` | Undefined query set index |
| `WGPU_STRING_VIEW_INIT` | `{ NULL, WGPU_STRLEN }` | Default initializer for `WGPUStringView` |

---

## Complete Migration Example

Here is a full before/after for the core initialization pattern the tutorial
teaches (create instance → request adapter → request device):

### Old (tutorial code)

```c
// Callbacks
void onAdapterReady(WGPURequestAdapterStatus status, WGPUAdapter adapter,
                    char const *msg, void *ud) {
    *(WGPUAdapter *)ud = adapter;
}
void onDeviceReady(WGPURequestDeviceStatus status, WGPUDevice device,
                   char const *msg, void *ud) {
    *(WGPUDevice *)ud = device;
}
void onDeviceLost(WGPUDeviceLostReason reason, char const *msg, void *ud) {
    fprintf(stderr, "Device lost: %s\n", msg);
}
void onError(WGPUErrorType type, char const *msg, void *ud) {
    fprintf(stderr, "Error: %s\n", msg);
}

// Init
WGPUInstanceDescriptor idesc = {};
WGPUInstance instance = wgpuCreateInstance(&idesc);

WGPURequestAdapterOptions opts = { .compatibleSurface = surface };
WGPUAdapter adapter = NULL;
wgpuInstanceRequestAdapter(instance, &opts, onAdapterReady, &adapter);

WGPUDeviceDescriptor ddesc = {};
ddesc.label = "My Device";
ddesc.deviceLostCallback = onDeviceLost;
WGPUDevice device = NULL;
wgpuAdapterRequestDevice(adapter, &ddesc, onDeviceReady, &device);

wgpuDeviceSetUncapturedErrorCallback(device, onError, NULL);

WGPUTextureFormat format = wgpuSurfaceGetPreferredFormat(surface, adapter);
```

### New (v29 code)

```c
// Callbacks — note changed signatures
void onAdapterReady(WGPURequestAdapterStatus status, WGPUAdapter adapter,
                    WGPUStringView msg, void *ud1, void *ud2) {
    *(WGPUAdapter *)ud1 = adapter;
}
void onDeviceReady(WGPURequestDeviceStatus status, WGPUDevice device,
                   WGPUStringView msg, void *ud1, void *ud2) {
    *(WGPUDevice *)ud1 = device;
}
void onDeviceLost(WGPUDevice const *dev, WGPUDeviceLostReason reason,
                  WGPUStringView msg, void *ud1, void *ud2) {
    fprintf(stderr, "Device lost: %.*s\n", (int)msg.length, msg.data);
}
void onError(WGPUDevice const *dev, WGPUErrorType type,
             WGPUStringView msg, void *ud1, void *ud2) {
    fprintf(stderr, "Error: %.*s\n", (int)msg.length, msg.data);
}

// Init
WGPUInstanceDescriptor idesc = {};
WGPUInstance instance = wgpuCreateInstance(&idesc);

WGPURequestAdapterOptions opts = { .compatibleSurface = surface };
WGPUAdapter adapter = NULL;
wgpuInstanceRequestAdapter(instance, &opts, (WGPURequestAdapterCallbackInfo){
    .mode = WGPUCallbackMode_AllowSpontaneous,
    .callback = onAdapterReady,
    .userdata1 = &adapter,
});

WGPUDeviceDescriptor ddesc = {};
ddesc.label = (WGPUStringView){ .data = "My Device", .length = WGPU_STRLEN };
ddesc.deviceLostCallbackInfo = (WGPUDeviceLostCallbackInfo){
    .mode = WGPUCallbackMode_AllowSpontaneous,
    .callback = onDeviceLost,
};
ddesc.uncapturedErrorCallbackInfo = (WGPUUncapturedErrorCallbackInfo){
    .callback = onError,
};
WGPUDevice device = NULL;
wgpuAdapterRequestDevice(adapter, &ddesc, (WGPURequestDeviceCallbackInfo){
    .mode = WGPUCallbackMode_AllowSpontaneous,
    .callback = onDeviceReady,
    .userdata1 = &device,
});

WGPUSurfaceCapabilities caps = {};
wgpuSurfaceGetCapabilities(surface, adapter, &caps);
WGPUTextureFormat format = caps.formats[0];
```
