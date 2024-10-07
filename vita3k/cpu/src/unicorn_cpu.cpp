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

#include <cpu/functions.h>
#include <cpu/impl/unicorn_cpu.h>
#include <mem/functions.h>
#include <mem/ptr.h>
#include <mem/util.h>
#include <util/log.h>

#include <cassert>
#include <cpu/disasm/functions.h>

#include <util/string_utils.h>

constexpr bool TRACE_RETURN_VALUES = true;
constexpr bool LOG_REGISTERS = false;

static inline void func_trace(CPUState &state) {
    if (TRACE_RETURN_VALUES)
        if (is_returning(state.disasm))
            LOG_TRACE("Returning, r0: {}", log_hex(read_reg(state, 0)));
}

void UnicornCPU::code_hook(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    LOG_TRACE("CODE HOOK");
    UnicornCPU &state = *static_cast<UnicornCPU *>(user_data);
    std::string disassembly = disassemble(*state.parent, address);
    if (LOG_REGISTERS) {
        for (uint8_t i = 0; i < 12; i++) {
            auto reg_name = fmt::format("r{}", i);
            auto reg_name_with_value = fmt::format("{}({})", reg_name, log_hex(state.get_reg(i)));
            string_utils::replace(disassembly, reg_name, reg_name_with_value);
        }

        string_utils::replace(disassembly, "lr", fmt::format("lr({})", log_hex(state.get_lr())));
        string_utils::replace(disassembly, "sp", fmt::format("sp({})", log_hex(state.get_sp())));
    }

    LOG_TRACE("{} ({}): {} {}", log_hex((uint64_t)uc), state.parent->thread_id, log_hex(address), disassembly);

    func_trace(*state.parent);
}

void UnicornCPU::read_hook(uc_engine *uc, uc_mem_type type, uint64_t address, int size, int64_t value, void *user_data) {
    LOG_TRACE("READ HOOK");
    assert(value == 0);

    UnicornCPU &state = *static_cast<UnicornCPU *>(user_data);
    MemState &mem = *state.parent->mem;
    auto start = state.parent->protocol->get_watch_memory_addr(address);
    if (start) {
        memcpy(&value, Ptr<const void>(static_cast<Address>(address)).get(mem), size);
        state.log_memory_access(uc, "Read", start, size, value, mem, *state.parent, address - start);
    }
}

void UnicornCPU::write_hook(uc_engine *uc, uc_mem_type type, uint64_t address, int size, int64_t value, void *user_data) {
    LOG_TRACE("WRITE HOOK");
    UnicornCPU &state = *static_cast<UnicornCPU *>(user_data);
    auto start = state.parent->protocol->get_watch_memory_addr(address);
    if (start) {
        MemState &mem = *state.parent->mem;
        state.log_memory_access(uc, "Write", start, size, value, mem, *state.parent, address - start);
    }
}

void UnicornCPU::log_memory_access(uc_engine *uc, const char *type, Address address, int size, int64_t value, MemState &mem, CPUState &cpu, Address offset) {
    LOG_TRACE("LOG MEM ACCESS");
    const char *const name = mem_name(address, mem);
    auto pc = get_pc();
    LOG_TRACE("{} ({}): {} {} bytes, address {} + {} ({}, {}), value {} at {}", log_hex((uint64_t)uc), cpu.thread_id, type, size, log_hex(address), log_hex(offset), log_hex(address + offset), name, log_hex(value), log_hex(pc));
}

constexpr uint32_t INT_SVC = 2;
constexpr uint32_t INT_BKPT = 7;

void UnicornCPU::intr_hook(uc_engine *uc, uint32_t intno, void *user_data) {
    LOG_TRACE("INTR HOOK");
    assert(intno == INT_SVC || intno == INT_BKPT);
    UnicornCPU &state = *static_cast<UnicornCPU *>(user_data);
    uint32_t pc = state.get_pc();
    state.is_inside_intr_hook = true;
    if (intno == INT_SVC) {
        assert(!state.is_thumb_mode());
        const Address svc_address = pc - 4;
        uint32_t svc_instruction = 0;
        const auto err = uc_mem_read(uc, svc_address, &svc_instruction, sizeof(svc_instruction));
        assert(err == UC_ERR_OK);
        const uint32_t svc = state.is_thumb_mode() ? (svc_instruction & 0xff0000) >> 16 : svc_instruction & 0xffffff;
        state.parent->svc_called = true;
        state.parent->svc = svc;
        state.stop();
    } else if (intno == INT_BKPT) {
        state.stop();
        state.did_break = true;
    }
    state.is_inside_intr_hook = false;
}

static void enable_vfp_fpu(uc_engine *uc) {
    LOG_TRACE("ENABLE VPU/FPU");
/*    uint64_t c1_c0_2 = 0;
    uc_err err = uc_reg_read(uc, UC_ARM_REG_CP_REG, &c1_c0_2);
    assert(err == UC_ERR_OK);

    c1_c0_2 |= (0xf << 20);

    err = uc_reg_write(uc, UC_ARM_REG_CP_REG, &c1_c0_2);
    assert(err == UC_ERR_OK);
*/
   // const uint64_t fpexc = 0xf0000000;
    const uint64_t fpexc = 0x40000000;

    err = uc_reg_write(uc, UC_ARM_REG_FPEXC, &fpexc);
    assert(err == UC_ERR_OK);
}

void UnicornCPU::log_error_details(uc_err code) {
    // I don't especially want the time logged for every line, but I also want it to print to the log file...
    LOG_ERROR("Unicorn error {}. {}\n{}", log_hex(code), uc_strerror(code), this->save_context().description());

    auto pc = this->get_pc();
    if (pc < parent->mem->page_size)
        LOG_CRITICAL("PC is 0x{:x}", pc);
    else
        LOG_WARN("Executing: {}", disassemble(*parent, pc, nullptr));
}

UnicornCPU::UnicornCPU(CPUState *state)
    : parent(state) {
    uc_engine *temp_uc = nullptr;
    LOG_TRACE("UC_OPEN");
    uc_err err = uc_open(UC_ARCH_ARM, UC_MODE_ARM, &temp_uc);
    assert(err == UC_ERR_OK);
    LOG_TRACE("UNICORNPTR");
    uc = UnicornPtr(temp_uc, uc_close);
    temp_uc = nullptr;

    uc_hook hh = 0;

    LOG_TRACE("uc_ctl_set_cpu_model");
    err = uc_ctl_set_cpu_model(uc.get(), UC_CPU_ARM_CORTEX_A9);
    assert(err == UC_ERR_OK);

    LOG_TRACE("uc_hook_add");
    err = uc_hook_add(uc.get(), &hh, UC_HOOK_INTR, reinterpret_cast<void *>(&intr_hook), this, 1, 0);
    assert(err == UC_ERR_OK);

    // Don't map the null page into unicorn so that unicorn returns access error instead of
    // crashing the whole emulator on invalid access
    LOG_TRACE("uc_mem_map_ptr");
    err = uc_mem_map_ptr(uc.get(), state->mem->page_size, GiB(4), UC_PROT_ALL, &state->mem->memory[state->mem->page_size]);
    assert(err == UC_ERR_OK);

    LOG_TRACE("enable_vfp_fpu");
    enable_vfp_fpu(uc.get());
}

int UnicornCPU::execute_instructions_no_check(int num) {
    LOG_TRACE("GET_PC");
    std::uint32_t pc = get_pc();
    LOG_TRACE("IS THUMB?");
    bool thumb_mode = is_thumb_mode();
    if (thumb_mode) {
        pc |= 1;
    }

    LOG_TRACE("uc_emu_start");
    uc_err err = uc_emu_start(uc.get(), pc, 1ULL << 63, 0, num);

    LOG_TRACE("uc_emu_start ENDED");
    if (err != UC_ERR_OK) {
        log_error_details(err);
        return -1;
    }

    return 0;
}

int UnicornCPU::run() {
    LOG_TRACE("UNICORN RUN");
    uint32_t pc = get_pc();
    LOG_TRACE("IS THUMB");
    bool thumb_mode = is_thumb_mode();
    did_break = false;
    parent->svc_called = false;

    LOG_TRACE("GET PC");
    pc = get_pc();
    if (thumb_mode) {
        pc |= 1;
    }

    LOG_TRACE("uc_emu_start");
    uc_err err = uc_emu_start(uc.get(), pc, 0, 0, 0);
    if (err != UC_ERR_OK) {
        log_error_details(err);
        return -1;
    }
    LOG_TRACE("GET PC");
    pc = get_pc();

    LOG_TRACE("IS THUMB");
    thumb_mode = is_thumb_mode();
    if (thumb_mode) {
        pc |= 1;
    }

    LOG_TRACE("RETURN HALT");
    return parent->halt_instruction_pc <= pc && pc <= parent->halt_instruction_pc + 4;
}

int UnicornCPU::step() {
    LOG_TRACE("STEP, GET PC");
    uint32_t pc = get_pc();
    LOG_TRACE("IS THUMB");
    bool thumb_mode = is_thumb_mode();

    did_break = false;
    parent->svc_called = false;

    LOG_TRACE("GET PC");
    pc = get_pc();
    if (thumb_mode) {
        pc |= 1;
    }

    LOG_TRACE("uc_emu_start");
    uc_err err = uc_emu_start(uc.get(), pc, 0, 0, 1);

    if (err != UC_ERR_OK) {
        log_error_details(err);
        return -1;
    }
    LOG_TRACE("GET PC");
    pc = get_pc();
    thumb_mode = is_thumb_mode();
    if (thumb_mode) {
        pc |= 1;
    }

    LOG_TRACE("RETURN HALT");
    return parent->halt_instruction_pc <= pc && pc <= parent->halt_instruction_pc + 4;
}

void UnicornCPU::stop() {
    LOG_TRACE("UNICORN STOP");
    const uc_err err = uc_emu_stop(uc.get());
    assert(err == UC_ERR_OK);
}

uint32_t UnicornCPU::get_reg(uint8_t idx) {
    uint32_t value = 0;
    LOG_TRACE("uc_reg_read GET REG");
    const uc_err err = uc_reg_read(uc.get(), UC_ARM_REG_R0 + static_cast<int>(idx), &value);
    assert(err == UC_ERR_OK);

    return value;
}

void UnicornCPU::set_reg(uint8_t idx, uint32_t val) {
    LOG_TRACE("uc_reg_write SET REG");
    const uc_err err = uc_reg_write(uc.get(), UC_ARM_REG_R0 + static_cast<int>(idx), &val);
    assert(err == UC_ERR_OK);
}

uint32_t UnicornCPU::get_sp() {
    uint32_t value = 0;
    LOG_TRACE("uc_reg_read GET SP");
    const uc_err err = uc_reg_read(uc.get(), UC_ARM_REG_SP, &value);
    assert(err == UC_ERR_OK);

    return value;
}

void UnicornCPU::set_sp(uint32_t val) {
    LOG_TRACE("uc_reg_write SET SP");
    const uc_err err = uc_reg_write(uc.get(), UC_ARM_REG_SP, &val);
    assert(err == UC_ERR_OK);
}

uint32_t UnicornCPU::get_pc() {
    LOG_TRACE("GET PC uc_reg_read");
    uint32_t value = 0;
    const uc_err err = uc_reg_read(uc.get(), UC_ARM_REG_PC, &value);
    assert(err == UC_ERR_OK);

    return value;
}

void UnicornCPU::set_pc(uint32_t val) {
    LOG_TRACE("SET PC uc_reg_write");
    const uc_err err = uc_reg_write(uc.get(), UC_ARM_REG_PC, &val);
    assert(err == UC_ERR_OK);
}

uint32_t UnicornCPU::get_lr() {
    LOG_TRACE("GET LR uc_reg_read");
    uint32_t value = 0;
    const uc_err err = uc_reg_read(uc.get(), UC_ARM_REG_LR, &value);
    assert(err == UC_ERR_OK);

    return value;
}

void UnicornCPU::set_lr(uint32_t val) {
    LOG_TRACE("SET LR uc_reg_write");
    const uc_err err = uc_reg_write(uc.get(), UC_ARM_REG_LR, &val);
    assert(err == UC_ERR_OK);
}

uint32_t UnicornCPU::get_cpsr() {
    LOG_TRACE("GET CPSR uc_reg_read");
    uint32_t value = 0;
    const uc_err err = uc_reg_read(uc.get(), UC_ARM_REG_CPSR, &value);
    assert(err == UC_ERR_OK);

    return value;
}

void UnicornCPU::set_cpsr(uint32_t val) {
    LOG_TRACE("SET CPSR uc_reg_write");
    const uc_err err = uc_reg_write(uc.get(), UC_ARM_REG_CPSR, &val);
    assert(err == UC_ERR_OK);
}

uint32_t UnicornCPU::get_tpidruro() {
    LOG_TRACE("GET TPIDRURO uc_reg_read");
    uint32_t value = 0;
    const uc_err err = uc_reg_read(uc.get(), UC_ARM_REG_C13_C0_3, &value);
    assert(err == UC_ERR_OK);

    return value;
}

void UnicornCPU::set_tpidruro(uint32_t val) {
    LOG_TRACE("SET TPIDRURO uc_reg_write");
    const uc_err err = uc_reg_write(uc.get(), UC_ARM_REG_C13_C0_3, &val);
    assert(err == UC_ERR_OK);
}

uint32_t UnicornCPU::get_fpscr() {
    LOG_TRACE("GET FPSCR uc_reg_read");
    uint32_t value = 0;
    const uc_err err = uc_reg_read(uc.get(), UC_ARM_REG_FPSCR, &value);
    assert(err == UC_ERR_OK);

    return value;
}

void UnicornCPU::set_fpscr(uint32_t val) {
    LOG_TRACE("SET FPSCR uc_reg_write");
    const uc_err err = uc_reg_write(uc.get(), UC_ARM_REG_FPSCR, &val);
    assert(err == UC_ERR_OK);
}

float UnicornCPU::get_float_reg(uint8_t idx) {
    LOG_TRACE("GET FLOAT REG uc_reg_read");
    DoubleReg value;

    const int single_index = idx / 2;
    const uc_err err = uc_reg_read(uc.get(), UC_ARM_REG_D0 + single_index, &value);
    assert(err == UC_ERR_OK);
    return value.f[idx % 2];
}

void UnicornCPU::set_float_reg(uint8_t idx, float val) {
    LOG_TRACE("SET FLOAT REG uc_reg_write");
    DoubleReg value;

    const int single_index = idx / 2;
    uc_err err = uc_reg_read(uc.get(), UC_ARM_REG_D0 + single_index, &value);
    assert(err == UC_ERR_OK);
    value.f[idx % 2] = val;

    err = uc_reg_write(uc.get(), UC_ARM_REG_D0 + single_index, &value);
    assert(err == UC_ERR_OK);
}

bool UnicornCPU::is_thumb_mode() {
    LOG_TRACE("IS THUMB MODE uc_query");
    size_t mode = 0;
    const uc_err err = uc_query(uc.get(), UC_QUERY_MODE, &mode);
    assert(err == UC_ERR_OK);

    return mode & UC_MODE_THUMB;
}

CPUContext UnicornCPU::save_context() {
    LOG_TRACE("SAVE CONTEXT");
    CPUContext ctx;
    for (uint8_t i = 0; i < 13; i++) {
        ctx.cpu_registers[i] = get_reg(i);
    }
    ctx.cpu_registers[13] = get_sp();
    ctx.cpu_registers[14] = get_lr();
    ctx.set_pc(is_thumb_mode() ? get_pc() | 1 : get_pc());

    for (auto i = 0; i < ctx.fpu_registers.size(); i++) {
        ctx.fpu_registers[i] = get_float_reg(i);
    }

    // Unicorn doesn't like tweaking cpsr
    // ctx.cpsr = get_cpsr();
    // ctx.fpscr = get_fpscr();

    return ctx;
}

void UnicornCPU::load_context(const CPUContext &ctx) {
    LOG_TRACE("LOAD CONTEXT");
    for (size_t i = 0; i < ctx.fpu_registers.size(); i++) {
        set_float_reg(i, ctx.fpu_registers[i]);
    }

    // Unicorn doesn't like tweaking cpsr
    // set_cpsr(ctx.cpsr);
    // set_fpscr(ctx.fpscr);

    for (uint8_t i = 0; i < 16; i++) {
        set_reg(i, ctx.cpu_registers[i]);
    }
    set_sp(ctx.get_sp());
    set_lr(ctx.get_lr());
    set_pc(ctx.thumb() ? ctx.get_pc() | 1 : ctx.get_pc());
}

void UnicornCPU::invalidate_jit_cache(Address start, size_t length) {
    LOG_TRACE("invalidate_jit_cache");
    uc_ctl_remove_cache(uc.get(), start, start + length);
}

bool UnicornCPU::hit_breakpoint() {
    LOG_TRACE("hit_breakpoint");
    return did_break;
}

void UnicornCPU::trigger_breakpoint() {
    LOG_TRACE("trigger_breakpoint");
    stop();
    did_break = true;
}

void UnicornCPU::set_log_code(bool log) {
    LOG_TRACE("set_log_code");
    if (get_log_code() == log) {
        return;
    }
    if (log) {
        const uc_err err = uc_hook_add(uc.get(), &code_hook_handle, UC_HOOK_CODE, reinterpret_cast<void *>(&code_hook), this, 1, 0);

        assert(err == UC_ERR_OK);
    } else {
        auto err = uc_hook_del(uc.get(), code_hook_handle);
        assert(err == UC_ERR_OK);
        code_hook_handle = 0;
    }
}

void UnicornCPU::set_log_mem(bool log) {
    LOG_TRACE("set_log_mem");
    if (get_log_mem() == log) {
        return;
    }
    if (log) {
        uc_err err = uc_hook_add(uc.get(), &memory_read_hook_handle, UC_HOOK_MEM_READ, reinterpret_cast<void *>(&read_hook), this, 1, 0);
        assert(err == UC_ERR_OK);

        err = uc_hook_add(uc.get(), &memory_write_hook_handle, UC_HOOK_MEM_WRITE, reinterpret_cast<void *>(&write_hook), this, 1, 0);
        assert(err == UC_ERR_OK);
    } else {
        auto err = uc_hook_del(uc.get(), memory_read_hook_handle);
        assert(err == UC_ERR_OK);
        memory_read_hook_handle = 0;

        err = uc_hook_del(uc.get(), memory_write_hook_handle);
        assert(err == UC_ERR_OK);
        memory_write_hook_handle = 0;
    }
}

bool UnicornCPU::get_log_code() {
    LOG_TRACE("get_log_code");
    return code_hook_handle != 0;
}

bool UnicornCPU::get_log_mem() {
    LOG_TRACE("get_log_mem");
    return memory_read_hook_handle != 0 && memory_write_hook_handle != 0;
}
