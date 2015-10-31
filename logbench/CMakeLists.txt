add_executable(logbench logbench.c)

set(CMAKE_C_FLAGS "-Wall -std=gnu99 ${CMAKE_C_FLAGS}")

include_directories(${GLIB_INCLUDE_DIRS})
target_link_libraries(logbench ${GLIB_LIBRARIES} db)
