add_library(ring-zkFLIOP
    utils/circuit.cpp
    protocol/rand_gen_pool.cpp
    protocol/zkfliop_evaluator.cpp
    protocol/semi_evaluator.cpp
    protocol/zkfliop_evaluator_dotp.cpp
    protocol/semi_evaluator_dotp.cpp)

target_include_directories(ring-zkFLIOP PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
IF(ENABLE_PROTOCOL_OMP)
message("OMP active for protocols")
target_link_libraries(ring-zkFLIOP PUBLIC EMPTool GMP OpenMP::OpenMP_CXX ${OpenMP_CXX_FLAGS})
ELSE()
message("No OMP for protocols")
target_link_libraries(ring-zkFLIOP PUBLIC EMPTool GMP)
ENDIF()
