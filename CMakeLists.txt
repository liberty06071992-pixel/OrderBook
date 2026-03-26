cmake_minimum_required(VERSION 3.20)
project(HFT_OrderBook LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(hft_engine src/main.cpp)
target_include_directories(hft_engine PRIVATE include)

# HFT optimization flags: 
# -O3: Maximum optimization
# -march=native: Use all CPU instructions (AVX, etc.)
# -flto: Link-time optimization for cross-file inlining
target_compile_options(hft_engine PRIVATE -O3 -march=native -flto -Wall -Wextra)
target_link_options(hft_engine PRIVATE -flto)
