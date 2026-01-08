#include <engine/audio/audio_engine.hpp>
#include <engine/core/log.hpp>
#include <thread>
#include <chrono>

using namespace engine::audio;
using namespace engine::core;

#include <iostream>

int main() {
    std::cout << "Starting audio test..." << std::endl;
    AudioSettings settings;
    // Disable audio device requirement if possible? Miniaudio defaults usually try generic.
    // settings.sample_rate = 44100; 
    
    get_audio_engine().init(settings);
    std::cout << "Engine initialized." << std::endl;
    
    // Create a dummy sound for testing API calls (won't actually play anything without a file)
    SoundHandle h = get_audio_engine().play("test.wav"); 
    std::cout << "Sound play requested. Handle valid: " << h.valid() << std::endl;

    // Even if invalid, these should not crash
    get_audio_engine().set_sound_attenuation_model(h, AttenuationModel::InverseSquare);
    get_audio_engine().set_sound_min_max_distance(h, 1.0f, 100.0f);
    get_audio_engine().set_sound_cone(h, 45.0f, 90.0f, 0.5f);
    get_audio_engine().set_sound_rolloff(h, 1.5f);
    get_audio_engine().set_sound_doppler_factor(h, 1.0f);
    std::cout << "3D settings applied." << std::endl;
    
    // Simulate Doppler updates
    for (int i = 0; i < 5; ++i) {
        get_audio_engine().set_sound_velocity(h, Vec3{10.0f, 0.0f, 0.0f});
        get_audio_engine().update(0.016f);
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    std::cout << "Update loop finished." << std::endl;
    
    get_audio_engine().shutdown();
    std::cout << "Engine shutdown." << std::endl;
    return 0;
}
