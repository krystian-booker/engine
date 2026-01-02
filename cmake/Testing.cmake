# Testing.cmake
# Helper functions for unit testing with Catch2

include(Catch)

# Helper function to add module tests
# Usage: engine_add_tests(MODULE_NAME [EXTRA_LIBS lib1 lib2 ...])
function(engine_add_tests MODULE_NAME)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "EXTRA_LIBS")

    set(TEST_TARGET ${MODULE_NAME}_tests)

    # Collect test sources
    file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "*.cpp")

    if(TEST_SOURCES)
        add_executable(${TEST_TARGET} ${TEST_SOURCES})

        target_link_libraries(${TEST_TARGET}
            PRIVATE
                engine::${MODULE_NAME}
                Catch2::Catch2WithMain
                ${ARG_EXTRA_LIBS}
        )

        set_target_properties(${TEST_TARGET} PROPERTIES
            FOLDER "tests"
        )

        # Auto-discover and register tests with CTest
        catch_discover_tests(${TEST_TARGET})
    endif()
endfunction()
