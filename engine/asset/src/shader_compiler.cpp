#include <engine/asset/shader_compiler.hpp>
#include <engine/core/filesystem.hpp>
#include <engine/core/log.hpp>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <filesystem>
#include <atomic>
#include <thread>

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

namespace engine::asset {

using namespace engine::core;

std::string ShaderCompiler::s_last_error;
bool ShaderCompiler::s_initialized = false;

namespace {

// Counter for unique temp file names
std::atomic<uint64_t> g_temp_file_counter{0};

// Validate path contains only safe characters (defense in depth for internal paths)
bool is_safe_path(const std::string& path) {
    for (char c : path) {
        // Allow alphanumeric, path separators, dots, underscores, hyphens, spaces
        if (!std::isalnum(static_cast<unsigned char>(c)) &&
            c != '/' && c != '\\' && c != '.' && c != '_' && c != '-' && c != ' ' && c != ':') {
            return false;
        }
    }
    return true;
}

// Escape shell special characters in a path
std::string escape_shell_path(const std::string& path) {
    std::string result;
    result.reserve(path.size() + 16);
    for (char c : path) {
        // Escape shell metacharacters
        if (c == '"' || c == '\\' || c == '$' || c == '`' || c == '!' || c == '&' ||
            c == '|' || c == ';' || c == '<' || c == '>' || c == '(' || c == ')' ||
            c == '\'' || c == '*' || c == '?' || c == '[' || c == ']' || c == '#') {
#ifdef _WIN32
            // On Windows, escape with caret for most, backslash for quotes
            if (c == '"') {
                result += "\\\"";
            } else {
                result += c;  // Most chars are safe in Windows quoted strings
            }
#else
            // On Unix, escape with backslash
            result += '\\';
            result += c;
#endif
        } else {
            result += c;
        }
    }
    return result;
}

// Generate unique temp file base name
std::string generate_temp_basename() {
    auto thread_id = std::hash<std::thread::id>{}(std::this_thread::get_id());
    auto counter = g_temp_file_counter.fetch_add(1);
    return "engine_shader_" + std::to_string(thread_id) + "_" + std::to_string(counter);
}

// Find shaderc executable
std::string find_shaderc() {
    // Check environment variable first
    const char* bgfx_dir = std::getenv("BGFX_DIR");
    if (bgfx_dir) {
        std::string path = std::string(bgfx_dir) + "/tools/bin/";
#ifdef _WIN32
        path += "windows/shaderc.exe";
#elif __APPLE__
        path += "darwin/shaderc";
#else
        path += "linux/shaderc";
#endif
        if (std::filesystem::exists(path)) {
            return path;
        }
    }

    // Check PATH
#ifdef _WIN32
    FILE* pipe = popen("where shaderc.exe 2>nul", "r");
#else
    FILE* pipe = popen("which shaderc 2>/dev/null", "r");
#endif
    if (pipe) {
        char buffer[512];
        if (fgets(buffer, sizeof(buffer), pipe)) {
            pclose(pipe);
            std::string result(buffer);
            // Remove newline
            while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
                result.pop_back();
            }
            if (!result.empty() && std::filesystem::exists(result)) {
                return result;
            }
        } else {
            pclose(pipe);
        }
    }

    // Check common locations
    std::vector<std::string> common_paths = {
#ifdef _WIN32
        "C:/bgfx/tools/bin/windows/shaderc.exe",
        "./tools/shaderc.exe",
        "../tools/shaderc.exe",
#else
        "/usr/local/bin/shaderc",
        "/usr/bin/shaderc",
        "./tools/shaderc",
        "../tools/shaderc",
#endif
    };

    for (const auto& p : common_paths) {
        if (std::filesystem::exists(p)) {
            return p;
        }
    }

    return "";
}

std::string g_shaderc_path;

} // anonymous namespace

bool ShaderCompiler::init() {
    if (s_initialized) return true;

    g_shaderc_path = find_shaderc();
    s_initialized = true;

    if (g_shaderc_path.empty()) {
        log(LogLevel::Warn, "Shader compiler (shaderc) not found. Source compilation disabled.");
        return false;
    }

    log(LogLevel::Debug, ("Shader compiler found at: " + g_shaderc_path).c_str());
    return true;
}

void ShaderCompiler::shutdown() {
    s_initialized = false;
    g_shaderc_path.clear();
}

bool ShaderCompiler::is_available() {
    if (!s_initialized) init();
    return !g_shaderc_path.empty();
}

bool ShaderCompiler::compile(
    const std::string& source_path,
    const std::string& output_path,
    ShaderStage stage,
    const CompileOptions& options)
{
    s_last_error.clear();

    if (!is_available()) {
        s_last_error = "Shader compiler not available";
        return false;
    }

    // Validate paths for safety (defense in depth)
    if (!is_safe_path(source_path)) {
        s_last_error = "Invalid characters in source path: " + source_path;
        log(LogLevel::Error, s_last_error.c_str());
        return false;
    }
    if (!is_safe_path(output_path)) {
        s_last_error = "Invalid characters in output path: " + output_path;
        log(LogLevel::Error, s_last_error.c_str());
        return false;
    }

    // Build shaderc command with escaped paths
    std::ostringstream cmd;
    cmd << "\"" << escape_shell_path(g_shaderc_path) << "\"";

    // Input file
    cmd << " -f \"" << escape_shell_path(source_path) << "\"";

    // Output file
    cmd << " -o \"" << escape_shell_path(output_path) << "\"";

    // Shader type
    switch (stage) {
        case ShaderStage::Vertex:
            cmd << " --type vertex";
            break;
        case ShaderStage::Fragment:
            cmd << " --type fragment";
            break;
        case ShaderStage::Compute:
            cmd << " --type compute";
            break;
    }

    // Platform and profile (auto-detect based on build)
#ifdef _WIN32
    cmd << " --platform windows -p s_5_0";  // DirectX 11 shader model 5.0
#elif __APPLE__
    cmd << " --platform osx -p metal";
#else
    cmd << " --platform linux -p 430";  // OpenGL 4.3 GLSL
#endif

    // Include paths (with validation and escaping)
    for (const auto& inc_path : options.include_paths) {
        if (!is_safe_path(inc_path)) {
            s_last_error = "Invalid characters in include path: " + inc_path;
            log(LogLevel::Error, s_last_error.c_str());
            return false;
        }
        cmd << " -i \"" << escape_shell_path(inc_path) << "\"";
    }

    // Defines
    for (const auto& define : options.defines) {
        cmd << " --define " << define;
    }

    // Optimization level
    if (options.optimize) {
        cmd << " -O 3";
    } else {
        cmd << " -O 0";
    }

    // Debug info
    if (options.debug_info) {
        cmd << " --debug";
    }

    // Redirect stderr to stdout for error capture
#ifdef _WIN32
    cmd << " 2>&1";
#else
    cmd << " 2>&1";
#endif

    std::string command = cmd.str();
    log(LogLevel::Debug, ("Compiling shader: " + command).c_str());

    // Execute compiler
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        s_last_error = "Failed to execute shader compiler";
        return false;
    }

    // Capture output
    std::ostringstream output;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        output << buffer;
    }

    int result = pclose(pipe);

    if (result != 0) {
        s_last_error = "Shader compilation failed:\n" + output.str();
        log(LogLevel::Error, s_last_error.c_str());
        return false;
    }

    // Verify output file was created
    if (!std::filesystem::exists(output_path)) {
        s_last_error = "Shader compiler succeeded but output file not created";
        return false;
    }

    log(LogLevel::Debug, ("Shader compiled successfully: " + output_path).c_str());
    return true;
}

std::vector<uint8_t> ShaderCompiler::compile_to_memory(
    const std::string& source,
    ShaderStage stage,
    const CompileOptions& options)
{
    s_last_error.clear();

    // Generate unique temp file names to avoid race conditions
    std::string temp_dir = std::filesystem::temp_directory_path().string();
    std::string basename = generate_temp_basename();
    std::string temp_source = temp_dir + "/" + basename + ".sc";
    std::string temp_output = temp_dir + "/" + basename + ".bin";

    {
        std::ofstream ofs(temp_source);
        if (!ofs) {
            s_last_error = "Failed to create temporary source file: " + temp_source;
            return {};
        }
        ofs << source;
    }

    // Compile
    if (!compile(temp_source, temp_output, stage, options)) {
        std::filesystem::remove(temp_source);
        return {};
    }

    // Read output
    auto result = FileSystem::read_binary(temp_output);

    // Cleanup temp files
    std::error_code ec;  // Ignore cleanup errors
    std::filesystem::remove(temp_source, ec);
    std::filesystem::remove(temp_output, ec);

    return result;
}

std::vector<uint8_t> ShaderCompiler::compile_file_to_memory(
    const std::string& source_path,
    ShaderStage stage,
    const CompileOptions& options)
{
    s_last_error.clear();

    // Compile to temp file with unique name
    std::string temp_dir = std::filesystem::temp_directory_path().string();
    std::string temp_output = temp_dir + "/" + generate_temp_basename() + ".bin";

    if (!compile(source_path, temp_output, stage, options)) {
        return {};
    }

    // Read output
    auto result = FileSystem::read_binary(temp_output);

    // Cleanup
    std::error_code ec;  // Ignore cleanup errors
    std::filesystem::remove(temp_output, ec);

    return result;
}

const std::string& ShaderCompiler::get_last_error() {
    return s_last_error;
}

} // namespace engine::asset
