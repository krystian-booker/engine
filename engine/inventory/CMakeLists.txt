# engine/inventory/CMakeLists.txt

add_library(engine_inventory STATIC
    src/item_definition.cpp
    src/item_instance.cpp
    src/inventory_component.cpp
    src/inventory_manager.cpp
    src/inventory_registration.cpp
)
add_library(engine::inventory ALIAS engine_inventory)

target_include_directories(engine_inventory
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_inventory
    PUBLIC
        engine::core
        engine::scene
        engine::stats
        engine::reflect
)

target_compile_features(engine_inventory PUBLIC cxx_std_20)

# Set IDE folder
set_target_properties(engine_inventory PROPERTIES FOLDER "Engine")
