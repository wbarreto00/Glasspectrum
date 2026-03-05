cmake_minimum_required(VERSION 3.16)

set(QA_LIB_SOURCES
    ${CMAKE_SOURCE_DIR}/src/lens_profile_db.cpp
    ${CMAKE_SOURCE_DIR}/src/color_pipeline.cpp
    ${CMAKE_SOURCE_DIR}/src/trait_mixer.cpp
    ${CMAKE_SOURCE_DIR}/src/glasspectum_processor.cpp
)

add_executable(glasspectum_qa glasspectum_qa.cpp ${QA_LIB_SOURCES})
target_include_directories(glasspectum_qa PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
