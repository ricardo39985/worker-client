# Dependencies and reference material

No third-party executable, model weight or source archive is bundled in this
source package. The bootstrap obtains Boost 1.89.0 from the official archive
and checks the SHA-256 recorded in config/dependencies.lock.json. Boost uses
Boost Software License 1.0; retain its license/attribution in a binary release.
Read the license in the downloaded source before redistribution.

MSVC Build Tools installation requires the user's explicit license acceptance.
The bootstrap uses the Microsoft-signed VS2022 installer channel; Windows APIs
and WinSQLite are OS-supplied components, not DLLs fetched from arbitrary sites.
FFmpeg licensing depends on the approved build configuration. This package does
not select or redistribute a particular FFmpeg binary, so its redistribution
obligations still need review when that artifact is chosen. No license for this
project's own code is selected; do not make it public without the owner's decision.

Primary references consulted while writing this implementation (not proof of
execution in this environment):

- Boost archive manifest: https://archives.boost.io/release/1.89.0/source/boost_1_89_0.tar.gz.json
- Boost.JSON: https://www.boost.org/doc/libs/1_89_0/libs/json/doc/html/index.html
- WinHTTP async/close ownership: https://learn.microsoft.com/en-us/windows/win32/api/winhttp/nf-winhttp-winhttpclosehandle
- WinHTTP WebSocket upgrade: https://learn.microsoft.com/en-us/windows/win32/api/winhttp/nf-winhttp-winhttpwebsocketcompleteupgrade
- Windows Job Objects: https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects
- DPAPI: https://learn.microsoft.com/en-us/windows/win32/api/dpapi/nf-dpapi-cryptprotectdata
- Tray API: https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shell_notifyiconw

The source lock deliberately does not claim any EmbeddingGemma, llama.cpp, Vulkan
or CUDA version has been installed, benchmarked or tested on the target laptop.
