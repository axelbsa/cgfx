struct Camera {
    projection: mat4x4f,
    view: mat4x4f,
};

struct Object {
    model: mat4x4f,
};

@group(0) @binding(0) var<uniform> camera: Camera;
@group(1) @binding(0) var<uniform> object: Object;

struct VertexInput {
    @location(0) position: vec3f,
    @location(1) normal: vec3f,
};

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) normal: vec3f,
};

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    let world_pos = object.model * vec4f(in.position, 1.0);
    let world_normal = (object.model * vec4f(in.normal, 0.0)).xyz;

    var out: VertexOutput;
    out.position = camera.projection * camera.view * world_pos;
    out.normal = normalize(world_normal);
    return out;
}

@fragment
fn fs_main(@location(0) normal: vec3f) -> @location(0) vec4f {
    let color = normal * 0.5 + 0.5;
    return vec4f(color, 1.0);
}
