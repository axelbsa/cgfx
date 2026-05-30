#pragma once
// =============================================================================
// engine_api.hpp — the ONLY header a consumer (game / editor / sandbox) includes.
//
// Including it drags in ZERO of flecs / cglm / Jolt / cgfx / WebGPU / GLFW
// (FOUNDATIONS sec 5). Everything below is opaque handles + POD config + the
// GLFW-free input enums.
// =============================================================================
#include "core/engine.hpp"        // Engine, EngineConfig
#include "core/entity_id.hpp"     // EntityID
#include "core/scene_id.hpp"      // SceneID, PrefabID
#include "systems/input_keys.hpp" // Key, MouseButton, MouseState
