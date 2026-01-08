# engine/inventory/tests/CMakeLists.txt

include(Testing)
engine_add_tests(inventory EXTRA_LIBS engine::stats engine::scene)
