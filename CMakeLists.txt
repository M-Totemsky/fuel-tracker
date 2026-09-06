cmake_minimum_required(VERSION 3.16)
project(fuel-tracker VERSION 0.3.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt5 COMPONENTS Core Quick Sql REQUIRED)

set(SOURCES
    src/main.cpp
    src/fueltracker.cpp
)

set(HEADERS
    src/fueltracker.h
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt5::Core
    Qt5::Quick
    Qt5::Sql
)

target_include_directories(${PROJECT_NAME} PRIVATE src)

# aa-exec-click only resolves binary names on $PKGDIR and
# $PKGDIR/lib/<multiarch-triplet>/bin. Install there (that is where the
# launcher's PATH points) and also into bin/ for compatibility.
install(TARGETS ${PROJECT_NAME} DESTINATION lib/aarch64-linux-gnu/bin)
install(PROGRAMS $<TARGET_FILE:${PROJECT_NAME}> DESTINATION bin)
install(DIRECTORY qml/ DESTINATION qml)
install(DIRECTORY icons/ DESTINATION icons)

# Click-package metadata (required by clickable/cmake builder)
install(FILES manifest.json apparmor.json fuel-tracker.desktop DESTINATION .)

# Desktop quick-build helper (not for click packaging)
if(DEFINED ENV{DESKTOP_BUILD})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/desktop)
endif()
