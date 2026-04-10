cmake_minimum_required(VERSION 3.24)

if(APPLE AND NOT DEFINED CMAKE_C_COMPILER AND NOT DEFINED CMAKE_CXX_COMPILER)
  set(STAR_IDLE_GCC_DIRS
    "/opt/homebrew/bin"
    "/usr/local/bin"
  )
  set(STAR_IDLE_GCC_VERSIONS 15 14 13)
  foreach(STAR_IDLE_GCC_DIR IN LISTS STAR_IDLE_GCC_DIRS)
    foreach(STAR_IDLE_GCC_VERSION IN LISTS STAR_IDLE_GCC_VERSIONS)
      if(
        EXISTS "${STAR_IDLE_GCC_DIR}/gcc-${STAR_IDLE_GCC_VERSION}"
        AND EXISTS "${STAR_IDLE_GCC_DIR}/g++-${STAR_IDLE_GCC_VERSION}"
      )
        set(CMAKE_C_COMPILER "${STAR_IDLE_GCC_DIR}/gcc-${STAR_IDLE_GCC_VERSION}" CACHE FILEPATH "C compiler" FORCE)
        set(CMAKE_CXX_COMPILER "${STAR_IDLE_GCC_DIR}/g++-${STAR_IDLE_GCC_VERSION}" CACHE FILEPATH "C++ compiler" FORCE)
        message(STATUS "Using GCC toolchain from ${STAR_IDLE_GCC_DIR} (${STAR_IDLE_GCC_VERSION})")
        break()
      endif()
    endforeach()
    if(DEFINED CMAKE_CXX_COMPILER)
      break()
    endif()
  endforeach()
endif()

project(StarIdle LANGUAGES C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "Disable online FetchContent updates" FORCE)

include(FetchContent)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET sdl3)

FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG        docking
)
FetchContent_MakeAvailable(imgui)

add_library(imgui_docking STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp
)
target_include_directories(imgui_docking SYSTEM PUBLIC
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui_docking PUBLIC PkgConfig::SDL3)

FetchContent_Declare(
  stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG        master
)
FetchContent_MakeAvailable(stb)

FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

add_library(star_idle_warnings INTERFACE)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
  target_compile_options(star_idle_warnings INTERFACE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -Wshadow
    -Wconversion
    -Wfloat-conversion
    -Wsign-conversion
    -Wcast-qual
    -Wcast-align
    -Wformat=2
    -Wundef
    -Wnull-dereference
    -Wnon-virtual-dtor
    -Woverloaded-virtual
    -Wold-style-cast
    -Wzero-as-null-pointer-constant
    -Wdouble-promotion
    -Wimplicit-fallthrough
    -Wreturn-type
  )
endif()

add_library(stb_image_lib STATIC
  src/star_idle/platform/stb_image_impl.cpp
)
target_include_directories(stb_image_lib SYSTEM PRIVATE
  ${stb_SOURCE_DIR}
)

add_executable(star_idle
  src/main.cpp
  src/star_idle/app/application.cpp
  src/star_idle/game/game_state.cpp
  src/star_idle/platform/save_system.cpp
  src/star_idle/platform/texture_cache.cpp
)

target_include_directories(star_idle
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  SYSTEM PRIVATE
    ${stb_SOURCE_DIR}
)

target_link_libraries(star_idle
  PRIVATE
    star_idle_warnings
    stb_image_lib
    imgui_docking
    PkgConfig::SDL3
    nlohmann_json::nlohmann_json
)

add_custom_command(TARGET star_idle POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_CURRENT_SOURCE_DIR}/assets
          $<TARGET_FILE_DIR:star_idle>/assets
)
