/**
 * @file cgfx_export.h
 * @brief Shared library export/import macros.
 *
 * When building cgfx as a shared library (-DCGFX_SHARED=ON), public
 * functions are decorated with platform-specific export attributes.
 * When consuming the shared library, the same functions are marked
 * for import. For static builds, the macro expands to nothing.
 */
#ifndef CGFX_EXPORT_H
#define CGFX_EXPORT_H

#if defined(_WIN32)
    #define CGFX_EXPORT __declspec(dllexport)
    #define CGFX_IMPORT __declspec(dllimport)
#else
    #define CGFX_EXPORT __attribute__((visibility("default")))
    #define CGFX_IMPORT
#endif

#if defined(CGFX_BUILD_SHARED)
    #define CGFX_API CGFX_EXPORT
#elif defined(CGFX_SHARED)
    #define CGFX_API CGFX_IMPORT
#else
    #define CGFX_API
#endif

#endif /* CGFX_EXPORT_H */
