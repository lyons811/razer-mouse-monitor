cmake_minimum_required(VERSION 3.20)
project(razer_mouse_monitor VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

add_executable(razer-mouse-monitor
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/MouseInfo.cpp
    src/MouseInfo.h
    src/RazerHidBackend.cpp
    src/RazerHidBackend.h
)

target_link_libraries(razer-mouse-monitor PRIVATE Qt6::Widgets Qt6::Core Qt6::Gui)

install(TARGETS razer-mouse-monitor RUNTIME DESTINATION bin)
install(FILES packaging/razer-mouse-monitor.desktop DESTINATION share/applications OPTIONAL)
install(FILES assets/razer-mouse-monitor.png DESTINATION share/icons/hicolor/512x512/apps OPTIONAL)
install(DIRECTORY assets/icons/hicolor DESTINATION share/icons OPTIONAL)


enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test)
add_executable(mouseinfo-tests
    tests/test_mouseinfo.cpp
    src/MouseInfo.cpp
    src/MouseInfo.h
    src/RazerHidBackend.cpp
    src/RazerHidBackend.h
)
target_include_directories(mouseinfo-tests PRIVATE src)
target_link_libraries(mouseinfo-tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME mouseinfo-tests COMMAND mouseinfo-tests)
