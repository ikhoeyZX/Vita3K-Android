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

#pragma once

#include <SPIRV/SpvBuilder.h>
#include <shader/usse_translator_types.h>
#include <shader/usse_types.h>

struct FeatureState;

using namespace SPIRV_CROSS_SPV_HEADER_NAMESPACE;
namespace shader::usse::utils {

struct SpirvUtilFunctions {
    Id std_builtins{};
    std::map<DataType, Function *> unpack_funcs;
    std::map<DataType, Function *> pack_funcs;
    Function *fetch_memory{ nullptr };
    Function *unpack_fx10{ nullptr };

    // buffer_address_vec[i][1] contains the buffer pointer with an array of vec_i and stride 16 bytes
    // 0 in the last index is for the read buffer, 1 is for the write buffer
    // this is technically not a function but is the best place to put it
    // buffer_address_vec[0] is for a packed float[] array
    Id buffer_address_vec[5][2] = {};
};

Id finalize(Builder &b, Id first, Id second, const Swizzle4 swizz, Id offset, const Imm4 dest_mask);
Id load(Builder &b, const SpirvShaderParameters &params, SpirvUtilFunctions &utils, const FeatureState &features, Operand op, const Imm4 dest_mask, int shift_offset);
void store(Builder &b, const SpirvShaderParameters &params, SpirvUtilFunctions &utils, const FeatureState &features, Operand dest, Id source, std::uint8_t dest_mask, int off);

Id unpack(Builder &b, SpirvUtilFunctions &utils, const FeatureState &features, Id target, const DataType type, Swizzle4 swizz, const Imm4 dest_mask,
    const int offset);

Id unpack_one(Builder &b, SpirvUtilFunctions &utils, const FeatureState &features, Id scalar, const DataType type);
Id pack_one(Builder &b, SpirvUtilFunctions &utils, const FeatureState &features, Id vec, const DataType source_type);

Id fetch_memory(Builder &b, const SpirvShaderParameters &params, SpirvUtilFunctions &utils, Id addr);
void buffer_address_access(Builder &b, const SpirvShaderParameters &params, SpirvUtilFunctions &utils, const FeatureState &features, Operand dest, int dest_offset, Id addr, uint32_t component_size, uint32_t nb_components, int buffer_idx = -1, bool is_buffer_store = false);

Id make_vector_or_scalar_type(Builder &b, Id component, int size);

Id unwrap_type(Builder &b, Id type);

Id convert_to_float(Builder &b, const SpirvUtilFunctions &utils, Id opr, DataType type, bool normal);
Id convert_to_int(Builder &b, const SpirvUtilFunctions &utils, Id opr, DataType type, bool normal);

Id add_uvec2_uint(Builder &b, Id vec, Id to_add);

size_t dest_mask_to_comp_count(shader::usse::Imm4 dest_mask);

Id create_access_chain(Builder &b, const StorageClass storage_class, const Id base, const std::vector<Id> &offsets);

template <typename T>
Id make_uniform_vector_from_type(Builder &b, Id type, T val) {
    const int num_comp = b.getNumTypeComponents(type);
    Id v_elem_type = (num_comp > 1) ? b.getContainedTypeId(type) : type;

    Id cnst = NoResult;

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

    std::vector<Id> c_vecs(num_comp, cnst);
    Id v0 = b.makeCompositeConstant(type, c_vecs);

    return v0;
}

template <typename F>
void make_for_loop(Builder &b, Id iterator, Id initial_value_ite, Id iterator_limit, F body) {
    auto blocks = b.makeNewLoop();
    b.createStore(initial_value_ite, iterator);
    b.createBranch(true, &blocks.head);

    b.setBuildPoint(&blocks.head);

    Id compare_result = b.createBinOp(OpSLessThan, b.makeBoolType(), b.createLoad(iterator, NoPrecision), iterator_limit);

    b.createLoopMerge(&blocks.merge, &blocks.continue_target, LoopControlMaskNone, {});
    b.createConditionalBranch(compare_result, &blocks.body, &blocks.merge);

    b.setBuildPoint(&blocks.body);
    body();

    // Increase i
    Id add_to_me = b.createBinOp(OpIAdd, b.makeIntegerType(32, true), b.createLoad(iterator, NoPrecision), b.makeIntConstant(1));
    b.createStore(add_to_me, iterator);

    b.createBranch(true, &blocks.continue_target);

    b.setBuildPoint(&blocks.continue_target);
    b.createBranch(true, &blocks.head);

    b.setBuildPoint(&blocks.merge);
    b.closeLoop();
}
} // namespace shader::usse::utils
