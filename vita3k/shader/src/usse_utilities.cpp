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

#include <shader/spirv_recompiler.h>
#include <shader/usse_constant_table.h>
#include <shader/usse_program_analyzer.h>
#include <shader/usse_utilities.h>

#include <util/bit_cast.h>
#include <util/float_to_half.h>
#include <util/log.h>

#include <SPIRV/GLSL.std.450.h>
#include <features/state.h>

#include <bitset>

namespace shader::usse::utils {

static sspv::Id get_correspond_constant_with_channel(sspv::Builder &b, shader::usse::SwizzleChannel swizz) {
    switch (swizz) {
    case shader::usse::SwizzleChannel::C_0: {
        return b.makeFloatConstant(0.0f);
    }

    case shader::usse::SwizzleChannel::C_1: {
        return b.makeFloatConstant(1.0f);
    }

    case shader::usse::SwizzleChannel::C_2: {
        return b.makeFloatConstant(2.0f);
    }

    case shader::usse::SwizzleChannel::C_H: {
        return b.makeFloatConstant(0.5f);
    }

    default:
        break;
    }

    return sspv::NoResult;
}

sspv::Id finalize(sspv::Builder &b, sspv::Id first, sspv::Id second, const Swizzle4 swizz, sspv::Id offset, const Imm4 dest_mask) {
    if (first == sspv::NoResult || second == sspv::NoResult) {
        return sspv::NoResult;
    }

    std::vector<sspv::Id> ops;

    sspv::Id target_type = utils::unwrap_type(b, b.getTypeId(first));

    const auto first_comp_count = b.getNumComponents(first);

    const bool offset_is_const = b.isConstant(offset);
    const int offset_value = offset_is_const ? b.getConstantScalar(offset) : 0;
    if (offset_is_const && (offset_value % 4 == 0) && is_default(swizz, 4) && dest_mask == 0b1111 && b.getNumComponents(first) == 4) {
        return first;
    }

    const sspv::Id i32 = b.makeIntType(32);
    // if offset is not constant, threshold to be considered in the first base
    sspv::Id first_base_threshold = 0;
    if (!offset_is_const) {
        // threshold is 4 - offset % 4
        sspv::Id off_mod_4 = b.createBinOp(sspv::OpBitwiseAnd, i32, offset, b.makeIntConstant(3));
        first_base_threshold = b.createBinOp(sspv::OpISub, i32, b.makeIntConstant(4), off_mod_4);
    }

    // Try to plant a composite construct
    for (auto i = 0; i < 4; i++) {
        if (dest_mask & (1 << i)) {
            if ((int)swizz[i] >= (int)SwizzleChannel::C_0) {
                ops.push_back(get_correspond_constant_with_channel(b, swizz[i]));
            } else if (offset_is_const) {
                int access_offset = offset_value % 4 + (int)swizz[i] - (int)SwizzleChannel::C_X;
                sspv::Id access_base = first;

                if (access_offset >= first_comp_count) {
                    access_offset -= first_comp_count;
                    access_base = second;
                }

                if (!b.isScalar(access_base)) {
                    ops.push_back(b.createOp(sspv::OpVectorExtractDynamic, target_type, { access_base, b.makeIntConstant(access_offset) }));
                } else {
                    ops.push_back(access_base);
                }
            } else {
                if (first_comp_count != 4) {
                    LOG_ERROR("Unhandled non-const offset without a vec4 entry");
                    return sspv::NoResult;
                }
                // do exactly as above, but in spirv
                sspv::Id delta = b.makeIntConstant((int)swizz[i] - (int)SwizzleChannel::C_X);
                sspv::Id access_offset = b.createBinOp(sspv::OpIAdd, i32, offset, delta);
                access_offset = b.createBinOp(sspv::OpBitwiseAnd, i32, access_offset, b.makeIntConstant(3));

                sspv::Id first_base = b.createOp(sspv::OpVectorExtractDynamic, target_type, { first, access_offset });
                sspv::Id second_base = b.createOp(sspv::OpVectorExtractDynamic, target_type, { second, access_offset });
                // select one if (int)swizz[i] - (int)SwizzleChannel::C_X < 4 - offset % 4
                sspv::Id cond = b.createBinOp(sspv::OpSLessThan, b.makeBoolType(), delta, first_base_threshold);
                ops.push_back(b.createOp(sspv::OpSelect, target_type, { cond, first_base, second_base }));
            }
        }
    }

    if (ops.size() == 1) {
        return ops[0];
    }

    return b.createCompositeConstruct(b.makeVectorType(target_type, static_cast<int>(ops.size())), ops);
}

size_t dest_mask_to_comp_count(shader::usse::Imm4 dest_mask) {
    std::bitset<4> bs(dest_mask);
    const auto bit_count = bs.count();
    assert(bit_count <= 4 && bit_count > 0);
    return bit_count;
}

sspv::Id create_access_chain(sspv::Builder &b, const sspv::StorageClass storage_class, const sspv::Id base, const std::vector<sspv::Id> &offsets) {
    sspv::Builder::AccessChain access_chain{};
    access_chain.base = base;
    access_chain.indexChain = offsets;
    b.setAccessChain(access_chain);
    return b.createAccessChain(storage_class, base, offsets);
}

static const SpirvVarRegBank *get_reg_bank(const shader::usse::SpirvShaderParameters &params, shader::usse::RegisterBank reg_bank) {
    switch (reg_bank) {
    case RegisterBank::PRIMATTR:
        return &params.ins;
    case RegisterBank::SECATTR:
        return &params.uniforms;
    case RegisterBank::OUTPUT:
        return &params.outs;
    case RegisterBank::TEMP:
        return &params.temps;
    case RegisterBank::FPINTERNAL:
        return &params.internals;
    case RegisterBank::PREDICATE:
        return &params.predicates;
    case RegisterBank::INDEX:
        return &params.indexes;
    default:
        // LOG_WARN("Reg bank {} unsupported", static_cast<uint8_t>(reg_bank));
        return nullptr;
    }
}

static sspv::Function *make_fx10_unpack_func(sspv::Builder &b, const SpirvUtilFunctions &utils, const FeatureState &features) {
    std::vector<std::vector<sspv::Decoration>> decorations;

    sspv::Block *fx10_unpack_func_block;
    sspv::Block *last_build_point = b.getBuildPoint();

    sspv::Id type_i32 = b.makeIntType(32);
    sspv::Id ivec3 = b.makeVectorType(type_i32, 3);
    sspv::Id uvec3 = b.makeVectorType(b.makeUintType(32), 3);
    sspv::Id type_f32 = b.makeFloatType(32);
    sspv::Id type_f32_v3 = b.makeVectorType(type_f32, 3);

    sspv::Function *fx10_unpack_func = b.makeFunctionEntry(
        sspv::NoPrecision, type_f32_v3, "unpack3xFX10", { type_f32 }, { "to_unpack" },
        decorations, &fx10_unpack_func_block);
    fx10_unpack_func->setReturnPrecision(sspv::DecorationRelaxedPrecision);

    sspv::Id extracted = fx10_unpack_func->getParamId(0);

    // Cast to int first
    extracted = b.createUnaryOp(sspv::OpBitcast, type_i32, extracted);
    sspv::Id vec = b.createCompositeConstruct(ivec3, { extracted, extracted, extracted });

    // vec = vec >> uvec3(0,10,20);
    // note: note entirely sure, I really hope the layout is the same as in a 32-bit little-endian integer
    const sspv::Id shift_amount = b.makeCompositeConstant(uvec3, { b.makeUintConstant(0), b.makeUintConstant(10), b.makeUintConstant(20) });
    vec = b.createBinOp(sspv::OpShiftRightLogical, ivec3, vec, shift_amount);

    // sign-extend the 10-bit integer:
    // vec <<= 22 (logical)
    // vec >>= 22 (arithmetic)
    sspv::Id extend_amount = b.makeUintConstant(22);
    extend_amount = b.makeCompositeConstant(uvec3, { extend_amount, extend_amount, extend_amount });
    vec = b.createBinOp(sspv::OpShiftLeftLogical, ivec3, vec, extend_amount);
    vec = b.createBinOp(sspv::OpShiftRightArithmetic, ivec3, vec, extend_amount);

    // normalize it
    vec = convert_to_float(b, utils, vec, DataType::C10, true);

    b.makeReturn(false, vec);
    b.setBuildPoint(last_build_point);

    return fx10_unpack_func;
}

static sspv::Function *make_unpack_func(sspv::Builder &b, const FeatureState &features, DataType source_type) {
    std::vector<std::vector<sspv::Decoration>> decorations;

    sspv::Block *unpack_func_block;
    sspv::Block *last_build_point = b.getBuildPoint();

    sspv::Id type_f32 = b.makeFloatType(32);
    sspv::Id type_i32 = b.makeIntType(32);
    sspv::Id type_ui32 = b.makeUintType(32);

    std::string func_name;
    sspv::Id output_type;
    int comp_count;
    bool is_signed;

    switch (source_type) {
    case DataType::UINT16: {
        func_name = "unpack2xU16";
        output_type = b.makeVectorType(type_ui32, 2);
        comp_count = 2;
        is_signed = false;
        break;
    }
    case DataType::INT16: {
        func_name = "unpack2xS16";
        output_type = b.makeVectorType(type_i32, 2);
        comp_count = 2;
        is_signed = true;
        break;
    }
    case DataType::UINT8: {
        func_name = "unpack4xU8";
        output_type = b.makeVectorType(type_ui32, 4);
        comp_count = 4;
        is_signed = false;
        break;
    }
    case DataType::INT8: {
        func_name = "unpack4xS8";
        output_type = b.makeVectorType(type_i32, 4);
        comp_count = 4;
        is_signed = true;
        break;
    }
    default:
        assert(false);
        return nullptr;
    }

    sspv::Function *unpack_func = b.makeFunctionEntry(
        sspv::NoPrecision, output_type, func_name.c_str(), { type_f32 }, { "to_unpack" },
        decorations, &unpack_func_block);
    unpack_func->setReturnPrecision(sspv::DecorationRelaxedPrecision);
    sspv::Id extracted = unpack_func->getParamId(0);

    const sspv::Id result_type = is_signed ? type_i32 : type_ui32;
    extracted = b.createUnaryOp(sspv::OpBitcast, result_type, extracted);

    const auto comp_bits = 32 / comp_count;
    sspv::Id comp_bits_val = b.makeUintConstant(comp_bits);

    std::vector<sspv::Id> comps;
    for (int i = 0; i < comp_count; ++i) {
        const sspv::Op op = is_signed ? sspv::OpBitFieldSExtract : sspv::OpBitFieldUExtract;
        sspv::Id comp = b.createTriOp(op, result_type, extracted, b.makeUintConstant(comp_bits * i), comp_bits_val);

        comps.push_back(comp);
    }

    auto output = b.createCompositeConstruct(output_type, comps);

    b.makeReturn(false, output);
    b.setBuildPoint(last_build_point);

    return unpack_func;
}

static sspv::Function *make_pack_func(sspv::Builder &b, const FeatureState &features, DataType source_type) {
    std::vector<std::vector<sspv::Decoration>> decorations;

    sspv::Block *pack_func_block;
    sspv::Block *last_build_point = b.getBuildPoint();

    sspv::Id type_ui32 = b.makeUintType(32);
    sspv::Id type_i32 = b.makeIntType(32);
    sspv::Id type_f32 = b.makeFloatType(32);

    std::string func_name;
    sspv::Id input_type;
    int comp_count;
    bool is_signed;

    switch (source_type) {
    case DataType::UINT16: {
        func_name = "pack2xU16";
        input_type = b.makeVectorType(type_ui32, 2);
        comp_count = 2;
        is_signed = false;
        break;
    }
    case DataType::INT16: {
        func_name = "pack2xS16";
        input_type = b.makeVectorType(type_i32, 2);
        comp_count = 2;
        is_signed = true;
        break;
    }
    case DataType::UINT8: {
        func_name = "pack4xU8";
        input_type = b.makeVectorType(type_ui32, 4);
        comp_count = 4;
        is_signed = false;
        break;
    }
    case DataType::INT8: {
        func_name = "pack4xS8";
        input_type = b.makeVectorType(type_i32, 4);
        comp_count = 4;
        is_signed = true;
        break;
    }
    default:
        assert(false);
        return nullptr;
    }

    sspv::Function *pack_func = b.makeFunctionEntry(
        sspv::NoPrecision, type_f32, func_name.c_str(), { input_type }, { "to_pack" },
        decorations, &pack_func_block);

    pack_func->addParamPrecision(0, sspv::DecorationRelaxedPrecision);
    sspv::Id extracted = pack_func->getParamId(0);
    const int comp_bits = 32 / comp_count;

    const sspv::Id comp_type = b.getContainedTypeId(input_type);

    auto output = is_signed ? b.makeIntConstant(0) : b.makeUintConstant(0);
    for (int i = 0; i < comp_count; ++i) {
        sspv::Id comp = b.createBinOp(sspv::OpVectorExtractDynamic, comp_type, extracted, b.makeIntConstant(i));
        output = b.createOp(sspv::OpBitFieldInsert, comp_type, { output, comp, b.makeIntConstant(comp_bits * i), b.makeIntConstant(comp_bits) });
    }

    output = b.createUnaryOp(sspv::OpBitcast, type_f32, output);

    b.makeReturn(false, output);
    b.setBuildPoint(last_build_point);

    return pack_func;
}

static sspv::Function *make_f16_unpack_func(sspv::Builder &b, const SpirvUtilFunctions &utils, const FeatureState &features) {
    std::vector<std::vector<sspv::Decoration>> decorations;

    sspv::Block *f16_unpack_func_block;
    sspv::Block *last_build_point = b.getBuildPoint();

    sspv::Id type_ui32 = b.makeUintType(32);
    sspv::Id type_f32 = b.makeFloatType(32);
    sspv::Id type_f32_v2 = b.makeVectorType(type_f32, 2);

    sspv::Function *f16_unpack_func = b.makeFunctionEntry(
        sspv::NoPrecision, type_f32_v2, "unpack2xF16", { type_f32 }, { "to_unpack" },
        decorations, &f16_unpack_func_block);
    f16_unpack_func->setReturnPrecision(sspv::DecorationRelaxedPrecision);

    sspv::Id extracted = f16_unpack_func->getParamId(0);

    extracted = b.createUnaryOp(sspv::OpBitcast, type_ui32, extracted);
    extracted = b.createBuiltinCall(type_f32_v2, utils.std_builtins, GLSLstd450UnpackHalf2x16, { extracted });

    b.makeReturn(false, extracted);
    b.setBuildPoint(last_build_point);

    return f16_unpack_func;
}

static sspv::Function *make_f16_pack_func(sspv::Builder &b, const SpirvUtilFunctions &utils, const FeatureState &features) {
    std::vector<std::vector<sspv::Decoration>> decorations;

    sspv::Block *f16_pack_func_block;
    sspv::Block *last_build_point = b.getBuildPoint();

    sspv::Id type_ui32 = b.makeUintType(32);
    sspv::Id type_f32 = b.makeFloatType(32);
    sspv::Id type_f32_v2 = b.makeVectorType(type_f32, 2);

    sspv::Function *f16_pack_func = b.makeFunctionEntry(
        sspv::NoPrecision, type_f32, "pack2xF16", { type_f32_v2 }, { "to_pack" },
        decorations, &f16_pack_func_block);

    f16_pack_func->addParamPrecision(0, sspv::DecorationRelaxedPrecision);
    sspv::Id extracted = f16_pack_func->getParamId(0);

    // use packHalf2x16
    extracted = b.createBuiltinCall(type_ui32, utils.std_builtins, GLSLstd450PackHalf2x16, { extracted });
    extracted = b.createUnaryOp(sspv::OpBitcast, type_f32, extracted);

    b.makeReturn(false, extracted);
    b.setBuildPoint(last_build_point);

    return f16_pack_func;
}

static sspv::Function *make_fetch_memory_func_for_array(sspv::Builder &b, sspv::Id buffer_container, const SpirvUniformBufferInfo &info, const int buffer_index) {
    // The address can be unaligned, so we load two words around address / 4 and combine them.
    // | = address
    // s = memory[address/4] (source)
    // f = memory[address/4+1] (friend)
    // sss|(sfff)f
    // Data inside () is what we want.

    sspv::Id type_f32 = b.makeFloatType(32);
    sspv::Id type_ui32 = b.makeUintType(32);
    sspv::Id type_i32 = b.makeIntType(32);
    sspv::Block *func_block;
    sspv::Block *last_build_point = b.getBuildPoint();

    const std::string func_name = fmt::format("fetchMemoryForBuffer{}Base{}", buffer_index, info.base);

    sspv::Function *fetch_func = b.makeFunctionEntry(sspv::NoPrecision, type_f32, func_name.c_str(), { type_i32 }, { "addr" },
        {}, &func_block);

    sspv::Id sixteen_cst = b.makeIntConstant(16);
    sspv::Id eight_cst = b.makeIntConstant(8);
    sspv::Id four_cst = b.makeIntConstant(4);
    sspv::Id one_cst = b.makeIntConstant(1);
    sspv::Id zero_cst = b.makeIntConstant(0);

    sspv::Id addr = fetch_func->getParamId(0);
    sspv::Id base_vector = b.createBinOp(sspv::OpSDiv, type_i32, addr, sixteen_cst);
    sspv::Id base_left = b.createBinOp(sspv::OpSRem, type_i32, addr, sixteen_cst);
    sspv::Id base_offset = b.createBinOp(sspv::OpSDiv, type_i32, base_left, four_cst);
    sspv::Id rem = b.createBinOp(sspv::OpSRem, type_i32, base_left, four_cst);
    sspv::Id rem_inv = b.createBinOp(sspv::OpISub, type_i32, four_cst, rem);

    // If int was shifted by more than 32 bits in nvidia glsl, the pipeline crashes.
    // rem_inv_overflow is the flag used to make sure >> 32 is not executed.
    sspv::Id rem_inv_overflow = b.createBinOp(sspv::OpIEqual, b.makeBoolType(), rem_inv, four_cst);
    rem_inv = b.createTriOp(sspv::OpSelect, type_i32, rem_inv_overflow, zero_cst, rem_inv);

    sspv::Id rem_in_bits = b.createBinOp(sspv::OpIMul, type_i32, rem, eight_cst);
    sspv::Id rem_inv_in_bits = b.createBinOp(sspv::OpIMul, type_i32, rem_inv, eight_cst);

    sspv::Id src = b.createLoad(utils::create_access_chain(b, sspv::StorageClassStorageBuffer, buffer_container, { b.makeIntConstant(info.index_in_container), base_vector, base_offset }), sspv::NoPrecision);

    sspv::Id friend_offset = b.createBinOp(sspv::OpIAdd, type_i32, base_offset, one_cst);
    sspv::Id friend_vector = b.createBinOp(sspv::OpIAdd, type_i32, base_vector, b.createBinOp(sspv::OpSDiv, type_i32, friend_offset, b.makeIntConstant(4)));

    friend_offset = b.createBinOp(sspv::OpSRem, type_i32, friend_offset, four_cst);

    sspv::Id src_friend = b.createLoad(utils::create_access_chain(b, sspv::StorageClassStorageBuffer, buffer_container, { b.makeIntConstant(info.index_in_container), friend_vector, friend_offset }), sspv::NoPrecision);
    sspv::Id src_casted = b.createUnaryOp(sspv::OpBitcast, type_ui32, src);
    sspv::Id src_friend_casted = b.createUnaryOp(sspv::OpBitcast, type_ui32, src_friend);

    sspv::Id high_part = b.createBinOp(sspv::OpShiftLeftLogical, type_ui32, src_casted, rem_in_bits);
    sspv::Id low_part = b.createBinOp(sspv::OpShiftRightLogical, type_ui32, src_friend_casted, rem_inv_in_bits);
    low_part = b.createTriOp(sspv::OpSelect, type_ui32, rem_inv_overflow, b.makeUintConstant(0), low_part);

    sspv::Id output = b.createBinOp(sspv::OpBitwiseOr, type_ui32, high_part, low_part);
    sspv::Id output_casted = b.createUnaryOp(sspv::OpBitcast, type_f32, output);

    b.makeReturn(false, output_casted);
    b.setBuildPoint(last_build_point);

    return fetch_func;
}

static sspv::Function *make_fetch_memory_func(sspv::Builder &b, const SpirvShaderParameters &params) {
    sspv::Id type_f32 = b.makeFloatType(32);
    sspv::Id type_i32 = b.makeIntType(32);
    sspv::Id type_bool = b.makeBoolType();

    sspv::Block *func_block;
    sspv::Block *last_build_point = b.getBuildPoint();

    sspv::Function *fetch_func = b.makeFunctionEntry(sspv::NoPrecision, type_f32, "fetchMemory", { type_i32 }, { "addr" },
        {}, &func_block);
    sspv::Id addr = fetch_func->getParamId(0);

    std::stack<std::unique_ptr<sspv::Builder::If>> fetch_stacks;

    for (auto &[index, buffer_info] : params.buffers) {
        if (!fetch_stacks.empty()) {
            fetch_stacks.top()->makeBeginElse();
        }

        const sspv::Id range_begin = b.makeIntConstant(buffer_info.base);
        const sspv::Id range_end = b.makeIntConstant(buffer_info.base + buffer_info.size);

        sspv::Id need1 = b.createBinOp(sspv::OpSGreaterThanEqual, type_bool, addr, range_begin);
        sspv::Id need2 = b.createBinOp(sspv::OpSLessThan, type_bool, addr, range_end);

        sspv::Id need_final = b.createBinOp(sspv::OpLogicalAnd, type_bool, need1, need2);

        fetch_stacks.push(std::make_unique<sspv::Builder::If>(need_final, sspv::SelectionControlMaskNone, b));

        sspv::Id subtracted_base = b.createBinOp(sspv::OpISub, type_i32, addr, range_begin);
        sspv::Function *access_func = make_fetch_memory_func_for_array(b, params.buffer_container, buffer_info, index);

        b.makeReturn(false, b.createFunctionCall(access_func, { subtracted_base }));
    }

    while (!fetch_stacks.empty()) {
        std::unique_ptr<sspv::Builder::If> fetch_if = std::move(fetch_stacks.top());
        fetch_if->makeEndIf();

        fetch_stacks.pop();
    }

    b.makeReturn(false, b.makeFloatConstant(0.0f));
    b.setBuildPoint(last_build_point);

    return fetch_func;
}

sspv::Id fetch_memory(sspv::Builder &b, const SpirvShaderParameters &params, SpirvUtilFunctions &utils, sspv::Id addr) {
    if (!utils.fetch_memory) {
        utils.fetch_memory = make_fetch_memory_func(b, params);
    }

    return b.createFunctionCall(utils.fetch_memory, { addr });
}

static sspv::Id make_or_get_buffer_ptr(sspv::Builder &b, shader::usse::utils::SpirvUtilFunctions &utils, int nb_components, int stride = 16, bool is_write = false) {
    const int buffer_utils_idx = (stride == 4) ? 0 : nb_components;

    if (utils.buffer_address_vec[buffer_utils_idx][is_write])
        return utils.buffer_address_vec[buffer_utils_idx][is_write];

    const sspv::Id f32 = b.makeFloatType(32);
    const sspv::Id vec = shader::usse::utils::make_vector_or_scalar_type(b, f32, nb_components);
    const sspv::Id runtime_array = b.makeRuntimeArray(vec);
    // always a stride of 16, even if the array size is less
    b.addDecoration(runtime_array, sspv::DecorationArrayStride, stride);
    const sspv::Id buffer_data = b.makeStructType({ runtime_array }, fmt::format("buffer_ptr{}_s{}", nb_components, stride).c_str());
    b.addDecoration(buffer_data, sspv::DecorationBlock);
    b.addMemberName(buffer_data, 0, "data");
    // non-writable for the time being
    if (is_write)
        b.addMemberDecoration(buffer_data, 0, sspv::DecorationNonReadable);
    else
        b.addMemberDecoration(buffer_data, 0, sspv::DecorationNonWritable);
    b.addMemberDecoration(buffer_data, 0, sspv::DecorationOffset, 0);

    utils.buffer_address_vec[buffer_utils_idx][is_write] = b.makePointer(sspv::StorageClassPhysicalStorageBuffer, buffer_data);
    return utils.buffer_address_vec[buffer_utils_idx][is_write];
}

void buffer_address_access(sspv::Builder &b, const SpirvShaderParameters &params, SpirvUtilFunctions &utils, const FeatureState &features, Operand dest, int dest_offset, sspv::Id addr, uint32_t component_size, uint32_t nb_components, int buffer_idx, bool is_buffer_store) {
    const sspv::Id i32 = b.makeIntType(32);
    const sspv::Id zero = b.makeIntConstant(0);

    sspv::Id buffer_idx_val;
    if (buffer_idx == -1) {
        // buffer index is in the upper 4 bits of addr
        buffer_idx_val = b.createBinOp(sspv::OpShiftRightLogical, i32, addr, b.makeIntConstant(28));
        // remove the buffer index bits from the address
        addr = b.createBinOp(sspv::OpBitwiseAnd, i32, addr, b.makeIntConstant((1 << 28) - 1));
    } else {
        buffer_idx_val = b.makeIntConstant(buffer_idx);
    }

    sspv::Id buffer_address = utils::create_access_chain(b, sspv::StorageClassUniform, params.render_info_id, { b.makeIntConstant(params.buffer_addresses_id), buffer_idx_val });
    buffer_address = b.createLoad(buffer_address, sspv::NoPrecision);
    // add the offset from the base address
    buffer_address = add_uvec2_uint(b, buffer_address, addr);

    if (component_size == sizeof(uint32_t)) {
        int buffer_idx_vec4 = 0;
        if (nb_components >= 4) {
            // first copy them 4 by 4 (using the fact that we can do 4-byte aligned reads)
            const sspv::Id buffer_container = make_or_get_buffer_ptr(b, utils, 4, 16, is_buffer_store);
            const sspv::Id buffer_address_vec4 = b.createUnaryOp(sspv::OpBitcast, buffer_container, buffer_address);
            while (nb_components >= 4) {
                sspv::Id accessed = utils::create_access_chain(b, sspv::StorageClassPhysicalStorageBuffer, buffer_address_vec4, { zero, b.makeIntConstant(buffer_idx_vec4) });

                if (is_buffer_store) {
                    sspv::Id data = load(b, params, utils, features, dest, 0b1111, dest_offset);
                    b.createStore(data, accessed, sspv::MemoryAccessAlignedMask, sspv::ScopeMax, 4);
                } else {
                    accessed = b.createLoad(accessed, sspv::NoPrecision, sspv::MemoryAccessAlignedMask, sspv::ScopeMax, 4);
                    store(b, params, utils, features, dest, accessed, 0b1111, dest_offset);
                }

                dest.num += 4;
                nb_components -= 4;
                buffer_idx_vec4++;
            }
        }

        assert(nb_components < 4);
        if (nb_components > 0) {
            // do one last load for the at most 3 last components
            const sspv::Id buffer_container = make_or_get_buffer_ptr(b, utils, nb_components, 16, is_buffer_store);
            const sspv::Id buffer_address_vec = b.createUnaryOp(sspv::OpBitcast, buffer_container, buffer_address);

            sspv::Id accessed = utils::create_access_chain(b, sspv::StorageClassPhysicalStorageBuffer, buffer_address_vec, { zero, b.makeIntConstant(buffer_idx_vec4) });

            if (is_buffer_store) {
                sspv::Id data = load(b, params, utils, features, dest, (1 << nb_components) - 1, dest_offset);
                b.createStore(data, accessed, sspv::MemoryAccessAlignedMask, sspv::ScopeMax, 4);
            } else {
                accessed = b.createLoad(accessed, sspv::NoPrecision, sspv::MemoryAccessAlignedMask, sspv::ScopeMax, 4);
                store(b, params, utils, features, dest, accessed, (1 << nb_components) - 1, dest_offset);
            }
        }
    } else {
        if (is_buffer_store) {
            LOG_ERROR("non-32 bit buffer store is not implemented! Please report it to the devs.");
            return;
        }

        // less optimized
        // TODO: if the gpu supports it, load it as a u16vec4 / u8vec4
        const sspv::Id buffer_container = make_or_get_buffer_ptr(b, utils, 1, 4);
        // pack the component by groups of 4 (except possible the last ones) when storing them
        std::vector<sspv::Id> loaded_components;

        for (uint32_t component_idx = 0; component_idx < nb_components; component_idx++) {
            sspv::Id component_addr = add_uvec2_uint(b, buffer_address, b.makeUintConstant(component_idx * component_size));
            // we must make it 4-byte aligned
            sspv::Id addr_low_bits = b.createCompositeExtract(component_addr, i32, 0);
            sspv::Id alignment = b.createBinOp(sspv::OpBitwiseAnd, i32, addr_low_bits, b.makeIntConstant(0b11));
            addr_low_bits = b.createBinOp(sspv::OpBitwiseAnd, i32, addr_low_bits, b.makeIntConstant(~0b11));
            component_addr = b.createCompositeInsert(addr_low_bits, component_addr, b.getTypeId(component_addr), 0);

            // now we can finally load it
            component_addr = b.createUnaryOp(sspv::OpBitcast, buffer_container, component_addr);
            sspv::Id loaded = utils::create_access_chain(b, sspv::StorageClassPhysicalStorageBuffer, component_addr, { zero, zero });
            loaded = b.createLoad(loaded, sspv::NoPrecision, sspv::MemoryAccessAlignedMask, sspv::ScopeMax, 4);

            // now keep only the interesting 8/16 bits
            loaded = b.createUnaryOp(sspv::OpBitcast, i32, loaded);
            sspv::Id shift = b.createBinOp(sspv::OpShiftLeftLogical, i32, alignment, b.makeIntConstant(3)); // 1 byte = 8 bits
            loaded = b.createOp(sspv::OpBitFieldSExtract, i32, { loaded, shift, b.makeIntConstant(component_size * 8) });

            loaded_components.push_back(loaded);

            if (loaded_components.size() == 4 || component_idx == nb_components - 1) {
                sspv::Id component_vec;
                if (loaded_components.size() == 1) {
                    component_vec = loaded_components[0];
                } else {
                    component_vec = b.createCompositeConstruct(b.makeVectorType(i32, loaded_components.size()), loaded_components);
                }
                store(b, params, utils, features, dest, component_vec, (1 << loaded_components.size()) - 1, dest_offset);

                dest.num += component_size;
                loaded_components.clear();
            }
        }
    }
}

sspv::Id unpack_one(sspv::Builder &b, SpirvUtilFunctions &utils, const FeatureState &features, sspv::Id scalar, const DataType type) {
    switch (type) {
    case DataType::INT8:
    case DataType::UINT8:
    case DataType::UINT16:
    case DataType::INT16: {
        auto iter = utils.unpack_funcs.find(type);
        if (iter == utils.unpack_funcs.end()) {
            iter = utils.unpack_funcs.emplace(type, make_unpack_func(b, features, type)).first;
        }
        return b.createFunctionCall(iter->second, { scalar });
    }
    case DataType::F16: {
        auto iter = utils.unpack_funcs.find(type);
        if (iter == utils.unpack_funcs.end()) {
            iter = utils.unpack_funcs.emplace(type, make_f16_unpack_func(b, utils, features)).first;
        }
        return b.createFunctionCall(iter->second, { scalar });
    }
    case DataType::C10: {
        if (!utils.unpack_fx10) {
            utils.unpack_fx10 = make_fx10_unpack_func(b, utils, features);
        }

        return b.createFunctionCall(utils.unpack_fx10, { scalar });
    }
    default: {
        LOG_ERROR("Unsupported unpack type: 0x{:0X}", fmt::underlying(type));
        break;
    }
    }

    return sspv::NoResult;
}

sspv::Id pack_one(sspv::Builder &b, SpirvUtilFunctions &utils, const FeatureState &features, sspv::Id vec, const DataType source_type) {
    switch (source_type) {
    case DataType::INT8:
    case DataType::UINT8:
    case DataType::UINT16:
    case DataType::INT16: {
        auto iter = utils.pack_funcs.find(source_type);
        if (iter == utils.pack_funcs.end()) {
            iter = utils.pack_funcs.emplace(source_type, make_pack_func(b, features, source_type)).first;
        }
        return b.createFunctionCall(iter->second, { vec });
    }
    case DataType::F16: {
        auto iter = utils.pack_funcs.find(source_type);
        if (iter == utils.pack_funcs.end()) {
            iter = utils.pack_funcs.emplace(source_type, make_f16_pack_func(b, utils, features)).first;
        }
        return b.createFunctionCall(iter->second, { vec });
    }

    default: {
        LOG_ERROR("Unsupported pack type: 0x{:0X}", fmt::underlying(source_type));
        break;
    }
    }

    return sspv::NoResult;
}

static sspv::Id apply_modifiers(sspv::Builder &b, const SpirvUtilFunctions &utils, const shader::usse::RegisterFlags flags, sspv::Id val) {
    sspv::Id contained_type = b.getTypeId(val);

    if (!b.isScalarType(contained_type)) {
        contained_type = b.getContainedTypeId(contained_type);
    }

    const bool is_int = b.isIntType(contained_type);
    const bool is_uint = b.isUintType(contained_type);

    const int num_comp = b.getNumComponents(val);
    sspv::Id dest_type = b.getTypeId(val);

    sspv::Id result = val;

    if (flags & shader::usse::RegisterFlags::Absolute) {
        // Absolute the result
        if (is_uint) {
            // It's already > 0, what do you expect more
            return result;
        }

        result = b.createBuiltinCall(dest_type, utils.std_builtins, GLSLstd450FAbs, { result });
    }

    // Apply modifier flags
    if (flags & shader::usse::RegisterFlags::Negative) {
        // Negate the value
        sspv::Id c0 = sspv::NoResult;
        sspv::Op sub_op = sspv::OpAny;

        if (is_int) {
            c0 = b.makeIntConstant(0);
            sub_op = sspv::OpISub;
        } else if (is_uint) {
            c0 = b.makeUintConstant(0);
            sub_op = sspv::OpISub;
        } else {
            c0 = b.makeFloatConstant(0.0f);
            sub_op = sspv::OpFSub;
        }

        std::vector<sspv::Id> ops(num_comp, c0);
        result = b.createBinOp(sub_op, dest_type, (num_comp == 1) ? c0 : b.makeCompositeConstant(dest_type, ops), result);
    }

    return result;
}

sspv::Id load(sspv::Builder &b, const SpirvShaderParameters &params, SpirvUtilFunctions &utils, const FeatureState &features, Operand op, const Imm4 dest_mask, int shift_offset) {
    sspv::Id type_f32 = b.makeFloatType(32);

    if (op.bank == RegisterBank::FPCONSTANT) {
        const bool integral_unsigned = (op.type == DataType::UINT32) || (op.type == DataType::UINT16);
        const bool integral_signed = (op.type == DataType::INT32) || (op.type == DataType::INT16);
        std::vector<sspv::Id> consts;

        auto handle_unexpect_swizzle = [&](const SwizzleChannel ch) {
#define GEN_CONSTANT(cnst)                                           \
    if (integral_unsigned)                                           \
        return b.makeUintConstant(static_cast<std::uint32_t>(cnst)); \
    else if (integral_signed)                                        \
        return b.makeIntConstant(static_cast<std::uint32_t>(cnst));  \
    else                                                             \
        return b.makeFloatConstant(static_cast<float>(cnst))
            switch (ch) {
            case SwizzleChannel::C_0:
                GEN_CONSTANT(0);
                break;
            case SwizzleChannel::C_1:
                GEN_CONSTANT(1);
                break;
            case SwizzleChannel::C_2:
                GEN_CONSTANT(2);
                break;
            case SwizzleChannel::C_H:
                GEN_CONSTANT(0.5f);
                break;
            default: break;
            }

            return sspv::NoResult;

#undef GEN_CONSTANT
        };

        // https://wiki.henkaku.xyz/vita/SGX543#Constants
        // Load constants. Ignore mask
        if ((op.type == DataType::F32) || (op.type == DataType::UINT32) || (op.type == DataType::INT32)) {
            auto get_f32_from_bank = [&](const int num) -> sspv::Id {
                int swizz_val = static_cast<int>(op.swizzle[num]) - static_cast<int>(SwizzleChannel::C_X);
                if (swizz_val >= 4)
                    return handle_unexpect_swizzle(op.swizzle[num]);

                uint32_t value = 0;
                // bank 1 is only used for channel 1
                if (swizz_val == 1) {
                    value = usse::f32_constant_table_bank_1_raw[op.num];
                } else {
                    value = usse::f32_constant_table_bank_0_raw[op.num];
                }

                if (integral_unsigned)
                    return b.makeUintConstant(value);
                else if (integral_signed)
                    return b.makeIntConstant(value);
                else
                    return b.makeFloatConstant(std::bit_cast<float>(value));
            };

            for (int i = 0; i < 4; i++) {
                if (dest_mask & (1 << i)) {
                    consts.push_back(get_f32_from_bank(i));
                }
            }
        } else if ((op.type == DataType::F16) || (op.type == DataType::UINT16) || (op.type == DataType::INT16)) {
            auto get_f16_from_bank = [&](const int num) -> sspv::Id {
                const int swizz_val = static_cast<int>(op.swizzle[num]) - static_cast<int>(SwizzleChannel::C_X);
                if (swizz_val >= 4)
                    return handle_unexpect_swizzle(op.swizzle[num]);

                float value = 0.f;
                switch (swizz_val) {
                case 1:
                    value = usse::f16_constant_table_bank1[op.num];
                    break;

                case 2:
                    value = usse::f16_constant_table_bank2[op.num];
                    break;

                case 3:
                    value = usse::f16_constant_table_bank3[op.num];
                    break;

                default:
                    value = usse::f16_constant_table_bank0[op.num];
                    break;
                }

                if (integral_unsigned || integral_signed) {
                    uint16_t value_int = util::encode_flt16(value);
                    if (integral_unsigned)
                        return b.makeUintConstant(value_int);
                    else
                        return b.makeIntConstant(static_cast<int16_t>(value));
                } else {
                    return b.makeFloatConstant(value);
                }
            };

            for (int i = 0; i < 4; i++) {
                if (dest_mask & (1 << i)) {
                    consts.push_back(get_f16_from_bank(i));
                }
            }
        }

        if (consts.size() == 1) {
            return apply_modifiers(b, utils, op.flags, consts[0]);
        } else {
            sspv::Id result = b.makeCompositeConstant(b.makeVectorType(type_f32, static_cast<int>(consts.size())), consts);
            return apply_modifiers(b, utils, op.flags, result);
        }
    }

    if (op.bank == RegisterBank::FPINTERNAL) {
        // Automatically F32
        switch (op.type) {
        case DataType::F16: {
            op.type = DataType::F32;
            break;
        }

        case DataType::INT16:
        case DataType::INT8: {
            op.type = DataType::INT32;
            break;
        }

        case DataType::UINT16:
        case DataType::UINT8: {
            op.type = DataType::UINT32;
            break;
        }

        default:
            break;
        }

        op.num <<= 2;
        shift_offset <<= 2;
    }

    const auto dest_comp_count = dest_mask_to_comp_count(dest_mask);
    const std::size_t size_comp = get_data_type_size(op.type);

    if (op.bank == RegisterBank::PREDICATE || op.bank == RegisterBank::INDEX) {
        sspv::Id bank_base = *get_reg_bank(params, op.bank);
        if (op.bank == RegisterBank::INDEX) {
            op.num -= 1;
        }
        sspv::Id result = b.createLoad(b.createOp(sspv::OpAccessChain, b.makePointer(sspv::StorageClassPrivate, b.getContainedTypeId(b.getContainedTypeId(b.getTypeId(bank_base)))), { bank_base, b.makeIntConstant(op.num) }), sspv::NoPrecision);

        if (!is_float_data_type(op.type) && size_comp < sizeof(int32_t)) {
            sspv::Id mask;
            sspv::Id type;
            switch (op.type) {
            case DataType::UINT8:
                mask = b.makeUintConstant(0xFF);
                type = b.makeUintType(32);
                break;
            case DataType::INT8:
                mask = b.makeIntConstant(0xFF);
                type = b.makeIntType(32);
                break;
            case DataType::UINT16:
                mask = b.makeUintConstant(0xFFFF);
                type = b.makeUintType(32);
                break;
            default: // DataType::UINT16
                mask = b.makeIntConstant(0xFFFF);
                type = b.makeIntType(32);
                break;
            }
            result = b.createBinOp(sspv::OpBitwiseAnd, type, result, mask);
        }

        return result;
    }

    if (op.bank == RegisterBank::IMMEDIATE || !get_reg_bank(params, op.bank)) {
        if (op.bank != RegisterBank::INDEXED1 && op.bank != RegisterBank::INDEXED2) {
            if (dest_comp_count == 1) {
                if ((int)op.swizzle[0] >= (int)SwizzleChannel::C_0) {
                    return get_correspond_constant_with_channel(b, op.swizzle[0]);
                }
            }

            sspv::Id constant = sspv::NoResult;

            const int imm = (op.bank == RegisterBank::IMMEDIATE) ? op.num : 0;

            if (is_unsigned_integer_data_type(op.type)) {
                constant = b.makeUintConstant(imm);
            } else if (is_signed_integer_data_type(op.type)) {
                constant = b.makeIntConstant(imm);
            } else {
                constant = b.makeFloatConstant(static_cast<float>(imm));
            }

            if (dest_comp_count == 1) {
                return apply_modifiers(b, utils, op.flags, constant);
            }

            std::vector<sspv::Id> ops(dest_comp_count, constant);
            sspv::Id pass = b.makeCompositeConstant(b.makeVectorType(b.getTypeId(constant), static_cast<int>(dest_comp_count)), ops);

            pass = finalize(b, pass, pass, op.swizzle, b.makeIntConstant(shift_offset), dest_mask);
            return apply_modifiers(b, utils, op.flags, pass);
        }
    }

    sspv::Id idx_in_arr_1 = sspv::NoResult;
    sspv::Id idx_in_arr_2 = sspv::NoResult;

    sspv::Id finalize_offset = b.makeIntConstant(op.num + shift_offset);
    if (op.bank == RegisterBank::INDEXED1 || op.bank == RegisterBank::INDEXED2) {
        // Decode the info. Usually the number of bits in a INDEXED number is 7.
        // TODO: Fix the assumption
        const Imm2 bank_enc = (op.num >> 5) & 0b11;
        const Imm5 add_off = (op.num & 0b11111) + shift_offset;

        const std::int8_t idx_off = (int)op.bank - (int)RegisterBank::INDEXED1;

        switch (bank_enc) {
        case 0: {
            op.bank = RegisterBank::TEMP;
            break;
        }

        case 1: {
            op.bank = RegisterBank::OUTPUT;
            break;
        }

        case 2: {
            op.bank = RegisterBank::PRIMATTR;
            break;
        }

        case 3: {
            op.bank = RegisterBank::SECATTR;
            break;
        }
        }

        sspv::Id type_i32 = b.makeIntType(32);

        // Calculate the "at" offset.
        sspv::Id idx_reg_val = b.createLoad(b.createOp(sspv::OpAccessChain, b.makePointer(sspv::StorageClassPrivate, type_i32), { params.indexes, b.makeIntConstant(idx_off) }), sspv::NoPrecision);

        sspv::Id real_idx = b.createBinOp(sspv::OpIAdd, type_i32, b.createBinOp(sspv::OpIMul, type_i32, idx_reg_val, b.makeIntConstant(2)), b.makeIntConstant(add_off));
        finalize_offset = real_idx;

        idx_in_arr_1 = b.createBinOp(sspv::OpSDiv, type_i32, real_idx, b.makeIntConstant(4));
        idx_in_arr_2 = b.createBinOp(sspv::OpSDiv, type_i32, b.createBinOp(sspv::OpIAdd, type_i32, real_idx, b.makeIntConstant(3)),
            b.makeIntConstant(4));
    }

    const int num_comp_in_single_float = static_cast<int>(4 / size_comp);

    // In here we calculate the highest/lowest offset of component that got written.
    // Starting from the nearest X component.
    int lowest_dest_write_offset = 999; ///< Lowest offset of the component to write.
    int highest_dest_write_offset = -1; ///< Highest offset of the component to write.

    for (int i = 0; i < 4; i++) {
        if (static_cast<int>(op.swizzle[i]) >= static_cast<int>(SwizzleChannel::C_0)) {
            continue;
        }

        const int swizzle_bit = static_cast<int>(op.swizzle[i]) - static_cast<int>(SwizzleChannel::C_X);

        if (dest_mask & (1 << i)) {
            lowest_dest_write_offset = std::min(lowest_dest_write_offset, swizzle_bit);
            highest_dest_write_offset = std::max(highest_dest_write_offset, swizzle_bit);
        }
    }

    if (lowest_dest_write_offset == 999 || highest_dest_write_offset == -1) {
        // This is the default value of the lowest swizzle channel.
        // Which means that no mask is on, all of the loadable ones, are constant swizzle channel.
        // Iterates through all of them and build a composite constant
        std::vector<sspv::Id> comps;

        for (int i = 0; i < 4; i++) {
            if (dest_mask & (1 << i)) {
                comps.push_back(get_correspond_constant_with_channel(b, op.swizzle[i]));
            }
        }
        // Create a constant composite
        if (comps.size() == 1) {
            return apply_modifiers(b, utils, op.flags, comps[0]);
        }

        else {
            return apply_modifiers(b, utils, op.flags, b.makeCompositeConstant(b.makeVectorType(type_f32, static_cast<int>(comps.size())), comps));
        }
    }

    // For non-F32 and non-I32 type, we need to make a destination mask to extract necessary F32 components out
    // For example: sa6.xz with DataType = f16
    // Would result at least sa6 and sa7 to be extracted out, since sa6 contains f16 x and y, sa7 contains f16 z and w
    Imm4 extract_mask = dest_mask;
    Swizzle4 extract_swizz = op.swizzle;

    if (size_comp != 4) {
        extract_mask = 0;

        for (int i = lowest_dest_write_offset / num_comp_in_single_float; i <= highest_dest_write_offset / num_comp_in_single_float; i++) {
            // Build up an extract mask
            extract_mask |= (1 << i);
        }

        // Set default swizzle
        // We only need to extract, the unpack will do the swizzling job later.
        extract_swizz = SWIZZLE_CHANNEL_4_DEFAULT;
    }

    sspv::Id first_pass = sspv::NoResult;
    sspv::Id connected_friend = sspv::NoResult;
    sspv::Id bank_base = *get_reg_bank(params, op.bank);
    sspv::Id comp_type = b.getContainedTypeId(b.getContainedTypeId(b.getTypeId(bank_base)));
    comp_type = b.makePointer(sspv::StorageClassPrivate, comp_type);

    // May access two float in the arrays to get U8 or F16 components
    // Need to divide it with number of source components that each float can hold
    if (idx_in_arr_1 == sspv::NoResult) {
        idx_in_arr_1 = b.makeIntConstant((op.num + shift_offset) >> 2);
    }

    if (idx_in_arr_2 == sspv::NoResult) {
        idx_in_arr_2 = b.makeIntConstant((op.num + shift_offset + 3) >> 2);
    }

    std::vector<sspv::Id> first_pass_operands;
    std::vector<sspv::Id> second_pass_operands;

    first_pass_operands.push_back(bank_base);
    second_pass_operands.push_back(bank_base);

    first_pass_operands.push_back(idx_in_arr_1);
    second_pass_operands.push_back(idx_in_arr_2);

    // Do an access chain
    first_pass = b.createOp(sspv::OpAccessChain, comp_type, first_pass_operands);
    connected_friend = b.createOp(sspv::OpAccessChain, comp_type, second_pass_operands);

    first_pass = finalize(b, b.createLoad(first_pass, sspv::NoPrecision), b.createLoad(connected_friend, sspv::NoPrecision), extract_swizz,
        finalize_offset, extract_mask);

    if (first_pass == sspv::NoResult) {
        return first_pass;
    }

    if (size_comp != 4) {
        // Second pass: Do unpack
        first_pass = unpack(b, utils, features, first_pass, op.type, op.swizzle, dest_mask, 0);
    } else if (op.type == DataType::INT32) {
        first_pass = b.createUnaryOp(sspv::OpBitcast, make_vector_or_scalar_type(b, b.makeIntType(32), static_cast<int>(dest_comp_count)), first_pass);
    } else if (op.type == DataType::UINT32) {
        first_pass = b.createUnaryOp(sspv::OpBitcast, make_vector_or_scalar_type(b, b.makeUintType(32), static_cast<int>(dest_comp_count)), first_pass);
    }

    if (first_pass == sspv::NoResult) {
        return first_pass;
    }

    return apply_modifiers(b, utils, op.flags, first_pass);
}

sspv::Id unpack(sspv::Builder &b, SpirvUtilFunctions &utils, const FeatureState &features, sspv::Id target, const DataType type, Swizzle4 swizz, const Imm4 dest_mask,
    const int offset) {
    if (type == DataType::F32 || target == sspv::NoResult) {
        return target;
    }

    sspv::Id type_f32 = b.makeFloatType(32);
    std::vector<sspv::Id> unpack_results;

    const sspv::Id target_type = b.getTypeId(target);
    const std::uint32_t target_comp_count = b.getNumTypeComponents(target_type);

    unpack_results.resize(target_comp_count);

    for (std::size_t i = 0; i < unpack_results.size(); i++) {
        sspv::Id extracted = target;

        if (!b.isScalar(target) && !b.isConstant(target)) {
            std::vector<sspv::Id> extract_ops;
            extract_ops.push_back(target);
            extract_ops.push_back(b.makeIntConstant(static_cast<int>(i)));

            if (target_comp_count > 1) {
                extracted = b.createOp(sspv::OpVectorExtractDynamic, type_f32, extract_ops);
            }
        }

        unpack_results[i] = unpack_one(b, utils, features, extracted, type);
    }

    return finalize(b, unpack_results[0], unpack_results.size() > 1 ? unpack_results[1] : unpack_results[0],
        swizz, b.makeIntConstant(offset), dest_mask);
}

void store(sspv::Builder &b, const SpirvShaderParameters &params, SpirvUtilFunctions &utils, const FeatureState &features, Operand dest,
    sspv::Id source, std::uint8_t dest_mask, int off) {
    if (source == sspv::NoResult) {
        LOG_WARN("Source invalid");
        return;
    }

    // Check for INDEX bank. INDEX bank are optimized to store an integer
    if (dest.bank == RegisterBank::INDEX || dest.bank == RegisterBank::PREDICATE) {
        sspv::Id bank_base = *get_reg_bank(params, dest.bank);

        if (dest.bank == RegisterBank::INDEX) {
            dest.num -= 1;

            if (!b.isIntType(source)) {
                std::vector<sspv::Id> ops{ source };
                source = b.createOp(sspv::OpBitcast, b.makeIntType(32), ops);
            }

            // if dest is a 8 or 16 bits integer
            // Todo: keep the content of the upper bits of idx (not done right now)
            if (!is_float_data_type(dest.type) && get_data_type_size(dest.type) < 4) {
                sspv::Id mask = (get_data_type_size(dest.type) == 1)
                    ? b.makeIntConstant(0xFF)
                    : b.makeIntConstant(0xFFFF);
                source = b.createBinOp(sspv::OpBitwiseAnd, b.makeIntType(32), source, mask);
            }
        }

        sspv::Id var = b.createOp(sspv::OpAccessChain, b.makePointer(sspv::StorageClassPrivate, b.getContainedTypeId(b.getContainedTypeId(b.getTypeId(bank_base)))), { bank_base, b.makeIntConstant(dest.num) });

        b.createStore(source, var);
        return;
    }

    if (!get_reg_bank(params, dest.bank)) {
        return;
    }

    // If dest has default swizzle, is full-length (all dest component) and starts at a
    // register boundary, translate it to just a createStore
    auto total_comp_source = static_cast<std::uint8_t>(b.getNumComponents(source));

    sspv::Id source_elm_type_id = b.getTypeId(source);
    if (b.isVectorType(source_elm_type_id)) {
        source_elm_type_id = b.getContainedTypeId(source_elm_type_id);
    }

    if (is_float_data_type(dest.type) && (b.isIntType(source_elm_type_id) || b.isUintType(source_elm_type_id))) {
        LOG_CRITICAL("Trying to store float to int storage");
        return;
    }

    if (!is_float_data_type(dest.type) && !b.isIntType(source_elm_type_id) && !b.isUintType(source_elm_type_id)) {
        LOG_CRITICAL("Trying to store int to float storage");
        return;
    }

    sspv::Id type_f32 = b.makeFloatType(32);

    // FPINTERNAL bank can't pack, always store 32-bit. So bitcast
    if (((dest.bank == RegisterBank::FPINTERNAL) && is_integer_data_type(dest.type))
        || ((dest.bank != RegisterBank::FPINTERNAL) && ((dest.type == DataType::UINT32) || (dest.type == DataType::INT32)))) {
        std::vector<sspv::Id> ops{ source };
        sspv::Id bitcast_type = utils::make_vector_or_scalar_type(b, type_f32, total_comp_source);

        source = b.createOp(sspv::OpBitcast, bitcast_type, ops);
        dest.type = DataType::F32;
    }

    if (dest.bank == RegisterBank::FPINTERNAL) {
        dest.type = DataType::F32;
        dest.num <<= 2;
        off <<= 2;
    }

    const std::size_t size_comp = get_data_type_size(dest.type);

    sspv::Id bank_base = *get_reg_bank(params, dest.bank);

    // Element inside an array inside a pointer.
    sspv::Id bank_base_elem_type = b.getContainedTypeId(b.getContainedTypeId(b.getTypeId(bank_base)));
    sspv::Id comp_type = b.makePointer(sspv::StorageClassPrivate, bank_base_elem_type);
    int insert_offset = dest.num + off;
    sspv::Id elem = sspv::NoResult;

    // Check the nearest avail swizzle
    int nearest_swizz_on = 0;

    for (int i = 0; i < 4; i++) {
        if (dest_mask & (1 << i)) {
            nearest_swizz_on = i;
            break;
        }
    }

    // Floor down to nearest component that a float can hold. We originally want to optimize it to store from the first offset in float unit that writes the data.
    // But for unit size smaller than float, we have to start from the beginning in float unit.
    const int num_comp_in_float = static_cast<int>(4 / size_comp);
    nearest_swizz_on = nearest_swizz_on / num_comp_in_float * num_comp_in_float;

    if (dest.type != DataType::F32) {
        std::vector<sspv::Id> composites;
        sspv::Id vec_comp_type = utils::unwrap_type(b, b.getTypeId(source));
        int source_value_taken_count = 0;

        // We need to pack source
        for (auto i = 0; i < 4 - nearest_swizz_on; i += num_comp_in_float) {
            // Shuffle to get the type out
            std::vector<sspv::Id> ops;
            for (auto j = 0; j < num_comp_in_float; j++) {
                if (dest_mask & (1 << (nearest_swizz_on + i + j))) {
                    if (b.isScalar(source) || total_comp_source == 1) {
                        ops.push_back(source);
                    } else {
                        ops.push_back(b.createOp(sspv::OpVectorExtractDynamic, vec_comp_type, { source, b.makeIntConstant(std::min(source_value_taken_count++, (int)total_comp_source - 1)) }));
                    }
                } else {
                    if (elem == sspv::NoResult) {
                        // Replace it
                        const int actual_offset_start_to_store = insert_offset + (i + nearest_swizz_on) / num_comp_in_float;
                        elem = b.createOp(sspv::OpAccessChain, comp_type, { bank_base, b.makeIntConstant(actual_offset_start_to_store >> 2) });
                        elem = b.createOp(sspv::OpVectorExtractDynamic, b.makeFloatType(32), { b.createLoad(elem, sspv::NoPrecision), b.makeIntConstant(actual_offset_start_to_store % 4) });

                        // Extract to f16
                        elem = unpack_one(b, utils, features, elem, dest.type);
                    }

                    ops.push_back(b.createOp(sspv::OpVectorExtractDynamic, vec_comp_type, { elem, b.makeIntConstant(j) }));
                }
            }

            sspv::Id result_type = utils::make_vector_or_scalar_type(b, vec_comp_type, (int)ops.size());
            sspv::Id result = ops.size() == 1 ? ops[0] : b.createCompositeConstruct(result_type, ops);
            result = pack_one(b, utils, features, result, dest.type);

            composites.push_back(result);

            elem = sspv::NoResult;
        }

        if (composites.size() == 1) {
            // A single package
            source = composites[0];
            total_comp_source = 1;
        } else {
            sspv::Id final_composite_type = b.makeVectorType(type_f32, static_cast<int>(composites.size()));

            // Composite a new vector
            source = b.createCompositeConstruct(final_composite_type, composites);
            total_comp_source = static_cast<std::uint8_t>(composites.size());
        }

        // Replace it with a full mask, since we have already pre-process this into storable F32
        dest_mask = 0b1111;
    }

    // Now we do store!
    if (total_comp_source == 1) {
        insert_offset += (int)(nearest_swizz_on / (4 / size_comp));
        elem = b.createOp(sspv::OpAccessChain, comp_type, { bank_base, b.makeIntConstant(insert_offset >> 2) });
        sspv::Id inserted = b.createOp(sspv::OpVectorInsertDynamic, bank_base_elem_type, { b.createLoad(elem, sspv::NoPrecision), source, b.makeIntConstant(insert_offset % 4) });

        b.createStore(inserted, elem);
        return;
    }

    if (total_comp_source == 4 && dest_mask == 0b1111 && insert_offset % 4 == 0) {
        // Store directly
        elem = b.createOp(sspv::OpAccessChain, comp_type, { bank_base, b.makeIntConstant(insert_offset >> 2) });
        b.createStore(source, elem);
        return;
    }

    std::vector<sspv::IdImmediate> ops;

    // The provided source are stored in continuous order, however the dest mask may not be the same
    const int total_elem_to_copy_first_vec = std::min<int>(4 - (insert_offset % 4), total_comp_source);
    std::uint32_t dest_comp_stored_so_far = 0;

    elem = b.createOp(sspv::OpAccessChain, comp_type, { bank_base, b.makeIntConstant((insert_offset) >> 2) });

    ops.emplace_back(true, b.createLoad(elem, sspv::NoPrecision));
    ops.emplace_back(true, source);

    for (auto i = 0; i < insert_offset % 4; i++) {
        ops.emplace_back(false, i);
    }

    for (auto i = insert_offset % 4; i < 4; i++) {
        if ((dest_comp_stored_so_far < total_comp_source) && (dest_mask & (1 << (i - (insert_offset % 4))))) {
            ops.emplace_back(false, 4 + (dest_comp_stored_so_far++));
        } else {
            ops.emplace_back(false, i);
        }
    }

    sspv::Id shuffled = b.createOp(sspv::OpVectorShuffle, b.makeVectorType(type_f32, 4), ops);
    b.createStore(shuffled, elem);

    // Check if there's leftover to be stored to next vec4 element.
    if ((insert_offset % 4) + total_comp_source > 4) {
        ops.clear();
        const int total_elem_left = ((insert_offset % 4) + total_comp_source) - 4;

        elem = b.createOp(sspv::OpAccessChain, comp_type, { bank_base, b.makeIntConstant((insert_offset + 3) >> 2) });

        // Do an access chain
        ops.emplace_back(true, b.createLoad(elem, sspv::NoPrecision));
        ops.emplace_back(true, source);

        // Start taking value that specified in the mask
        for (auto i = 0; i < total_elem_left; i++) {
            if ((dest_comp_stored_so_far < total_comp_source) && (dest_mask & (1 << (total_elem_to_copy_first_vec + i)))) {
                ops.emplace_back(false, 4 + (dest_comp_stored_so_far++));
            } else {
                ops.emplace_back(false, i);
            }
        }

        for (auto i = total_elem_left; i < 4; i++) {
            ops.emplace_back(false, i);
        }

        sspv::Id shuffled = b.createOp(sspv::OpVectorShuffle, bank_base_elem_type, ops);
        b.createStore(shuffled, elem);
    }
}

sspv::Id make_vector_or_scalar_type(sspv::Builder &b, sspv::Id component, int size) {
    if (size == 1) {
        return component;
    }
    return b.makeVectorType(component, size);
}

sspv::Id unwrap_type(sspv::Builder &b, sspv::Id type) {
    if (b.isVectorType(type)) {
        return b.getContainedTypeId(type);
    }
    return type;
}

static float get_int_normalize_range_constants(DataType type) {
    switch (type) {
    case DataType::UINT8:
        return 255.0f;
    case DataType::INT8:
        return 127.0f;
    case DataType::C10:
        // signed 10-bit, with a range of [-2, 2]
        return 255.0f;
    case DataType::UINT16:
        return 65535.0f;
    case DataType::INT16:
        return 32767.0f;
    case DataType::UINT32:
        return 4294967295.0f;
    case DataType::INT32:
        return 2147483647.0f;
    default:
        assert(false);
        return 0.0f;
    }
}

static sspv::Id create_constant_vector_or_scalar(sspv::Builder &b, sspv::Id constant, int comp_count) {
    if (comp_count == 1) {
        return constant;
    }
    std::vector<sspv::Id> oprs(comp_count, constant);
    return b.createCompositeConstruct(b.makeVectorType(b.getTypeId(constant), comp_count), oprs);
}

sspv::Id convert_to_float(sspv::Builder &b, const SpirvUtilFunctions &utils, sspv::Id opr, DataType type, bool normal) {
    const auto spv_type = unwrap_type(b, b.getTypeId(opr));
    const auto comp_count = b.isVector(opr) ? b.getNumComponents(opr) : 1;
    const auto target_type = b.isVector(opr) ? b.makeVectorType(b.makeFloatType(32), comp_count) : b.makeFloatType(32);
    assert(b.isIntType(spv_type) || b.isUintType(spv_type));

    const auto is_sint = b.isIntType(spv_type);

    if (is_sint) {
        opr = b.createUnaryOp(sspv::OpConvertSToF, target_type, opr);
    } else {
        opr = b.createUnaryOp(sspv::OpConvertUToF, target_type, opr);
    }

    if (normal) {
        const float normalizer = b.makeFloatConstant(get_int_normalize_range_constants(type));
        const sspv::Id normalizer_vec = create_constant_vector_or_scalar(b, normalizer, comp_count);

        opr = b.createBinOp(sspv::OpFDiv, target_type, opr, normalizer_vec);
        if (is_sint) {
            // opr = max(-1.0f, opr) (or -2.0f for fx10)
            float lower_bound = type == DataType::C10 ? -2.f : -1.f;
            const sspv::Id minus1 = create_constant_vector_or_scalar(b, b.makeFloatConstant(lower_bound), comp_count);
            opr = b.createBuiltinCall(target_type, utils.std_builtins, GLSLstd450FMax, { opr, minus1 });
        }
    }
    return opr;
}

sspv::Id convert_to_int(sspv::Builder &b, const SpirvUtilFunctions &utils, sspv::Id opr, DataType type, bool normal) {
    const auto opr_type = b.getTypeId(opr);
    assert(b.isFloatType(unwrap_type(b, b.getTypeId(opr))));

    const auto comp_count = b.isVector(opr) ? b.getNumComponents(opr) : 1;
    const auto is_uint = is_unsigned_integer_data_type(type);
    const auto target_comp_type = is_uint ? b.makeUintType(32) : b.makeIntType(32);
    const auto target_type = b.isVector(opr) ? b.makeVectorType(target_comp_type, comp_count) : target_comp_type;

    if (normal) {
        const float constant_range = get_int_normalize_range_constants(type);
        const sspv::Id normalizer = b.makeFloatConstant(constant_range);
        const auto normalizer_vec = create_constant_vector_or_scalar(b, normalizer, comp_count);
        const bool is_fx10 = type == DataType::C10; // fx10 range is [-2,2]
        const auto range_begin_vec = create_constant_vector_or_scalar(b, b.makeFloatConstant(is_uint ? 0.f : (is_fx10 ? -2.f : -1.f)), comp_count);
        const auto range_end_vec = create_constant_vector_or_scalar(b, b.makeFloatConstant(is_fx10 ? 2.f : 1.f), comp_count);

        // opr = round(clamp(opr * norm), -1, 1)
        opr = b.createBuiltinCall(opr_type, utils.std_builtins, GLSLstd450FClamp, { opr, range_begin_vec, range_end_vec });
        opr = b.createBinOp(sspv::OpFMul, opr_type, opr, normalizer_vec);
        opr = b.createBuiltinCall(opr_type, utils.std_builtins, GLSLstd450Round, { opr });
    }

    if (!is_uint) {
        opr = b.createUnaryOp(sspv::OpConvertFToS, target_type, opr);
    } else {
        opr = b.createUnaryOp(sspv::OpConvertFToU, target_type, opr);
    }

    return opr;
}

sspv::Id add_uvec2_uint(sspv::Builder &b, sspv::Id vec, sspv::Id to_add) {
    if (b.isConstant(to_add) && b.getConstantScalar(to_add) == 0)
        return vec;

    const sspv::Id u32 = b.makeUintType(32);
    const sspv::Id uvec2 = b.makeVectorType(u32, 2);
    const sspv::Id add_result_type = b.makeStructResultType(u32, u32);

    if (!b.isUintType(b.getTypeId(to_add)))
        // convert i32 to u32
        to_add = b.createUnaryOp(sspv::OpBitcast, u32, to_add);

    // add to_add to the lower part of vec then add the carry to the upper part of vec
    // something like this
    // uint carry;
    // vec.x = uaddCarry(vec.x, to_add, carry);
    // vec.y += carry;
    sspv::Id lower = b.createCompositeExtract(vec, u32, 0);
    sspv::Id lower_add = b.createBinOp(sspv::OpIAddCarry, add_result_type, lower, to_add);
    sspv::Id carry = b.createCompositeExtract(lower_add, u32, 1);
    sspv::Id upper = b.createCompositeExtract(vec, u32, 1);
    upper = b.createBinOp(sspv::OpIAdd, u32, upper, carry);
    lower = b.createCompositeExtract(lower_add, u32, 0);
    return b.createCompositeConstruct(uvec2, { lower, upper });
}

} // namespace shader::usse::utils
