# The golden corpus's Communication test (issue #30): tests/golden/communication_tests.cpp and its world
# (commworld.cpp) with Communication.cpp and Control.cpp (Control.cpp's ReceivePackets is the dispatcher the test
# feeds; the x64 client's Control.cpp, PATELLA_VULKAN as in golden_client), with golden_client's settings
# (../CMakeLists.txt) but another exception model.
#
# The oracle (communicationtest.exe) builds with /EHa, and its error round raises structured exceptions that both
# sides' catch (...) catch: a fake TRSA::Encrypt raises one, and a block of a zero-sized patch divides by zero in
# ReceivePatchBlock. The original (VC7.1) catches them in catch (...); under the product's /EHsc (or /EHs) an x64
# catch (...) does not, and the runner would stop. This directory compiles with /EHa (CMake's default flags have
# /EHsc; replaced here, for this directory only, as tests/golden/network replaces it with /EHs).

string(REPLACE "/EHsc" "/EHa" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
# The Debug build's run-time checks fill every local with 0xcc (/RTCs, in CMake's /RTC1). The test fills the stack
# with a pattern before each call (commworld.h FillStack), so that a local read before it is written reads what the
# original's read in the oracle: Control.cpp's ReceivePackets passes its Container, not yet assigned, to
# ReceiveOpenContainer and ReceiveCloseContainer, whose TEND prints it when the first GetByte throws. Without
# /RTC1 the Debug build reads the pattern as the Release build does (this directory only).
string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")

add_library(golden_comm_tests OBJECT
	../commworld.cpp
	../communication_tests.cpp
	"${PROJECT_SOURCE_DIR}/src/Communication.cpp"
	"${PROJECT_SOURCE_DIR}/src/Control.cpp")
golden_client_settings(golden_comm_tests)
target_compile_definitions(golden_comm_tests PRIVATE PATELLA_VULKAN)
