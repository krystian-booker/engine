// image_compare — per-pixel RMSE comparison of two PNG images
// Usage: image_compare <golden.png> <test.png> [--threshold=0.01] [--diff=diff.png]
// Exit code 0 = pass (RMSE <= threshold), 1 = fail or error

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// stb_image_write implementation is already provided by engine/asset/src/manager.cpp
// when linked with the engine. For standalone use, define it here.
#ifndef STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#endif
#include <stb_image_write.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static void print_usage() {
    std::fprintf(stderr, "Usage: image_compare <golden.png> <test.png> [--threshold=0.01] [--diff=diff.png]\n");
}

int main(int argc, char** argv) {
    if (argc < 3) {
        print_usage();
        return 1;
    }

    const char* golden_path = argv[1];
    const char* test_path = argv[2];
    float threshold = 0.01f;
    std::string diff_path;

    // Parse optional args
    for (int i = 3; i < argc; ++i) {
        if (std::strncmp(argv[i], "--threshold=", 12) == 0) {
            threshold = std::strtof(argv[i] + 12, nullptr);
        } else if (std::strncmp(argv[i], "--diff=", 7) == 0) {
            diff_path = argv[i] + 7;
        }
    }

    // Load images
    int gw, gh, gc;
    unsigned char* golden = stbi_load(golden_path, &gw, &gh, &gc, 4);
    if (!golden) {
        std::fprintf(stderr, "Error: cannot load golden image '%s'\n", golden_path);
        return 1;
    }

    int tw, th, tc;
    unsigned char* test = stbi_load(test_path, &tw, &th, &tc, 4);
    if (!test) {
        std::fprintf(stderr, "Error: cannot load test image '%s'\n", test_path);
        stbi_image_free(golden);
        return 1;
    }

    if (gw != tw || gh != th) {
        std::fprintf(stderr, "Error: image dimensions differ — golden %dx%d vs test %dx%d\n", gw, gh, tw, th);
        stbi_image_free(golden);
        stbi_image_free(test);
        return 1;
    }

    // Compute per-pixel RMSE (normalized 0-1)
    int total_pixels = gw * gh;
    int channels = 4;
    double sum_sq = 0.0;

    std::vector<unsigned char> diff_data;
    bool save_diff = !diff_path.empty();
    if (save_diff) {
        diff_data.resize(static_cast<size_t>(total_pixels) * 4);
    }

    for (int i = 0; i < total_pixels; ++i) {
        int base = i * channels;
        double pixel_diff_sq = 0.0;
        for (int c = 0; c < 3; ++c) { // Compare RGB only
            double d = (static_cast<double>(golden[base + c]) - static_cast<double>(test[base + c])) / 255.0;
            pixel_diff_sq += d * d;
            if (save_diff) {
                // 10x amplified diff for visualization
                int amplified = static_cast<int>(std::abs(d) * 255.0 * 10.0);
                if (amplified > 255) amplified = 255;
                diff_data[base + c] = static_cast<unsigned char>(amplified);
            }
        }
        sum_sq += pixel_diff_sq / 3.0;  // Average across channels
        if (save_diff) {
            diff_data[base + 3] = 255;  // Full alpha
        }
    }

    double rmse = std::sqrt(sum_sq / static_cast<double>(total_pixels));

    // Save diff image if requested
    if (save_diff) {
        stbi_write_png(diff_path.c_str(), gw, gh, 4, diff_data.data(), gw * 4);
        std::printf("Diff image saved: %s\n", diff_path.c_str());
    }

    // Report
    bool pass = rmse <= static_cast<double>(threshold);
    std::printf("RMSE: %.6f (threshold: %.6f) — %s\n", rmse, static_cast<double>(threshold), pass ? "PASS" : "FAIL");

    stbi_image_free(golden);
    stbi_image_free(test);

    return pass ? 0 : 1;
}
