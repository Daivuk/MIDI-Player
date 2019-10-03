cmake_minimum_required(VERSION 3.4.0)

# Define LINUX
if (UNIX AND NOT APPLE)
    set(LINUX 1)
endif()

# Setup modules path
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

# Project name
project(midi_experiment)

# Some compiler flags
set(CMAKE_CXX_STANDARD 11) # C++11
if (MSVC)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" "/MP") # Multi core in VS
endif()

# Define _DEBUG
if (CMAKE_BUILD_TYPE MATCHES Debug)
    add_definitions(-D_DEBUG)
endif()

#justwindowsthings
if (WIN32)
    add_definitions(-DNOMINMAX)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

# Project files
file(GLOB srcfiles ./src/*.*)

list(APPEND includes PUBLIC ./src/)

#------------------------------------------------------------------------------
# Third parties
#------------------------------------------------------------------------------
if (WIN32)
    list(APPEND libs PUBLIC Mfplat)
endif()

#------------------------------------------------------------------------------
# Assets
#------------------------------------------------------------------------------

file(GLOB files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/*.*")
foreach(file ${files})
    message("configuring ${file}")
    configure_file(${file} ${CMAKE_BINARY_DIR}/${file} COPYONLY)
endforeach()

#------------------------------------------------------------------------------
# Exe
#------------------------------------------------------------------------------

# midi_experiment.exe, use WinMain on Windows
source_group("thirdparty" FILES ${srcthirdparty})
source_group("game" FILES ${srcfiles})
add_executable(midi_experiment ${srcfiles} ${srcthirdparty})

# Lib/Headers
target_include_directories(midi_experiment ${includes})
target_link_libraries(midi_experiment ${libs})
