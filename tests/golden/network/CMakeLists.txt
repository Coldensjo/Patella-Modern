# The golden corpus's Network test (issue #30): tests/golden/network_tests.cpp, which compiles Network.cpp in,
# with golden_client's settings (../CMakeLists.txt) but another exception model.
#
# Its error round makes the WinSock functions and rand() throw (fakes in their import slots), as the oracle's
# did (its test programs built with /EHa). With the product's /EHsc the compiler assumes that a C function never
# throws, and our x64 code lets such an exception pass Network.cpp's functions, where the original's TEND blocks
# catch it; no C function throws in either client, so only the fakes see the difference. This directory compiles
# with /EHs, /EHsc without the c: synchronous C++ exceptions, from C functions too (CMake's default flags have
# /EHsc; replaced here, for this directory only, so that no option overrides another).

string(REPLACE "/EHsc" "/EHs" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

add_library(golden_network_tests OBJECT ../network_tests.cpp)
golden_client_settings(golden_network_tests)
# Linux with UBSan (linux-x64-clang, issue #91): the Network test calls ConnectSocket without a socket, as its rounds
# reach it, and Network.cpp calls Socket->Connect on the null pointer, as the original does; the test's CAsyncSocket
# takes a null this (network_tests.cpp SocketConnect). UBSan's null check reports the member call, so it is off for
# this file only (notes/decisions.md, "The golden corpus on Linux (issue #91)").
if(golden_linux AND PATELLA_SANITIZE MATCHES "undefined")
	target_compile_options(golden_network_tests PRIVATE -fno-sanitize=null)
endif()
