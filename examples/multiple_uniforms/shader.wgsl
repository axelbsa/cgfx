struct MyUniforms {
    color: vec4f,
    offset: vec4f,
};

@group(0) @binding(0) var<uniform> u: MyUniforms;

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) color: vec4f,
};

@vertex
fn vs_main(@builtin(vertex_index) vi: u32) -> VertexOutput {
    var p = array<vec2f, 3>(
        vec2f(-0.25, -0.25),
        vec2f( 0.25, -0.25),
        vec2f( 0.0,   0.25),
    );
    let angle = u.offset.z;
    let c = cos(angle);
    let s = sin(angle);
    let rotated = vec2f(p[vi].x * c - p[vi].y * s,
                        p[vi].x * s + p[vi].y * c);
    var out: VertexOutput;
    out.position = vec4f(rotated + u.offset.xy, 0.0, 1.0);
    out.color = u.color;
    return out;
}

@fragment
fn fs_main(@location(0) color: vec4f) -> @location(0) vec4f {
    return color;
}
