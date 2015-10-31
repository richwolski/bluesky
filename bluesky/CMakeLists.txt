set(KVSTORE_DIR "${CMAKE_SOURCE_DIR}/kvstore")
include_directories("${LIBS3_BUILD_DIR}/include" ${KVSTORE_DIR})
link_directories("${LIBS3_BUILD_DIR}/lib" ${KVSTORE_DIR})

add_library(bluesky SHARED
            cache.c cleaner.c cloudlog.c crc32c.c crypto.c debug.c dir.c file.c
            imap.c init.c inode.c log.c serialize.c store.c store-azure.c
            store-bdb.c store-kv.cc store-multi.c store-s3.c store-simple.c
            util.c)
add_executable(bluesky-test main.c)

set(CMAKE_C_FLAGS "-Wall -std=gnu99 ${CMAKE_C_FLAGS}")
set(INSTALL_RPATH_USE_LINK_PATH 1)

include_directories(${GLIB_INCLUDE_DIRS})
target_link_libraries(bluesky ${GLIB_LIBRARIES} db gcrypt s3 kvclient)
target_link_libraries(bluesky-test bluesky ${GLIB_LIBRARIES})

#set_target_properties(bluesky PROPERTIES LINK_INTERFACE_LIBRARIES "")
