include_guard()

get_filename_component(SUPLA_DEVICE_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(NETTLE_SRC_DIR "${SUPLA_DEVICE_ROOT_DIR}/src/nettle")


set(NETTLE_SRCS
  ${NETTLE_SRC_DIR}/bignum.c
  ${NETTLE_SRC_DIR}/gmp-glue.c
  ${NETTLE_SRC_DIR}/mini-gmp.c
  ${NETTLE_SRC_DIR}/pkcs1-rsa-sha256.c
  ${NETTLE_SRC_DIR}/pkcs1.c
  ${NETTLE_SRC_DIR}/rsa-sha256-verify.c
  ${NETTLE_SRC_DIR}/rsa-verify.c
  ${NETTLE_SRC_DIR}/rsa.c
  ${NETTLE_SRC_DIR}/sha256-compress.c
  ${NETTLE_SRC_DIR}/sha256.c
  ${NETTLE_SRC_DIR}/write-be32.c
  )

function(nettle target_name)
  target_sources(${target_name}
    PRIVATE
    ${NETTLE_SRCS}
    )

  target_include_directories(${target_name}
    PUBLIC
    ${NETTLE_SRC_DIR}
    )

    #  target_compile_definitions(${target_name} PUBLIC SUPLA_DEVICE)
endfunction()

