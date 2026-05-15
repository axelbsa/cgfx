# Export

Shared library symbol visibility control.

**Header:** `cgfx_export.h`

---

## CGFX_API macro

The `CGFX_API` macro decorates every public function in cgfx. It controls symbol visibility for shared library builds and expands to nothing for static builds.

```c
CGFX_API bool cgfx_ctx_init(CgfxCtx *ctx, const CgfxCtxDesc *desc);
```

### Behavior by build configuration

| Configuration | `CGFX_SHARED` | `CGFX_BUILD_SHARED` | Windows | Unix |
|---------------|:---:|:---:|---------|------|
| Static build (default) | not defined | not defined | *(empty)* | *(empty)* |
| Shared build, building cgfx | defined | defined | `__declspec(dllexport)` | `__attribute__((visibility("default")))` |
| Shared build, consuming cgfx | defined | not defined | `__declspec(dllimport)` | *(empty)* |

### Preprocessor logic

```c
#ifdef CGFX_SHARED
#  ifdef CGFX_BUILD_SHARED
#    ifdef _WIN32
#      define CGFX_API __declspec(dllexport)
#    else
#      define CGFX_API __attribute__((visibility("default")))
#    endif
#  else
#    ifdef _WIN32
#      define CGFX_API __declspec(dllimport)
#    else
#      define CGFX_API
#    endif
#  endif
#else
#  define CGFX_API
#endif
```

### CMake integration

Both defines are controlled by the CMake option `-DCGFX_SHARED=ON`:

- **`CGFX_SHARED`** is added as a `PUBLIC` compile definition on the `cgfx` target, so both the library and any target that links against it see it.
- **`CGFX_BUILD_SHARED`** is added as a `PRIVATE` compile definition, visible only when compiling the library itself.

!!! note "Static builds require no action"
    When building cgfx as a static library (the default), `CGFX_API` expands to nothing and neither define is set. No special flags or defines are needed from the consumer.

!!! tip "Shared library build command"
    ```bash
    cmake . -B build -DCGFX_SHARED=ON
    cmake --build build
    ```
    This produces `libcgfx.so` on Linux or `cgfx.dll` on Windows, suitable for C# P/Invoke or dynamic loading.
