struct Uniforms {
    time: f32,
};

@group(0) @binding(0) var<uniform> u: Uniforms;

struct VertexInput {
    @location(0) position: vec3f,
    @location(1) normal: vec3f,
    @location(5) color: vec3f,
};

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) color: vec3f,
};

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    let angle = u.time;
    let c = cos(angle);
    let s = sin(angle);

    let x = in.position.x * c + in.position.z * s;
    let y = in.position.y;
    let z = -in.position.x * s + in.position.z * c;

    let ratio = 1280.0 / 720.0;

    var out: VertexOutput;
    out.position = vec4f(x, y * ratio, z * 0.5 + 0.5, 1.0);
    out.color = in.color;
    return out;
}

@fragment
fn fs_main(@location(0) color: vec3f) -> @location(0) vec4f {
    return vec4f(color, 1.0);
}
