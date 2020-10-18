SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
project (wastedris)

add_executable (wastedris wastedris.cpp noncanonical.cpp game_core.cpp)
target_link_libraries (wastedris pthread)

