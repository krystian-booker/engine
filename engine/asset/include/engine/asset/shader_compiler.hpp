#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace engine::asset {

// Shader stage type
enum class ShaderStage {
    Vertex,
    Fragment,
    Compute
};

// Shader compiler for source-to-binary compilation
class ShaderCompiler {
public:
    struct CompileOptions {
        std::vector<std::string> defines;       // Preprocessor defines (e.g., "DEBUG", "MAX_LIGHTS=8")
        std::vector<std::string> include_paths; // Directories to search for #include files
        bool optimize = true;                   // Enable optimizations
        bool debug_info = false;                // Include debug information
        std::string entry_point = "main";       // Entry point function name
    };

    // Initialize the shader compiler (loads shaderc if available)
    static bool init();

    // Shutdown and cleanup
    static void shutdown();

    // Check if shader compilation is available
    static bool is_available();

    // Compile shader source file to binary file
    // Returns true on success
    static bool compile(
        const std::string& source_path,
        const std::string& output_path,
        ShaderStage stage,
        const CompileOptions& options = {});

    // Compile shader source to binary in memory
    // Returns empty vector on failure
    static std::vector<uint8_t> compile_to_memory(
        const std::string& source,
        ShaderStage stage,
        const CompileOptions& options = {});

    // Compile from source file to memory
    static std::vector<uint8_t> compile_file_to_memory(
        const std::string& source_path,
        ShaderStage stage,
        const CompileOptions& options = {});

    // Get the last error message
    static const std::string& get_last_error();

private:
    static std::string s_last_error;
    static bool s_initialized;
};

} // namespace engine::asset
