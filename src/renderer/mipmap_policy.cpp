#include "renderer/mipmap_policy.h"
#include "renderer/vulkan_context.h"
#include "core/texture_data.h"
#include "core/types.h"
#include <iostream>
#include <cassert>

// Helper function to check if a format supports storage image (for compute shaders)
static bool CanUseCompute(VulkanContext* context, VkFormat format) {
    if (!context) return false;

    // Check primary format
    if (context->SupportsStorageImage(format)) {
        return true;
    }

    // sRGB formats dont support storage images, so we cannot use compute shaders
    // They will fall back to blit or CPU mipmap generation
    return false;
}

// Helper function to check if a format supports blit filtering
static bool CanUseBlit(VulkanContext* context, VkFormat format) {
    if (!context) return false;
    return context->SupportsLinearBlit(format);
}

// Helper to check if format is sRGB
static bool IsFormatSRGB(VkFormat format) {
    switch (format) {
        case VK_FORMAT_R8G8B8A8_SRGB:
        case VK_FORMAT_B8G8R8A8_SRGB:
        case VK_FORMAT_R8_SRGB:
        case VK_FORMAT_R8G8_SRGB:
        case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
        case VK_FORMAT_BC2_SRGB_BLOCK:
        case VK_FORMAT_BC3_SRGB_BLOCK:
        case VK_FORMAT_BC7_SRGB_BLOCK:
        case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
        case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:
            return true;
        default:
            return false;
    }
}

MipmapMethod SelectMipGenerator(const MipmapGenerationParams& params) {
    // Handle forced policies
    if (params.policy == MipmapPolicy::ForceCPU) {
        return MipmapMethod::CPU;
    }

    if (params.policy == MipmapPolicy::ForceBlit) {
        if (CanUseBlit(params.context, params.format)) {
            return MipmapMethod::Blit;
        } else {
            // Debug assert to alert developer, but fall back in release
            #ifdef _DEBUG
            std::cerr << "MipmapPolicy::ForceBlit specified, but format does not support linear blit filtering!" << std::endl;
            assert(false && "ForceBlit policy failed: format does not support linear blit");
            #endif

            // Fallback: try compute, then CPU
            if (CanUseCompute(params.context, params.format)) {
                std::cerr << "Warning: ForceBlit failed, falling back to Compute" << std::endl;
                return MipmapMethod::Compute;
            }
            std::cerr << "Warning: ForceBlit failed, falling back to CPU" << std::endl;
            return MipmapMethod::CPU;
        }
    }

    if (params.policy == MipmapPolicy::ForceCompute) {
        if (CanUseCompute(params.context, params.format)) {
            return MipmapMethod::Compute;
        } else {
            // Debug assert to alert developer, but fall back in release
            #ifdef _DEBUG
            std::cerr << "MipmapPolicy::ForceCompute specified, but format does not support storage image!" << std::endl;
            assert(false && "ForceCompute policy failed: format does not support storage image");
            #endif

            // Fallback: try blit, then CPU
            if (CanUseBlit(params.context, params.format)) {
                std::cerr << "Warning: ForceCompute failed, falling back to Blit" << std::endl;
                return MipmapMethod::Blit;
            }
            std::cerr << "Warning: ForceCompute failed, falling back to CPU" << std::endl;
            return MipmapMethod::CPU;
        }
    }

    // Auto policy - usage-based rules
    MipmapMethod preferredMethod = MipmapMethod::Blit;  // Default preference

    switch (params.usage) {
        case TextureUsage::Normal:
        case TextureUsage::Height:
        case TextureUsage::PackedPBR:
        case TextureUsage::Roughness:
            // PBR-aware filtering is critical for these texture types
            // Always prefer compute for correct renormalization/Toksvig/channel handling
            preferredMethod = MipmapMethod::Compute;
            break;

        case TextureUsage::Albedo:
        case TextureUsage::AO:
            // For albedo and AO, prefer compute if sRGB (for gamma-correct filtering)
            if (IsFormatSRGB(params.format)) {
                // Quality-based decision for sRGB textures
                switch (params.quality) {
                    case MipmapQuality::High:
                        // High quality: always use compute for gamma-correct filtering
                        preferredMethod = MipmapMethod::Compute;
                        break;

                    case MipmapQuality::Balanced:
                        // Balanced: use size heuristic
                        // Large textures (≥512px) benefit from compute quality
                        // Small textures can use blit for speed
                        if (params.width >= 512 || params.height >= 512) {
                            preferredMethod = MipmapMethod::Compute;
                        } else {
                            preferredMethod = MipmapMethod::Blit;
                        }
                        break;

                    case MipmapQuality::Fast:
                        // Fast: prefer blit for speed
                        preferredMethod = MipmapMethod::Blit;
                        break;
                }
            } else {
                // Linear format: simple averaging is fine, use blit
                preferredMethod = MipmapMethod::Blit;
            }
            break;

        case TextureUsage::Metalness:
        case TextureUsage::Generic:
        default:
            // Simple averaging is sufficient, prefer blit for speed
            preferredMethod = MipmapMethod::Blit;
            break;
    }

    // Capability-based fallback
    // Try preferred method first, fall back to alternatives if unsupported

    if (preferredMethod == MipmapMethod::Compute) {
        if (CanUseCompute(params.context, params.format)) {
            return MipmapMethod::Compute;
        }

        // Compute not supported, try blit
        if (CanUseBlit(params.context, params.format)) {
            std::cerr << "Warning: Compute mipmap generation preferred but unsupported for this format, falling back to Blit" << std::endl;
            return MipmapMethod::Blit;
        }

        // Neither compute nor blit supported, fall back to CPU
        std::cerr << "Warning: Neither Compute nor Blit supported for this format, falling back to CPU" << std::endl;
        return MipmapMethod::CPU;
    }

    if (preferredMethod == MipmapMethod::Blit) {
        if (CanUseBlit(params.context, params.format)) {
            return MipmapMethod::Blit;
        }

        // Blit not supported, try compute
        if (CanUseCompute(params.context, params.format)) {
            // Silent fallback to compute (no warning needed, compute is actually better)
            return MipmapMethod::Compute;
        }

        // Neither blit nor compute supported, fall back to CPU
        std::cerr << "Warning: Neither Blit nor Compute supported for this format, falling back to CPU" << std::endl;
        return MipmapMethod::CPU;
    }

    // Should never reach here
    return MipmapMethod::CPU;
}
