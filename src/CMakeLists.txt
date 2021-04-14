add_executable(${PROJECT_NAME})
file (GLOB cpps  *.cpp)

target_sources(${PROJECT_NAME} PRIVATE ${cpps})
target_link_libraries(${PROJECT_NAME} vulkan)
target_link_libraries(${PROJECT_NAME} xcb)
target_link_libraries(${PROJECT_NAME} xcb-composite)
#target_link_libraries(${PROJECT_NAME} SDL2 ) 
