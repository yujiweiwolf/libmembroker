# cmake_minimum_required(VERSION <specify CMake version here>)
cmake_minimum_required(VERSION 3.19)
project(libmembroker)

set(CMAKE_CXX_STANDARD 17)
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
set(CMAKE_BUILD_TYPE "Debug")

#set_target_properties(libbroker PROPERTIES OUTPUT_NAME broker)
#
#set(LIBBROKER_VERSION 1.1.0)
#set(DEST_DIR "${CMAKE_CURRENT_BINARY_DIR}/libbroker-${LIBBROKER_VERSION}")
#set(DEST_FILE "${DEST_DIR}.tar.gz")
#set(INSTALL_PREFIX "$ENV{HOME}/sys/lib/")
#set(INSTALL_DIR "${INSTALL_PREFIX}/libbroker-${LIBBROKER_VERSION}")
#
#add_custom_command(
#        OUTPUT ${DEST_FILE} ${DEST_DIR}
#        COMMAND sh -c "rm -rf ${CMAKE_CURRENT_BINARY_DIR}/libbroker-*"
#        COMMAND sh -c "mkdir -p ${DEST_DIR}; cd ${DEST_DIR}; mkdir -p include/broker lib/cmake/broker bin;"
#        COMMAND sh -c "cp -rf ${CMAKE_CURRENT_SOURCE_DIR}/src/broker/*.h ${DEST_DIR}/include/broker/"
#        COMMAND cp -rf ${CMAKE_CURRENT_SOURCE_DIR}/broker-config.cmake ${DEST_DIR}/lib/cmake/broker/
#        COMMAND cp -rf ${CMAKE_CURRENT_BINARY_DIR}/$<TARGET_FILE_NAME:libbroker> ${DEST_DIR}/lib/
#        # COMMAND python setup.py sdist
#        COMMAND tar zcf ${DEST_FILE} libbroker-${LIBBROKER_VERSION}
#        # COMMAND rm -rf ${DEST_DIR}
#        DEPENDS libbroker
#        VERBATIM)
#
#add_custom_target(dist ALL DEPENDS ${DEST_FILE})
#
#set(CMAKE_INSTALL_PREFIX ${INSTALL_PREFIX})
#install(CODE "execute_process(COMMAND sh -c \"rm -rf libbroker-*\" WORKING_DIRECTORY ${INSTALL_PREFIX})")
#install(FILES ${DEST_FILE} DESTINATION ${INSTALL_PREFIX})
#install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E tar zxf ${DEST_FILE} WORKING_DIRECTORY ${INSTALL_PREFIX})")
#
## ----------------------------------------------------
#add_executable(fake_broker
#        EXCLUDE_FROM_ALL
#        src/fake_broker/main.cc
#        src/fake_broker/fake_broker.cc
#        src/fake_broker/config.h
#        src/fake_broker/libbroker_fake.h
#        src/fake_broker/fake_broker.h
#        src/fake_broker/config.cc
#        )
#target_link_libraries(fake_broker libbroker ${CORAL_LIBRARIES} ${X_LIBRARIES} boost_program_options)
#
#FILE(COPY Dockerfile DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
#FILE(WRITE ${CMAKE_CURRENT_BINARY_DIR}/image.sh "n=$1; v=`./$n -v`; ldd $n | grep /sys/lib/ | awk '{print $3}' | xargs -i cp -L -n {} .; docker build -t $n:$v .; rm -rf \${n}_$v.docker.tar.gz; docker save $n:$v > \${n}_$v.docker.tar && gzip \${n}_$v.docker.tar")
#add_custom_target(image COMMAND sh image.sh fake_broker DEPENDS fake_broker)


aux_source_directory (./src/mem_broker LIB_LIST)
add_library (membroker ${LIB_LIST})

aux_source_directory (./src/simulate_broker MEM_TEST_LIST)
add_executable(imitative_broker ${MEM_TEST_LIST})
target_link_libraries(imitative_broker
        membroker coral swordfish x stdc++fs yaml-cpp  clickhouse-cpp-lib-static boost_date_time boost_filesystem boost_regex boost_system  boost_chrono boost_log boost_program_options boost_thread boost_iostreams boost_context boost_coroutine z protobuf protobuf-lite sodium zmq ssl crypto iconv pthread dl)

add_executable(send_req src/send_req/send_req.cc)
target_link_libraries(send_req
        coral swordfish x stdc++fs yaml-cpp  clickhouse-cpp-lib-static boost_date_time boost_filesystem boost_regex boost_system  boost_chrono boost_log boost_program_options boost_thread boost_iostreams boost_context boost_coroutine z protobuf protobuf-lite sodium zmq ssl crypto iconv pthread dl)

SET(ROOT_DIR "/home/work/sys/lib")
FILE(GLOB COMMON_LIB_NAME "${ROOT_DIR}/common_lib/lib/*so*")
FILE(COPY ${COMMON_LIB_NAME} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

add_custom_target(cpplint COMMAND cpplint --recursive ${CMAKE_CURRENT_SOURCE_DIR}/src/mem_broker/*.*)





