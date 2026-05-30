#pragma once
#include <cstdint>
#include <bitset>
#include <flecs.h>
#include "input_keys.hpp"     // Key, MouseButton, MouseState (GLFW-free, public)

// =============================================================================
// InputSystem
//
// Each frame poll_and_write() READS the platform's immediate input state and
// writes an InputState component onto the player entity. Gameplay reads InputState
// from the ECS; it never calls InputSystem. The public Key/MouseButton enums are
// GLFW-free; ALL GLFW lives in input_system.cpp (PHASE_05). cgfx is caller-polled,
// so poll_and_write does NOT call glfwPollEvents (the platform loop does that).
// =============================================================================

class InputSystem {
public:
    void init();
    void shutdown();

    // Attach to the cgfx window (GLFWwindow* passed as void* so this header stays
    // GLFW-free). Registers the scroll callback. Call after the Renderer exists.
    void attach(void* glfw_window);

    // Called at the START of Engine::tick (after the loop's glfwPollEvents).
    void poll_and_write(flecs::world& world, flecs::entity player_entity);

    // Cursor capture for mouse-look; resets the delta spike on toggle.
    void set_cursor_captured(bool captured);

    // Direct queries (editor / controllers)
    bool key_held(Key k)     const;
    bool key_pressed(Key k)  const;   // edge: down this frame only
    bool key_released(Key k) const;   // edge: up this frame only
    bool mouse_held(MouseButton b)    const;
    bool mouse_pressed(MouseButton b) const;
    const MouseState& mouse() const { return m_mouse; }

private:
    void* m_window = nullptr;          // GLFWwindow* (cast in the .cpp)

    std::bitset<(size_t)Key::COUNT>         m_held, m_pressed, m_released, m_held_prev;
    std::bitset<(size_t)MouseButton::COUNT> m_mouse_held, m_mouse_pressed;
    MouseState m_mouse;

    float m_scroll_accum = 0.f;        // filled by the GLFW scroll callback
    bool  m_first_frame  = true;       // discard the first (bogus) mouse delta
};
