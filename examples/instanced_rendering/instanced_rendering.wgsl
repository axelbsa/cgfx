struct Camera {
    projection: mat4x4f,
    view: mat4x4f,
};
@group(0) @binding(0) var<uniform> camera: Camera;

struct VertexInput {
    @location(0) position: vec3f,
    @location(1) normal: vec3f,
    @location(8)  model_col0: vec4f,
    @location(9)  model_col1: vec4f,
    @location(10) model_col2: vec4f,
    @location(11) model_col3: vec4f,
    @location(12) color: vec4f,
};

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) normal: vec3f,
    @location(1) color: vec4f,
};

@vertex fn vs_main(in: VertexInput) -> VertexOutput {
    let model = mat4x4f(in.model_col0, in.model_col1, in.model_col2, in.model_col3);
    let world_pos = model * vec4f(in.position, 1.0);

    var out: VertexOutput;
    out.position = camera.projection * camera.view * world_pos;
    out.normal = (model * vec4f(in.normal, 0.0)).xyz;
    out.color = in.color;
    return out;
}

@fragment fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let light_dir = normalize(vec3f(0.5, 1.0, 0.3));
    let ndotl = max(dot(normalize(in.normal), light_dir), 0.0);
    let diffuse = 0.3 + 0.7 * ndotl;
    return vec4f(in.color.rgb * diffuse, 1.0);
}
