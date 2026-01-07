#include "tiny_vfs.h"

#include <iostream>
#include <string>
#include <vector>

namespace fs = tinyvfs::fs;

#ifndef TINYVFS_SOURCE_DIR
#define TINYVFS_SOURCE_DIR "."
#endif

int main()
{
    tinyvfs::Vfs vfs;

    fs::path assets_root = fs::path(TINYVFS_SOURCE_DIR) / "examples" / "assets";
    if (!vfs.mount_disk("assets", assets_root))
    {
        std::cerr << "Failed to mount assets from: " << assets_root << "\n";
        return 1;
    }

    auto text = vfs.read_text("assets/hello.txt");
    if (!text)
    {
        std::cerr << "Missing assets/hello.txt\n";
        return 1;
    }

    std::cout << "Loaded assets/hello.txt:\n" << *text << "\n";

    std::vector<std::string> files;
    vfs.list_files(
        "assets",
        {"txt"},
        [&](std::string_view name)
        {
            files.emplace_back(name);
        });

    if (!files.empty())
    {
        std::cout << "Asset text files:\n";
        for (const auto& name : files)
            std::cout << "  " << name << "\n";
    }

    return 0;
}
