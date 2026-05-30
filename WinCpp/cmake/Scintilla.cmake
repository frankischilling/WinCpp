# Build Scintilla as a static library for Win32.
if (TARGET scintilla)
  return()
endif()

file(GLOB scintilla_sources CONFIGURE_DEPENDS
  "${scintilla_SOURCE_DIR}/src/*.cxx"
  "${scintilla_SOURCE_DIR}/win32/*.cxx"
)

add_library(scintilla STATIC ${scintilla_sources})

target_include_directories(scintilla PUBLIC
  "${scintilla_SOURCE_DIR}/include"
  "${scintilla_SOURCE_DIR}/src"
  "${scintilla_SOURCE_DIR}/win32"
)

set_target_properties(scintilla PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED YES
  CXX_EXTENSIONS NO
)

target_compile_definitions(scintilla PRIVATE DISABLE_D2D)

# Scintilla uses OLE drag/drop and IME APIs on Windows.
target_link_libraries(scintilla PUBLIC imm32 ole32 oleaut32 uuid)
