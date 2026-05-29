# Tier 3 — Coherence & Convention Drift

These don't break functionality — they raise the cost of *learning and remembering* the API.
Each is a place where a second idiom was added without retrofitting the first, or where the
naming/ownership rule a user must hold in their head isn't learnable. Individually minor;
together they're what makes the API feel arbitrary at the edges. Fix these to make cgfx
*coherent*, not just capable.

⊕ = independently flagged by multiple reviewers.

---

## T3.1 — Wrap-vs-raw policy inconsistent; destroy asymmetric ⊕

**Where:** across modules. Wrapped: `cgfx_shader_create`→`CgfxShader`,
`cgfx_texture_create`→`CgfxTexture`, `cgfx_buffer_create_*`→`CgfxBuffer` (each with a
`cgfx_*_destroy`). Raw: `cgfx_pipeline_create`→`WGPURenderPipeline`,
`cgfx_sampler_create`→`WGPUSampler`, `cgfx_*_bind_group*`→`WGPUBindGroup` (each freed with
`wgpu*Release`).

**What's wrong:** No learnable rule for which things come back wrapped vs raw, and therefore
none for how to free them. `examples/compute/main.c` teardown interleaves three release
conventions in one block (`cgfx_buffer_destroy`, `wgpuBindGroupRelease`,
`wgpuComputePipelineRelease`, `cgfx_shader_destroy`) — the tell that the policy is arbitrary.
A user must memorize, per type, both whether it's wrapped *and* which destroy verb applies.

**Severity:** Major (friction, not breakage).

**Direction:** State an explicit, defensible rule in CLAUDE.md — e.g. *"cgfx wraps objects that
carry metadata (size/format/view); leaf GPU handles stay raw."* Pipeline/sampler/bind-group
genuinely are just handles, so leaving them raw is fine **if documented**. Then add the missing
destroy wrappers for *vocabulary* symmetry even if one-liners: `cgfx_pipeline_destroy`,
`cgfx_sampler_destroy`, `cgfx_bind_group_destroy`. A user should never have to reach for
`wgpu*Release` on something cgfx created.

**Notes:**
- Anchor this on the T1.5 decision — once the error/ownership contract is written down, this
  rule slots in next to it.
- The destroy wrappers are trivial and non-breaking; the *value* is that the API vocabulary
  becomes uniform (`cgfx_*_create` ↔ `cgfx_*_destroy` for everything).

---

## T3.2 — Three bind-group APIs with inverted naming ⊕

**Where:** `cgfx_shader.c` — `cgfx_shader_create_bind_group` (positional, buffer-only),
`cgfx_bind_group_create` (mixed, explicit `.binding`), plus `cgfx_uniform_create` /
`cgfx_camera_create` (create one internally).

**What's wrong:** Two issues.
1. **Naming:** `cgfx_shader_create_bind_group` vs `cgfx_bind_group_create` — opposite
   subject-verb order, breaking the `cgfx_<noun>_<verb>` pattern used everywhere else
   (`cgfx_buffer_create`, `cgfx_texture_create`, …). "How do I make a bind group" is filed
   under *shader* in one case and *bind_group* in the other.
2. **Footgun:** the positional API maps `buffers[i] → @binding(i)`. If a group has bindings at
   0 and 2 (legal in WGSL), or buffers declared out of order, it silently produces a wrong
   bind group or a validation error. And it's the one `cgfx_uniform`/`cgfx_camera` build on.

**Severity:** Minor (the explicit API is the escape hatch) — but the positional footgun is real.

**Direction:** Land on one verb order: `cgfx_bind_group_create*`. Rename
`cgfx_shader_create_bind_group` → `cgfx_bind_group_create_buffers` (or fold it into
`cgfx_bind_group_create` as the buffer-only case). Keep the buffer-only convenience, but in
the same namespace as its sibling. Document the contiguous-from-0 requirement loudly until then.

**Notes:**
- Renames are API-breaking — batch all naming fixes (this + T3.6 loader verbs) into one
  "naming pass" release so consumers update once.
- The strategic dissolver is shader reflection (auto-derive layouts from WGSL) — it removes
  positional-vs-explicit *and* the auto-visibility guessing (T3.5) *and* sampler/texture-type
  mismatches. Big scope; note it as the long-term direction, not this pass.

---

## T3.3 — Two non-mirroring "split lifecycle" idioms

**Where:** frame — `cgfx_frame_begin` (all-in-one) vs `cgfx_frame_begin_encoder` +
`cgfx_frame_begin_render_pass` (separate functions). compute — `cgfx_compute_begin`/`_end`
(owns encoder, submits) vs `cgfx_compute_pass_begin`/`_pass_end` (borrows encoder, doesn't
submit), disambiguated at runtime by a `bool owns_encoder` field.

**What's wrong:** The same concept — "standalone vs borrow-an-encoder" — is expressed two
structurally different ways. A user who learns the frame idiom can't transfer it to compute.

**Severity:** Minor (each works) — raises learning cost.

**Direction:** Pick one shape for both: either an `owns_encoder`-style flag, or a
`_begin`/`_begin_borrowed` naming pair. Apply uniformly to frame and compute.

**Notes:**
- Lower priority than T3.1/T3.2. Best folded into the generalized pass-API work (T2.1/T2.2) —
  if we redesign pass-begin anyway, make the frame and compute lifecycles mirror each other
  while we're there.

---

## T3.4 — Camera hard-assumes one buffer at binding 0

**Where:** `cgfx_camera.c:41-43` (`cgfx_shader_create_bind_group(..., &cam.buffer, 1)` →
positional, single buffer at binding 0)

**What's wrong:** A camera can only live in a group containing exactly one binding, at index 0,
that is a buffer. The moment you want camera + lights + time together in `@group(0)` (the
natural per-frame group), or camera at `@binding(1)`, the camera module can't build the bind
group — and this contradicts the library's own stated principle "caller owns bind groups."

**Severity:** Major as a design coupling, though the transparent struct lets you build the
bind group yourself and assign `cam.bind_group` (escape hatch exists).

**Direction:** Either let `CgfxCameraDesc` take a `binding` index, or — better — decouple:
have the camera own only the *buffer* and let the caller place it in a bind group (mirroring
the `CgfxUniform` philosophy).

**Notes:**
- The "own only the buffer" option is the cleaner long-term shape and makes camera composable
  with per-frame light/time data. It's a small API change to `CgfxCamera`/`CgfxCameraDesc`.
- Depends on the T3.2 bind-group cleanup landing first (so camera builds on the non-positional
  API).

---

## T3.5 — Auto-visibility heuristic silently wrong cross-stage

**Where:** `cgfx_shader.c:98-112` — visibility=0 → texture/sampler=Fragment,
storage_texture=Compute, buffer=Vertex|Fragment.

**What's wrong:** Right for the 80% case, silently wrong for legit uses: a texture sampled in
the **vertex** stage (vertex displacement / heightmap) defaults to Fragment-only → fails; a
uniform used **only in compute** defaults to Vertex|Fragment → invalid for a compute-only
pipeline layout. Because `visibility` *can* be set explicitly, it's a soft default — but
silent-wrong-by-default produces confusing downstream validation errors.

**Severity:** Minor (explicit field is the escape hatch).

**Direction:** Keep the heuristic; document the cross-stage gotchas at the field. Optionally:
when building a compute-only pipeline, default buffers to Compute.

**Notes:**
- Lowest-effort mitigation is a doc comment. The real fix is reflection (see T3.2 note).
- Consider: if a binding's `visibility` is 0 *and* the shader is used only for compute, the
  Vertex|Fragment default is the actively-broken case — worth a targeted special-case even
  before full reflection.

---

## T3.6 — Loader marked "temporary" but public + in umbrella; u16 indices

**Where:** `cgfx_loader.h` ("Temporary module — remove when no longer following the tutorial"),
`cgfx.h` (includes it), `CgfxGeometry.index_data` is `uint16_t`.

**What's wrong:** A temporary tutorial loader sits in the public, `CGFX_API`-exported surface
and the umbrella header, and it uses `uint16_t` indices while the rest of the library
standardized on `uint32_t` (`cgfx_buffer_create_index`, `cgfx_mesh`). It even parses u16 then
widens to u32 to feed `cgfx_mesh_create` — paying 2× index memory to discard 16-bit data it
already had. Naming also drifts: `cgfx_load_geometry` / `cgfx_free_geometry` use
`cgfx_<verb>_<noun>` and `free` instead of the `cgfx_<noun>_<verb>` + `destroy` convention.

**Severity:** Minor (coherence smell).

**Direction:** Drop it from `cgfx.h` (let tutorial examples include `cgfx_loader.h` directly),
or commit to it and align to `uint32_t` + the naming convention. Given it's marked temporary,
removing it from the umbrella is the cleaner call.

**Notes:**
- Decide intent first: is a mesh loader part of cgfx's scope long-term? If yes, the real answer
  is a glTF path and this tutorial loader gets deleted. If no, demote it out of the umbrella now.
- Cheap either way; batch the naming fix with the T3.2 naming pass.

---

## T3.7 — `cgfx_default_limits()` returns all-`0xFF`

**Where:** `cgfx_ctx.c:49` (memset the limits struct to `0xFF`)

**What's wrong:** Works today — `0xFFFFFFFF` is the documented WGPU "undefined" sentinel for
32-bit limit fields — but it's a byte-pattern assumption across a struct of mixed-width fields
and relies on the limit struct containing only integer limits. If WGPU ever adds a
non-integer or differently-sentineled field, it silently breaks.

**Severity:** Minor.

**Direction:** Either a comment pinning the assumption, or field-by-field assignment of
`WGPU_LIMIT_U32_UNDEFINED` / `WGPU_LIMIT_U64_UNDEFINED`.

**Notes:**
- Pure robustness/hygiene. A comment is enough for now; field-by-field if we touch ctx-init for
  T2.3 (features) anyway.
- Also noted in the coherence review: `cgfx_default_limits` is declared *between* the doc block
  for `cgfx_ctx_init` and the function itself (`cgfx_ctx.h:88-98`), splitting them. Minor
  discoverability smell — move the declaration while we're in there.
