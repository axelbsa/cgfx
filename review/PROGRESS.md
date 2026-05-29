# cgfx API Refactor — Progress & Handoff

**Last updated:** 2026-05-29 (dev machine, headless — no Vulkan).
**Branch:** `fix_inconstistancies`

This file is the resume point. The conversation history and the approved plan files under
`~/.claude/plans/` will **not** travel between machines — everything needed to continue is
captured here and in the `review/` tier files (which are in the repo).

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
| **T1.5** | Error model (`bool ok` on wrapped structs + WGSL compile-error capture) | **Implemented** — build-verified; runtime pending |
| **T1.1** | Hardcoded alpha blend → per-target blend, opaque default | **Implemented** — build-verified; runtime pending |
| **T2.1** | Single surface-format target → `CgfxColorTarget[]` + offscreen/MRT + generalized pass-begin | **Implemented** — build-verified; runtime pending |
| **T4.3** | `cgfx_frame_end` null-deref guard | **Fixed** (incidental, with T2.1) |
| everything else | T1.2–T1.7 (rest), T2.2–T2.8, T3.x, T4.1/T4.2/T4.4 | Not started |

Also done outside the tiers: README.md cleanup (stale content removed) and the error-handling
contract in `CLAUDE.md` rewritten to match T1.5.

---

## What was implemented (so a fresh session needs no history)

Detailed per-finding notes already live in the tier files' **Status:** sections. Quick map of
the code touched:

- **T1.5** — `bool ok;` added to `CgfxShader/Buffer/Texture/Mesh/Uniform/Camera`, set on the
  success path of each constructor and propagated through the chains (mesh→buffers,
  uniform/camera→buffer+bind_group, ctx_init/resize→depth texture). `cgfx_shader.c` gained a
  `wgpuShaderModuleGetCompilationInfo` wrapper that prints WGSL errors with line/column and sets
  `ok=false`. Examples `multiple_uniforms` and `depth_texture` now check `.ok`.
- **T1.1 + T2.1** — `cgfx_pipeline.{h,c}`: `CgfxColorTarget` + `color_target_count`/
  `color_targets`, default single **opaque** surface-format target, blend presets
  `cgfx_blend_alpha/additive/premultiplied`. `cgfx_frame.{h,c}`: `CgfxRenderPassDesc` +
  `cgfx_frame_begin_render_pass_ex` (offscreen/MRT views) + `cgfx_frame_end_render_pass`, and
  `cgfx_frame_begin_render_pass` now delegates to `_ex`. New `examples/mrt/`.
- **T4.3** — `cgfx_frame_end` ends the render pass only `if (frame->render_pass)`.

---

## Verification status — READ THIS

- **Build:** passes clean on the dev machine — the cgfx library compiles with `-Werror -Wall
  -Wextra -pedantic` (zero warnings); all examples link, including `mrt`.
- **Runtime: NOT yet verified.** *This is why we're moving machines.* The dev machine is
  headless: GLFW can't open a window, and over X11 forwarding `wgpuSurfaceConfigure` loses the
  device (X forwards the window, not the GPU). A real Vulkan context is needed.

### Runtime checks to run on the main machine (in order)

```bash
cmake . -B build && cmake --build build
```

1. **No-regression:** `./build/examples/triangle` and `./build/examples/depth_texture` render
   exactly as before (opaque default matches their `alpha = 1.0` output — verified by shader
   audit, but confirm visually).

2. **T1.5 headline (shader compile error is now surfaced):** introduce a WGSL typo, e.g. in
   `examples/multiple_uniforms/multiple_uniforms.wgsl` misspell an identifier, then:
   ```bash
   cmake --build build && ./build/examples/multiple_uniforms
   ```
   Expect on stderr: `[cgfx_shader] WGSL compile error in 'uniform shader' at L:C: ...`, and the
   program prints `shader creation failed` and exits at `if (!shader.ok)` — instead of crashing
   later in pipeline creation. **Revert the typo afterwards.**

3. **T1.1 + T2.1 (`mrt` example):** `./build/examples/mrt` → window split in half. Left =
   target A (red/green gradient), right = target B (green/blue gradient). Proves two distinct
   MRT outputs were written in one pass to offscreen RGBA8 textures and sampled back.

4. **Optional headless `.ok` unit check** (no window needed — adapter+device only):
   ```bash
   gcc -std=c23 -DWEBGPU_BACKEND_WGPU \
     -I cgfx -I vendor/cglm/include \
     -I build/_deps/webgpu-backend-wgpu-src/include \
     review/okcheck.c build/cgfx/libcgfx.a build/examples/libwgpu_native.so \
     -lm -o /tmp/okcheck
   LD_LIBRARY_PATH=build/examples /tmp/okcheck
   ```
   Expect `GOOD: ok=1`, `BAD: ok=0` (with a WGSL error printed), `ZERO: ok=0`, `RESULT: PASS`.
   (Source: `review/okcheck.c`.)

---

## Moving the work between machines (git)

The changes are **uncommitted** in the working tree, and two paths are **untracked**:
`review/` and `examples/mrt/`. To bring everything via git:

```bash
git add -A          # includes review/ and examples/mrt/
git commit -m "API review + T1.5, T1.1, T2.1 (T4.3 incidental)"
git push -u origin fix_inconstistancies
```

Modified files in this work: `CLAUDE.md`, `README.md`, all `cgfx/cgfx_*.{c,h}` touched for the
above, `examples/CMakeLists.txt`, `examples/depth_texture/main.c`,
`examples/multiple_uniforms/main.c`. Untracked: `review/`, `examples/mrt/`.

> The approved plan files in `~/.claude/plans/*.md` and the chat history are **not** in the repo
> and won't transfer — this doc plus the tier files are the substitute.

---

## Next steps (resume here)

Per the fix order in `overview.md`, recommended remaining sequence:

1. **T2.4** `cgfx_buffer_read()` — synchronous map-readback helper. Closes the backend
   `#ifdef` leak duplicated in the `compute`/`playing_with_buffers` examples and retires the
   `CgfxBuffer.ready` field. (~30-min extraction; pattern already in `cgfx_internal.h`.)
2. **T2.3 + T4.4** — add `features[]`/`feature_count` and device-lost/error callbacks to the
   ctx descriptors (same descriptor surface; do together).
3. **T2.8** — reconfigure-and-retry on `Outdated`/`Lost` in the surface-acquire path (fixes
   black-window-on-resize).
4. **T1.2** depth compare/write, **T2.7** dynamic offsets, then the rest.
5. **T4.1 / T4.2** (index-buffer label typo; phantom `data` param) — independent quick wins,
   anytime.

**Working method:** for each finding, plan first (explore → confirm approach → write plan →
implement → build), then add a **Status:** note to its tier-file section. Keep changes scoped to
one finding so history stays reviewable.

## Conventions established
- **Error model (T1.5):** wrapped structs carry `bool ok;` (set true only on full success);
  raw-handle creators return `NULL`; lifecycle functions return `bool`. All failures also print
  to stderr. This is the canonical contract — see `CLAUDE.md` "Design conventions → Error
  handling".
- **No em-dashes** in prose the user reads (use `-` or commas).
- Mark each finding's tier-file section with **Status:** when done.
