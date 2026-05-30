#pragma once
#include <memory>
#include "../core/engine.hpp"     // EngineConfig (engine.hpp is a cheap full-PIMPL header)

// =============================================================================
// Renderer — RAII owner of the CgfxCtx (window/device/queue/surface/depth).
//
// The thin C++ wrapper over cgfx context lifetime. cgfx is included only in
// renderer.cpp (via cgfx_c.hpp); this header stays cgfx-free by holding the
// CgfxCtx behind a PIMPL. Nullable use is handled by Engine (headless skips
// constructing/initialising the Renderer). PHASE_00 0.5 / RENDER_INTEGRATION 1.
// =============================================================================

struct CgfxCtx;   // cgfx; returned by pointer to the cgfx-aware TUs only

class Renderer {
public:
    Renderer();
    ~Renderer();                              // .cpp (owns the CgfxCtx)
    Renderer(const Renderer&)            = delete;
    Renderer& operator=(const Renderer&) = delete;

    // Creates the CgfxCtx: a GLFW window from cfg, or cgfx_ctx_init_external when
    // cfg.external_window is set. Routes on_device_lost/on_device_error to the log.
    void init(const EngineConfig& cfg);
    void shutdown();

    CgfxCtx* ctx()    const;     // for AssetManager / RenderBridge (cgfx-aware .cpp)
    void*    window() const;     // GLFWwindow* as void* (for InputSystem)

private:
    struct Impl;                 // holds the CgfxCtx (cgfx-typed) in renderer.cpp
    std::unique_ptr<Impl> m_impl;
};
