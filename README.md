# tiny_vfs

Tiny, header-only virtual filesystem for games and rendering.

Usage (quick start)
```cpp
#include "tiny_vfs.h"

int main() {
    tinyvfs::Vfs vfs;
    vfs.mount_disk("assets", "data/assets"); // base content

    auto text = vfs.read_text("assets/hello.txt");
    if (text) {
        // Use text->c_str() or *text
    }
}
```

Use cases
- Modding and overrides: mount a mod folder last to replace base assets without code changes.
- Patching/hotfixes: mount a small patch folder after base content.
- DLC/optional packs: mount extra content under a separate virtual root.
- Testing: mount a test data folder last to inject fixtures.

Example: patch + mod overlay
```cpp
tinyvfs::Vfs vfs;
vfs.mount_disk("assets", "C:/Game/Content");            // base
vfs.mount_disk("assets", "C:/Game/Patches/1.0.1");      // hotfix
vfs.mount_disk("assets", "C:/Users/Alice/Mods/CoolMod"); // mod

auto mat = vfs.read_text("assets/materials/metal.json"); // mod wins if present
```

End-user API (most useful calls)

Mounting
- `mount_disk(virtual_root, disk_path)` mounts a folder under a virtual root.
- `unmount(virtual_root)` removes a mount.

```cpp
tinyvfs::Vfs vfs;
vfs.mount_disk("assets", "data/assets");
vfs.unmount("assets");
```

Loading files
- `read_text(path)` loads a text file into `std::string`.
- `read_file(path)` loads binary data into `tinyvfs::Blob`.

```cpp
auto text = vfs.read_text("assets/shaders/basic.hlsl");
if (text) {
    compile_shader(*text);
}

auto blob = vfs.read_file("assets/textures/albedo.dds");
if (blob) {
    upload_texture(blob->data(), blob->size());
}
```

Existence checks
- `exists_file(path)` and `exists_dir(path)` for quick checks.

```cpp
if (!vfs.exists_file("assets/config/game.json")) {
    // fallback to defaults
}
```

Enumerating
- `list_files(path, extensions, callback)` lists files (non-recursive).
- `list_dirs(path, callback)` lists directories and mounted subfolders.

```cpp
vfs.list_files("assets", {"png", "dds"}, [&](std::string_view name) {
    register_texture(name);
});

vfs.list_dirs("assets", [&](std::string_view name) {
    scan_folder(name);
});
```

Features
- Mount multiple backends under a single virtual path tree.
- Overlay behavior: the most recent mount wins for reads/writes.
- Small, modern C++17 API with a simple blob type and callbacks.
- Disk backend included; custom backends can be implemented via `tinyvfs::Backend`.

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
