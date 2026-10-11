add_executable(maintenance_tests
        test_main.cpp
        test_config.cpp
        test_form_response.cpp
        ${CMAKE_SOURCE_DIR}/src/config.cpp
        ${CMAKE_SOURCE_DIR}/src/form_response.cpp)
target_include_directories(maintenance_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)
maintenance_set_warnings(maintenance_tests)

add_test(NAME maintenance_tests COMMAND maintenance_tests)
