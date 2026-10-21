if(NOT OW_LAUNCHER_SOURCE OR NOT OW_LAUNCHER_OUTPUT)
  message(FATAL_ERROR "Headless launcher input/output required")
endif()
file(READ "${OW_LAUNCHER_SOURCE}" launcher)
string(REPLACE "\r\n" "\n" launcher "${launcher}")
# file(WRITE) uses host EOL on Windows. A single-variable template forces LF,
# without recursively interpreting any PowerShell ${...}, @...@ or comments.
string(REGEX REPLACE "\n$" "" OW_LAUNCHER_LITERAL "${launcher}")
file(WRITE "${OW_LAUNCHER_OUTPUT}.in" "@OW_LAUNCHER_LITERAL@")
configure_file("${OW_LAUNCHER_OUTPUT}.in" "${OW_LAUNCHER_OUTPUT}" @ONLY NEWLINE_STYLE UNIX)
file(SHA256 "${OW_LAUNCHER_OUTPUT}" OW_HEADLESS_LAUNCHER_SHA256)
