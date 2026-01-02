#include <engine/audio/audio_engine.hpp>
#include <engine/audio/audio_components.hpp>
#include <iostream>
#include <thread>
#include <chrono>

using namespace engine::audio;
using namespace engine::core;

int main(int argc, char** argv) {
    std::cout << "Initializing Audio Engine..." << std::endl;
    
    AudioSettings settings;
    settings.master_volume = 1.0f;
    get_audio_engine().init(settings);

    std::cout << "Testing Bus System..." << std::endl;
    AudioBusHandle sfxBus = get_audio_engine().get_bus(BuiltinBus::SFX);
    get_audio_engine().set_bus_volume(sfxBus, 0.5f);
    if (std::abs(get_audio_engine().get_bus_volume(sfxBus) - 0.5f) > 0.01f) {
        std::cerr << "Bus volume mismatch!" << std::endl;
        return 1;
    }
    
    std::cout << "Testing Reverb Params..." << std::endl;
    AudioEngine::ReverbParams reverb;
    reverb.room_size = 0.8f;
    reverb.wet_volume = 0.5f;
    get_audio_engine().set_reverb_params(reverb);
    
    std::cout << "Testing Thread Safety (Hammering API)..." << std::endl;
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([i]() {
            for (int j = 0; j < 100; ++j) {
                get_audio_engine().set_master_volume(0.5f + (j % 10) * 0.05f);
                get_audio_engine().get_playing_sound_count();
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    std::cout << "Thread safety test passed (no crash)." << std::endl;

    // Try to load a file if provided
    if (argc > 1) {
        std::cout << "Loading sound: " << argv[1] << std::endl;
        SoundHandle h = get_audio_engine().load_sound(argv[1]);
        if (h.valid()) {
            std::cout << "Playing sound..." << std::endl;
            get_audio_engine().play_sound(h);
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            
            std::cout << "Pausing..." << std::endl;
            get_audio_engine().pause(h);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            std::cout << "Resuming..." << std::endl;
            get_audio_engine().resume(h);
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            
            std::cout << "Fading out..." << std::endl;
            get_audio_engine().fade_out(h, 1.0f);
            std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        } else {
            std::cout << "Failed to load sound." << std::endl;
        }
    } else {
        std::cout << "No sound file provided, skipping playback test." << std::endl;
    }

    get_audio_engine().shutdown();
    std::cout << "Test Complete." << std::endl;
    return 0;
}
