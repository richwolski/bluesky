set(KVSTORE_DIR "${CMAKE_SOURCE_DIR}/kvstore")
include_directories("${LIBS3_BUILD_DIR}/include" ${KVSTORE_DIR})
link_directories("${LIBS3_BUILD_DIR}/lib" ${KVSTORE_DIR})

add_executable(nfsproxy
               common.c nfsd.c rpc.c mount.c nfs3.c
               mount_prot_xdr.c nfs3_prot_xdr.c)

add_executable(synclient synclient.c common.c nfs3_prot_xdr.c)
add_executable(synreadbench synreadbench.c common.c nfs3_prot_xdr.c)

include_directories(${GLIB_INCLUDE_DIRS} "${CMAKE_SOURCE_DIR}/bluesky")
target_link_libraries(nfsproxy bluesky ${GLIB_LIBRARIES})
target_link_libraries(synclient ${GLIB_LIBRARIES})
target_link_libraries(synreadbench ${GLIB_LIBRARIES})
