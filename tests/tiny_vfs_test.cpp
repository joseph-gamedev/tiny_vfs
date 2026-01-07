#include "tiny_vfs.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace fs = tinyvfs::fs;

namespace
{
    struct Tester
    {
        int failures = 0;

        void check(bool ok, std::string_view message)
        {
            if (ok)
                return;
            ++failures;
            std::cerr << "FAIL: " << message << "\n";
        }
    };

    bool write_text_file(const fs::path& path, std::string_view text)
    {
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file)
            return false;
        file.write(text.data(), static_cast<std::streamsize>(text.size()));
        return file.good();
    }

    bool write_bytes_file(const fs::path& path, const std::vector<std::byte>& bytes)
    {
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file)
            return false;
        if (!bytes.empty())
            file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        return file.good();
    }

    fs::path make_temp_dir()
    {
        fs::path base = fs::temp_directory_path();

        for (int attempt = 0; attempt < 32; ++attempt)
        {
            auto stamp = static_cast<unsigned long long>(
                std::chrono::high_resolution_clock::now().time_since_epoch().count());
            fs::path dir = base / ("tiny_vfs_test_" + std::to_string(stamp + attempt));

            std::error_code ec;
            if (fs::create_directories(dir, ec))
                return dir;
        }

        return {};
    }

    bool contains(const std::vector<std::string>& items, const std::string& name)
    {
        return std::find(items.begin(), items.end(), name) != items.end();
    }
}

int main()
{
    Tester t;
    fs::path root = make_temp_dir();
    t.check(!root.empty(), "created temp root");
    if (root.empty())
        return 1;

    fs::path content = root / "content";
    fs::path overlay = root / "overlay";
    fs::path shaders = root / "shaders";
    fs::create_directories(content / "textures");
    fs::create_directories(shaders);

    t.check(write_text_file(content / "hello.txt", "hello from disk"), "write hello.txt");
    t.check(write_bytes_file(content / "data.bin", {std::byte{0x01}, std::byte{0x02}}), "write data.bin");
    t.check(write_text_file(content / "textures" / "albedo.txt", "albedo"), "write textures/albedo.txt");
    t.check(write_text_file(shaders / "basic.hlsl", "float4 main() : SV_Target { return 1; }"),
        "write shaders/basic.hlsl");

    tinyvfs::Vfs vfs;
    t.check(vfs.mount_disk("content", content), "mount content");
    t.check(vfs.mount_disk("shaders", shaders), "mount shaders");

    t.check(vfs.exists_file("content/hello.txt"), "exists_file content/hello.txt");
    t.check(!vfs.exists_file("content/missing.txt"), "missing file is absent");

    auto text = vfs.read_text("content/hello.txt");
    t.check(text.has_value(), "read_text returns value");
    t.check(text && *text == "hello from disk", "read_text content matches");

    std::vector<std::string> files;
    tinyvfs::Result list_result = vfs.list_files(
        "content",
        {"txt"},
        [&](std::string_view name)
        {
            files.emplace_back(name);
        });
    t.check(list_result == tinyvfs::Result::ok, "list_files returns ok");
    t.check(contains(files, "hello.txt"), "list_files returns hello.txt");
    t.check(!contains(files, "data.bin"), "list_files filters by extension");
    t.check(!contains(files, "albedo.txt"), "list_files is not recursive");

    std::vector<std::string> dirs;
    tinyvfs::Result dir_result = vfs.list_dirs(
        "",
        [&](std::string_view name)
        {
            dirs.emplace_back(name);
        });
    t.check(dir_result == tinyvfs::Result::ok, "list_dirs returns ok");
    t.check(contains(dirs, "content"), "list_dirs includes content mount");
    t.check(contains(dirs, "shaders"), "list_dirs includes shaders mount");

    const char out_data[] = "out";
    tinyvfs::Result write_result = vfs.write_file("content/out.txt", out_data, sizeof(out_data) - 1);
    t.check(write_result == tinyvfs::Result::ok, "write_file returns ok");
    t.check(fs::exists(content / "out.txt"), "write_file hits disk");

    t.check(write_text_file(overlay / "hello.txt", "hello from overlay"), "write overlay hello.txt");
    t.check(write_text_file(overlay / "overlay.txt", "overlay file"), "write overlay file");
    t.check(vfs.mount_disk("content", overlay), "mount overlay content");

    auto overlay_text = vfs.read_text("content/hello.txt");
    t.check(overlay_text.has_value(), "read_text overlay returns value");
    t.check(overlay_text && *overlay_text == "hello from overlay", "overlay wins for hello.txt");

    std::vector<std::string> overlay_files;
    vfs.list_files(
        "content",
        {"txt"},
        [&](std::string_view name)
        {
            overlay_files.emplace_back(name);
        });
    t.check(contains(overlay_files, "overlay.txt"), "list_files sees overlay file");

    std::error_code ec;
    fs::remove_all(root, ec);

    if (t.failures == 0)
        std::cout << "tiny_vfs_test: OK\n";
    return t.failures == 0 ? 0 : 1;
}
