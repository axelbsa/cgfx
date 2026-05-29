# Tier 4 — Outright Defects (Quick Wins)

Plain bugs and lies-in-the-signature. None are design questions — they're wrong and should be
fixed regardless of any larger direction. All are small and independent; batch them whenever.

---

## T4.1 — Index buffer mislabeled `"cgfx vertex buffer"`

**Status: FIXED.** Label changed to `"cgfx index buffer"`.

**Where:** `cgfx_buffer.c:51`

**What's wrong:** `cgfx_buffer_create_index` labels its WGPU buffer `"cgfx vertex buffer"`
(copy-paste from the vertex creator). The label is what shows up in GPU capture/debug tools
(RenderDoc, PIX, wgpu tracing), so index buffers are indistinguishable from vertex buffers
there.

**Severity:** Minor — cosmetic, but actively misleading during debugging.

**Fix:** Change the label string to `"cgfx index buffer"`. One line.

**Notes:**
- Zero risk, zero API change. Trivially safe to include in any commit.

---

## T4.2 — `cgfx_buffer_create_mapping` ignores its `data` parameter

**Status: FIXED.** Removed `const void *data` from the signature. Callers updated (both
passed `nullptr`). Done alongside T2.4.

**Where:** `cgfx_buffer.c:99` (`(void) data;`)

**What's wrong:** The signature takes `const void *data` but the body explicitly discards it.
A caller passing initial data is silently no-op'd — and it couldn't work anyway: a
`MapRead|CopyDst` buffer isn't meaningfully `writeBuffer`-targeted at creation. The parameter
is a lie in the API.

**Severity:** Minor — but a misleading signature invites real misuse.

**Fix:** Remove `data` from the signature (preferred), or document hard that it's ignored and
why. Removing it is the honest fix.

**Notes:**
- Removing the param is technically API-breaking for anyone who passes it (they pass a pointer
  that does nothing today, so behavior is unchanged — only the signature shifts). Batch with
  the T2.4 readback work, which makes this creator's role clearer anyway (mapping buffers exist
  to be filled by `cgfx_buffer_copy` then read by `cgfx_buffer_read`).

---

## T4.3 — `cgfx_frame_end` null-derefs on a compute-only frame

**Status: FIXED** (incidentally, with T2.1). `cgfx_frame_end` now ends the render pass only
`if (frame->render_pass)`, and the new `cgfx_frame_end_render_pass` nulls it between passes.

**Where:** `cgfx_frame.c:124` (`wgpuRenderPassEncoderEnd(frame->render_pass)` unconditional);
`cgfx_frame.c:70` (`begin_encoder` leaves `frame->render_pass = nullptr`)

**What's wrong:** If a user calls `cgfx_frame_begin_encoder` (no render pass) — e.g. a
compute-only frame, a documented use of the split API — and then `cgfx_frame_end`, the end
path calls `wgpuRenderPassEncoderEnd(nullptr)`, a likely crash / validation error. No guard.

**Severity:** Minor (latent crash on a supported path).

**Fix:** Guard `frame_end` against a null `render_pass` (only end it if non-null). While there,
consider the symmetric `cgfx_frame_end_render_pass` so multi-render-pass frames work through
the wrapper (ties to T2.2).

**Notes:**
- The guard is a 2-line safety fix, fully backward compatible — do it now.
- The symmetric end function is the larger pass-API question (T2.2/T3.3); keep that separate.

---

## T4.4 — No device-lost / uncaptured-error user hook

**Where:** `cgfx_ctx.c:26-44` (both callbacks only `fprintf(stderr, ...)`, `user_data=nullptr`)

**What's wrong:** Device-lost and uncaptured-error callbacks just print to stderr; no
`user_data` is forwarded, no callback field in the ctx desc, no recovery. On a GPU reset
(driver TDR, laptop GPU switch, device removed) or a production validation error, the app keeps
running blind — every subsequent GPU call silently no-ops/errors and the only signal is stderr
text. Combined with T2.8, a `DeviceLost` from the surface is invisible to app logic.

**Severity:** Major for any non-toy app (listed in Tier 4 because the *fix* is small and
mechanical, like the other quick wins).

**Fix:** Add optional callback fields (+ `void *user_data`) to `CgfxCtxDesc` for device-lost
and uncaptured-error; default to the current stderr behavior when unset.

**Notes:**
- Same descriptor surface and the same "device desc needs more inputs" shape as **T2.3**
  (features) — do them in one ctx-desc pass.
- Pairs with **T2.8**: surfacing `DeviceLost` from the frame loop + a user callback is what
  makes the render loop survivable in production. Together they're the "robust app" story.
