#pragma once
#include <cstdint>
#include <bitset>
#include <flecs.h>

// =============================================================================
// InputSystem
//
// Polls the platform layer each frame, then writes the results into the ECS
// as an InputState component on the designated "player" entity.
//
// Gameplay systems read InputState from ECS — they never call InputSystem
// directly. This keeps them testable and platform-agnostic.
// =============================================================================

// Thin key/button constants so callers don't depend on a windowing library
enum class Key : uint32_t {
    W=0, A, S, D,
    Space, LShift, LCtrl,
    Escape,
    COUNT
};

enum class MouseButton : uint32_t {
    Left=0, Right, Middle,
    COUNT
};

struct MouseState {
    float x = 0.f, y = 0.f;        // current position (pixels)
    float dx = 0.f, dy = 0.f;      // delta since last frame
    float scroll_dy = 0.f;
};

class InputSystem {
public:
    // Lifecycle
    void init();
    void shutdown();

    // Called at the START of Engine::tick(), before ECS progress.
    // Reads platform events and writes InputState onto player_entity.
    void poll_and_write(flecs::world& world, flecs::entity player_entity);

    // --- Direct queries (for systems that need raw state, e.g. editor camera) ---
    bool key_held(Key k)    const;
    bool key_pressed(Key k) const;   // true only on the frame it went down
    bool key_released(Key k) const;

    bool mouse_held(MouseButton b)    const;
    bool mouse_pressed(MouseButton b) const;

    const MouseState& mouse() const { return m_mouse; }

private:
    std::bitset<(size_t)Key::COUNT>         m_held;
    std::bitset<(size_t)Key::COUNT>         m_pressed;
    std::bitset<(size_t)Key::COUNT>         m_released;
    std::bitset<(size_t)MouseButton::COUNT> m_mouse_held;
    std::bitset<(size_t)MouseButton::COUNT> m_mouse_pressed;
    MouseState                              m_mouse;

    // Previous frame state for edge detection
    std::bitset<(size_t)Key::COUNT> m_held_prev;
};
