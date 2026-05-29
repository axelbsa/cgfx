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

1. **Pipeline/frame descriptors hardcode choices that should be parameters** — and several
   of those defaults are *silently wrong*, not just limiting.
2. **The CPU↔GPU data story is half-finished** — upload works, readback is unwrapped and
   duplicated across examples.
3. **Cross-cutting conventions (errors, ownership, naming) drifted** as second idioms were
   bolted on without retrofitting the first.

Recurring theme: the "thin wrapper, drop to raw WebGPU" escape hatch is **non-uniform**.
Where cgfx monopolizes the object you'd need (the device, the pipeline's blend/target
state, the render pass), there is no escape short of abandoning the abstraction.

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

| ID | Finding | Key location | Note |
|----|---------|--------------|------|
| **T1.1 ⊕** | Hardcoded alpha blend on every pipeline | `cgfx_pipeline.c:71-86` | WebGPU default is *no blend*; cgfx inverted it. Opaque shaders emitting alpha<1 ghost; framebuffer alpha never written. No field to control. |
| **T1.2** | `depthCompare=Less` + `depthWriteEnabled=true` hardcoded | `cgfx_pipeline.c:109-110` | Breaks skybox (needs LessEqual), depth pre-pass / read-only-depth transparency, and reverse-Z — and the build forces `CGLM_FORCE_DEPTH_ZERO_TO_ONE`, the exact reverse-Z setup. |
| **T1.3** | `topology` settable but `stripIndexFormat=Undefined` hardcoded | `cgfx_pipeline.c:46-47` | Pure coherence bug: a knob that can't be used correctly. Indexed strip draw → validation error from a hidden field. |
| **T1.4** | Stencil attachment is internally contradictory | `cgfx_frame.c:97-100` | `stencilLoadOp=Clear`+`Store` **and** `stencilReadOnly=true` — spec violation, masked only because depth format is locked to Depth24Plus (no stencil aspect). Latent. |
| **T1.5 ⊕** | Error model broken, contradicts its own contract | `cgfx_shader.c:80,172`; CLAUDE.md | Constructors return struct-by-value with no error channel; shader compile errors never surfaced. No way to write a robust app today. |
| **T1.6** | MSAA advertised but impossible end-to-end | `cgfx_pipeline.c:129`, `cgfx_frame.c:81` | `sample_count` exists on textures but pipeline `multisample.count=1` and `resolveTarget=nullptr` are hardcoded. A trap. |
| **T1.7** | Mipmaps: sampler configured for content the API can't produce | `cgfx_texture.c` (write mip 0 only), sampler `lodMaxClamp=32` | `mip_levels>1` allocates levels that are never filled → garbage. Trilinear/aniso inert. Must be library-provided. |

---

## Tier 2 — Hard walls (common task, no in-desc escape)

| ID | Finding | Key location | Note |
|----|---------|--------------|------|
| **T2.1 ⊕** | Single color target, format-locked to surface | `cgfx_pipeline.c:84-90`, `cgfx_frame.c:105` | No MRT (deferred/G-buffer) and no offscreen render to a different format (HDR/picking). `CgfxTexture` RTs exist but pipeline can't connect. |
| **T2.2** | No load-op control — can't preserve target | `cgfx_frame.c:82,94` | `loadOp=Clear` hardcoded. UI-over-3D, accumulation, partial redraw all wiped. |
| **T2.3 ⊕** | No device feature-request path | `cgfx_ctx.c:119` | `requiredFeatureCount=0`; descs expose only limits. Cannot enable timestamp queries, texture compression, float32-filterable, etc. Cheap mechanical fix. |
| **T2.4 ⊕** | Async readback unwrapped; backend `#ifdef` leaks into user code | examples `compute`, `playing_with_buffers` | Sync-wrapper pattern already exists in-tree (`cgfx__request_adapter_sync`) but not applied to mapping. ~25 lines duplicated; copies disagree on poll flag. |
| **T2.5** | Sampler binding type hardcoded to `Filtering` | `cgfx_shader.c:132-135` | No `comparison` (shadow maps) or `non-filtering` (R32F data). No field in `CgfxBindingDesc`. |
| **T2.6** | No instancing in `cgfx_mesh_draw` | `cgfx_mesh.c:38` | `instanceCount=1` hardwired. Particles/grass/tiles must abandon the helper. |
| **T2.7** | No dynamic offsets / sub-buffer ranges | `cgfx_shader.c:196-198,255` | Headline "uniforms per object" pattern forces one buffer + one bind group per object (10k objects → 10k of each). |
| **T2.8** | Black-window-on-resize: surface status collapsed to pass/fail | `cgfx_frame.c:31` | `Outdated`/`Lost` discarded; no reconfigure-and-retry; resize recovery depends on manual GLFW wiring. |

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

| ID | Finding | Key location | Note |
|----|---------|--------------|------|
| **T4.1** | Index buffer mislabeled `"cgfx vertex buffer"` | `cgfx_buffer.c:51` | Pollutes GPU capture tooling. One-line fix. |
| **T4.2** | `cgfx_buffer_create_mapping` ignores its `data` param | `cgfx_buffer.c:99` | `(void)data;` — the parameter is a lie. Remove from signature or document hard. |
| **T4.3** | `cgfx_frame_end` null-derefs on compute-only frame | `cgfx_frame.c:124` | `wgpuRenderPassEncoderEnd(frame->render_pass)` unconditional; encoder-only frame crashes. Needs a guard. |
| **T4.4** | No device-lost / uncaptured-error user hook | `cgfx_ctx.c:26-44` | Callbacks only `fprintf(stderr)`, `user_data=nullptr`. App can't recover or show UI. |

---

## Suggested fix order (cross-tier)

1. **T1.5** error model + surface shader-compile errors — without it no robust app is possible.
2. **T1.1** blend control, defaulting to opaque — biggest single correctness win.
3. **T2.4** `cgfx_buffer_read()` — closes the backend-abstraction leak, deletes the most-copied snippet, retires the leaked `CgfxBuffer.ready` field.
4. **T2.3 + T4.4** plumb `features[]` + device-lost callback into the ctx desc — mechanical, removes a hard ceiling.
5. **T2.8** reconfigure-and-retry on Outdated/Lost — fixes black-window-on-resize.
6. **T1.2 + T2.1 + T2.7** depth compare/write + color-target/format/MRT + dynamic offsets — the real architectural work that makes a forward+ renderer expressible.
7. **T1.3 + T1.4** strip index format + stencil contradiction — fix regardless; simply wrong.

The quick wins in Tier 4 (T4.1–T4.3) can be batched anytime; they're independent.

## Dependency notes

- **T1.7 (mipmaps)** needs per-mip texture *views* (currently only the whole-resource view
  exists) — implementing mip generation forces that view work, which also unblocks
  texture readback / per-face views.
- **T2.4 (readback)** retires the leaked `CgfxBuffer.ready` field (a T3-class smell).
- **T2.1 (MRT/offscreen)** and **T1.6 (MSAA)** both argue for a small off-screen
  render-target type feeding pipeline target formats and a generalized pass-begin — do them
  together rather than twice.
- **T1.1 (blend)** and **T2.1 (color targets)** are the same descriptor surface; a
  `CgfxColorTarget[] { format, blend, writeMask }` addition subsumes both.
