// Fullscreen triangle vertex shader for Forward+ debug visualization
// Generates a fullscreen triangle using vertex ID

struct VSOutput {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

VSOutput main(uint vertexID : SV_VertexID) {
    VSOutput output;

    // Generate fullscreen triangle
    // Vertex 0: (-1, -1) -> UV (0, 0)
    // Vertex 1: ( 3, -1) -> UV (2, 0)
    // Vertex 2: (-1,  3) -> UV (0, 2)
    output.texCoord = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(output.texCoord * float2(2, -2) + float2(-1, 1), 0, 1);

    return output;
}
