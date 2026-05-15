/**
 * @file hwnd3webgpu.c
 * @brief Implementation of hwndGetWGPUSurface for Win32.
 *
 * Based on the Windows branch of glfw3webgpu.c, but takes a raw HWND
 * instead of extracting one from a GLFW window.
 */
#include "hwnd3webgpu.h"

#ifdef _WIN32
#include <windows.h>
#endif

WGPUSurface hwndGetWGPUSurface(WGPUInstance instance, void *native_handle) {
#ifdef _WIN32
    HWND hwnd = (HWND)native_handle;
    HINSTANCE hinstance = GetModuleHandle(NULL);

    WGPUSurfaceDescriptorFromWindowsHWND fromWindowsHWND;
    fromWindowsHWND.chain.next = NULL;
    fromWindowsHWND.chain.sType = WGPUSType_SurfaceDescriptorFromWindowsHWND;
    fromWindowsHWND.hinstance = hinstance;
    fromWindowsHWND.hwnd = hwnd;

    WGPUSurfaceDescriptor surfaceDescriptor;
    surfaceDescriptor.nextInChain = &fromWindowsHWND.chain;
    surfaceDescriptor.label = NULL;

    return wgpuInstanceCreateSurface(instance, &surfaceDescriptor);
#else
    (void)instance;
    (void)native_handle;
    return NULL;
#endif
}
