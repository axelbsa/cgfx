/**
 * @file hwnd3webgpu.h
 * @brief Create a WebGPU surface from a native Windows HWND.
 *
 * Sibling to glfw3webgpu — use this when the window is created externally
 * (e.g., by a C# WinForms host) instead of by GLFW.
 *
 * The native_handle parameter is void* to avoid pulling in <windows.h>.
 * Pass the HWND directly — it will be cast internally.
 */
#ifndef HWND3WEBGPU_H
#define HWND3WEBGPU_H

#include <webgpu/webgpu.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create a WebGPU surface from a Win32 HWND.
 *
 * @param instance  A valid WebGPU instance.
 * @param native_handle  The Win32 HWND (passed as void* to avoid windows.h).
 * @return  A WGPUSurface, or NULL on failure.
 */
WGPUSurface hwndGetWGPUSurface(WGPUInstance instance, void *native_handle);

#ifdef __cplusplus
}
#endif

#endif /* HWND3WEBGPU_H */
