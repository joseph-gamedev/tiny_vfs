// tiny_vfs.h - small header-only virtual filesystem for games and rendering.
// Public domain. See LICENSE.

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstring>
#if defined(__has_include)
#if __has_include(<filesystem>)
#include <filesystem>
#define TINYVFS_HAS_STD_FS 1
#elif __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
#define TINYVFS_HAS_EXP_FS 1
#else
#error "tiny_vfs requires <filesystem> support"
#endif
#else
#include <filesystem>
#define TINYVFS_HAS_STD_FS 1
#endif
#include <fstream>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace tinyvfs
{
#if defined(TINYVFS_HAS_STD_FS)
    namespace fs = std::filesystem;
#elif defined(TINYVFS_HAS_EXP_FS)
    namespace fs = std::experimental::filesystem;
#endif

    enum class Result
    {
        ok = 0,
        not_found,
        io_error,
        not_supported,
        invalid_path
    };

    struct Blob
    {
        std::vector<std::byte> bytes;

        bool empty() const noexcept { return bytes.empty(); }
        size_t size() const noexcept { return bytes.size(); }
        const std::byte* data() const noexcept { return bytes.data(); }

        std::string_view as_string_view() const
        {
            return std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        }

        std::string to_string(bool append_null = false) const
        {
            std::string text;
            text.resize(bytes.size() + (append_null ? 1 : 0));
            if (!bytes.empty())
                std::memcpy(text.data(), bytes.data(), bytes.size());
            if (append_null)
                text.back() = '\0';
            return text;
        }
    };

    using EnumerateFn = std::function<void(std::string_view)>;

    class Backend
    {
    public:
        virtual ~Backend() = default;

        virtual bool exists_file(std::string_view path) = 0;
        virtual bool exists_dir(std::string_view path) = 0;
        virtual std::optional<Blob> read_file(std::string_view path) = 0;
        virtual Result write_file(std::string_view path, const void* data, size_t size) = 0;
        virtual Result list_files(std::string_view path,
            const std::vector<std::string_view>& extensions,
            const EnumerateFn& callback,
            bool allow_duplicates = false) = 0;
        virtual Result list_dirs(std::string_view path,
            const EnumerateFn& callback,
            bool allow_duplicates = false) = 0;
    };

    namespace detail
    {
        inline bool normalize_virtual_path(std::string_view input, std::string& out)
        {
            fs::path p(input);
            if (p.has_root_name())
                return false;

            p = p.lexically_normal();

            for (const auto& part : p)
            {
                if (part == "..")
                    return false;
            }

            std::string s = p.generic_string();
            if (s == "." || s == "/")
            {
                out.clear();
                return true;
            }

            while (!s.empty() && s.front() == '/')
                s.erase(s.begin());
            while (!s.empty() && s.back() == '/')
                s.pop_back();

            if (s == ".")
                s.clear();

            out = s;
            return true;
        }

        inline bool relative_to_mount(std::string_view full,
            std::string_view mount,
            std::string& out)
        {
            if (mount.empty())
            {
                out.assign(full);
                return true;
            }

            if (full == mount)
            {
                out.clear();
                return true;
            }

            if (full.size() > mount.size() &&
                full.compare(0, mount.size(), mount) == 0 &&
                full[mount.size()] == '/')
            {
                out.assign(full.substr(mount.size() + 1));
                return true;
            }

            return false;
        }

        inline bool child_mount_name(std::string_view parent,
            std::string_view mount,
            std::string& out)
        {
            if (mount.empty())
                return false;

            if (parent.empty())
            {
                size_t split = mount.find('/');
                out.assign(mount.substr(0, split));
                return true;
            }

            if (mount.size() <= parent.size())
                return false;

            if (mount.compare(0, parent.size(), parent) != 0 || mount[parent.size()] != '/')
                return false;

            std::string_view rest = mount.substr(parent.size() + 1);
            size_t split = rest.find('/');
            out.assign(rest.substr(0, split));
            return true;
        }

        inline fs::path to_os_path(std::string_view path)
        {
            fs::path p(path);
            return p.make_preferred();
        }

        inline bool extension_matches(std::string_view ext,
            const std::vector<std::string_view>& extensions)
        {
            if (extensions.empty())
                return true;

            for (std::string_view entry : extensions)
            {
                if (entry.empty())
                    continue;

                if (entry.front() == '.')
                {
                    if (ext == entry)
                        return true;
                }
                else
                {
                    if (ext.size() == entry.size() + 1 &&
                        ext.front() == '.' &&
                        ext.substr(1) == entry)
                    {
                        return true;
                    }
                }
            }

            return false;
        }
    }

    class DiskBackend final : public Backend
    {
    public:
        bool exists_file(std::string_view path) override
        {
            std::error_code ec;
            return fs::is_regular_file(detail::to_os_path(path), ec);
        }

        bool exists_dir(std::string_view path) override
        {
            std::error_code ec;
            return fs::is_directory(detail::to_os_path(path), ec);
        }

        std::optional<Blob> read_file(std::string_view path) override
        {
            std::ifstream file(detail::to_os_path(path), std::ios::binary | std::ios::ate);
            if (!file)
                return std::nullopt;

            std::streamsize size = file.tellg();
            if (size < 0)
                return std::nullopt;

            Blob blob;
            if (size > 0)
            {
                blob.bytes.resize(static_cast<size_t>(size));
                file.seekg(0, std::ios::beg);
                if (!file.read(reinterpret_cast<char*>(blob.bytes.data()), size))
                    return std::nullopt;
            }

            return blob;
        }

        Result write_file(std::string_view path, const void* data, size_t size) override
        {
            fs::path os_path = detail::to_os_path(path);
            fs::path parent = os_path.parent_path();

            if (!parent.empty())
            {
                std::error_code ec;
                if (!fs::exists(parent, ec))
                    return Result::not_found;
            }

            std::ofstream file(os_path, std::ios::binary | std::ios::trunc);
            if (!file)
                return Result::io_error;

            if (size > 0)
            {
                file.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
                if (!file.good())
                    return Result::io_error;
            }

            return Result::ok;
        }

        Result list_files(std::string_view path,
            const std::vector<std::string_view>& extensions,
            const EnumerateFn& callback,
            bool allow_duplicates) override
        {
            fs::path dir = detail::to_os_path(path);
            std::error_code ec;

            if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec))
                return Result::not_found;

            std::unordered_set<std::string> seen;

            for (fs::directory_iterator it(dir, ec), end; it != end; it.increment(ec))
            {
                if (ec)
                    return Result::io_error;

                const fs::directory_entry& entry = *it;
                if (!entry.is_regular_file(ec))
                {
                    if (ec)
                        return Result::io_error;
                    continue;
                }

                std::string name = entry.path().filename().generic_string();
                std::string ext = entry.path().extension().generic_string();
                if (!detail::extension_matches(ext, extensions))
                    continue;

                if (!allow_duplicates && !seen.insert(name).second)
                    continue;

                callback(name);
            }

            return Result::ok;
        }

        Result list_dirs(std::string_view path,
            const EnumerateFn& callback,
            bool allow_duplicates) override
        {
            fs::path dir = detail::to_os_path(path);
            std::error_code ec;

            if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec))
                return Result::not_found;

            std::unordered_set<std::string> seen;

            for (fs::directory_iterator it(dir, ec), end; it != end; it.increment(ec))
            {
                if (ec)
                    return Result::io_error;

                const fs::directory_entry& entry = *it;
                if (!entry.is_directory(ec))
                {
                    if (ec)
                        return Result::io_error;
                    continue;
                }

                std::string name = entry.path().filename().generic_string();
                if (!allow_duplicates && !seen.insert(name).second)
                    continue;

                callback(name);
            }

            return Result::ok;
        }
    };

    class SubtreeBackend final : public Backend
    {
    public:
        SubtreeBackend(std::shared_ptr<Backend> backend, fs::path base)
            : backend_(std::move(backend))
            , base_(std::move(base).lexically_normal())
        {
        }

        bool exists_file(std::string_view path) override
        {
            return backend_->exists_file(map(path));
        }

        bool exists_dir(std::string_view path) override
        {
            return backend_->exists_dir(map(path));
        }

        std::optional<Blob> read_file(std::string_view path) override
        {
            return backend_->read_file(map(path));
        }

        Result write_file(std::string_view path, const void* data, size_t size) override
        {
            return backend_->write_file(map(path), data, size);
        }

        Result list_files(std::string_view path,
            const std::vector<std::string_view>& extensions,
            const EnumerateFn& callback,
            bool allow_duplicates) override
        {
            return backend_->list_files(map(path), extensions, callback, allow_duplicates);
        }

        Result list_dirs(std::string_view path,
            const EnumerateFn& callback,
            bool allow_duplicates) override
        {
            return backend_->list_dirs(map(path), callback, allow_duplicates);
        }

    private:
        std::shared_ptr<Backend> backend_;
        fs::path base_;

        std::string map(std::string_view path) const
        {
            if (base_.empty())
                return fs::path(path).lexically_normal().generic_string();
            if (path.empty())
                return base_.generic_string();

            fs::path combined = base_ / fs::path(path);
            return combined.lexically_normal().generic_string();
        }
    };

    class Vfs final
    {
    public:
        bool mount(std::string_view path, std::shared_ptr<Backend> backend)
        {
            if (!backend)
                return false;

            std::string normalized;
            if (!detail::normalize_virtual_path(path, normalized))
                return false;

            mounts_.push_back(MountPoint{normalized, std::move(backend)});
            return true;
        }

        bool mount_disk(std::string_view path, const fs::path& root)
        {
            auto backend = std::make_shared<SubtreeBackend>(
                std::make_shared<DiskBackend>(),
                root);
            return mount(path, backend);
        }

        bool unmount(std::string_view path)
        {
            std::string normalized;
            if (!detail::normalize_virtual_path(path, normalized))
                return false;

            bool removed = false;
            for (size_t i = 0; i < mounts_.size();)
            {
                if (mounts_[i].mount == normalized)
                {
                    mounts_.erase(mounts_.begin() + static_cast<std::ptrdiff_t>(i));
                    removed = true;
                }
                else
                {
                    ++i;
                }
            }

            return removed;
        }

        bool exists_file(std::string_view path) const
        {
            std::string normalized;
            if (!detail::normalize_virtual_path(path, normalized))
                return false;

            for (auto it = mounts_.rbegin(); it != mounts_.rend(); ++it)
            {
                std::string relative;
                if (!detail::relative_to_mount(normalized, it->mount, relative))
                    continue;
                if (it->backend->exists_file(relative))
                    return true;
            }

            return false;
        }

        bool exists_dir(std::string_view path) const
        {
            std::string normalized;
            if (!detail::normalize_virtual_path(path, normalized))
                return false;

            if (normalized.empty())
                return !mounts_.empty();

            for (const auto& mount : mounts_)
            {
                if (mount.mount == normalized)
                    return true;

                if (mount.mount.size() > normalized.size() &&
                    mount.mount.compare(0, normalized.size(), normalized) == 0 &&
                    mount.mount[normalized.size()] == '/')
                {
                    return true;
                }
            }

            for (auto it = mounts_.rbegin(); it != mounts_.rend(); ++it)
            {
                std::string relative;
                if (!detail::relative_to_mount(normalized, it->mount, relative))
                    continue;
                if (it->backend->exists_dir(relative))
                    return true;
            }

            return false;
        }

        std::optional<Blob> read_file(std::string_view path) const
        {
            std::string normalized;
            if (!detail::normalize_virtual_path(path, normalized))
                return std::nullopt;

            for (auto it = mounts_.rbegin(); it != mounts_.rend(); ++it)
            {
                std::string relative;
                if (!detail::relative_to_mount(normalized, it->mount, relative))
                    continue;
                if (auto data = it->backend->read_file(relative))
                    return data;
            }

            return std::nullopt;
        }

        std::optional<std::string> read_text(std::string_view path, bool append_null = false) const
        {
            auto blob = read_file(path);
            if (!blob)
                return std::nullopt;
            return blob->to_string(append_null);
        }

        Result write_file(std::string_view path, const void* data, size_t size) const
        {
            std::string normalized;
            if (!detail::normalize_virtual_path(path, normalized))
                return Result::invalid_path;

            bool matched = false;
            Result last_result = Result::not_supported;

            for (auto it = mounts_.rbegin(); it != mounts_.rend(); ++it)
            {
                std::string relative;
                if (!detail::relative_to_mount(normalized, it->mount, relative))
                    continue;

                matched = true;
                Result result = it->backend->write_file(relative, data, size);
                if (result == Result::ok || result == Result::io_error)
                    return result;
                if (result != Result::not_supported)
                    last_result = result;
            }

            if (!matched)
                return Result::not_found;

            return last_result;
        }

        Result list_files(std::string_view path,
            const std::vector<std::string_view>& extensions,
            const EnumerateFn& callback,
            bool allow_duplicates = false) const
        {
            std::string normalized;
            if (!detail::normalize_virtual_path(path, normalized))
                return Result::invalid_path;

            bool matched = false;
            std::unordered_set<std::string> seen;

            auto emit = [&](std::string_view name)
            {
                if (allow_duplicates)
                {
                    callback(name);
                    return;
                }

                if (seen.insert(std::string(name)).second)
                    callback(name);
            };

            for (auto it = mounts_.rbegin(); it != mounts_.rend(); ++it)
            {
                std::string relative;
                if (!detail::relative_to_mount(normalized, it->mount, relative))
                    continue;

                matched = true;
                Result result = it->backend->list_files(
                    relative,
                    extensions,
                    emit,
                    true);
                if (result == Result::io_error)
                    return result;
            }

            return matched ? Result::ok : Result::not_found;
        }

        Result list_files(std::string_view path,
            std::initializer_list<std::string_view> extensions,
            const EnumerateFn& callback,
            bool allow_duplicates = false) const
        {
            std::vector<std::string_view> ext_list(extensions);
            return list_files(path, ext_list, callback, allow_duplicates);
        }

        Result list_dirs(std::string_view path,
            const EnumerateFn& callback,
            bool allow_duplicates = false) const
        {
            std::string normalized;
            if (!detail::normalize_virtual_path(path, normalized))
                return Result::invalid_path;

            std::unordered_set<std::string> seen;

            auto emit = [&](std::string_view name)
            {
                if (allow_duplicates)
                {
                    callback(name);
                    return;
                }

                if (seen.insert(std::string(name)).second)
                    callback(name);
            };

            for (const auto& mount : mounts_)
            {
                std::string child;
                if (detail::child_mount_name(normalized, mount.mount, child))
                    emit(child);
            }

            bool matched = false;
            for (auto it = mounts_.rbegin(); it != mounts_.rend(); ++it)
            {
                std::string relative;
                if (!detail::relative_to_mount(normalized, it->mount, relative))
                    continue;

                matched = true;
                Result result = it->backend->list_dirs(relative, emit, true);
                if (result == Result::io_error)
                    return result;
            }

            return (matched || !seen.empty()) ? Result::ok : Result::not_found;
        }

    private:
        struct MountPoint
        {
            std::string mount;
            std::shared_ptr<Backend> backend;
        };

        std::vector<MountPoint> mounts_;
    };
}
