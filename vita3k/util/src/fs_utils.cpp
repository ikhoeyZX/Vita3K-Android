// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#include <util/fs.h>
#include <util/log.h>
#include <util/string_utils.h>

#ifdef ANDROID
#include  <SDL3/SDL_system.h>
#include  <SDL3/SDL_iostream.h>
#endif

namespace fs_utils {

fs::path construct_file_name(const fs::path &base_path, const fs::path &folder_path, const fs::path &file_name, const fs::path &extension) {
    fs::path full_file_path{ base_path / folder_path / file_name };
    if (!extension.empty())
        full_file_path.replace_extension(extension);

    return full_file_path.generic_path();
}

std::string path_to_utf8(const fs::path &path) {
    if constexpr (sizeof(fs::path::value_type) == sizeof(wchar_t)) {
        return string_utils::wide_to_utf(path.wstring());
    } else {
        return path.string();
    }
}

fs::path utf8_to_path(const std::string &str) {
    if constexpr (sizeof(fs::path::value_type) == sizeof(wchar_t)) {
        return fs::path{ string_utils::utf_to_wide(str) };
    } else {
        return fs::path{ str };
    }
}

fs::path path_concat(const fs::path &path1, const fs::path &path2) {
    return fs::path{ path1.native() + path2.native() };
}

void dump_data(const fs::path &path, const void *data, const std::streamsize size) {
    fs::ofstream of{ path, fs::ofstream::binary };
    if (!of.fail()) {
        of.write(static_cast<const char *>(data), size);
        of.close();
    }
}


std::vector<uint8_t> read_asset_raw(const fs::path &path) {
#ifdef ANDROID
    static uint32_t base_path_size = strlen(SDL_GetAndroidExternalStoragePath()) + 1;
    std::string file_path = path.string().substr(base_path_size);
    SDL_IOStream *file = SDL_IOFromFile(file_path.c_str(), "r");
    if (file == nullptr) {
        LOG_ERROR("Could not open external asset file {}", path.string());
        SDL_CloseIO(file);
        base_path_size = strlen(SDL_GetAndroidInternalStoragePath()) + 1;
        file_path = path.string().substr(base_path_size);
        file = SDL_IOFromFile(file_path.c_str(), "r");
        if (file == nullptr) {
           LOG_ERROR("Could not open internal asset file {}", path.string());
           return {};
        }
    }

    Sint64 size_read = SDL_GetIOSize(file);
    std::vector<uint8_t> raw_data(size_read);

    if(size_read > 0)
        SDL_ReadIO(file, raw_data.data(), size_read)
    else
        LOG_ERROR("Could not read asset file {}", path.string());
        return {};
    }

    SDL_CloseIO(file);

    return raw_data;
#else
    fs::ifstream is(path, fs::ifstream::binary);
    if (!is) {
        return {};
    }

    is.seekg(0, fs::ifstream::end);
    uint32_t size_read = is.tellg();
    is.seekg(0);

    if (size_read == 0) {
        return {};
    }

    std::vector<uint8_t> raw_data(size_read);

    is.read(reinterpret_cast<char *>(raw_data.data()), size_read);
    return raw_data;
#endif
}

} // namespace fs_utils
