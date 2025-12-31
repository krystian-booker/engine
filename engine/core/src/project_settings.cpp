#include <engine/core/project_settings.hpp>
#include <engine/core/filesystem.hpp>
#include <nlohmann/json.hpp>

namespace engine::core {

using json = nlohmann::json;

ProjectSettings& ProjectSettings::get() {
    static ProjectSettings instance;
    return instance;
}

bool ProjectSettings::load(const std::string& path) {
    std::string content = FileSystem::read_text(path);
    if (content.empty()) {
        return false;
    }

    try {
        json j = json::parse(content);

        project_name = j.value("project_name", project_name);
        asset_directory = j.value("asset_directory", asset_directory);
        startup_scene = j.value("startup_scene", startup_scene);

        if (j.contains("physics")) {
            auto& p = j["physics"];
            physics.fixed_timestep = p.value("fixed_timestep", physics.fixed_timestep);
            physics.max_substeps = p.value("max_substeps", physics.max_substeps);
            if (p.contains("gravity") && p["gravity"].is_array() && p["gravity"].size() >= 3) {
                physics.gravity.x = p["gravity"][0].get<float>();
                physics.gravity.y = p["gravity"][1].get<float>();
                physics.gravity.z = p["gravity"][2].get<float>();
            }
        }

        if (j.contains("render")) {
            auto& r = j["render"];
            render.max_draw_calls = r.value("max_draw_calls", render.max_draw_calls);
            render.vsync = r.value("vsync", render.vsync);
            render.msaa_samples = r.value("msaa_samples", render.msaa_samples);
            render.shadow_map_size = r.value("shadow_map_size", render.shadow_map_size);
            render.render_scale = r.value("render_scale", render.render_scale);
        }

        if (j.contains("audio")) {
            auto& a = j["audio"];
            audio.sample_rate = a.value("sample_rate", audio.sample_rate);
            audio.channels = a.value("channels", audio.channels);
            audio.master_volume = a.value("master_volume", audio.master_volume);
        }

        if (j.contains("window")) {
            auto& w = j["window"];
            window.width = w.value("width", window.width);
            window.height = w.value("height", window.height);
            window.fullscreen = w.value("fullscreen", window.fullscreen);
            window.borderless = w.value("borderless", window.borderless);
            window.title = w.value("title", window.title);
        }

        return true;
    } catch (const json::exception&) {
        return false;
    }
}

bool ProjectSettings::save(const std::string& path) const {
    json j;

    j["project_name"] = project_name;
    j["asset_directory"] = asset_directory;
    j["startup_scene"] = startup_scene;

    j["physics"] = {
        {"fixed_timestep", physics.fixed_timestep},
        {"max_substeps", physics.max_substeps},
        {"gravity", {physics.gravity.x, physics.gravity.y, physics.gravity.z}}
    };

    j["render"] = {
        {"max_draw_calls", render.max_draw_calls},
        {"vsync", render.vsync},
        {"msaa_samples", render.msaa_samples},
        {"shadow_map_size", render.shadow_map_size},
        {"render_scale", render.render_scale}
    };

    j["audio"] = {
        {"sample_rate", audio.sample_rate},
        {"channels", audio.channels},
        {"master_volume", audio.master_volume}
    };

    j["window"] = {
        {"width", window.width},
        {"height", window.height},
        {"fullscreen", window.fullscreen},
        {"borderless", window.borderless},
        {"title", window.title}
    };

    return FileSystem::write_text(path, j.dump(4));
}

void ProjectSettings::reset() {
    *this = ProjectSettings{};
}

} // namespace engine::core
