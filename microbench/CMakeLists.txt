add_executable(bench bench.c)
target_link_libraries(bench pthread rt)

add_executable(readbench readbench.c)
target_link_libraries(readbench pthread rt)

add_executable(statbench statbench.c)
target_link_libraries(statbench pthread rt)

add_executable(mixedbench mixedbench.c)
target_link_libraries(mixedbench pthread rt m)

add_executable(lockmem lockmem.c)

set(CMAKE_C_FLAGS "-Wall -std=gnu99 ${CMAKE_C_FLAGS}")
