cmake_minimum_required(VERSION 3.21)

project(Maintenance
        VERSION 3.0.0
        DESCRIPTION "Password-protected maintenance mode for Endstone servers"
        LANGUAGES CXX)

option(MAINTENANCE_BUILD_PLUGIN "Build the Endstone plugin (fetches Endstone)" ON)
option(MAINTENANCE_BUILD_TESTS "Build the unit tests" OFF)

set(ENDSTONE_VERSION "v0.11.13" CACHE STRING "Endstone git tag to build against")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Sources that do not depend on Endstone. They are compiled straight into the plugin and the test
# executable (rather than a static library) so both always use the same C++ standard library ABI.
# Absolute paths so the list can be reused unchanged from the tests/ subdirectory.
set(MAINTENANCE_CORE_SOURCES
        ${PROJECT_SOURCE_DIR}/src/config.cpp
        ${PROJECT_SOURCE_DIR}/src/form_response.cpp
        ${PROJECT_SOURCE_DIR}/src/settings.cpp)

# Endstone's public headers trigger -Wextra/-Wpedantic warnings we cannot fix, so the plugin target only
# gets -Wall; the Endstone-independent core and the tests are built with the strict set.
function(maintenance_set_warnings target)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "LENIENT" "" "")
    if (MSVC)
        target_compile_options(${target} PRIVATE /utf-8 $<IF:$<BOOL:${ARG_LENIENT}>,/W3,/W4>)
    elseif (ARG_LENIENT)
        target_compile_options(${target} PRIVATE -Wall)
    else ()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif ()
endfunction()

if (MAINTENANCE_BUILD_PLUGIN)
    include(FetchContent)
    FetchContent_Declare(
            endstone
            GIT_REPOSITORY https://github.com/EndstoneMC/endstone.git
            GIT_TAG ${ENDSTONE_VERSION}
            GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(endstone)

    endstone_add_plugin(${PROJECT_NAME}
            src/plugin.cpp
            ${MAINTENANCE_CORE_SOURCES})
    target_include_directories(${PROJECT_NAME} PRIVATE include)
    target_compile_definitions(${PROJECT_NAME} PRIVATE MAINTENANCE_VERSION="${PROJECT_VERSION}")
    maintenance_set_warnings(${PROJECT_NAME} LENIENT)
endif ()

if (MAINTENANCE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()
