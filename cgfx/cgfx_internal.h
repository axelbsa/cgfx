/**
 * @file cgfx_internal.h
 * @brief Internal helpers for cgfx — not part of the public API.
 *
 * Contains synchronous wrappers around WebGPU's asynchronous adapter and
 * device request callbacks. These are used by cgfx_ctx_init() internally.
 *
 * Functions prefixed with cgfx__ (double underscore) are internal.
 */
#ifndef CGFX_INTERNAL_H
#define CGFX_INTERNAL_H

#include <assert.h>
#include <stdio.h>
#include <stdbool.h>

#include <webgpu/webgpu.h>


/* ── Adapter request (synchronous wrapper) ────────────────────────── */

typedef struct {
    WGPUAdapter adapter;
    bool request_ended;
} CgfxAdapterRequestData;

static void cgfx__on_adapter_request_ended(WGPURequestAdapterStatus status,
                                            WGPUAdapter adapter,
                                            char const *message,
                                            void *user_data) {
    CgfxAdapterRequestData *data = user_data;
    if (status == WGPURequestAdapterStatus_Success) {
        data->adapter = adapter;
    } else {
        fprintf(stderr, "[cgfx] Could not get WebGPU adapter: %s\n", message);
    }
    data->request_ended = true;
}

/**
 * Request a WebGPU adapter synchronously.
 *
 * Internally calls wgpuInstanceRequestAdapter with a callback that captures
 * the result. On Emscripten, polls with emscripten_sleep until the async
 * operation completes. On native backends the callback fires synchronously.
 *
 * @param instance  The WebGPU instance to request from.
 * @param options   Adapter request options (e.g., compatible surface).
 * @return          The adapter handle, or NULL on failure.
 */
static WGPUAdapter cgfx__request_adapter_sync(WGPUInstance instance,
                                               const WGPURequestAdapterOptions *options) {
    CgfxAdapterRequestData data = { .adapter = nullptr, .request_ended = false };

    wgpuInstanceRequestAdapter(instance, options,
                               &cgfx__on_adapter_request_ended,
                               (void *)&data);

#ifdef __EMSCRIPTEN__
    while (!data.request_ended) {
        emscripten_sleep(100);
    }
#endif

    assert(data.request_ended);
    return data.adapter;
}


/* ── Device request (synchronous wrapper) ─────────────────────────── */

typedef struct {
    WGPUDevice device;
    bool request_ended;
} CgfxDeviceRequestData;

static void cgfx__on_device_request_ended(WGPURequestDeviceStatus status,
                                           WGPUDevice device,
                                           char const *message,
                                           void *user_data) {
    CgfxDeviceRequestData *data = user_data;
    if (status == WGPURequestDeviceStatus_Success) {
        data->device = device;
    } else {
        fprintf(stderr, "[cgfx] Could not get WebGPU device: %s\n", message);
    }
    data->request_ended = true;
}

/**
 * Request a WebGPU device synchronously.
 *
 * Same pattern as cgfx__request_adapter_sync: wraps the async callback
 * into a blocking call. On Emscripten, polls with emscripten_sleep.
 *
 * @param adapter     The adapter to request the device from.
 * @param descriptor  Device descriptor (features, limits, queue label, etc.).
 * @return            The device handle, or NULL on failure.
 */
static WGPUDevice cgfx__request_device_sync(WGPUAdapter adapter,
                                             const WGPUDeviceDescriptor *descriptor) {
    CgfxDeviceRequestData data = { .device = nullptr, .request_ended = false };

    wgpuAdapterRequestDevice(adapter, descriptor,
                             &cgfx__on_device_request_ended,
                             (void *)&data);

#ifdef __EMSCRIPTEN__
    while (!data.request_ended) {
        emscripten_sleep(100);
    }
#endif

    assert(data.request_ended);
    return data.device;
}

#endif /* CGFX_INTERNAL_H */
