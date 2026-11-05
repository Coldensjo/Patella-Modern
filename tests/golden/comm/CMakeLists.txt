# The golden corpus's Communication and Control tests (issue #30): tests/golden/communication_tests.cpp,
# control_tests.cpp (which compiles Control.cpp in: the test reads and sets its statics by name; its ReceivePackets is
# the dispatcher Communication feeds; the x64 client's Control.cpp, PATELLA_VULKAN as in golden_client) and their
# world (commworld.cpp) with Communication.cpp, with golden_client's settings (../CMakeLists.txt) but another
# exception model.
#
# The oracle (communicationtest.exe) builds with /EHa, and its error round raises structured exceptions that both
# sides' catch (...) catch: a fake TRSA::Encrypt raises one (until issue #75 also a block of a zero-sized patch, which
# divided by zero in ReceivePatchBlock). The original (VC7.1) catches them in catch (...); under the product's /EHsc
# (or /EHs) an x64 catch (...) does not, and the runner would stop. This directory compiles with /EHa (CMake's
# default flags have /EHsc; replaced here, for this directory only, as tests/golden/network replaces it with /EHs).

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
	../control_tests.cpp
	"${PROJECT_SOURCE_DIR}/src/Communication.cpp")
golden_client_settings(golden_comm_tests)
target_compile_definitions(golden_comm_tests PRIVATE PATELLA_VULKAN)
