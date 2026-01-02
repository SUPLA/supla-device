include_guard()

function(supla_apply_warnings tgt)
  target_compile_options(${tgt} PRIVATE
    -Wall -Wextra -Werror -Wformat=2 -Wcast-align -Wcast-qual
    -Wmissing-field-initializers -Wpointer-arith -Wredundant-decls
    -Wstrict-aliasing=2 -Wunreachable-code -Wunused -Wunused-parameter
    -Wvariadic-macros -Wwrite-strings

    -Wstringop-truncation
    -Werror=stringop-truncation
  )
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(${tgt} PRIVATE
      -Wstringop-truncation
      -Werror=stringop-truncation
    )
  endif()
endfunction()

