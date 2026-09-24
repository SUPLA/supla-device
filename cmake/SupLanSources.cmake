include_guard()

get_filename_component(SUPLA_DEVICE_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(SUPLAN_CORE_SRCS
  ${SUPLA_DEVICE_ROOT_DIR}/src/suplan/suplan_wire.cpp
  ${SUPLA_DEVICE_ROOT_DIR}/src/suplan/suplan_crypto.cpp
  ${SUPLA_DEVICE_ROOT_DIR}/src/suplan/suplan_session.cpp
  ${SUPLA_DEVICE_ROOT_DIR}/src/suplan/suplan_acl.cpp
  ${SUPLA_DEVICE_ROOT_DIR}/src/suplan/suplan_data.cpp
  ${SUPLA_DEVICE_ROOT_DIR}/src/suplan/suplan_fragment.cpp
  ${SUPLA_DEVICE_ROOT_DIR}/src/suplan/suplan_runtime.cpp
)
