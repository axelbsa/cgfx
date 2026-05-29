# cgfx API Refactor — Progress & Handoff

**Last updated:** 2026-05-29
**Branch:** `fix_inconstistancies`

This file is the resume point. Each finding's detailed status lives in its tier file's
**Status:** section.

---

## What this effort is

A multi-agent review of the cgfx public API against the WebGPU spec, looking for gaps,
developer friction, and incoherent design. Findings are written up in this directory and being
fixed **one finding at a time**, plan-first.

- `review/overview.md` — master index of all findings + the recommended cross-tier fix order.
- `review/tier-1-wrong-or-incoherent.md` — silently-wrong / self-contradictory (T1.x)
- `review/tier-2-hard-walls.md` — common task, no escape hatch (T2.x)
- `review/tier-3-coherence-drift.md` — naming / ownership / convention (T3.x)
- `review/tier-4-defects.md` — outright bugs, quick wins (T4.x)

Each finding has a stable ID (e.g. `T1.5`). When one is implemented, a **Status:** note is added
to its section in the tier file.

---

## Status snapshot

| ID | Finding | Status |
|----|---------|--------|
| **T1.5** | Error model (`bool ok` on wrapped structs + WGSL compile-error capture) | **Implemented** — runtime-verified; `wgpuShaderModuleGetCompilationInfo` disabled (see note) |
| **T1.1** | Hardcoded alpha blend → per-target blend, opaque default | **Implemented** — runtime-verified |
| **T2.1** | Single surface-format target → `CgfxColorTarget[]` + offscreen/MRT + generalized pass-begin | **Implemented** — runtime-verified |
| **T4.3** | `cgfx_frame_end` null-deref guard | **Fixed** (incidental, with T2.1) |
| **T2.4** | `cgfx_buffer_read()` sync readback helper | **Implemented** - runtime-verified |
| **T4.1** | Index buffer mislabeled | **Fixed** (with T2.4) |
| **T4.2** | `cgfx_buffer_create_mapping` phantom `data` param | **Fixed** (with T2.4) |
| **T1.2** | Depth compare/write configurable | **Implemented** - runtime-verified |
| **T1.3** | Strip index format auto-derived | **Implemented** - runtime-verified |
| **T1.4** | Stencil contradiction (step 1) | **Fixed** - runtime-verified |
| **T1.6** | MSAA sample_count + resolve targets | **Implemented** - runtime-verified |
| everything else | T1.7, T2.2-T2.3/T2.5-T2.8, T3.x, T4.4 | Not started |

Also done outside the tiers: README.md cleanup (stale content removed) and the error-handling
contract in `CLAUDE.md` rewritten to match T1.5.

---

## What was implemented

Quick map of the code touched (detailed notes in each tier file's **Status:** section):

- **T1.5** - `bool ok;` on all 6 wrapped structs. `wgpuShaderModuleGetCompilationInfo`
  wrapper written but disabled (`#if 0`) due to wgpu-native v0.19 not implementing it.
  Fallback: `module == NULL` check + device uncaptured-error callback.
- **T1.1 + T2.1** - `CgfxColorTarget[]` on pipeline (opaque default, blend presets).
  `CgfxRenderPassDesc` + `cgfx_frame_begin_render_pass_ex` (offscreen/MRT). New `examples/mrt/`.
- **T1.2** - `depth_compare` and `depth_write_disabled` fields on `CgfxPipelineDesc`.
- **T1.3** - `stripIndexFormat` auto-derived (`Uint32` for strip topologies).
- **T1.4** - Stencil contradiction fixed (step 1): ops set to `Undefined` for Depth24Plus.
- **T1.6** - `sample_count` + `alpha_to_coverage` on `CgfxPipelineDesc`. `resolve_views`
  parallel array on `CgfxRenderPassDesc`.
- **T2.4** - `cgfx_buffer_read()` sync readback. `CgfxBuffer.ready` removed.
- **T4.1** - Index buffer label fixed.
- **T4.2** - Phantom `data` param removed from `cgfx_buffer_create_mapping`.
- **T4.3** - `cgfx_frame_end` null-deref guard.

---

## Verification

- **Build:** passes clean - zero warnings with `-Wall -Wextra -pedantic`; all examples link.
- **Runtime:** verified (2026-05-29, Vulkan/RADV on AMD Ryzen 9 7950X3D, Mesa 26.0.3).
  All examples launch and render without errors. Visually confirmed by user.
  Compute and buffer readback verified with correct data output.

---

## Next steps

Remaining findings, roughly prioritized:

1. **T2.3 + T4.4** - device features + device-lost callbacks in ctx desc (same descriptor
   surface, removes a hard capability ceiling)
2. **T2.8** - reconfigure-and-retry on `Outdated`/`Lost` (fixes black-window-on-resize)
3. **T2.2** - load-op control (can't preserve target contents across passes)
4. **T2.5 + T2.7** - sampler binding type + dynamic offsets (both add to `CgfxBindingDesc`)
5. **T2.6** - instanced draw
6. **T3.x** - coherence/naming cleanup (T3.1 wrap-vs-raw, T3.2 bind-group naming, etc.)
7. **T1.7** - mipmap generation (larger feature, needs per-mip views)

**Working method:** for each finding, plan first (explore → confirm approach → write plan →
implement → build), then add a **Status:** note to its tier-file section. Keep changes scoped to
one finding so history stays reviewable.

## Known limitations

- **Bad-shader detection incomplete (wgpu-native v0.19):**
  `wgpuShaderModuleGetCompilationInfo` is disabled. Invalid WGSL may return a non-null
  poisoned module, so `shader.ok` can be a false positive. Errors still reach stderr through
  the device uncaptured-error callback. Full compile-info code is preserved in `cgfx_shader.c`
  under `#if 0`, ready to re-enable when wgpu-native is updated.

---

## Conventions established
- **Error model (T1.5):** wrapped structs carry `bool ok;` (set true only on full success);
  raw-handle creators return `NULL`; lifecycle functions return `bool`. All failures also print
  to stderr. This is the canonical contract — see `CLAUDE.md` "Design conventions → Error
  handling".
- **No em-dashes** in prose the user reads (use `-` or commas).
- Mark each finding's tier-file section with **Status:** when done.
