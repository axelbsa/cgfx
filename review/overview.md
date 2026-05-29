# cgfx API Review — Master Overview

Source: multi-agent audit of the cgfx public API surface (May 2026) against the WebGPU
spec. Every claim was verified against source; `file:line` references are included.
Items flagged by multiple independent reviewers are marked **⊕ (high confidence)**.

This file is the single-page index of every finding, each with a short note. Full detail
and implementation guidance live in the per-tier files:

- [tier-1-wrong-or-incoherent.md](tier-1-wrong-or-incoherent.md) — silently wrong / self-contradictory
- [tier-2-hard-walls.md](tier-2-hard-walls.md) — common task, no in-desc escape hatch
- [tier-3-coherence-drift.md](tier-3-coherence-drift.md) — naming / ownership / convention drift
- [tier-4-defects.md](tier-4-defects.md) — outright bugs, quick wins

Finding IDs are stable (e.g. `T1.1`) — use them when we pick work.

---

## The shape of the problem

cgfx's core idea is sound and well-executed: zero-init descriptors with documented
defaults, transparent structs, "you record raw draws between begin/end." The triangle path
reads beautifully. The trouble is concentrated in three places:

1. ~~**Pipeline/frame descriptors hardcode choices that should be parameters.**~~
   Largely resolved - blend, depth compare/write, MSAA, MRT, strip index format, stencil all
   configurable now. Remaining: load-op control (T2.2).
2. ~~**The CPU-GPU data story is half-finished.**~~
   Resolved - `cgfx_buffer_read()` wraps readback; backend `#ifdef` no longer leaks.
3. **Cross-cutting conventions (errors, ownership, naming) drifted** as second idioms were
   bolted on without retrofitting the first. (T3.x - not yet addressed.)

Remaining theme: **device-level control is still locked** (no feature requests T2.3, no
device-lost callback T4.4, no resize recovery T2.8). These are the ctx-level gaps.

---

## What's genuinely well-designed (keep)

- Zero-init descriptors with documented per-field defaults — `(CgfxPipelineDesc){ .shader = &s }`
  is exactly the right altitude.
- "Shader owns layouts, caller owns bind groups" — well-motivated, consistently honored
  (the camera coupling, T3.4, is the one violation).
- `CgfxUniform`/`CgfxCamera` as "bundle the 5-call dance into 3" — cleanly scoped.
- The compute module's two-tier design (standalone vs borrow-encoder) is the cleanest
  pattern in the codebase — it's the model the buffer-copy and readback paths should follow.

---

## Tier 1 — Wrong or incoherent (fail silently or self-contradict)

| ID | Finding | Status |
|----|---------|--------|
| **T1.1 ⊕** | Hardcoded alpha blend on every pipeline | **Done** - `CgfxColorTarget[]`, opaque default, blend presets |
| **T1.2** | `depthCompare=Less` + `depthWriteEnabled=true` hardcoded | **Done** - `depth_compare` + `depth_write_disabled` fields |
| **T1.3** | `stripIndexFormat=Undefined` hardcoded | **Done** - auto-derived for strip topologies |
| **T1.4** | Stencil attachment contradictory | **Done** (step 1) - ops set to Undefined for Depth24Plus |
| **T1.5 ⊕** | Error model broken | **Done** (partial) - `bool ok;` on structs. Compile-info disabled (wgpu v0.19) |
| **T1.6** | MSAA impossible end-to-end | **Done** - `sample_count` on pipeline, `resolve_views` on frame |
| **T1.7** | Mipmaps: sampler configured for unfillable content | Not started |

---

## Tier 2 — Hard walls (common task, no in-desc escape)

| ID | Finding | Status |
|----|---------|--------|
| **T2.1 ⊕** | Single color target, format-locked to surface | **Done** - `CgfxColorTarget[]`, `CgfxRenderPassDesc`, MRT example |
| **T2.2** | No load-op control - can't preserve target | Not started |
| **T2.3 ⊕** | No device feature-request path | Not started |
| **T2.4 ⊕** | Async readback unwrapped; backend `#ifdef` leaks | **Done** - `cgfx_buffer_read()` |
| **T2.5** | Sampler binding type hardcoded to `Filtering` | Not started |
| **T2.6** | No instancing in `cgfx_mesh_draw` | Not started |
| **T2.7** | No dynamic offsets / sub-buffer ranges | Not started |
| **T2.8** | Black-window-on-resize | Not started |

---

## Tier 3 — Coherence & convention drift

| ID | Finding | Key location | Note |
|----|---------|--------------|------|
| **T3.1 ⊕** | Wrap-vs-raw inconsistent; destroy asymmetric | across modules | shader/texture/buffer wrapped (`cgfx_*_destroy`); pipeline/sampler/bind-group raw (`wgpu*Release`). No learnable rule, unstated. |
| **T3.2 ⊕** | Three bind-group APIs, inverted naming | `cgfx_shader.c` | `cgfx_shader_create_bind_group` (positional, footgun) vs `cgfx_bind_group_create` (explicit) vs `cgfx_uniform_create` (implicit). First breaks `noun_verb` convention. |
| **T3.3** | Two non-mirroring split-lifecycle idioms | frame vs compute | Frame: separate functions; compute: different pairs + `owns_encoder` flag. Learning one doesn't transfer. |
| **T3.4** | Camera hard-assumes one buffer at binding 0 | `cgfx_camera.c:41-43` | Can't group camera+lights+time in one per-frame group; contradicts "caller owns bind groups." |
| **T3.5** | Auto-visibility heuristic silently wrong cross-stage | `cgfx_shader.c:98-112` | Vertex-stage texture sampling or compute-only uniforms get wrong default → confusing validation errors. |
| **T3.6** | Loader "temporary" but public + in umbrella; u16 indices | `cgfx_loader.h`, `cgfx.h` | Conflicts with the u32-everywhere standard (even parses u16 then widens). |
| **T3.7** | `cgfx_default_limits()` returns all-`0xFF` | `cgfx_ctx.c:49` | Works today (matches U32/U64 undefined sentinels) but fragile byte-pattern assumption across mixed-width fields. |

---

## Tier 4 — Outright defects (quick wins)

| ID | Finding | Status |
|----|---------|--------|
| **T4.1** | Index buffer mislabeled `"cgfx vertex buffer"` | **Done** - label fixed |
| **T4.2** | `cgfx_buffer_create_mapping` ignores its `data` param | **Done** - param removed |
| **T4.3** | `cgfx_frame_end` null-derefs on compute-only frame | **Done** - null guard |
| **T4.4** | No device-lost / uncaptured-error user hook | Not started |

---

## Fix order and status

**Done:**
1. ~~**T1.5** error model~~ - `bool ok;` on wrapped structs. WGSL compile-info disabled (wgpu-native v0.19 limitation).
2. ~~**T1.1 + T2.1** blend control + color targets/MRT~~ - `CgfxColorTarget[]`, `CgfxRenderPassDesc`, blend presets.
3. ~~**T2.4 + T4.1 + T4.2** buffer readback~~ - `cgfx_buffer_read()`, label fix, phantom param removed.
4. ~~**T1.2 + T1.3 + T1.4 + T1.6** pipeline/frame cluster~~ - depth compare/write, strip index auto-derive, stencil contradiction fix, MSAA sample_count + resolve targets.
5. ~~**T4.3** null-deref guard~~ - incidental with T2.1.

**Remaining (recommended order):**
1. **T2.3 + T4.4** - device features + device-lost callback in ctx desc. Same descriptor surface, removes a hard capability ceiling.
2. **T2.8** - reconfigure-and-retry on Outdated/Lost. Fixes black-window-on-resize.
3. **T2.2** - load-op control (can't preserve target contents). Touches the pass-begin surface.
4. **T2.5 + T2.7** - sampler binding type + dynamic offsets. Both add to `CgfxBindingDesc`.
5. **T2.6** - instanced draw. Standalone new function.
6. **T3.x** - coherence/naming cleanup pass.
7. **T1.7** - mipmap generation. Larger feature, needs per-mip views.

## wgpu-native version constraint

The vendored wgpu-native (v0.19.4.1, from Elie Michel's LearnWebGPU distribution) is
significantly behind the current WebGPU spec. Some APIs exist in the header but are
unimplemented stubs that panic at runtime (e.g. `wgpuShaderModuleGetCompilationInfo`).

This affects implemented work:
- **T1.5**: detailed WGSL compile-error capture (`wgpuShaderModuleGetCompilationInfo`) is
  disabled; falls back to NULL-check.

Updating wgpu-native is not in scope for this review pass, but it is a prerequisite for the
full T1.5 implementation. Future findings relying on newer APIs (T2.3 device features may
need newer feature enum values) could also be constrained.

---

## Dependency notes

- **T1.7 (mipmaps)** needs per-mip texture *views* (currently only the whole-resource view
  exists) - implementing mip generation forces that view work, which also unblocks
  texture readback / per-face views.
- ~~**T2.4 (readback)** retires the leaked `CgfxBuffer.ready` field.~~ Done.
- ~~**T2.1 + T1.6** generalized pass-begin + MSAA resolve.~~ Done - `CgfxRenderPassDesc`
  with `color_views` and `resolve_views`.
- ~~**T1.1 + T2.1** blend + color targets.~~ Done - `CgfxColorTarget[]`.
- **T3.4 (camera)** depends on T3.2 (bind-group naming cleanup) landing first.
