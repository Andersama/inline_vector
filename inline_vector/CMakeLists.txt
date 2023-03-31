# CMakeList.txt : CMake project for inline_vector, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable (inline_vector "inline_vector.cpp" "inline_vector.h" "std_headers.h")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET inline_vector PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
