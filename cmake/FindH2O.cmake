include(CPM)
include(CheckCSourceCompiles)

set(_h2o_options)
list(APPEND _h2o_options "WITH_MRUBY OFF")
list(APPEND _h2o_options "DISABLE_LIBUV ON")

cmake_policy(SET CMP0042 NEW)
CPMAddPackage(NAME h2o GIT_REPOSITORY https://github.com/h2o/h2o GIT_TAG cec8046 VERSION 2.3.0-cec8046 OPTIONS "${_h2o_options}")
set_property(TARGET libh2o PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET libh2o-evloop PROPERTY POSITION_INDEPENDENT_CODE ON)
