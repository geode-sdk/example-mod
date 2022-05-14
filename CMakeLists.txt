cmake_minimum_required(VERSION 3.3.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(Template VERSION 1.0.0)

# Use GLOB_RECURSE instead of GLOB
# to recursively add all source files 
# under src/ 
file(GLOB SOURCES
	src/*.cpp
)

# Set up the mod binary
add_library(${PROJECT_NAME} SHARED ${SOURCES})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")

# Find Geode. If you've installed 
# the Geode developer tools using 
# the official installer, this should 
# work out-of-the-box; otherwise, you 
# will have to add the GEODE_SUITE 
# environment variable yourself.
find_path(GEODE_SDK_PATH
	NAMES Geode.cmake
	PATHS $ENV{GEODE_SUITE}/sdk /Users/Shared/Geode/suite/sdk /usr/local/geode/sdk
	DOC "Geode SDK path."
	REQUIRED
)
include(${GEODE_SDK_PATH}/Geode.cmake)

# Set up this project as a Geode mod.
# This will include all the required 
# directories and link the necessary
# libraries, as well as create & 
# install the .geode package after 
# builds.
setup_geode_mod()
