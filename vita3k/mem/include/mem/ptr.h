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

#pragma once

#include <mem/atomic.h>
#include <mem/functions.h>
#include <mem/state.h>

#include <cassert>
#include <cstdint>
#include <limits>
#include <type_traits>

template <class T>
class Ptr {
public:
    Ptr() = default;

    explicit Ptr(Address address)
        : addr(address) {
    }

    template <class U>
    Ptr(const Ptr<U> &other)
        : addr(other.address()) {
        static_assert(std::is_convertible_v<U *, T *>, "Ptr is not convertible.");
    }

    Address address() const {
        return addr;
    }

    template <class U>
    Ptr<U> cast() const {
        return Ptr<U>(addr);
    }

    T *get(const MemState &mem) const {
        if (addr == 0) {
            return nullptr;
        } else {
            const PagePtr page_base = mem.page_table ? mem.page_table[addr / KiB(4)] : mem.memory.get();
            return page_base ? reinterpret_cast<T *>(page_base + addr) : nullptr;
        }
    }

    template <class U>
    bool atomic_compare_and_swap(MemState &mem, U value, U expected) {
        static_assert(std::is_arithmetic_v<U>);
        static_assert(std::is_same_v<U, T>);
        uint8_t *mem_ptr = mem.page_table ? mem.page_table[addr / KiB(4)] : mem.memory.get();
        assert(mem_ptr != nullptr);
        const auto ptr = reinterpret_cast<volatile U *>(&mem_ptr[addr]);
        return ::atomic_compare_and_swap(ptr, value, expected);
    }

    bool valid(const MemState &mem) const {
        return is_valid_addr(mem, addr);
    }

    void reset() {
        addr = 0;
    }

    explicit operator bool() const {
        return addr != 0;
    }

    Ptr &operator=(const Address &address) {
        addr = address;
        return *this;
    }

    Ptr &operator=(std::nullptr_t) {
        addr = 0;
        return *this;
    }

private:
    Address addr{};
};

static_assert(sizeof(Ptr<const void>) == 4, "Size of Ptr isn't 4 bytes.");

template <class T>
Ptr<T> operator+(const Ptr<T> &base, int32_t offset) {
    return Ptr<T>(base.address() + (offset * sizeof(T)));
}

template <class T, class U>
bool operator<(const Ptr<T> &a, const Ptr<U> &b) {
    return a.address() < b.address();
}

template <class T>
bool operator==(const Ptr<T> &a, const Ptr<T> &b) {
    return a.address() == b.address();
}

template <class T, class U>
// Derive a guest subpointer from a known guest/host base pair for the same object or range.
Ptr<T> guest_subptr(const Ptr<U> &guest_base, const void *host_base, const T *host_ptr) {
    if (!host_ptr) {
        return Ptr<T>{};
    }

    assert(host_base != nullptr);

    const auto host_base_addr = reinterpret_cast<std::uintptr_t>(host_base);
    const auto host_ptr_addr = reinterpret_cast<std::uintptr_t>(host_ptr);
    assert(host_ptr_addr >= host_base_addr);

    const auto offset = host_ptr_addr - host_base_addr;
    assert(offset <= std::numeric_limits<Address>::max() - guest_base.address());

    return Ptr<T>(guest_base.address() + static_cast<Address>(offset));
}

template <class T>
Ptr<T> alloc(MemState &mem, const char *name) {
    const Address address = alloc(mem, sizeof(T), name);
    const Ptr<T> ptr(address);
    if (!ptr) {
        return ptr;
    }

    T *const memory = ptr.get(mem);
    new (memory) T;

    return ptr;
}

template <class T>
void free(MemState &mem, const Ptr<T> &ptr) {
    ptr.get(mem)->~T();
    free(mem, ptr.address());
}
