cmake_minimum_required(VERSION 3.16)
project(PokerAPI LANGUAGES CXX)

# ---- Build settings ----
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Warnings (feel free to tweak)
if(MSVC)
  add_compile_options(/W4 /permissive-)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ---- Dependencies: Pistache + Threads ----
# Try CMake package first
find_package(Pistache QUIET)

# Fallback to pkg-config if CMake package not found
if(NOT Pistache_FOUND)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(PISTACHE REQUIRED IMPORTED_TARGET libpistache pistache)
endif()

find_package(Threads REQUIRED)

# ---- Sources ----
set(SRC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/src")

set(SOURCES
  ${SRC_ROOT}/main.cpp

  ${SRC_ROOT}/http/server.cpp
  ${SRC_ROOT}/http/routes.cpp

  ${SRC_ROOT}/controllers/players_controller.cpp
  ${SRC_ROOT}/controllers/tables_controller.cpp
  ${SRC_ROOT}/controllers/state_controller.cpp
  ${SRC_ROOT}/controllers/chat_controller.cpp

  ${SRC_ROOT}/store/store.cpp

  ${SRC_ROOT}/util/time.cpp
  ${SRC_ROOT}/util/id.cpp
)

add_executable(pokerapi ${SOURCES})

# ---- Includes ----
target_include_directories(pokerapi PRIVATE
  ${SRC_ROOT}           # so we can #include "util/..." etc.
  ${CMAKE_CURRENT_SOURCE_DIR}  # for external/json.hpp at ./external/json.hpp
)

# ---- Linkage ----
if(Pistache_FOUND)
  # Using CMake's Pistache package (provides pistache::pistache target)
  target_link_libraries(pokerapi PRIVATE pistache::pistache Threads::Threads)
else()
  # Using pkg-config imported target
  target_link_libraries(pokerapi PRIVATE PkgConfig::PISTACHE Threads::Threads)
endif()

# ---- Platform tweaks ----
if(WIN32)
  # Pistache is primarily Linux/*nix; if building on Windows (via WSL or ports),
  # you may need extra libs. Nothing added here by default.
else()
  # On Unix-like systems, ensure -pthread is used (Threads::Threads already handles it).
endif()

# ---- Install (optional) ----
install(TARGETS pokerapi RUNTIME DESTINATION bin)
