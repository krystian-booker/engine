# engine/ai/tests/CMakeLists.txt
# Unit tests for AI module

include(Testing)
engine_add_tests(ai EXTRA_LIBS engine::scene)
