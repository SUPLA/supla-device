set(SUPLA_DEVICE_FREERTOS_SRCS
  freertos_platform.cpp
  freertos_client.cpp
  freertos_log.c
)
# FreeRTOS sources required for Linux compilation (from portable folder)
set(FREERTOS_SRC
  $ENV{SUPLA_FREERTOS_PATH}/FreeRTOS/Source/event_groups.c
  $ENV{SUPLA_FREERTOS_PATH}/FreeRTOS/Source/list.c
  $ENV{SUPLA_FREERTOS_PATH}/FreeRTOS/Source/queue.c
  $ENV{SUPLA_FREERTOS_PATH}/FreeRTOS/Source/tasks.c
  $ENV{SUPLA_FREERTOS_PATH}/FreeRTOS/Source/timers.c
  #  $ENV{SUPLA_FREERTOS_PATH}/FreeRTOS/Source/croutine.c
  $ENV{SUPLA_FREERTOS_PATH}/FreeRTOS/Source/portable/ThirdParty/GCC/Posix/port.c
  $ENV{SUPLA_FREERTOS_PATH}/FreeRTOS/Source/portable/ThirdParty/GCC/Posix/utils/wait_for_event.c
  $ENV{SUPLA_FREERTOS_PATH}/FreeRTOS/Source/portable/MemMang/heap_3.c
  )

target_sources(supladevice
  PUBLIC
  ${SUPLA_DEVICE_FREERTOS_SRCS}
  ${FREERTOS_SRC}
  )

target_include_directories (supladevice
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  $ENV{SUPLA_FREERTOS_PATH}/FreeRTOS/Source/include
  $ENV{SUPLA_FREERTOS_PATH}/FreeRTOS/Source/portable/ThirdParty/GCC/Posix
  )

target_compile_definitions(supladevice PUBLIC SUPLA_FREERTOS)

