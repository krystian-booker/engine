#ifndef LIGHT_PROBES_SH
#define LIGHT_PROBES_SH

SAMPLER2D(s_probeTexture, 14);

float probeVolumeContains(vec3 worldPos)
{
    return step(u_probeVolumeMin.x, worldPos.x) *
           step(u_probeVolumeMin.y, worldPos.y) *
           step(u_probeVolumeMin.z, worldPos.z) *
           step(worldPos.x, u_probeVolumeMax.x) *
           step(worldPos.y, u_probeVolumeMax.y) *
           step(worldPos.z, u_probeVolumeMax.z);
}

vec2 probeTextureUV(float pixelIndex)
{
    float x = mod(pixelIndex, u_probeTextureInfo.x);
    float y = floor(pixelIndex / u_probeTextureInfo.x);
    return vec2((x + 0.5) * u_probeTextureInfo.z, (y + 0.5) * u_probeTextureInfo.w);
}

vec3 loadProbeCoefficient(float probeIndex, float coefficientIndex)
{
    float pixelIndex = probeIndex * 9.0 + coefficientIndex;
    return texture2D(s_probeTexture, probeTextureUV(pixelIndex)).rgb;
}

vec3 evaluateProbeSH(float probeIndex, vec3 dir)
{
    vec3 coeff0 = loadProbeCoefficient(probeIndex, 0.0);
    vec3 coeff1 = loadProbeCoefficient(probeIndex, 1.0);
    vec3 coeff2 = loadProbeCoefficient(probeIndex, 2.0);
    vec3 coeff3 = loadProbeCoefficient(probeIndex, 3.0);
    vec3 coeff4 = loadProbeCoefficient(probeIndex, 4.0);
    vec3 coeff5 = loadProbeCoefficient(probeIndex, 5.0);
    vec3 coeff6 = loadProbeCoefficient(probeIndex, 6.0);
    vec3 coeff7 = loadProbeCoefficient(probeIndex, 7.0);
    vec3 coeff8 = loadProbeCoefficient(probeIndex, 8.0);

    float x = dir.x;
    float y = dir.y;
    float z = dir.z;

    return coeff0 * 0.282095 +
           coeff1 * (0.488603 * y) +
           coeff2 * (0.488603 * z) +
           coeff3 * (0.488603 * x) +
           coeff4 * (1.092548 * x * y) +
           coeff5 * (1.092548 * y * z) +
           coeff6 * (0.315392 * (3.0 * z * z - 1.0)) +
           coeff7 * (1.092548 * x * z) +
           coeff8 * (0.546274 * (x * x - y * y));
}

float getProbeIndex(float x, float y, float z)
{
    return u_probeVolumeResolution.w +
           (z * u_probeVolumeResolution.y + y) * u_probeVolumeResolution.x +
           x;
}

vec3 sampleProbeIrradiance(vec3 worldPos, vec3 normal)
{
    if (u_probeState.x < 0.5 || probeVolumeContains(worldPos) < 0.5)
    {
        return vec3_splat(0.0);
    }

    vec3 cellSize = (u_probeVolumeMax.xyz - u_probeVolumeMin.xyz) /
        max(u_probeVolumeResolution.xyz, vec3_splat(1.0));
    vec3 localPos = worldPos - u_probeVolumeMin.xyz;
    vec3 gridPos = localPos / max(cellSize, vec3_splat(0.0001));

    vec3 baseCell = min(floor(gridPos), max(u_probeVolumeResolution.xyz - vec3_splat(1.0), vec3_splat(0.0)));
    vec3 nextCell = min(baseCell + vec3_splat(1.0), max(u_probeVolumeResolution.xyz - vec3_splat(1.0), vec3_splat(0.0)));
    vec3 weights = clamp(gridPos - baseCell, vec3_splat(0.0), vec3_splat(1.0));

    vec3 samples000 = evaluateProbeSH(getProbeIndex(baseCell.x, baseCell.y, baseCell.z), normal);
    vec3 samples100 = evaluateProbeSH(getProbeIndex(nextCell.x, baseCell.y, baseCell.z), normal);
    vec3 samples010 = evaluateProbeSH(getProbeIndex(baseCell.x, nextCell.y, baseCell.z), normal);
    vec3 samples110 = evaluateProbeSH(getProbeIndex(nextCell.x, nextCell.y, baseCell.z), normal);
    vec3 samples001 = evaluateProbeSH(getProbeIndex(baseCell.x, baseCell.y, nextCell.z), normal);
    vec3 samples101 = evaluateProbeSH(getProbeIndex(nextCell.x, baseCell.y, nextCell.z), normal);
    vec3 samples011 = evaluateProbeSH(getProbeIndex(baseCell.x, nextCell.y, nextCell.z), normal);
    vec3 samples111 = evaluateProbeSH(getProbeIndex(nextCell.x, nextCell.y, nextCell.z), normal);

    vec3 sample00 = mix(samples000, samples100, weights.x);
    vec3 sample10 = mix(samples010, samples110, weights.x);
    vec3 sample01 = mix(samples001, samples101, weights.x);
    vec3 sample11 = mix(samples011, samples111, weights.x);

    vec3 sample0 = mix(sample00, sample10, weights.y);
    vec3 sample1 = mix(sample01, sample11, weights.y);
    return max(mix(sample0, sample1, weights.z), vec3_splat(0.0));
}

#endif // LIGHT_PROBES_SH
