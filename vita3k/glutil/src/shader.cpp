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

#include <glutil/shader.h>

#include <glutil/gl.h>
#include <glutil/object.h>

#include <util/log.h>

#include <fstream>
#include <memory>
#include <sstream>
#include <vector>

/**
 * \brief Loads (compiles, links) GLSL shaders at specified file paths
 * \param vertex_file_path File path of the vertex shader
 * \param fragment_file_path File path of the fragment shader
 * \return SharedGLObject that holds the resulting program id. Empty if loading was unsuccessful
 */
UniqueGLObject gl::load_shaders(const fs::path &vertex_file_path, const fs::path &fragment_file_path) {
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);

    // Read the vertex/fragment shader code from files
    std::string vs_code;
    {
        const std::vector<uint8_t> vs_code_raw = fs_utils::read_asset_raw(fs::path(vertex_file_path));
        vs_code.resize(vs_code_raw.size());
        memcpy(vs_code.data(), vs_code_raw.data(), vs_code_raw.size());
    }

    std::string fs_code;
    {
        const std::vector<uint8_t> fs_code_raw = fs_utils::read_asset_raw(fs::path(fragment_file_path));
        fs_code.resize(fs_code_raw.size());
        memcpy(fs_code.data(), fs_code_raw.data(), fs_code_raw.size());
    }

    if(vs_code.empty() || fs_code.empty()){
        LOG_ERROR("Couldn't open shader: {}", vertex_file_path);
        return UniqueGLObject();
    }

    GLint result = 0;

    // Compile vertex shader
    char const *vs_source_pointer = vs_code.data();
    glShaderSource(vs, 1, &vs_source_pointer, nullptr);
    glCompileShader(vs);

    // Check vertex shader
    glGetShaderiv(vs, GL_COMPILE_STATUS, &result);
    if (!result) {
        int info_log_length;
        glGetShaderiv(vs, GL_INFO_LOG_LENGTH, &info_log_length);
        std::vector<char> vertex_shader_error_message(info_log_length + 1);
        glGetShaderInfoLog(vs, info_log_length, nullptr, vertex_shader_error_message.data());
        LOG_ERROR("Error compiling vertex shader: {}\n", vertex_shader_error_message.data());
        return {};
    }

    // Compile fragment shader
    char const *fs_source_pointer = fs_code.data();
    glShaderSource(fs, 1, &fs_source_pointer, nullptr);
    glCompileShader(fs);

    // Check fragment shader
    glGetShaderiv(fs, GL_COMPILE_STATUS, &result);
    if (!result) {
        int info_log_length;
        glGetShaderiv(fs, GL_INFO_LOG_LENGTH, &info_log_length);
        std::vector<char> fragment_shader_error_message(info_log_length + 1);
        glGetShaderInfoLog(fs, info_log_length, nullptr, fragment_shader_error_message.data());
        LOG_ERROR("Error compiling fragment shader: {}\n", fragment_shader_error_message.data());
        return {};
    }

    // Link the program
    const auto program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    // Check the program
    glGetProgramiv(program, GL_LINK_STATUS, &result);
    if (!result) {
        int info_log_length;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_log_length);
        std::vector<char> program_error_message(info_log_length + 1);
        glGetProgramInfoLog(program, info_log_length, nullptr, program_error_message.data());
        LOG_ERROR("Error linking shader program: {}\n", program_error_message.data());
        return {};
    }

    glDetachShader(program, vs);
    glDetachShader(program, fs);

    glDeleteShader(vs);
    glDeleteShader(fs);

    UniqueGLObject program_ptr = std::make_unique<GLObject>();
    if (!program_ptr->init(program, glDeleteProgram)) {
        return {};
    }

    return program_ptr;
}
