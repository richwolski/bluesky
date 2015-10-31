include_directories("${LIBS3_BUILD_DIR}/include")
link_directories("${LIBS3_BUILD_DIR}/lib")

add_executable(s3readbench readbench.c)
add_executable(s3readlatency readlatency.c)
add_executable(s3readdelay readdelay.c)

set(CMAKE_C_FLAGS "-Wall -std=gnu99 ${CMAKE_C_FLAGS}")
set(INSTALL_RPATH_USE_LINK_PATH 1)

target_link_libraries(s3readbench m pthread s3)
target_link_libraries(s3readlatency pthread s3)
target_link_libraries(s3readdelay pthread s3)
