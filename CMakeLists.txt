cmake_minimum_required(VERSION 3.15)
project(HedgeBot VERSION 1.0.0 LANGUAGES CXX C)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set default build type
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Compiler flags
if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find required packages
find_package(Eigen3 3.3 QUIET)
if(NOT Eigen3_FOUND)
  message(STATUS "Eigen3 not found - will use standard C++ only")
endif()

find_package(Boost 1.70 QUIET)
if(NOT Boost_FOUND)
  message(STATUS "Boost not found - some features may be unavailable")
endif()

# Optional: QuantLib for additional support
# find_package(QuantLib REQUIRED)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Source files
set(HEDGEBOT_SOURCES
    src/market_data.cpp
    src/option_data.cpp
    src/bsm_pricer.cpp
    src/merton_pricer.cpp
    src/greeks_calculator.cpp
    src/model_calibrator.cpp
    src/hedging_engine.cpp
    src/broker_simulator.cpp
    src/risk_manager.cpp
    src/backtester.cpp
    src/monte_carlo_simulator.cpp
    src/logger.cpp
    src/live_trading.cpp
)

# Create library
add_library(hedgebot_lib ${HEDGEBOT_SOURCES})

if(Eigen3_FOUND)
  target_link_libraries(hedgebot_lib PUBLIC Eigen3::Eigen)
endif()

if(Boost_FOUND)
  target_link_libraries(hedgebot_lib PUBLIC Boost::system)
endif()

target_include_directories(hedgebot_lib PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Create executable
add_executable(hedge_bot src/main.cpp)
target_link_libraries(hedge_bot PRIVATE hedgebot_lib)

# Live trading executable (paper/live-dryrun scaffold)
add_executable(live_bot src/live_main.cpp)
target_link_libraries(live_bot PRIVATE hedgebot_lib)

# Enable testing
enable_testing()

# Test executable
file(GLOB TEST_SOURCES "tests/*.cpp")
add_executable(run_tests ${TEST_SOURCES})
target_link_libraries(run_tests PRIVATE hedgebot_lib)
add_test(NAME HedgeBotTests COMMAND run_tests)

# Output directories
set_target_properties(hedge_bot PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

set_target_properties(live_bot PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

message(STATUS "HedgeBot Build Configuration:")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Eigen3 found: ${Eigen3_FOUND}")
message(STATUS "  Boost found: ${Boost_FOUND}")
