include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_library(protobufrpc protobufrpc.cc socket_pool.cc workqueue.cc)
add_library(kvservice kvservice.cc backend.cc
            ${CMAKE_CURRENT_BINARY_DIR}/kvstore.pb.cc)

add_executable(kvstore kvstore.cc)

add_library(kvclient SHARED
            kvclient.cc kvservice.cc backend.cc kvstore.pb.cc
            protobufrpc.cc socket_pool.cc workqueue.cc)
target_link_libraries(kvclient
                      boost_thread-mt boost_regex boost_system
                      boost_program_options db protobuf pthread)

target_link_libraries(kvstore
                      kvservice
                      boost_thread-mt boost_regex boost_system
                      boost_program_options db protobuf protobufrpc pthread)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/kvstore.pb.cc
                          ${CMAKE_CURRENT_BINARY_DIR}/kvstore.pb.h
                   COMMAND protoc --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
                                  --proto_path=${CMAKE_CURRENT_SOURCE_DIR}
                                  ${CMAKE_CURRENT_SOURCE_DIR}/kvstore.proto
                   DEPENDS kvstore.proto)

