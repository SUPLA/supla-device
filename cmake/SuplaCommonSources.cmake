include_guard()

get_filename_component(SUPLA_DEVICE_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(SUPLA_COMMON_SRC_DIR "${SUPLA_DEVICE_ROOT_DIR}/src/supla-common")


set(SUPLA_COMMON_SRCS
  ${SUPLA_COMMON_SRC_DIR}/lck.c
  ${SUPLA_COMMON_SRC_DIR}/log.c
  ${SUPLA_COMMON_SRC_DIR}/proto.c
  ${SUPLA_COMMON_SRC_DIR}/srpc.c
  )

function(supla_common target_name)
  target_sources(${target_name}
    PRIVATE
    ${SUPLA_COMMON_SRCS}
    )

  target_include_directories(${target_name}
    PUBLIC
    ${SUPLA_COMMON_SRC_DIR}
    )

  target_compile_definitions(${target_name} PUBLIC SUPLA_DEVICE)
endfunction()

