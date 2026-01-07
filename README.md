# tiny_vfs

Tiny, header-only virtual filesystem for games and rendering.

Features
- Mount multiple backends under a single virtual path tree.
- Overlay behavior: the most recent mount wins for reads/writes.
- Small, modern C++17 API with a simple blob type and callbacks.
- Disk backend included; custom backends can be implemented via `tinyvfs::Backend`.

Quick start
```cpp
#include "tiny_vfs.h"

int main() {
    tinyvfs::Vfs vfs;
    vfs.mount_disk("assets", "data/assets");

    auto text = vfs.read_text("assets/hello.txt");
    if (text) {
        // Use text->c_str() or *text
    }
}
```

Build and test (MSVC via CMake)
```bat
build_and_test.bat
```

Manual CMake
```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release
```

Examples and tests
- `examples/example_vfs.cpp` loads `examples/assets/hello.txt`.
- `tests/tiny_vfs_test.cpp` covers mounts, overlays, read/write, and enumeration.

Requirements
- C++17 compiler with `<filesystem>` support (MSVC 2019+ recommended).

License
- Public domain. See `LICENSE`.
