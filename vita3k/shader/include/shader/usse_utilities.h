// Vita3K emulator project
// Copyright (C) 2024 Vita3K team
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

#pragma once

#include <SPIRV/SpvBuilder.h>
#include <shader/usse_translator_types.h>
#include <shader/usse_types.h>

struct FeatureState;

namespace shader::usse::utils {

struct SpirvUtilFunctions {
    sspv::Id std_builtins{};
    std::map<DataType, sspv::Function *> unpack_funcs;
    std::map<DataType, sspv::Function *> pack_funcs;
    sspv::Function *fetch_memory{ nullptr };
    sspv::Function *unpack_fx10{ nullptr };

    // buffer_address_vec[i][1] contains the buffer pointer with an array of vec_i and stride 16 bytes
    // 0 in the last index is for the read buffer, 1 is for the write buffer
    // this is technically not a function but is the best place to put it
    // buffer_address_vec[0] is for a packed float[] array
    sspv::Id buffer_address_vec[5][2] = {};
};

sspv::Id finalize(sspv::Builder &b, sspv::Id first, sspv::Id second, const Swizzle4 swizz, sspv::Id offset, const Imm4 dest_mask);
sspv::Id load(sspv::Builder &b, const SpirvShaderParameters &params, SpirvUtilFunctions &utils, const FeatureState &features, Operand op, const Imm4 dest_mask, int shift_offset);
void store(sspv::Builder &b, const SpirvShaderParameters &params, SpirvUtilFunctions &utils, const FeatureState &features, Operand dest, sspv::Id source, std::uint8_t dest_mask, int off);

sspv::Id unpack(sspv::Builder &b, SpirvUtilFunctions &utils, const FeatureState &features, sspv::Id target, const DataType type, Swizzle4 swizz, const Imm4 dest_mask,
    const int offset);

sspv::Id unpack_one(sspv::Builder &b, SpirvUtilFunctions &utils, const FeatureState &features, sspv::Id scalar, const DataType type);
sspv::Id pack_one(sspv::Builder &b, SpirvUtilFunctions &utils, const FeatureState &features, sspv::Id vec, const DataType source_type);

sspv::Id fetch_memory(sspv::Builder &b, const SpirvShaderParameters &params, SpirvUtilFunctions &utils, sspv::Id addr);
void buffer_address_access(sspv::Builder &b, const SpirvShaderParameters &params, SpirvUtilFunctions &utils, const FeatureState &features, Operand dest, int dest_offset, sspv::Id addr, uint32_t component_size, uint32_t nb_components, int buffer_idx = -1, bool is_buffer_store = false);

sspv::Id make_vector_or_scalar_type(sspv::Builder &b, sspv::Id component, int size);

sspv::Id unwrap_type(sspv::Builder &b, sspv::Id type);

sspv::Id convert_to_float(sspv::Builder &b, const SpirvUtilFunctions &utils, sspv::Id opr, DataType type, bool normal);
sspv::Id convert_to_int(sspv::Builder &b, const SpirvUtilFunctions &utils, sspv::Id opr, DataType type, bool normal);

sspv::Id add_uvec2_uint(sspv::Builder &b, sspv::Id vec, sspv::Id to_add);

size_t dest_mask_to_comp_count(shader::usse::Imm4 dest_mask);

sspv::Id create_access_chain(sspv::Builder &b, const sspv::StorageClass storage_class, const sspv::Id base, const std::vector<sspv::Id> &offsets);

template <typename T>
sspv::Id make_uniform_vector_from_type(sspv::Builder &b, sspv::Id type, T val) {
    const int num_comp = b.getNumTypeComponents(type);
    sspv::Id v_elem_type = (num_comp > 1) ? b.getContainedTypeId(type) : type;

    sspv::Id cnst = sspv::NoResult;

    if (b.isUintType(v_elem_type)) {
        cnst = b.makeUintConstant(val);
    } else if (b.isIntType(v_elem_type)) {
        cnst = b.makeIntConstant(val);
    } else {
        cnst = b.makeFloatConstant(val);
    }

    if (num_comp == 1) {
        return cnst;
    }

    std::vector<sspv::Id> c_vecs(num_comp, cnst);
    sspv::Id v0 = b.makeCompositeConstant(type, c_vecs);

    return v0;
}

template <typename F>
void make_for_loop(sspv::Builder &b, sspv::Id iterator, sspv::Id initial_value_ite, sspv::Id iterator_limit, F body) {
    auto blocks = b.makeNewLoop();
    b.createStore(initial_value_ite, iterator);
    b.createBranch(&blocks.head);

    b.setBuildPoint(&blocks.head);

    sspv::Id compare_result = b.createBinOp(sspv::OpSLessThan, b.makeBoolType(), b.createLoad(iterator, sspv::NoPrecision), iterator_limit);

    b.createLoopMerge(&blocks.merge, &blocks.continue_target, sspv::LoopControlMaskNone, {});
    b.createConditionalBranch(compare_result, &blocks.body, &blocks.merge);

    b.setBuildPoint(&blocks.body);
    body();

    // Increase i
    sspv::Id add_to_me = b.createBinOp(sspv::OpIAdd, b.makeIntegerType(32, true), b.createLoad(iterator, sspv::NoPrecision), b.makeIntConstant(1));
    b.createStore(add_to_me, iterator);

    b.createBranch(&blocks.continue_target);

    b.setBuildPoint(&blocks.continue_target);
    b.createBranch(&blocks.head);

    b.setBuildPoint(&blocks.merge);
    b.closeLoop();
}
} // namespace shader::usse::utils
