include(${NAP_ROOT}/cmake/nap_module.cmake)

add_subdirectory(thirdparty/vban)
target_link_libraries(${PROJECT_NAME} vban)