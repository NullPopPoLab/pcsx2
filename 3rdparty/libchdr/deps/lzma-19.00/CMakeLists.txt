add_library(lzma_chdr STATIC
  include/7zTypes.h
  include/Alloc.h
  include/Bra.h
  include/Compiler.h
  include/CpuArch.h
  include/Delta.h
  include/LzFind.h
  include/LzHash.h
  include/Lzma86.h
  include/LzmaDec.h
  include/LzmaEnc.h
  include/LzmaLib.h
  include/Precomp.h
  include/Sort.h
  src/Alloc.c
  src/Bra86.c
  src/BraIA64.c
  src/CpuArch.c
  src/Delta.c
  src/LzFind.c
  src/Lzma86Dec.c
  src/LzmaDec.c
  src/LzmaEnc.c
  src/Sort.c
)

target_compile_definitions(lzma_chdr PRIVATE _7ZIP_ST)

target_include_directories(lzma_chdr PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(lzma_chdr INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")

