project(xcbwraper LANGUAGES CXX)
add_library(${PROJECT_NAME})
file (GLOB cpps  *.cpp)

target_sources(${PROJECT_NAME} PUBLIC ${cpps})
target_link_libraries(${PROJECT_NAME} xcb)
target_link_libraries(${PROJECT_NAME} xcb-composite)
