# Tier 1 — Wrong or Incoherent

These are the highest-priority findings. They are not "missing features" — they either
**fail silently** (a default that produces visibly wrong output with no error) or are
**internally self-contradictory** (a descriptor that violates the WebGPU spec, or an API
contract that contradicts itself). Several have *no escape hatch* short of abandoning the
cgfx abstraction.

⊕ = independently flagged by multiple reviewers.

---

## T1.1 — Hardcoded alpha blend on every pipeline ⊕

**Status: IMPLEMENTED** (with T2.1). Added `CgfxColorTarget { format; blend_enable; blend;
write_mask; }` and `color_target_count`/`color_targets` to `CgfxPipelineDesc`. The default
(count 0) is now a single **opaque** target at the surface format; per-target blend is opt-in
via `blend_enable` with presets `cgfx_blend_alpha/additive/premultiplied`. Audited all example
shaders — every fragment outputs `alpha = 1.0`, so the alpha→opaque default flip is visually
identical for them.

**Where:** `cgfx_pipeline.c:71-86`

**What's wrong:** `color_target.blend = &blend_state` is attached *unconditionally* to every
pipeline. `blend_state` is always `SrcAlpha / OneMinusSrcAlpha` (add) for color and
`Zero / One` for alpha. There is no field in `CgfxPipelineDesc` to control or disable it.
The header comment claims this "works for both opaque and transparent" — false:

- Alpha blending reads the destination. Any opaque shader emitting `vec4(color, a)` with
  `a < 1.0` (common when alpha carries packed/non-color data) gets unintended transparency.
- The alpha-channel blend `Zero/One` means the source never writes framebuffer alpha
  (output alpha = dst alpha always). Any offscreen render target you later sample for its
  alpha, or any premultiplied/compositing workflow, is silently broken.

**Breaks:** opaque 3D with data in alpha (ghosting); additive particles (impossible);
render-to-texture with meaningful alpha; premultiplied UI atlases.

**Severity:** Critical — silently wrong for the *most common* case (opaque), no escape hatch
short of bypassing `cgfx_pipeline_create`.

**Direction:** Add `const WGPUBlendState *blend` to `CgfxPipelineDesc` (NULL = no blend =
opaque). Default should be **opaque** (matches WebGPU, where `blend` is optional/absent).
Provide presets: `CGFX_BLEND_NONE`, `CGFX_BLEND_ALPHA`, `CGFX_BLEND_ADDITIVE`,
`CGFX_BLEND_PREMULTIPLIED`.

**Notes:**
- API-breaking *behavior* change: existing examples that rely on the implicit alpha blend
  (if any draw translucent geometry) will change. Most examples are opaque, so flipping the
  default to opaque is almost certainly *more* correct for them. Audit examples when we do this.
- Best combined with **T2.1** — both live on the color-target descriptor surface. A single
  `CgfxColorTarget { format, blend, write_mask }` array addition fixes blend, per-target
  blend, format selection, MRT, and write mask in one coherent stroke.
- Cheapest interim (non-breaking): add the field, keep current alpha blend as the zero
  default. But I'd argue for the opaque default — silently-wrong is worse than a one-time
  example fixup.

---

## T1.2 — `depthCompare=Less` + `depthWriteEnabled=true` hardcoded

**Where:** `cgfx_pipeline.c:109-110`

**What's wrong:** Whenever `depth_test` is true, compare is always `Less` and depth write is
always `true`. No control over either.

- **Skybox:** drawn at the far plane (depth ≈ 1.0) needs `LessEqual` to survive the depth
  clear; with `Less` it's rejected and vanishes.
- **Depth pre-pass / multi-pass forward:** the color pass wants `Equal` + write-disabled
  (read-only depth). Transparent passes want test-on / write-off so translucents don't
  occlude each other. Both impossible.
- **Reverse-Z:** the build sets `CGLM_FORCE_DEPTH_ZERO_TO_ONE` (verified, `cgfx/CMakeLists.txt`),
  the exact [0,1]-depth setup where reverse-Z (`Greater` + clear-to-0) is the recommended
  precision fix. Hardcoded `Less` forecloses it.

**Severity:** Major — skybox and read-only-depth transparency are common; no escape hatch in
the desc.

**Direction:** Add `WGPUCompareFunction depth_compare` (0 → Less) and `bool depth_write_disabled`
(or a tri-state) to `CgfxPipelineDesc`. One-liners to thread through.

**Notes:**
- Fully backward compatible if zero stays Less + write-enabled.
- `depthClearValue` is also hardcoded to 1.0 in `cgfx_frame.c:94` — reverse-Z needs clear-to-0,
  so a complete reverse-Z story touches the frame clear too. Flag when we get there; for now
  the pipeline fields are the 80%.

---

## T1.3 — `topology` settable but `stripIndexFormat=Undefined` hardcoded

**Where:** `cgfx_pipeline.c:46-47`

**What's wrong:** A genuine **coherence bug**, not a missing feature. `topology` is taken from
the user, but `stripIndexFormat` is always `Undefined`. Per the WebGPU spec, an *indexed*
draw with a strip topology (`TriangleStrip`/`LineStrip`) requires `stripIndexFormat` to be
set (it defines the primitive-restart value). Left Undefined, an indexed strip draw is a
validation error — caused by a field cgfx hides. The API offers a knob that cannot be used
correctly.

**Severity:** Major (coherence). Either the knob works or it shouldn't be offered.

**Direction:** Either add `WGPUIndexFormat strip_index_format` to the desc, **or** auto-derive
`Uint32` when topology is a strip. Deriving is friendlier and keeps the desc small — and
since meshes are u32-indexed everywhere, `Uint32` is the obviously-correct derivation.

**Notes:**
- I lean toward auto-derive: it makes the existing `topology` knob correct with zero new
  surface area. Revisit if/when u16 indices are ever supported (T3.6 / m1).

---

## T1.4 — Stencil attachment is internally contradictory

**Where:** `cgfx_frame.c:97-100` (attachment); `cgfx_ctx.c:157` (format locked to Depth24Plus)

**What's wrong:** When a depth texture exists, the depth-stencil attachment sets
`stencilLoadOp=Clear`, `stencilStoreOp=Store`, `stencilClearValue=0` **and**
`stencilReadOnly=true` simultaneously. Per spec, when `stencilReadOnly` is true the stencil
load/store ops must be Undefined — specifying Clear+Store on a read-only stencil is a
validation error. It's currently masked only because the depth format is hardcoded to
`Depth24Plus`, which has **no stencil aspect**, so the stencil fields are ignored. Switch the
depth texture to `Depth24PlusStencil8` and the attachment becomes invalid.

Second half: `depth_buffer` is a bool (`cgfx_ctx.h`) with the format hardcoded — there is no
way to request a stencil format through ctx at all, so stencil techniques (outlines, masking,
portals, decals) are unreachable from the managed depth buffer.

**Severity:** Major — latent-wrong (a self-contradictory descriptor) and blocks all stencil
work through the managed path.

**Direction:**
1. Fix the contradiction now: derive stencil config from the actual format. No stencil aspect
   → leave stencil ops Undefined and keep `stencilReadOnly` consistent. Has stencil aspect →
   set load/store and `stencilReadOnly=false`.
2. Replace `bool depth_buffer` with an optional depth *format* (or add a `depth_format` field)
   so `Depth24PlusStencil8` is selectable, and keep the ctx depth texture and the pipeline's
   `depth_format` in agreement.

**Notes:**
- The contradiction fix (step 1) is a pure correctness fix with no API change — do it even if
  we defer step 2.
- Pipeline `depth_format` (`cgfx_pipeline.h`) can currently be set to a stencil format while
  the ctx depth texture is Depth24Plus → mismatch. Step 2 should make these derive from one
  source of truth.

---

## T1.5 — Error model is broken and contradicts its own contract ⊕

**Status: IMPLEMENTED.** Added a trailing `bool ok;` to the 6 wrapped structs
(`CgfxShader`/`Buffer`/`Texture`/`Mesh`/`Uniform`/`Camera`), set on the success path of every
constructor and propagated through the internal chains (mesh→buffers, uniform/camera→buffer+
bind_group, ctx→depth texture). `cgfx_shader_create` now captures WGSL compile errors via a
synchronous `wgpuShaderModuleGetCompilationInfo` wrapper and prints them with source
line/column. Raw-handle constructors keep `NULL`-on-failure. CLAUDE.md contract updated to
match. Non-breaking (additive field).

**Where:** `cgfx_shader.c:80` (no compile check), `cgfx_shader.c:172` (returns `(CgfxShader){}`);
CLAUDE.md ("functions return bool, errors go to stderr")

**What's wrong:** Only a handful of functions return `bool` (`cgfx_ctx_init`, `cgfx_frame_begin`,
`cgfx_compute_begin`, `cgfx_ctx_resize`). **Every constructor that matters returns a struct by
value or a raw handle with no error channel:** `cgfx_shader_create`, `cgfx_buffer_create_*`,
`cgfx_mesh_create`, `cgfx_texture_create`, `cgfx_uniform_create`, `cgfx_camera_create`
(by-value), and `cgfx_pipeline_create` / `cgfx_compute_pipeline_create` (NULL handle). On
failure they return a zeroed struct, often after printing to stderr, with no `.ok`/`.valid`
flag. **Shader compile errors are never surfaced** — `cgfx_shader_create` just calls
`create_module` and returns; a WGSL syntax error yields a poisoned/NULL module that looks
identical to a good one.

Verified in `examples/multiple_uniforms/main.c`: it calls `cgfx_shader_create_from_file` then
immediately `cgfx_pipeline_create(...)` with no check *possible*. Missing file or malformed
WGSL → the program marches into pipeline creation with a zero module and the user gets a wall
of WebGPU stderr with no clean failure point.

**Severity:** Critical. The user explicitly asked "does the API even make sense" — the error
model does not, and it violates the library's own stated contract.

**Direction (pick one, apply uniformly):**
- **Cheapest, most C-idiomatic:** add `bool ok;` to every wrapped struct (`CgfxShader`,
  `CgfxBuffer`, `CgfxTexture`, `CgfxMesh`, `CgfxUniform`, `CgfxCamera`); document `if (!x.ok)`.
  `CgfxBuffer` already has a stray `bool ready;` so the status-bit precedent exists.
- **Contract-matching:** switch constructors to out-param + `bool` return
  (`bool cgfx_shader_create(ctx, ..., CgfxShader *out)`). More invasive but matches CLAUDE.md
  verbatim.
- Either way: **capture WGSL compilation errors** (wgpu-native exposes a compilation-info
  callback) and set the failure flag. Then update CLAUDE.md so the contract matches reality.

**Notes:**
- This is the foundation finding — pick the model here and the rest of the codebase follows.
  My recommendation: the `bool ok;` field approach. It keeps the by-value/zero-init ergonomics
  that are the best part of the API, is minimally invasive, and the `ready` field precedent
  means it won't look foreign.
- Pipeline/compute-pipeline return raw handles; for those, NULL is already a usable signal —
  just document it and make sure internal failures return NULL rather than a poisoned handle.
- Decide the policy once and write it into CLAUDE.md as the canonical contract; this is also
  the anchor for the T3.1 wrap/raw cleanup.

---

## T1.6 — MSAA looks supported but is impossible end-to-end

**Where:** `cgfx_pipeline.c:129` (`multisample.count=1`, no desc field), `cgfx_frame.c:81`
(`resolveTarget=nullptr`); `CgfxTextureDesc.sample_count` *is* wired through.

**What's wrong:** You can create a 4× multisampled texture (the field exists and reaches
`texture_desc.sampleCount`), but the other two halves of an MSAA pipeline are hardcoded
against it: the pipeline's sample count is fixed at 1 (and `CgfxPipelineDesc` has no field for
it), and the frame's color attachment never sets a resolve target (and always uses the
single-sampled surface view). A pipeline rendering into a 4× texture is invalid per spec, and
there is no path to resolve an MSAA texture into the surface. The feature advertises itself in
the public texture desc + docs and dead-ends.

**Severity:** Critical — the single most common quality knob, partially exposed as a trap.

**Direction:** Add `sample_count` to `CgfxPipelineDesc` (→ `multisample.count`). Then either
(a) store an optional MSAA color target on the ctx and auto-set `resolveTarget` to the surface
view in `begin_render_pass`, or (b) let the frame API specify color attachment view + resolve
target. Minimum viable: pipeline `sample_count` + a `resolve_target` field on a frame-config
struct.

**Notes:**
- Couple this with **T2.1** (offscreen render targets) — MSAA-to-surface resolve is naturally
  expressed through the same generalized color-attachment / render-target type. Doing them
  together avoids designing the attachment surface twice.
- Lower-effort stopgap if we want to de-trap it sooner: add `sample_count` to the pipeline desc
  *and* document that MSAA currently requires the raw frame path — at least the pieces stop
  silently contradicting each other.

---

## T1.7 — Mipmaps: the sampler is configured for content the API can't produce

**Where:** `cgfx_texture.c` (writes mip level 0 only; `dest.mipLevel = 0` hardcoded);
`cgfx_sampler_create` (default `mipmapFilter=Linear`, `lodMaxClamp=32`); view created with
`mipLevelCount = mip_levels`.

**What's wrong:** Nothing in the texture module generates mips. `cgfx_texture_write` /
`_write_layer` only write mip 0. So:

- Set `mip_levels > 1` → levels 1..N are allocated but never filled → sampling them yields
  garbage/black. `mip_levels > 1` is therefore *worse* than the default.
- Leave `mip_levels = 1` (default) → `lodMaxClamp=32` and trilinear filtering are inert (no
  mips to sample). Distant/minified surfaces shimmer and alias.

The "good defaults" actively mislead.

**Severity:** Critical — the largest incoherence in the texture module; the sampler is set up
for trilinear/aniso against content that can never exist via the public API.

**Direction:** Add `cgfx_texture_generate_mips()` (a render-pass blit-down chain, or a compute
downsample). WebGPU has **no** built-in mip generation, so this must be library-provided or
every user reinvents it (wrong). At minimum, document loudly that `mip_levels > 1` requires
manual per-level uploads, and don't default `lodMaxClamp=32` when only mip 0 is reachable.

**Notes:**
- **Dependency:** mip generation needs *per-mip texture views* as render targets for each
  downsample step. Today only the whole-resource default view exists. So this work naturally
  pulls in per-mip/per-layer view creation — which also unblocks texture readback and
  cube-face views later. Sequence T1.7 after (or together with) that view work.
- This is more implementation effort than the other Tier-1 items; it's Tier 1 because of the
  *silently-wrong* defaults, but the *fix* is a small feature, not a one-liner. We may split
  it: (a) immediate doc/default correction (cheap), (b) `generate_mips` (later).
