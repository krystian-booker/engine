// Forward+ Debug Visualization Fragment Shader
// Renders a heatmap overlay showing light count per tile

// Tile light data structure (must match GPU format)
struct TileLightData {
    uint lightCount;
    uint lightIndices[256];
};

// Bindings
[[vk::binding(0, 0)]] StructuredBuffer<TileLightData> tileLightData;

// Push constants for screen/tile information
[[vk::push_constant]]
struct PushConstants {
    uint screenWidth;
    uint screenHeight;
    uint tileSize;
    uint debugMode;      // 0 = heatmap, 1 = tile boundaries, 2 = both
    float intensity;     // Heatmap intensity (0-1)
    float padding1;
    float padding2;
    float padding3;
} constants;

struct PSInput {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

// Color ramp for heatmap: Blue -> Cyan -> Green -> Yellow -> Red
float3 GetHeatmapColor(float t) {
    // t in range [0, 1]
    // 0.0 = Blue (0 lights)
    // 0.25 = Cyan (64 lights)
    // 0.5 = Green (128 lights)
    // 0.75 = Yellow (192 lights)
    // 1.0 = Red (256 lights)

    if (t < 0.25) {
        // Blue to Cyan
        float localT = t / 0.25;
        return lerp(float3(0.0, 0.0, 1.0), float3(0.0, 1.0, 1.0), localT);
    } else if (t < 0.5) {
        // Cyan to Green
        float localT = (t - 0.25) / 0.25;
        return lerp(float3(0.0, 1.0, 1.0), float3(0.0, 1.0, 0.0), localT);
    } else if (t < 0.75) {
        // Green to Yellow
        float localT = (t - 0.5) / 0.25;
        return lerp(float3(0.0, 1.0, 0.0), float3(1.0, 1.0, 0.0), localT);
    } else {
        // Yellow to Red
        float localT = (t - 0.75) / 0.25;
        return lerp(float3(1.0, 1.0, 0.0), float3(1.0, 0.0, 0.0), localT);
    }
}

float4 main(PSInput input) : SV_TARGET {
    // Calculate pixel coordinates
    uint2 pixelCoord = uint2(input.position.xy);

    // Calculate tile coordinates
    uint tileX = pixelCoord.x / constants.tileSize;
    uint tileY = pixelCoord.y / constants.tileSize;

    // Calculate number of tiles
    uint numTilesX = (constants.screenWidth + constants.tileSize - 1) / constants.tileSize;
    uint numTilesY = (constants.screenHeight + constants.tileSize - 1) / constants.tileSize;

    // Bounds check
    if (tileX >= numTilesX || tileY >= numTilesY) {
        return float4(0, 0, 0, 0);
    }

    // Calculate tile index
    uint tileIndex = tileY * numTilesX + tileX;

    // Read light count from tile data
    uint lightCount = tileLightData[tileIndex].lightCount;

    // Calculate tile boundary
    uint tileLocalX = pixelCoord.x % constants.tileSize;
    uint tileLocalY = pixelCoord.y % constants.tileSize;
    bool isTileBoundary = (tileLocalX == 0 || tileLocalY == 0);

    // Debug mode selection
    float3 debugColor = float3(0, 0, 0);
    float alpha = 0.5;

    if (constants.debugMode == 0 || constants.debugMode == 2) {
        // Heatmap mode
        // Normalize light count to 0-1 range (0 to 256 lights)
        float t = saturate(float(lightCount) / 256.0);

        // Apply intensity
        t = pow(t, 1.0 - constants.intensity * 0.5);  // Adjust contrast

        debugColor = GetHeatmapColor(t);
        alpha = 0.5 * constants.intensity;

        // Brighten if this tile has lights
        if (lightCount > 0) {
            debugColor *= 1.0 + (constants.intensity * 0.5);
        }
    }

    if ((constants.debugMode == 1 || constants.debugMode == 2) && isTileBoundary) {
        // Tile boundary mode
        debugColor = float3(1, 1, 1);  // White boundaries
        alpha = 0.8;
    }

    // Draw light count text (very basic - just indicate high counts)
    if (lightCount >= 256 && tileLocalX < constants.tileSize / 2 && tileLocalY < constants.tileSize / 2) {
        // Flash red for overflow
        debugColor = float3(1, 0, 0);
        alpha = 0.9;
    }

    return float4(debugColor, alpha);
}
