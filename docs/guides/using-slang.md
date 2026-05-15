# Using Slang Shaders

[Slang](https://shader-slang.com/) is a shader language with HLSL-style syntax, struct member functions, generics, and cross-compilation to multiple targets. cgfx supports Slang by compiling `.slang` files to WGSL at build time using the `slangc` compiler, which CMake fetches automatically.

## How it works

The workflow is entirely build-time — there is no Slang runtime dependency:

```
triangle.slang          (you write this)
       |
       |  slangc -target wgsl
       v
triangle.wgsl           (generated at build time)
       |
       |  cgfx_shader_create()
       v
WGPUShaderModule        (loaded at runtime like any other WGSL shader)
```

The generated `.wgsl` file is placed in the build directory and copied next to the executable. At runtime, your application loads it as a text file and passes it to `cgfx_shader_create()` — cgfx never sees Slang directly.

## CMake setup

cgfx provides a `cgfx_compile_slang()` CMake function (defined in `vendor/slang/slang.cmake`). It takes three arguments:

| Parameter | Description |
|-----------|-------------|
| `SOURCE` | Path to the `.slang` shader file |
| `ENTRIES` | List of entry point pairs: `"<name> <stage>"` |
| `OUTPUT` | Output path for the compiled `.wgsl` file |

Here is how the `slang_triangle` example uses it:

```cmake
set(SLANG_SHADER "${CMAKE_CURRENT_SOURCE_DIR}/slang_triangle/triangle.slang")
set(WGSL_DIR     "${CMAKE_CURRENT_BINARY_DIR}")

cgfx_compile_slang(
    SOURCE  "${SLANG_SHADER}"
    ENTRIES "vertexMain vertex" "fragmentMain fragment"
    OUTPUT  "${WGSL_DIR}/triangle.wgsl"
)

cgfx_add_example(slang_triangle slang_triangle/main.c)
target_sources(slang_triangle PRIVATE "${WGSL_DIR}/triangle.wgsl")
```

Each `ENTRIES` item is a space-separated pair of the entry point function name and its shader stage (`vertex`, `fragment`, or `compute`). The `target_sources` line tells CMake that the executable depends on the generated `.wgsl` file, so it gets compiled before the C code links.

!!! tip "Automatic compiler download"
    The first time you configure CMake, it downloads a pre-built `slangc` binary from the [Slang GitHub releases](https://github.com/shader-slang/slang/releases) via `FetchContent`. No manual installation is needed. Linux (x86_64/aarch64), macOS, and Windows are supported.

## Writing a Slang shader

Here is the triangle shader from the `slang_triangle` example:

```hlsl
struct VSOutput {
    float4 position : SV_Position;
    float3 color    : COLOR;
};

struct Light {
    float3 color;
    float intensity;

    float3 getRadiance() {
        return this.color * this.intensity;
    }
};

[shader("vertex")]
VSOutput vertexMain(uint vertexIndex : SV_VertexID) {
    float2 positions[3] = {
        float2(-0.5, -0.5),
        float2( 0.5, -0.5),
        float2( 0.0,  0.5),
    };

    float3 colors[3] = {
        float3(1.0, 0.2, 0.2),
        float3(0.2, 1.0, 0.2),
        float3(0.2, 0.2, 1.0),
    };

    Light lo;
    lo.color = float3(0.4, 0.3, 0.1);
    lo.intensity = 0.9;

    VSOutput output;
    output.position = float4(positions[vertexIndex], 0.0, 1.0);
    output.color = colors[vertexIndex] * lo.getRadiance();
    return output;
}

[shader("fragment")]
float4 fragmentMain(float3 color : COLOR) : SV_Target {
    return float4(color, 1.0);
}
```

### Slang vs WGSL at a glance

| Slang | WGSL equivalent |
|-------|-----------------|
| `[shader("vertex")]` | `@vertex` |
| `[shader("fragment")]` | `@fragment` |
| `SV_Position` | `@builtin(position)` |
| `SV_VertexID` | `@builtin(vertex_index)` |
| `SV_Target` | `@location(0)` return |
| `COLOR` | `@location(0)` varying |
| `float4`, `float3`, `float2` | `vec4<f32>`, `vec3<f32>`, `vec2<f32>` |
| `uint` | `u32` |
| Struct member functions | Compiled to standalone functions |

Slang supports features that WGSL does not have natively, like struct member functions (`Light.getRadiance()`), generics, interfaces, and modules. The compiler flattens these into valid WGSL.

## What slangc generates

The generated WGSL is valid but machine-generated — identifiers are mangled with suffixes and floating-point constants are fully expanded. Here is a trimmed version of the output:

```wgsl
struct Light_0 {
    color_0 : vec3<f32>,
    intensity_0 : f32,
};

fn Light_getRadiance_0(this_0 : Light_0) -> vec3<f32> {
    return this_0.color_0 * vec3<f32>(this_0.intensity_0);
}

struct VSOutput_0 {
    @builtin(position) position_0 : vec4<f32>,
    @location(0) color_1 : vec3<f32>,
};

@vertex
fn vertexMain(@builtin(vertex_index) vertexIndex_0 : u32) -> VSOutput_0 {
    // ... positions, colors, light setup ...
}

@fragment
fn fragmentMain(_S1 : pixelInput_0) -> pixelOutput_0 {
    // ...
}
```

Entry point names (`vertexMain`, `fragmentMain`) are preserved from the Slang source. This is important — it means you can reference them by name in your C code without worrying about mangling.

## Loading the compiled shader

At runtime, load the `.wgsl` file and pass it to cgfx like any other shader. The only difference from a hand-written WGSL shader is that you may need to set custom entry point names if they differ from the cgfx defaults (`vs_main` / `fs_main`):

```c
char *wgsl = load_text_file("triangle.wgsl");

CgfxShader shader = cgfx_shader_create(&ctx, "slang triangle", wgsl,
    &(CgfxShaderDesc){});
free(wgsl);

WGPURenderPipeline pipeline = cgfx_pipeline_create(&ctx, &(CgfxPipelineDesc){
    .shader         = &shader,
    .vertex_entry   = "vertexMain",
    .fragment_entry = "fragmentMain",
});
```

The `.vertex_entry` and `.fragment_entry` fields override the default entry point names. If your Slang shader uses `vs_main` and `fs_main` as entry point names, you can omit these fields entirely.

!!! note "File loading"
    cgfx provides `cgfx_shader_create_from_file()` for loading WGSL from disk, but the Slang example uses a manual `load_text_file()` helper since the generated `.wgsl` sits in the build output directory rather than a `shaders/` subdirectory. Either approach works.

## Adding Slang to your own project

To add a Slang shader to an existing cgfx project:

1. Write your `.slang` file
2. Add a `cgfx_compile_slang()` call in your `CMakeLists.txt`
3. Add the output `.wgsl` as a source dependency with `target_sources`
4. Load the generated `.wgsl` at runtime and set entry point names in `CgfxPipelineDesc`

```cmake
cgfx_compile_slang(
    SOURCE  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/my_shader.slang"
    ENTRIES "vsMain vertex" "fsMain fragment"
    OUTPUT  "${CMAKE_CURRENT_BINARY_DIR}/my_shader.wgsl"
)

add_executable(my_app main.c)
target_link_libraries(my_app PRIVATE cgfx)
target_sources(my_app PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/my_shader.wgsl")
```

!!! warning "Emscripten"
    Slang is not available for Emscripten builds. The CMake module skips Slang setup entirely when targeting the web. For Emscripten projects, write your shaders in WGSL directly.
