// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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

#include <mem/functions.h>
#include <mem/state.h>

#include <util/align.h>
#include <util/log.h>

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <mutex>
#include <utility>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#ifdef _MSC_VER
#include <intrin.h>
#pragma intrinsic(_ReturnAddress)
#endif
#else
#include <csignal>
#include <sys/mman.h>
#include <unistd.h>
#endif

constexpr uint32_t STANDARD_PAGE_SIZE = KiB(4);
#ifdef __arm__
constexpr uint32_t GUEST_ADDRESS_SPACE_SIZE = 1ULL << 31; // 2GB
#else
constexpr uint64_t GUEST_ADDRESS_SPACE_SIZE = 1ULL << 32; // 4GB
#endif
constexpr bool LOG_PROTECT = false;
#ifdef NDEBUG
constexpr bool PAGE_NAME_TRACKING = false;
#else
constexpr bool PAGE_NAME_TRACKING = true;
#endif

// TODO: support multiple handlers
static AccessViolationHandler access_violation_handler;
static void register_access_violation_handler(const AccessViolationHandler &handler);

static Address alloc_inner(MemState &state, uint32_t start_page, uint32_t page_count, const char *name, const bool force);
static void delete_memory(uint8_t *memory);

#ifdef _WIN32
std::string get_error_msg() {
    return std::system_category().message(GetLastError());
}
#else
std::string get_error_msg() {
    return strerror(errno);
}
#endif

namespace {
size_t mirror_size_bytes() {
    return sizeof(void *) >= sizeof(uint64_t) ? static_cast<size_t>(GUEST_ADDRESS_SPACE_SIZE) : 0;
}

PagePtr canonical_page_base(const MemState &state, Address guest_addr) {
    if (state.backing_mode == MemBackingMode::DirectMirror) {
        return state.memory.get();
    }

    const Address chunk_start = align_down(guest_addr, state.host_page_size);
    const auto mapping = state.guest_mappings.find(chunk_start);
    if (mapping == state.guest_mappings.end()) {
        LOG_ERROR("guest_mappings: can't align down!");
        return nullptr;
    }

    return mapping->second.host_ptr - chunk_start;
}

uint8_t *canonical_host_ptr(const MemState &state, Address guest_addr) {
    const PagePtr base = canonical_page_base(state, guest_addr);
    return base ? (base + guest_addr) : nullptr;
    return base ? base : nullptr;
}

template <typename Fn>
void for_each_guest_host_chunk(const MemState &state, Address addr, uint32_t size, Fn &&fn) {
    const Address start = align_down(addr, state.host_page_size);
    const Address end = align(addr + size, state.host_page_size);
    for (Address chunk = start; chunk < end; chunk += state.host_page_size) {
        fn(chunk);
    }
}

template <typename Fn>
void for_each_active_host_page(MemState &state, Address addr, uint32_t size, Fn &&fn) {
    Address page_start = align_down(addr, STANDARD_PAGE_SIZE);
    const Address page_end = align(addr + size, STANDARD_PAGE_SIZE);
    uint8_t *last_host_page = nullptr;

    while (page_start < page_end) {
        const PagePtr active_page_base = state.page_table[page_start / STANDARD_PAGE_SIZE];
        if (active_page_base) {
            uint8_t *const page_ptr = active_page_base + page_start;
            uint8_t *const host_page = reinterpret_cast<uint8_t *>(
                align_down(reinterpret_cast<uintptr_t>(page_ptr), state.host_page_size));
            if (host_page != last_host_page) {
                fn(host_page, state.host_page_size);
                last_host_page = host_page;
            }
        }

        page_start += STANDARD_PAGE_SIZE;
    }
}

auto find_host_mapping(MemState &state, const HostAddress host_addr) {
    auto mapping = state.host_mappings.upper_bound(host_addr);
    if (mapping == state.host_mappings.begin()) {
        LOG_INFO("find_host_mapping = state.host_mappings.end");
        return state.host_mappings.end();
    }

    --mapping;
    if (host_addr < mapping->first + mapping->second.size) {
        return mapping;
    }

    LOG_ERROR("find_host_mapping = not found!, set as state.host_mappings.end");
    state.use_page_table = false;
    
    return state.host_mappings.end();
}

auto find_external_mapping(MemState &state, Address guest_addr) {
    return std::find_if(state.external_mapping.begin(), state.external_mapping.end(), [guest_addr](const auto &entry) {
        const MemExternalMapping &mapping = entry.second;
        return mapping.address == guest_addr;
    });
}

bool is_host_chunk_free(const MemState &state, Address chunk_start) {
    const uint32_t first_guest = chunk_start / STANDARD_PAGE_SIZE;
    const uint32_t last_guest = (chunk_start + state.host_page_size) / STANDARD_PAGE_SIZE;
    return state.allocator.free_slot_count(first_guest, last_guest) == (last_guest - first_guest);
}

void apply_page_table_range(MemState &state, Address addr, uint32_t size, PagePtr base) {
    for (Address page_addr = addr; page_addr < addr + size; page_addr += STANDARD_PAGE_SIZE) {
        state.page_table[page_addr / STANDARD_PAGE_SIZE] = base;
    }
}

void restore_canonical_page_table_range(MemState &state, Address addr, uint32_t size) {
    for (Address page_addr = addr; page_addr < addr + size; page_addr += STANDARD_PAGE_SIZE) {
        state.page_table[page_addr / STANDARD_PAGE_SIZE] = canonical_page_base(state, page_addr);
    }
}

void clear_protect_range_locked(MemState &state, Address addr, uint32_t size) {
    auto protect_it = state.protect_tree.lower_bound(addr);
    if (protect_it == state.protect_tree.end() || protect_it->first + protect_it->second.size <= addr) {
        if (protect_it == state.protect_tree.begin()) {
            return;
        }
        --protect_it;
    }

    while (protect_it != state.protect_tree.end() && protect_it->first < addr + size) {
        if (protect_it == state.protect_tree.begin()) {
            state.protect_tree.erase(protect_it);
            break;
        }

        state.protect_tree.erase(protect_it--);
    }
}

void clear_guest_protect_range(MemState &mem, Address addr, uint32_t size) {
    unprotect_inner(mem, addr, size);

    {
        const std::unique_lock<std::mutex> lock(mem.protect_mutex);
        clear_protect_range_locked(mem, addr, size);
    }
}

uintptr_t caller_address() {
#ifdef _MSC_VER
    return reinterpret_cast<uintptr_t>(_ReturnAddress());
#else
    return reinterpret_cast<uintptr_t>(__builtin_return_address(0));
   // return 0;
#endif
}

#ifdef _WIN32
bool protect_host_memory(uint8_t *memory, size_t size, DWORD protection) {
    DWORD old_protect = 0;
    const BOOL ret = VirtualProtect(memory, size, protection, &old_protect);
    LOG_CRITICAL_IF(!ret, "VirtualProtect failed: {}", std::system_category().message(GetLastError()));
    return ret != 0;
}

uint8_t *map_sparse_chunk(uint32_t size) {
       return static_cast<uint8_t *>(VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
}

void unmap_sparse_chunk(uint8_t *memory, uint32_t size) {
    (void)size;
    const BOOL ret = VirtualFree(memory, 0, MEM_RELEASE);
    LOG_CRITICAL_IF(!ret, "VirtualFree failed: {}", std::system_category().message(GetLastError()));
}

bool commit_direct_chunk(MemState &state, Address chunk_start) {
    uint8_t *const chunk_ptr = state.memory.get() + chunk_start;
    const void *const ret = VirtualAlloc(chunk_ptr, state.host_page_size, MEM_COMMIT, PAGE_READWRITE);
    LOG_CRITICAL_IF(!ret, "VirtualAlloc failed: {}", std::system_category().message(GetLastError()));
    return ret != nullptr;
}

void decommit_direct_chunk(MemState &state, Address chunk_start) {
    uint8_t *const chunk_ptr = state.memory.get() + chunk_start;
    const BOOL ret = VirtualFree(chunk_ptr, state.host_page_size, MEM_DECOMMIT);
    LOG_CRITICAL_IF(!ret, "VirtualFree failed: {}", std::system_category().message(GetLastError()));
}
#else
bool protect_host_memory(uint8_t *memory, size_t size, int protection) {
    const int ret = mprotect(memory, size, protection);
    LOG_CRITICAL_IF(ret == -1, "mprotect failed: {}", strerror(errno));
    return ret != -1;
}

uint8_t *map_sparse_chunk(uint32_t size) {
    const int fd = -1;
    const off_t offset = 0;
    const int prot = PROT_READ | PROT_WRITE;
    const int flags = MAP_PRIVATE | MAP_ANONYMOUS;
    
#ifndef __arm__
    void *preferred_address = reinterpret_cast<void *>(1ULL << 34);
    void *const mapping = mmap(preferred_address, size, prot, flags, fd, offset);
#else
    void *const mapping = mmap(nullptr, size, prot, flags, fd, offset);
#endif
    if (mapping == MAP_FAILED)
        LOG_CRITICAL("mmap failed {}", get_error_msg());
    
    return mapping == MAP_FAILED ? nullptr : static_cast<uint8_t *>(mapping);
}

void unmap_sparse_chunk(uint8_t *memory, uint32_t size) {
    const int ret = munmap(memory, size);
    LOG_CRITICAL_IF(ret == -1, "munmap failed: {}", get_error_msg());
}

bool commit_direct_chunk(MemState &state, Address chunk_start) {
    uint8_t *const chunk_ptr = state.memory.get() + chunk_start;
    return protect_host_memory(chunk_ptr, state.host_page_size, PROT_READ | PROT_WRITE);
}

void decommit_direct_chunk(MemState &state, Address chunk_start) {
    uint8_t *const chunk_ptr = state.memory.get() + chunk_start;
    int ret = mprotect(chunk_ptr, state.host_page_size, PROT_NONE);
    LOG_CRITICAL_IF(ret == -1, "mprotect failed: {}", get_error_msg());
    ret = madvise(chunk_ptr, state.host_page_size, MADV_DONTNEED);
    LOG_CRITICAL_IF(ret == -1, "madvise failed: {}", get_error_msg());
}
#endif

bool map_guest_chunk(MemState &state, Address chunk_start) {
    if (state.backing_mode == MemBackingMode::DirectMirror) {
        return commit_direct_chunk(state, chunk_start);
    }

    if (state.guest_mappings.find(chunk_start) != state.guest_mappings.end()) {
        return true;
    }

    uint8_t *const host_ptr = map_sparse_chunk(state.host_page_size);
    if (!host_ptr) {
        LOG_CRITICAL("Failed to map sparse guest chunk @ {}", log_hex(chunk_start));
        return false;
    }

    const MemGuestHostMapping mapping { chunk_start, state.host_page_size, host_ptr, false };
    state.guest_mappings.emplace(chunk_start, mapping);
    state.host_mappings.emplace(reinterpret_cast<HostAddress>(host_ptr), mapping);
    apply_page_table_range(state, chunk_start, state.host_page_size, host_ptr - chunk_start);
    return true;
}

void unmap_guest_chunk(MemState &state, Address chunk_start) {
    if (state.backing_mode == MemBackingMode::DirectMirror) {
        decommit_direct_chunk(state, chunk_start);
        return;
    }

    const auto mapping = state.guest_mappings.find(chunk_start);
    if (mapping == state.guest_mappings.end()) {
        return;
    }

    state.host_mappings.erase(reinterpret_cast<HostAddress>(mapping->second.host_ptr));
    unmap_sparse_chunk(mapping->second.host_ptr, mapping->second.size);
    state.guest_mappings.erase(mapping);
    apply_page_table_range(state, chunk_start, state.host_page_size, nullptr);
}

bool try_reserve_direct_mirror(MemState &state) {
    if (mirror_size_bytes() == 0) {
        return false;
    }

    void *preferred_address = reinterpret_cast<void *>(1ULL << 34);

#ifdef _WIN32
    uint8_t *memory = static_cast<uint8_t *>(VirtualAlloc(preferred_address, mirror_size_bytes(), MEM_RESERVE, PAGE_NOACCESS));
    if (!memory) {
        memory = static_cast<uint8_t *>(VirtualAlloc(nullptr, mirror_size_bytes(), MEM_RESERVE, PAGE_NOACCESS));
    }
    if (!memory) {
        return false;
    }
    state.memory = Memory(memory, delete_memory);
#else
    const int prot = PROT_NONE;
    const int flags = MAP_PRIVATE | MAP_ANONYMOUS;
    // const int fd = -1;
    const int fd = 0;
    const off_t offset = 0;
    void *memory = nullptr;
    memory = mmap(preferred_address, mirror_size_bytes(), prot, flags, fd, offset);
    if (memory == MAP_FAILED) {
        LOG_CRITICAL("mmap failed {}, retry...", get_error_msg());
        memory = mmap(nullptr, mirror_size_bytes(), prot, flags, fd, offset);
    }
    if (memory == MAP_FAILED) {
        LOG_CRITICAL("mmap failed {}", get_error_msg());
        return false;
    } else {
        LOG_CRITICAL("mmap status {}", get_error_msg());
    }
        
    state.memory = Memory(static_cast<uint8_t *>(memory), delete_memory);
#endif

    state.backing_mode = MemBackingMode::DirectMirror;
// #ifndef __arm__
//    std::fill_n(state.page_table.get(), GUEST_PAGE_COUNT, state.memory.get());
// #endif
    return true;
}
} // namespace

bool init(MemState &state, const bool use_page_table) {
#ifdef _WIN32
    SYSTEM_INFO system_info = {};
    GetSystemInfo(&system_info);
    state.host_page_size = system_info.dwPageSize;
#else
    state.host_page_size = static_cast<int>(sysconf(_SC_PAGESIZE));
#endif
    auto tmp = static_cast<int>(sysconf(_SC_PHYS_PAGES))/KiB(1);
    const size_t GUEST_PAGE_COUNT = GUEST_ADDRESS_SPACE_SIZE / state.host_page_size;
    
    LOG_DEBUG("physical_size = {} KB", tmp);
    LOG_DEBUG("host_page_size = {} KB", state.host_page_size/KiB(1));
    LOG_DEBUG("memory_real = {} KB", state.host_page_size/KiB(1) * tmp);
    LOG_DEBUG("GUEST_PAGE_COUNT = {} KB", GUEST_PAGE_COUNT/KiB(1));
 
    assert(state.host_page_size >= STANDARD_PAGE_SIZE);
    assert((state.host_page_size % STANDARD_PAGE_SIZE) == 0);

    state.alloc_table = AllocPageTable(new AllocMemPage[GUEST_PAGE_COUNT]);
    memset(state.alloc_table.get(), 0, sizeof(AllocMemPage) * GUEST_PAGE_COUNT);
    state.allocator.set_maximum(GUEST_PAGE_COUNT);

    state.use_page_table = use_page_table;
    if (!try_reserve_direct_mirror(state)) {
        LOG_INFO_ONCE("MemBackingMode::SparseMappings");
        state.backing_mode = MemBackingMode::SparseMappings;
    } else
        LOG_INFO_ONCE("MemBackingMode::directMirror");

    const auto handler = [&state](uint8_t *addr, bool write) noexcept {
        return handle_access_violation(state, addr, write);
    };
    register_access_violation_handler(handler);

    const Address null_address = alloc_inner(state, 0, state.host_page_size / STANDARD_PAGE_SIZE, "null", true);
    assert(null_address == 0);
    
#ifndef ANDROID
    protect_inner(state, 0, state.host_page_size, MemPerm::None);
#endif
    
    if (use_page_table) {
       state.page_table = PageTable(new PagePtr[GUEST_PAGE_COUNT]);
       // std::fill_n(state.page_table.get(), GUEST_PAGE_COUNT, nullptr);
       std::fill_n(state.page_table.get(), GUEST_PAGE_COUNT, state.memory.get());
    }
    
    return true;
}

static void delete_memory(uint8_t *memory) {
    if (memory != nullptr) {
#ifdef _WIN32
        const BOOL ret = VirtualFree(memory, 0, MEM_RELEASE);
        assert(ret);
#else
        const int ret = munmap(memory, mirror_size_bytes());
        assert(ret == 0);
#endif
    }
}

bool is_valid_addr(const MemState &state, Address addr) {
    const uint32_t page_num = addr / STANDARD_PAGE_SIZE;
    return addr && state.allocator.free_slot_count(page_num, page_num + 1) == 0;
}

bool is_valid_addr_range(const MemState &state, Address start, Address end) {
    const uint32_t start_page = start / STANDARD_PAGE_SIZE;
    const uint32_t end_page = (end + STANDARD_PAGE_SIZE - 1) / STANDARD_PAGE_SIZE;
    return state.allocator.free_slot_count(start_page, end_page) == 0;
}

static Address alloc_inner(MemState &state, uint32_t start_page, uint32_t page_count, const char *name, const bool force) {
    int page_num;
    if (force) {
        if (state.allocator.allocate_at(start_page, page_count) < 0) {
            LOG_CRITICAL("Failed to allocate at specific page");
            return 0;
        }
        page_num = start_page;
    } else {
        page_num = state.allocator.allocate_from(start_page, page_count, false);
        if (page_num < 0)
            return 0;
    }

    const uint32_t size = page_count * STANDARD_PAGE_SIZE;
    const Address addr = page_num * STANDARD_PAGE_SIZE;
    std::vector<Address> newly_mapped_chunks;

    for_each_guest_host_chunk(state, addr, size, [&](Address chunk_start) {
        const bool chunk_was_unmapped = state.backing_mode == MemBackingMode::SparseMappings
            && (state.guest_mappings.find(chunk_start) == state.guest_mappings.end());

    if (!map_guest_chunk(state, chunk_start)) {
            page_num = -1;
        } else if (chunk_was_unmapped) {
            newly_mapped_chunks.push_back(chunk_start);
        }
    });
    if (page_num < 0) {
        state.allocator.free(start_page, page_count);
        for (Address chunk_start : newly_mapped_chunks) {
            unmap_guest_chunk(state, chunk_start);
        }
        return 0;
    }

    uint8_t *const host_ptr = canonical_host_ptr(state, addr);
    assert(host_ptr != nullptr);
    std::memset(host_ptr, 0, size);
    AllocMemPage &page = state.alloc_table[page_num];
    assert(!page.allocated);
    page.allocated = 1;
    page.size = page_count;

    if (PAGE_NAME_TRACKING) {
        state.page_name_map.emplace(page_num, name);
    }

    return addr;
}

Address alloc_aligned(MemState &state, uint32_t size, const char *name, unsigned int alignment, Address start_addr) {
    if (alignment == 0)
        return alloc(state, size, name, start_addr);
    const std::lock_guard<std::mutex> lock(state.generation_mutex);
    size += alignment;
    const uint32_t page_count = align(size, STANDARD_PAGE_SIZE) / STANDARD_PAGE_SIZE;
    const Address addr = alloc_inner(state, start_addr / STANDARD_PAGE_SIZE, page_count, name, false);
    const Address align_addr = align(addr, alignment);
    const uint32_t page_num = addr / STANDARD_PAGE_SIZE;
    const uint32_t align_page_num = align_addr / STANDARD_PAGE_SIZE;

    if (page_num != align_page_num) {
        AllocMemPage &page = state.alloc_table[page_num];
        AllocMemPage &align_page = state.alloc_table[align_page_num];
        const uint32_t remnant_front = align_page_num - page_num;
        state.allocator.free(page_num, remnant_front);
        page.allocated = 0;
        align_page.allocated = 1;
        align_page.size = page.size - remnant_front;
    }

    return align_addr;
}

static void align_to_page(MemState &state, Address &addr, Address &size) {
    const Address end = align(addr + size, STANDARD_PAGE_SIZE);
    addr = align_down(addr, STANDARD_PAGE_SIZE);
    size = end - addr;
}

void unprotect_inner(MemState &state, Address addr, uint32_t size) {
    if (LOG_PROTECT) {
        fmt::print("Unprotect: {} {}\n", log_hex(addr), size);
    }
    for_each_active_host_page(state, addr, size, [&](uint8_t *host_page, uint32_t host_page_size) {

#ifdef _WIN32
    protect_host_memory(host_page, host_page_size, PAGE_READWRITE);
#else
    protect_host_memory(host_page, host_page_size, PROT_READ | PROT_WRITE);
#endif
    });
}

void protect_inner(MemState &state, Address addr, uint32_t size, const MemPerm perm) {
    for_each_active_host_page(state, addr, size, [&](uint8_t *host_page, uint32_t host_page_size) {

#ifdef _WIN32
    protect_host_memory(host_page, host_page_size, (perm == MemPerm::None) ? PAGE_NOACCESS : ((perm == MemPerm::ReadOnly) ? PAGE_READONLY : PAGE_READWRITE));
#else
    protect_host_memory(host_page, host_page_size, (perm == MemPerm::None) ? PROT_NONE : ((perm == MemPerm::ReadOnly) ? PROT_READ : (PROT_READ | PROT_WRITE)));
#endif
    });
}

bool handle_access_violation(MemState &state, uint8_t *addr, bool write) noexcept {
    Address vaddr = 0;
#if !defined(__aarch64__ ) || !defined(__x86_64__)
    const HostAddress fault_addr = reinterpret_cast<HostAddress>(addr);
#else
    const HostAddress fault_addr = std::bit_cast<HostAddress>(addr);
#endif

    const std::unique_lock<std::mutex> lock(state.protect_mutex);
    if (state.backing_mode == MemBackingMode::DirectMirror && state.memory) {
#if !defined(__aarch64__ ) || !defined(__x86_64__)
        const HostAddress memory_addr = reinterpret_cast<HostAddress>(state.memory.get());
#else
        const HostAddress memory_addr = std::bit_cast<HostAddress>(state.memory.get());
#endif
        if (fault_addr >= memory_addr && fault_addr < memory_addr + mirror_size_bytes()) {
            vaddr = static_cast<Address>(fault_addr - memory_addr);
        } else {
            const auto mapping = find_host_mapping(state, fault_addr);
            if (mapping == state.host_mappings.end()) {
                return false;
            }

            vaddr = static_cast<Address>(mapping->second.address + (fault_addr - mapping->first));
        }
    } else {
        const auto mapping = find_host_mapping(state, fault_addr);
        if (mapping == state.host_mappings.end()) {
            return false;
        }

        vaddr = static_cast<Address>(mapping->second.address + (fault_addr - mapping->first));
    }

    if (!is_valid_addr(state, vaddr)) {
        return false;
    }
    if (LOG_PROTECT) {
        fmt::print("Access: {}\n", log_hex(vaddr));
    }

    auto protect_it = state.protect_tree.lower_bound(vaddr);
    if (protect_it == state.protect_tree.end()) {
        unprotect_inner(state, align_down(vaddr, state.host_page_size), state.host_page_size);
        LOG_CRITICAL("Unhandled write protected region was valid. Address=0x{:X}", vaddr);
        return true;
    }

    ProtectSegmentInfo &info = protect_it->second;
    if (vaddr < protect_it->first || vaddr >= protect_it->first + info.size) {
        unprotect_inner(state, align_down(vaddr, state.host_page_size), state.host_page_size);
        LOG_CRITICAL("Unhandled write protected region was valid. Address=0x{:X}", vaddr);
        return true;
    }

    for (auto &[block_addr, block] : info.blocks) {
        block.callback(vaddr, write);
    }

    unprotect_inner(state, protect_it->first, info.size);
    state.protect_tree.erase(protect_it);

    return true;
}

bool add_protect(MemState &state, Address addr, const uint32_t size, const MemPerm perm, const ProtectCallback &callback) {
    const std::lock_guard<std::mutex> lock(state.protect_mutex);
    ProtectSegmentInfo protect(size, perm);
    align_to_page(state, addr, protect.size);

    ProtectBlockInfo block;
    block.size = size;
    block.callback = callback;

    protect.blocks.emplace(addr, std::move(block));

    auto it = state.protect_tree.lower_bound(addr);
    if (it == state.protect_tree.end() || it->first + it->second.size <= addr) {
        if (it == state.protect_tree.begin()) {
            it = state.protect_tree.end();
        } else {
            --it;
        }
    }

    while (it != state.protect_tree.end() && it->first < addr + size) {
        const Address start = std::min(it->first, addr);
        protect.size = std::max(it->first + it->second.size, addr + protect.size) - start;
        addr = start;
        protect.blocks.merge(it->second.blocks);
        protect.perm = most_restrictive_perm(protect.perm, it->second.perm);

        if (it == state.protect_tree.begin()) {
            state.protect_tree.erase(it);
            break;
        }

        state.protect_tree.erase(it--);
    }

    protect_inner(state, addr, protect.size, protect.perm);
    state.protect_tree.emplace(addr, std::move(protect));
    return true;
}

bool is_protecting(MemState &state, Address addr, MemPerm *perm) {
    const std::lock_guard<std::mutex> lock(state.protect_mutex);
    auto ite = state.protect_tree.lower_bound(addr);

    if (ite != state.protect_tree.end() && addr < ite->first + ite->second.size) {
        if (perm)
            *perm = ite->second.perm;

        return true;
    }

    return false;
}

void add_external_mapping(MemState &mem, Address addr, uint32_t size, uint8_t *addr_ptr) {
    assert((size & 4095) == 0);

    for (uint32_t block = 0; block < size / KiB(4); block++) {
        uint8_t *const original_address = canonical_host_ptr(mem, addr + block * KiB(4));
        assert(original_address != nullptr);
        memcpy(addr_ptr + block * KiB(4), original_address, KiB(4));
    }
        
    apply_page_table_range(mem, addr, size, addr_ptr - addr);
    protect_inner(mem, addr, size, MemPerm::None);

    const MemGuestHostMapping mapping { addr, size, addr_ptr, true };
    mem.host_mappings[reinterpret_cast<HostAddress>(addr_ptr)] = mapping;
    mem.external_mapping[reinterpret_cast<HostAddress>(addr_ptr)] = { addr, size };
}

void remove_external_mapping(MemState &mem, Address addr, uint32_t size) {
    const auto mapping_it = find_external_mapping(mem, addr);
    if (mapping_it == mem.external_mapping.end()) {
    LOG_INFO("clear_guest_protect_range");
        // Some mapping modes, like double-buffer trapping, only use guest protections and never install an external host mapping.
        clear_guest_protect_range(mem, addr, size);
        return;
    }

    const MemExternalMapping mapping = mapping_it->second;
    LOG_ERROR_IF(mapping.size != size, "External mapping size mismatch while removing guest address {} (expected {}, got {})", log_hex(addr), mapping.size, size);
    uint8_t *const addr_ptr = reinterpret_cast<uint8_t *>(mapping_it->first);
    clear_guest_protect_range(mem, mapping.address, mapping.size);

    restore_canonical_page_table_range(mem, mapping.address, mapping.size);
    unprotect_inner(mem, mapping.address, mapping.size);

    for (uint32_t block = 0; block < mapping.size / KiB(4); block++) {
        uint8_t *const destination = canonical_host_ptr(mem, mapping.address + block * KiB(4));
        assert(destination != nullptr);
        memcpy(destination, addr_ptr + block * KiB(4), KiB(4));
    }

    mem.external_mapping.erase(mapping_it);
    mem.host_mappings.erase(reinterpret_cast<HostAddress>(addr_ptr));
}

Address alloc(MemState &state, uint32_t size, const char *name, Address start_addr) {
    const std::lock_guard<std::mutex> lock(state.generation_mutex);
    const uint32_t page_count = align(size, STANDARD_PAGE_SIZE) / STANDARD_PAGE_SIZE;
    return alloc_inner(state, start_addr / STANDARD_PAGE_SIZE, page_count, name, false);
}

Address alloc_at(MemState &state, Address address, uint32_t size, const char *name) {
    auto addr = try_alloc_at(state, address, size, name);
    LOG_CRITICAL_IF(addr == 0, "Failed to allocate at specific page. Memory address:{}, size:{}, name:{}", log_hex(address), log_hex(size), name);
    return addr;
}

Address try_alloc_at(MemState &state, Address address, uint32_t size, const char *name) {
    const std::lock_guard<std::mutex> lock(state.generation_mutex);
    const uint32_t wanted_page = address / STANDARD_PAGE_SIZE;
    size += address % STANDARD_PAGE_SIZE;
    const uint32_t page_count = align(size, STANDARD_PAGE_SIZE) / STANDARD_PAGE_SIZE;
    const Address addr = alloc_inner(state, wanted_page, page_count, name, true);
    return addr ? address : 0;
}

Block alloc_block(MemState &mem, uint32_t size, const char *name, Address start_addr) {
    const Address address = alloc(mem, size, name, start_addr);
    return Block(address, [&mem](Address stack) {
        free(mem, stack);
    });
}

void free(MemState &state, Address address) {
    const std::lock_guard<std::mutex> lock(state.generation_mutex);

    if (!state.alloc_table) {
        LOG_ERROR("free called after mem teardown for address {} (caller 0x{:X})", log_hex(address), caller_address());
        return;
    }

    const uint32_t page_num = address / STANDARD_PAGE_SIZE;
    if (page_num >= GUEST_PAGE_COUNT) {
        LOG_ERROR("free called with out-of-range address {} (caller 0x{:X})", log_hex(address), caller_address());
        return;
    }

    AllocMemPage &page = state.alloc_table[page_num];
    if (!page.allocated) {
        LOG_CRITICAL("Freeing unallocated page");
    }
    page.allocated = 0;

    state.allocator.free(page_num, page.size);
    if (PAGE_NAME_TRACKING) {
        state.page_name_map.erase(page_num);
    }

    const Address region_start = page_num * STANDARD_PAGE_SIZE;
    const Address region_end = region_start + page.size * STANDARD_PAGE_SIZE;

    for_each_guest_host_chunk(state, region_start, region_end - region_start, [&](Address chunk_start) {
        if (is_host_chunk_free(state, chunk_start)) {
            unmap_guest_chunk(state, chunk_start);
        }
    });
}

uint32_t mem_available(MemState &state) {
    return state.allocator.free_slot_count(0, state.allocator.max_offset) * STANDARD_PAGE_SIZE;
}

const char *mem_name(Address address, MemState &state) {
    if (PAGE_NAME_TRACKING) {
        auto page_name = state.page_name_map.find(address / STANDARD_PAGE_SIZE);
        return page_name != state.page_name_map.end() ? page_name->second.c_str() : "";
    }
    return "";
}

void deinit_mem(MemState &state) {
    const std::lock_guard<std::mutex> gen_lock(state.generation_mutex);

    {
        const std::lock_guard<std::mutex> prot_lock(state.protect_mutex);
        state.protect_tree.clear();
    }

    if (state.backing_mode == MemBackingMode::SparseMappings) {
        for (const auto &[guest_addr, mapping] : state.guest_mappings) {
            unmap_sparse_chunk(mapping.host_ptr, mapping.size);
        }
    }

    state.memory.reset();
    state.alloc_table.reset();
    state.allocator.reset();
    state.page_name_map.clear();
    state.page_table.reset();
    state.guest_mappings.clear();
    state.host_mappings.clear();
    state.external_mapping.clear();
    state.use_page_table = false;
    state.backing_mode = MemBackingMode::DirectMirror;
    state.host_page_size = 0;
}

#ifdef _WIN32 // ifdef 1

static LONG WINAPI exception_handler(PEXCEPTION_POINTERS pExp) noexcept {
    if (pExp->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT && IsDebuggerPresent()) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    const auto ptr = reinterpret_cast<uint8_t *>(pExp->ExceptionRecord->ExceptionInformation[1]);
    const bool is_writing = pExp->ExceptionRecord->ExceptionInformation[0] == 1;
    const bool is_executing = pExp->ExceptionRecord->ExceptionInformation[0] == 8;

    if (pExp->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && !is_executing) {
        if (access_violation_handler(ptr, is_writing)) {
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

static void register_access_violation_handler(const AccessViolationHandler &handler) {
    access_violation_handler = handler;
    if (!AddVectoredExceptionHandler(1, exception_handler)) {
        LOG_CRITICAL("Failed to register an exception handler");
    }
}

#else // ifdef 1

static void signal_handler(int sig, siginfo_t *info, void *uct) noexcept {
    auto context = static_cast<ucontext_t *>(uct);

#ifdef __arm__ // ifdef 2
    // TODO: handle ARM exceptions
    const bool is_executing = false;
    const bool is_writing = false;
    LOG_CRITICAL("Unhandled ARM access violation at {}", log_hex(reinterpret_cast<uintptr_t>(info->si_addr)));
    raise(SIGTRAP);
    return;
#elif __aarch64__ // ifdef 2
#ifdef __APPLE__
    const uint32_t esr = context->uc_mcontext->__es.__esr;
#else
    _aarch64_ctx *ctx = reinterpret_cast<_aarch64_ctx *>(context->uc_mcontext.__reserved);
    // get the ESR register
    while (ctx->magic != ESR_MAGIC) {
        if (ctx->magic == 0) {
            [[unlikely]]
            raise(SIGTRAP);
        } else {
            [[likely]]
            ctx = reinterpret_cast<_aarch64_ctx *>(reinterpret_cast<uint8_t *>(ctx) + ctx->size);
        }
    }

    const uint64_t esr = reinterpret_cast<esr_context *>(ctx)->esr;
#endif
    // https://developer.arm.com/documentation/ddi0595/2021-03/AArch64-Registers/ESR-EL1--Exception-Syndrome-Register--EL1-
    const uint32_t exception_class = static_cast<uint32_t>(esr) >> 26;
    const bool is_executing = (exception_class == 0b100000) || (exception_class == 0b100001);
    const bool is_data_abort = (exception_class == 0b100100) || (exception_class == 0b100101);
    const bool is_writing = is_data_abort && (esr & (1 << 6));
#else
#ifdef __APPLE__
    const uint64_t err = context->uc_mcontext->__es.__err;
#else
    const uint64_t err = context->uc_mcontext.gregs[REG_ERR];
#endif
    const bool is_executing = err & 0x10;
    const bool is_writing = err & 0x2;
#endif

    if (!is_executing) {
        if (access_violation_handler(reinterpret_cast<uint8_t *>(info->si_addr), is_writing)) {
            return;
        }
    }

    LOG_CRITICAL("Unhandled access to 0x{:X}", reinterpret_cast<uintptr_t>(info->si_addr));
    raise(SIGTRAP);
    // return;
}

static void register_access_violation_handler(const AccessViolationHandler &handler) {
    access_violation_handler = handler;
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = signal_handler;
    if (sigaction(SIGSEGV, &sa, NULL) == -1) {
        LOG_CRITICAL("Failed to register an exception handler");
    }
#ifdef __APPLE__
    // When accessing memory region which is PROT_NONE on macOS, it is raising SIGBUS not SIGSEGV.
    // So apply same signal handler to SIGBUS
    if (sigaction(SIGBUS, &sa, NULL) == -1) {
        LOG_CRITICAL("Failed to register an exception handler to SIGBUS");
    }
#endif
}

#endif
