# Buffers, Layouts, and the Pipeline

A guide to the different buffer-related types in cgfx / WebGPU and how
they fit together.

## The core insight

**Buffers are DATA. Layouts are DESCRIPTION. They are matched at draw
time by slot number.**

They start as completely separate things -- the data lives on the GPU,
the description is baked into the pipeline at creation. Neither knows
about the other until a draw call binds them together by slot.

This decoupling is what lets a single pipeline draw 1000 different
meshes: each `SetVertexBuffer` call swaps the data, the description
stays the same.

## End-to-end flow

```
┌──────────────────────────────────────────────────────────────────┐
│                          CPU — DATA                              │
│                                                                  │
│   CgfxVertex vertex[4] = { ... };                                │
│                                                                  │
│   memory layout (96 B per vertex):                               │
│   ┌─────┬─────┬─────┬─────┬─────┬─────┬──────┬──────┐            │
│   │ pos │ nrm │ tan │ uv0 │ uv1 │ col │ jnts │ wgts │ ...        │
│   │ 12B │ 12B │ 16B │  8B │  8B │ 16B │  8B  │ 16B  │            │
│   └─────┴─────┴─────┴─────┴─────┴─────┴──────┴──────┘            │
│      vertex 0                                                    │
└────────────────────────────────┬─────────────────────────────────┘
                                 │ cgfx_mesh_create()
                                 │  └─ cgfx_buffer_create_vertex()
                                 │      └─ wgpuQueueWriteBuffer()
                                 ▼
┌──────────────────────────────────────────────────────────────────┐
│                       GPU — DATA                                 │
│                                                                  │
│   CgfxBuffer (CPU-side wrapper struct)                           │
│   ┌────────────────────────────────┐                             │
│   │ WGPUBuffer  buffer; ───────────┼──► [v0][v1][v2][v3] (VRAM)  │
│   │ uint64_t    size = 176;        │                             │
│   │ uint32_t    count = 4;         │                             │
│   └────────────────────────────────┘                             │
└──────────────────────────────────────────────────────────────────┘


┌──────────────────────────────────────────────────────────────────┐
│                CPU — DESCRIPTION (no data, just shape)           │
│                                                                  │
│   WGPUVertexAttribute attrs[8] = {                               │
│     { Float32x3, offset =  0, shaderLocation = 0 }, ◄ position   │
│     { Float32x3, offset = 12, shaderLocation = 1 }, ◄ normal     │
│     { Float32x4, offset = 24, shaderLocation = 2 }, ◄ tangent    │
│     { Float32x2, offset = 40, shaderLocation = 3 }, ◄ texcoord0  │
│     { Float32x2, offset = 48, shaderLocation = 4 }, ◄ texcoord1  │
│     { Float32x4, offset = 56, shaderLocation = 5 }, ◄ color      │
│     { Uint16x4,  offset = 72, shaderLocation = 6 }, ◄ joints     │
│     { Float32x4, offset = 80, shaderLocation = 7 }, ◄ weights    │
│   };                                                             │
│           │                                                      │
│           ▼ "these attributes belong to ONE buffer"              │
│   WGPUVertexBufferLayout layout = {                              │
│     arrayStride    = 96,        ── how far between vertices      │
│     stepMode       = Vertex,    ── advance per-vertex            │
│     attributes     = attrs,                                      │
│     attributeCount = 8,                                          │
│   };                                                             │
│           │                                                      │
│           ▼ "the pipeline will receive THIS shape at slot 0"     │
│   cgfx_pipeline_create(&ctx, &(CgfxPipelineDesc){                │
│       .vertex_layouts      = &layout,                            │
│       .vertex_buffer_count = 1,                                  │
│   });                                                            │
│           │                                                      │
│           ▼                                                      │
│   ┌──────────────────────┐                                       │
│   │  WGPURenderPipeline  │  layout is BAKED IN at creation       │
│   └──────────────────────┘                                       │
└──────────────────────────────────────────────────────────────────┘


┌──────────────────────────────────────────────────────────────────┐
│                  DRAW TIME — they finally meet                   │
│                                                                  │
│   SetPipeline(pass, pipeline);                                   │
│      └─► GPU: "use THIS description for vertex slots"            │
│                                                                  │
│   SetVertexBuffer(pass, slot=0, mesh.vertex_buffer.buffer);      │
│      └─► GPU: "DATA at slot 0 is THIS WGPUBuffer"                │
│                                                                  │
│   SetIndexBuffer(pass, mesh.index_buffer.buffer, Uint32);        │
│   DrawIndexed(pass, index_count, 1, 0, 0, 0);                    │
│      └─► For each index i:                                       │
│           offset    = i * arrayStride          (96 B)            │
│           position  = buffer[offset +  0 .. +12]                 │
│           normal    = buffer[offset + 12 .. +24]                 │
│           tangent   = buffer[offset + 24 .. +40]                 │
│           ...                                                    │
│           → fed to shader @location(0..7)                        │
└──────────────────────────────────────────────────────────────────┘
```

## Type cheat sheet

| Thing | Lives where | Carries | When created |
|---|---|---|---|
| [`CgfxBuffer`](../api/buffer.md) / `WGPUBuffer` | GPU memory | the actual bytes | per-mesh, runtime |
| `WGPUVertexAttribute` | CPU struct | "field X is at offset Y, format Z, shader location L" | once, baked into pipeline |
| `WGPUVertexBufferLayout` | CPU struct | "buffer at slot N has stride S and these attributes" | once, baked into pipeline |
| `WGPURenderPipeline` | GPU object | the layouts + shader + blend + ... | once at startup |

## Using `cgfx_mesh_vertex_layout()`

For [`CgfxVertex`](../api/mesh.md)-based meshes, cgfx provides a pre-built layout that is
guaranteed to match the struct:

```c
WGPUVertexBufferLayout layout = cgfx_mesh_vertex_layout();

WGPURenderPipeline pipeline = cgfx_pipeline_create(&ctx, &(CgfxPipelineDesc){
    .shader = &shader,
    .vertex_buffer_count = 1,
    .vertex_layouts = &layout,
});
```

This returns a layout with stride `sizeof(CgfxVertex)` (96 bytes) and 8
attributes at locations 0--7. The returned pointer references static
internal storage and is valid for the program's lifetime.

## A common bug

!!! bug "Stride mismatch"
    If `arrayStride` (description) disagrees with the actual stride of the
    data in the bound buffer, the GPU will dutifully march at the stride
    you told it to -- through records of a different size. The two sides
    disagree about the shape, and the GPU has no way to detect that.

Example: a layout with `arrayStride = 8` (two floats) bound against a
buffer of 96-byte `CgfxVertex` structs will produce a few correct
vertices and then garbage, because the pipeline reads 8 bytes at a
time from records that are 96 bytes apart.

The fix is to make the description match the data. For
`CgfxVertex`-based meshes, use [`cgfx_mesh_vertex_layout()`](../api/mesh.md#cgfx_mesh_vertex_layout) -- it returns
a layout that is guaranteed to match the struct.
