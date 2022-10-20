cmake_minimum_required(VERSION 3.0)

project(thread_pool)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

include_directories(${PROJECT_SOURCE_DIR}/include)

aux_source_directory(src SRC_LIST)

add_executable(mythread_pool_test myThread_pool_test.c ${SRC_LIST})

aux_source_directory(Client SRC_LIST2)

add_executable(client ${SRC_LIST2})

target_link_libraries(mythread_pool_test  pthread)