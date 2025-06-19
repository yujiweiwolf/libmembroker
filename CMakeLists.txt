# cmake_minimum_required(VERSION <specify CMake version here>)
cmake_minimum_required(VERSION 3.19)
project(libmembroker)

set(CMAKE_BUILD_TYPE "Debug") # Release Debug
add_compile_options(-std=c++17) # -std=c++17 -Wall -O2
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")

message("*****************************************")
find_package(X REQUIRED)
include_directories(${X_INCLUDE_DIRS})
link_directories(${X_LIBRARY_DIRS})

find_package(CORAL REQUIRED)
include_directories(${CORAL_INCLUDE_DIRS})
link_directories(${CORAL_LIBRARY_DIRS})
message("*****************************************")


add_library(risker
        src/risker/base_risker.cc
        src/risker/fancapital/fancapital_risker.cc
        src/risker/fancapital/fancapital_risker.h
        src/risker/risk_options.h
        src/risker/common/anti_self_knock_risker.cc
        src/risker/common/order_book.cc
        src/risker/common/anti_self_knock_risker.h
        src/risker/common/order_book.h
        src/risker/base_risker.h
        src/risker/risk_master.cc
        src/risker/risk_options.cc
        src/risker/risk_master.h
        )

aux_source_directory (./src/mem_broker LIB_LIST)
#message(${LIB_LIST})
add_library (membroker ${LIB_LIST})
add_dependencies(membroker risker)

aux_source_directory (./src/simulate_broker MEM_TEST_LIST)
add_executable(imitative_broker ${MEM_TEST_LIST})
target_link_libraries(imitative_broker
        membroker risker coral swordfish x stdc++fs yaml-cpp  clickhouse-cpp-lib-static boost_date_time boost_filesystem boost_regex boost_system  boost_chrono boost_log boost_program_options boost_thread boost_iostreams boost_context boost_coroutine z protobuf protobuf-lite sodium zmq ssl crypto iconv pthread dl)

#add_executable(send_req src/send_req/send_req.cc)
#target_link_libraries(send_req
#        coral swordfish x stdc++fs yaml-cpp  clickhouse-cpp-lib-static boost_date_time boost_filesystem boost_regex boost_system  boost_chrono boost_log boost_program_options boost_thread boost_iostreams boost_context boost_coroutine z protobuf protobuf-lite sodium zmq ssl crypto iconv pthread dl)

aux_source_directory (src/gtest/test_membroker TESTBROKER)
add_executable(gtest_broker ${TESTBROKER})
target_link_libraries(gtest_broker
        gtest gtest_main membroker coral swordfish x stdc++fs yaml-cpp  clickhouse-cpp-lib-static boost_date_time boost_filesystem boost_regex boost_system  boost_chrono boost_log boost_program_options boost_thread boost_iostreams z protobuf protobuf-lite sodium zmq ssl crypto iconv pthread dl)


add_custom_target(cpplint COMMAND cpplint --recursive ${CMAKE_CURRENT_SOURCE_DIR}/src/mem_broker/*.*)





