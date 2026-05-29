# Tier 2 — Hard Walls

These are common, expected tasks that cgfx makes impossible *through its own API* — you must
either drop entirely to raw WebGPU or you can't do them at all. Unlike Tier 1, these don't
produce silently-wrong output; they're walls. The distinction that matters per item:
**is there a per-call raw-WebGPU escape, or does cgfx monopolize the object you'd need?**

⊕ = independently flagged by multiple reviewers.

---

## T2.1 — Single color target, format-locked to the surface ⊕

**Status: IMPLEMENTED** (with T1.1). Pipeline side: `CgfxColorTarget[]` lets a pipeline target
any format(s) and up to 8 MRT outputs (default stays the single surface-format target). Frame
side: `cgfx_frame_begin_render_pass_ex(CgfxRenderPassDesc)` renders into caller-supplied color
views (offscreen / MRT) with optional depth, and `cgfx_frame_end_render_pass` closes a pass so
multiple passes can run in one frame. New `examples/mrt` exercises a two-target pass sampled
back split-screen. Load-op stays Clear/Store (T2.2) and resolveTarget stays null (T1.6) —
those remain separate. **T4.3 incidentally fixed**: `cgfx_frame_end` now guards against a null
render pass (needed for the multi-pass flow), so compute-only / already-ended frames no longer
deref null.

**Where:** `cgfx_pipeline.c:84-90` (`format = ctx->surface_format`, `targetCount = 1`),
`cgfx_frame.c:105` (one color attachment)

**What's wrong:** A pipeline can only render to a target whose format equals the swapchain
surface format, and only one of them. So:
- **No offscreen render to a different format** — HDR `RGBA16Float`, picking/distance
  `R32Float`, G-buffer `RGBA8Unorm` — even though `CgfxTexture` already supports render-target
  textures. The building blocks exist; the pipeline can't connect to them.
- **No MRT** — deferred shading / G-buffers impossible.

**Severity:** Critical — offscreen rendering is table stakes for a "rendering engine," and the
RT textures already exist. No escape in the desc.

**Direction:** Add `uint32_t color_target_count` + `const CgfxColorTarget *color_targets`
where `CgfxColorTarget { WGPUTextureFormat format; const WGPUBlendState *blend; WGPUColorWriteMask write_mask; }`.
`count == 0` → fall back to the single-surface-format default. The frame side needs a
generalized pass-begin that takes an explicit color-view array (the surface view being the
default case).

**Notes:**
- **Subsumes T1.1** (blend) and the hardcoded write-mask in one stroke — do them together.
- **Pairs with T1.6** (MSAA resolve target) — the same attachment surface carries the resolve
  target. Design the color-attachment/render-target type *once* for T2.1 + T1.6.
- This is the biggest architectural item in the set. The surface-bound `cgfx_frame_*` API is
  fine for the swapchain pass; the fix is to stop making it the *only* pass API. Suggest a
  small `CgfxRenderTarget` (color views + optional depth) that both the pipeline (formats) and
  a generalized pass-begin consume.

---

## T2.2 — No load-op control: can't preserve target contents

**Where:** `cgfx_frame.c:82` (color `loadOp=Clear`), `cgfx_frame.c:94` (depth `loadOp=Clear`)

**What's wrong:** Every `begin_render_pass` clears. `clear_color` is mandatory; there is no
`Load` variant. So you cannot composite multiple passes onto one frame:
- **UI over 3D** — pass B (UI) clears the scene from pass A.
- **Multi-pass accumulation** — additive/deferred passes reading prior color.
- **Partial / dirty-rect redraw** — can't preserve last frame.

The encoder/render-pass split (the only multi-pass primitive) still always clears.

**Severity:** Critical — compositing passes is a basic engine need and the API actively
prevents it. Raw-WebGPU only.

**Direction:** Add load-op control. Minimal: a `bool load` / `WGPULoadOp load_op` plus an
optional "begin render pass" that takes `Load` and no clear color. Better: a small
`CgfxRenderPassDesc { load_op, store_op, clear_color, optional color view }` so multi-pass is
first-class.

**Notes:**
- Interacts with T2.1 and T4.3 — all three touch the pass-begin/end surface. The clean move is
  one generalized pass API: explicit color views (T2.1) + load/store ops (T2.2) + a guarded
  end (T4.3). Consider designing that pass API as a unit.
- Also interacts with the begin/end asymmetry noted in the frame review: there's a public
  `begin_render_pass` but `frame_end` only ends one pass, so two *render* passes in one frame
  can't be done through the wrapper. A symmetric `cgfx_frame_end_render_pass` would round this out.

---

## T2.3 — No device feature-request path ⊕

**Where:** `cgfx_ctx.c:119` (`requiredFeatureCount = 0`); `CgfxCtxDesc`/`CgfxCtxExternalDesc`
expose only `limits`

**What's wrong:** No optional WebGPU feature can ever be enabled — `timestamp-query`,
`float32-filterable`, `depth32float-stencil8`, texture compression (`texture-compression-bc`/
`-etc2`/`-astc`), `indirect-first-instance`, etc. The device is created entirely inside
`cgfx__init_from_surface` and never exposed for re-request, and you can't add features to an
already-created `WGPUDevice`. A true monopoly, not a thin-wrapper escape hatch.

**Severity:** Critical — an entire category of WebGPU capability is unreachable. (It also makes
m1/T-profiling and T2.4-compressed-textures permanently unfixable until this lands.)

**Direction:** Add `const WGPUFeatureName *features; uint32_t feature_count;` to both ctx
descs; pass into the device descriptor. Cheap, mechanical, removes a hard ceiling.

**Notes:**
- **Highest leverage-to-effort ratio in the whole review.** Do early. Bundle with T4.4
  (device-lost callback) — same descriptor surface, same "the device desc needs more inputs"
  shape.
- Consider also surfacing the *adapter* options (power preference, force-fallback) on the same
  pass, since adapter selection is equally monopolized (`compatibleSurface` only). Lower
  priority than features.

---

## T2.4 — Async readback unwrapped; backend `#ifdef` leaks into user code ⊕

**Status: IMPLEMENTED.** Added `bool cgfx_buffer_read(ctx, buf, out, size)` - a synchronous
map-read helper following the established sync-wrapper pattern from `cgfx_internal.h`. Maps
the buffer, memcpys into caller-owned output, unmaps. Backend `#ifdef` (wgpuDevicePoll vs
wgpuDeviceTick) is now inside the library. Both examples (`compute`, `playing_with_buffers`)
updated to use it - the `on_buffer_mapped` callbacks, poll loops, and raw `wgpu*` includes
are gone. `CgfxBuffer.ready` field removed (only existed for the user-written callback
pattern). `cgfx_buffer_create_mapping` phantom `data` param also removed (T4.2). Runtime
verified: compute prints "All 256 results correct!", buffers prints correct readback.

**Where:** examples `compute/main.c:99-129`, `playing_with_buffers/main.c:45-58`; pattern
exists but unused at `cgfx_internal.h` (`cgfx__request_adapter_sync` / `_device_sync`)

**What's wrong:** The sync-wrapper pattern (issue async call → poll → block) is already
established in-tree for adapter/device but was **never applied to buffer mapping**. So every
compute/readback user hand-reimplements the same ~25-line dance: `wgpuBufferMapAsync` +
callback + backend `#ifdef` (`wgpuDeviceTick` vs `wgpuDevicePoll`) + `GetConstMappedRange` +
`Unmap`. The two example copies even disagree (one polls `true`, the other `false`). This
directly violates CLAUDE.md's promise that backend differences are "handled inside the
library," in the one workflow where it's unavoidable — and readback is the whole point of
compute.

**Severity:** Critical for the compute story; the most-duplicated code in the examples.

**Direction:** Add
`bool cgfx_buffer_read(const CgfxCtx *ctx, const CgfxBuffer *mapping_buf, void *out, uint64_t size)`
that maps-sync, memcpy's out of the mapped range, and unmaps. The pattern is proven in
`cgfx_internal.h`; ~30-min extraction.

**Notes:**
- **Retires the leaked `CgfxBuffer.ready` field** (a T3-class smell that only exists to
  support the user-written callback) and the phantom `data` param in
  `cgfx_buffer_create_mapping` (**T4.2**) becomes moot once a real read path exists.
- Top-3 fix overall: closes the abstraction leak, deletes the worst boilerplate, and cleans
  up two other findings as a side effect.
- The compute module's two-tier design is the model: a convenience `cgfx_buffer_read` (own
  sync) is the standalone tier; consider a non-blocking variant later if needed.

---

## T2.5 — Sampler binding type hardcoded to `Filtering`

**Where:** `cgfx_shader.c:132-135` (`CGFX_BINDING_SAMPLER` → `WGPUSamplerBindingType_Filtering`)

**What's wrong:** WebGPU's `GPUSamplerBindingType` is `filtering` / `non-filtering` /
`comparison`. cgfx can only declare `filtering`. **Shadow mapping** needs a `comparison`
sampler bound to a layout entry of type `comparison` (used with `textureSampleCompare`) —
impossible via the desc path. `non-filtering` (required when sampling non-filterable formats
like `R32Float` data textures) is also impossible. `CgfxBindingDesc` has dedicated
`sample_type`/`view_dimension` fields for textures but **none for sampler type**.

**Severity:** Major — hard blocker for shadows; no desc-level escape (must drop to raw
`wgpuDeviceCreateBindGroupLayout`).

**Direction:** Add `WGPUSamplerBindingType sampler_type` to `CgfxBindingDesc` (0 → Filtering).
One line in the switch.

**Notes:**
- Trivial, backward compatible. Good candidate to bundle with other small binding-desc
  additions (T2.7 dynamic offsets) since they touch the same struct.
- Note the sampler *object* (`cgfx_sampler_create`) already exposes `compare` — so you can make
  a comparison *sampler* but can't declare a matching *layout entry*. That asymmetry is the tell.

---

## T2.6 — No instancing in `cgfx_mesh_draw`

**Where:** `cgfx_mesh.c:38` (`DrawIndexed(index_count, instanceCount=1, 0,0,0)`)

**What's wrong:** `instanceCount` is hardwired to 1. No instance count, no first-index/
base-vertex (submesh) range, no non-indexed `Draw`, no indirect. Instanced rendering
(particles, grass, tiles, "draw 10k of this mesh") is the single most impactful perf
primitive WebGPU offers, and the draw helper can't express it — so the case where meshes
matter most must bypass `cgfx_mesh_draw`.

**Severity:** Major — instancing absence is a real wall. (Non-indexed / indirect are fine as
raw escape hatches; they're genuinely niche.)

**Direction:** Add `cgfx_mesh_draw_instanced(pass, mesh, instance_count)` (and optionally a
sub-range variant for submeshes). Leave non-indexed/indirect to raw WebGPU.

**Notes:**
- Cheap and non-breaking (new function). The instance *data* still needs a second vertex
  buffer with `stepMode=Instance` — which ties to the missing vertex-layout builder (T3-class,
  m9 in the binding review). Instanced draw is usable today with a hand-built instance layout;
  this helper just removes the `instanceCount` hardcode.

---

## T2.7 — No dynamic offsets / sub-buffer ranges

**Where:** `cgfx_shader.c:196-198,230-231` (every entry `offset=0, size=buffer.size`);
`cgfx_shader.c:255,264` (`SetBindGroup` always `dynamicOffsetCount=0`); `cgfx_uniform.c`
(one buffer + one bind group per uniform)

**What's wrong:** The documented headline pattern — "same shader, different uniforms per
object" — is implemented as one `WGPUBuffer` **and** one `WGPUBindGroup` per object. WebGPU's
intended path (one big uniform buffer with `hasDynamicOffset: true` layout entries + per-draw
`dynamicOffsets` in `setBindGroup`) is entirely absent. You also can't bind a sub-range of a
buffer (offset/size forced), so even manual sub-allocation into one buffer is blocked.

**Cost:** 10,000 objects → 10,000 tiny GPU buffers (each rounded to 256-byte alignment) +
10,000 bind groups + 10,000 from-scratch `setBindGroup` calls. Dynamic offsets would keep one
buffer, one bind group, and a changing per-draw offset.

**Severity:** Major for any non-trivial scene; Minor for the handful-of-objects examples
shipped today.

**Direction:**
1. `bool has_dynamic_offset` on `CgfxBindingDesc`;
2. `offset`/`size` on `CgfxBindGroupEntry`;
3. a `cgfx_shader_bind_dynamic(pass, groups, offsets, count)` (or extend `cgfx_shader_bind`).

**Notes:**
- Highest-leverage *perf* fix. Not urgent for correctness, but it's the difference between
  "renders a few objects" and "renders a scene."
- Bundle the binding-desc field additions with T2.5 (sampler type) — same struct, one editing
  pass.

---

## T2.8 — Black-window-on-resize: surface status collapsed to pass/fail

**Where:** `cgfx_frame.c:31` (`cgfx__get_surface_texture_view` treats anything `!= Success`
as "return NULL → skip frame")

**What's wrong:** wgpu-native distinguishes `Success` / `Timeout` / `Outdated` / `Lost` /
`OutOfMemory` / `DeviceLost`. `Outdated`/`Lost` specifically mean "the surface config is stale
— reconfigure and retry," which is exactly what happens after a resize, monitor/DPI change, or
compositor event. cgfx neither reconfigures nor retries; it drops the frame, and if the
surface stays Outdated (common until reconfigure), it drops *every* frame → frozen/black
window. The status info is right there in `surface_texture.status` and is discarded.

Compounding: `cgfx_ctx_resize` exists but the docs make the caller wire it via a GLFW
callback; without auto-reconfigure-on-Outdated there's still a one-frame failure window.

**Severity:** Major — the classic "resize my window and it goes black" bug, with no built-in
recovery.

**Direction:** In the acquire helper, on `Outdated`/`Lost` call
`cgfx__configure_surface(ctx, ctx->width, ctx->height)` and retry once before returning NULL.
Optionally surface the status to the caller (return an enum) so they can distinguish "skip
this frame" (Timeout/minimized) from "fatal" (DeviceLost/OutOfMemory) — today
`cgfx_frame_begin` returning `false` is ambiguous across all of these.

**Notes:**
- The reconfigure-and-retry is self-contained and high-value — good standalone fix.
- Returning a status enum from frame-begin is a small API change; worth it for robustness but
  can be a follow-up. Pairs conceptually with T4.4 (device-lost handling) — together they make
  the frame loop survivable in production.
