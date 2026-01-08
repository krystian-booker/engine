# engine/effects/tests/CMakeLists.txt

include(Testing)
engine_add_tests(effects EXTRA_LIBS engine::stats engine::scene)
