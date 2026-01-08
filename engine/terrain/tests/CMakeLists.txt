# engine/terrain/tests/CMakeLists.txt

include(Testing)
engine_add_tests(terrain EXTRA_LIBS 
    engine::cinematic
    engine::navigation 
    engine::vegetation
    engine::ui 
    engine::script
    engine::audio
    engine::plugin
)
