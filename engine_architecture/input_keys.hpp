#pragma once
#include <cstdint>

// =============================================================================
// input_keys.hpp — the GLFW-free input vocabulary that IS part of the public API
// (engine_api.hpp includes this). No GLFW, no flecs. The mapping from these enums
// to GLFW key codes lives only in input_system.cpp (FOUNDATIONS sec 5, PHASE_05).
// =============================================================================

enum class Key : uint32_t {
    W = 0, A, S, D,
    Space, LShift, LCtrl, Escape,
    COUNT
};

enum class MouseButton : uint32_t {
    Left = 0, Right, Middle,
    COUNT
};

struct MouseState {
    float x = 0.f, y = 0.f;     // current position (pixels)
    float dx = 0.f, dy = 0.f;   // delta since last frame (zeroed on frame 1 / capture toggle)
    float scroll_dy = 0.f;      // accumulated scroll this frame
};
