// Triangle shader — Slang version

struct VSOutput {
    float4 position : SV_Position;
    float3 color    : COLOR;
};

struct Light {
    float3 color;
    float intensity;

    // Member function
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
