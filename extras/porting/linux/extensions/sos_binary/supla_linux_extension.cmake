supla_linux_register_extension(
  NAME sos_binary
  INIT_FUNCTION initSosBinaryExtension
  SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/sos_binary.cpp
  INCLUDE_DIRS
    ${SUPLA_LINUX_PORT_DIR}
    ${_SUPLA_ROOT_FROM_LINUX}/src
)
