cmake_minimum_required(VERSION 3.16)

# Test sources
set(TEST_LIB_SOURCES
    ${CMAKE_SOURCE_DIR}/src/lens_profile_db.cpp
    ${CMAKE_SOURCE_DIR}/src/color_pipeline.cpp
    ${CMAKE_SOURCE_DIR}/src/trait_mixer.cpp
)

# Test: Lens DB validation
add_executable(test_lens_db test_lens_db.cpp ${TEST_LIB_SOURCES})
target_include_directories(test_lens_db PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)

# Test: Pipeline stages
add_executable(test_pipeline test_pipeline.cpp
    ${TEST_LIB_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/glasspectum_processor.cpp
)
target_include_directories(test_pipeline PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
