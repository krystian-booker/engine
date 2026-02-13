$input a_position, a_normal, a_texcoord0, a_color0, a_tangent, a_indices, a_weight
$output v_worldPos, v_normal, v_tangent, v_bitangent, v_texcoord0, v_color0, v_clipPos

#include <bgfx_shader.sh>

// Bone matrices uniform (max 128 bones, each bone is a 4x4 matrix)
// Using vec4 array: 4 vec4s per matrix, 128 bones = 512 vec4s
uniform vec4 u_boneMatrices[512];

// Get bone matrix from the uniform array
mat4 getBoneMatrix(int boneIndex)
{
    int base = boneIndex * 4;
    return mat4(
        u_boneMatrices[base + 0],
        u_boneMatrices[base + 1],
        u_boneMatrices[base + 2],
        u_boneMatrices[base + 3]
    );
}

// Apply skinning to a position
vec4 skinPosition(vec3 position, ivec4 indices, vec4 weights)
{
    vec4 skinnedPos = vec4(0.0, 0.0, 0.0, 0.0);

    // Accumulate weighted bone transforms
    for (int i = 0; i < 4; i++)
    {
        float weight = weights[i];
        if (weight > 0.0)
        {
            mat4 boneMatrix = getBoneMatrix(indices[i]);
            skinnedPos += weight * mul(boneMatrix, vec4(position, 1.0));
        }
    }

    return skinnedPos;
}

// Apply skinning to a direction (normal, tangent)
vec3 skinDirection(vec3 direction, ivec4 indices, vec4 weights)
{
    vec3 skinnedDir = vec3(0.0, 0.0, 0.0);

    for (int i = 0; i < 4; i++)
    {
        float weight = weights[i];
        if (weight > 0.0)
        {
            mat4 boneMatrix = getBoneMatrix(indices[i]);
            // Use upper 3x3 for direction transformation
            mat3 boneMat3 = mat3(boneMatrix[0].xyz, boneMatrix[1].xyz, boneMatrix[2].xyz);
            skinnedDir += weight * mul(boneMat3, direction);
        }
    }

    return normalize(skinnedDir);
}

void main()
{
    // Get bone indices and weights
    ivec4 boneIndices = a_indices;
    vec4 boneWeights = a_weight;

    // Normalize weights to ensure they sum to 1.0
    float totalWeight = boneWeights.x + boneWeights.y + boneWeights.z + boneWeights.w;
    if (totalWeight > 0.0)
    {
        boneWeights /= totalWeight;
    }
    else
    {
        // No skinning - use identity (first bone with full weight)
        boneWeights = vec4(1.0, 0.0, 0.0, 0.0);
    }

    // Apply skinning
    vec4 skinnedPosition = skinPosition(a_position, boneIndices, boneWeights);
    vec3 skinnedNormal = skinDirection(a_normal, boneIndices, boneWeights);
    vec3 skinnedTangent = skinDirection(a_tangent, boneIndices, boneWeights);

    // Transform skinned position to world space
    vec4 worldPos = mul(u_model[0], skinnedPosition);

    // Calculate view-space position for depth (used for cascade selection)
    vec4 viewPos = mul(u_view, worldPos);
    float viewSpaceDepth = -viewPos.z;

    v_worldPos = vec4(worldPos.xyz, viewSpaceDepth);

    // Transform skinned normal and tangent to world space
    // Use cofactor matrix (== inverse-transpose * det) to handle non-uniform scale.
    // Since we normalize afterwards, skipping the determinant division is fine.
    vec3 c0 = u_model[0][0].xyz;
    vec3 c1 = u_model[0][1].xyz;
    vec3 c2 = u_model[0][2].xyz;
    mat3 normalMatrix = mat3(
        cross(c1, c2),
        cross(c2, c0),
        cross(c0, c1)
    );
    v_normal = normalize(mul(normalMatrix, skinnedNormal));
    v_tangent = normalize(mul(normalMatrix, skinnedTangent));
    v_bitangent = cross(v_normal, v_tangent);

    // Pass through UV and vertex color
    v_texcoord0 = a_texcoord0;
    v_color0 = a_color0;

    // Calculate clip-space position
    vec4 clipPos = mul(u_viewProj, worldPos);
    v_clipPos = clipPos;

    gl_Position = clipPos;
}
