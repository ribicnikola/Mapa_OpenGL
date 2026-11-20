cmake_minimum_required(VERSION 3.20)
project(Mapa_OpenGL LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "Ovaj paket koristi gotove Win64/MSVC GLFW i GLEW biblioteke iz RG vezbi. Pokrenuti na Windows-u sa Visual Studio C++.")
endif()
add_executable(Mapa_OpenGL src/Main.cpp src/Util.cpp)
target_include_directories(Mapa_OpenGL PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/vendor/include)
target_link_libraries(Mapa_OpenGL PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/lib/glew32.lib
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/lib/glfw3dll.lib
    opengl32.lib
)
# U VS-u: Debug > Start Without Debugging; radni folder je folder exe-a.
set_target_properties(Mapa_OpenGL PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:Mapa_OpenGL>")
add_custom_command(TARGET Mapa_OpenGL POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/shaders" "$<TARGET_FILE_DIR:Mapa_OpenGL>/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/res" "$<TARGET_FILE_DIR:Mapa_OpenGL>/res"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/vendor/bin/glew32.dll" "$<TARGET_FILE_DIR:Mapa_OpenGL>/glew32.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/vendor/bin/glfw3.dll" "$<TARGET_FILE_DIR:Mapa_OpenGL>/glfw3.dll"
)
