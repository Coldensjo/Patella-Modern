# The golden corpus's Network test (issue #30): tests/golden/network_tests.cpp, which compiles Network.cpp in,
# with golden_client's settings (../CMakeLists.txt) but another exception model.
#
# Its error round makes the WinSock functions and rand() throw (fakes in their import slots), as the oracle's
# does (difftest builds with /EHa). With the product's /EHsc the compiler assumes that a C function never
# throws, and our x64 code lets such an exception pass Network.cpp's TEND blocks, where the original's catch
# it; no C function throws in either client, so only the fakes see the difference. This directory compiles
# with /EHs, /EHsc without the c: synchronous C++ exceptions, from C functions too (CMake's default flags have
# /EHsc; replaced here, for this directory only, so that no option overrides another).

string(REPLACE "/EHsc" "/EHs" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

add_library(golden_network_tests OBJECT ../network_tests.cpp)
golden_client_settings(golden_network_tests)
