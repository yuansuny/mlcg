cmake_minimum_required(VERSION 2.8.4)

project(GCP)

set(CMAKE_MODULE_PATH               "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_CXX_FLAGS                 "${CMAKE_CXX_FLAGS} -std=c++11 -O3 -flto -Wall -fPIC -fopenmp")
set(CMAKE_INCLUDE_SYSTEM_FLAG_CXX   "-isystem ")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY  "${CMAKE_CURRENT_SOURCE_DIR}/build")
set(LINKER_OPTIONS                  -flto -Wl,--no-as-needed)

FILE(GLOB SOURCE_FILES src/*.cpp src/*.cc src/*/*.c src/*/*.cpp src/*/*/*.c)

find_package(Gurobi)
include_directories(SYSTEM ${GUROBI_INCLUDE_DIRS})
include_directories(SYSTEM "/home/yuans/igraph-0.8.5/lib/usr/local/include/igraph")
include_directories(SYSTEM "/home/yuans/igraph-0.8.5/lib/usr/local/lib/")

add_executable(GCP ${SOURCE_FILES})

target_include_directories(GCP PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/svm"
				       "${CMAKE_CURRENT_SOURCE_DIR}/src/linear_svm"
				       "${CMAKE_CURRENT_SOURCE_DIR}/src/linear_svm/blas")
target_link_libraries(  GCP PRIVATE ${GUROBI_CXX_LIBRARY})
target_link_libraries(  GCP PRIVATE ${GUROBI_LIBRARY})
target_link_libraries(  GCP PRIVATE dl)
target_link_libraries(  GCP PRIVATE -L/home/yuans/igraph-0.8.5/lib/usr/local/lib igraph)

