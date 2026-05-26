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

#include <module/module.h>

#include <cstring>
#include <string.h>

EXPORT(int, __aeabi_idiv) {
    return UNIMPLEMENTED();
}

EXPORT(int, __aeabi_lcmp) {
    return UNIMPLEMENTED();
}

EXPORT(int, __aeabi_ldivmod) {
    return UNIMPLEMENTED();
}

EXPORT(int, __aeabi_lmul) {
    return UNIMPLEMENTED();
}

EXPORT(int, __aeabi_uidiv) {
    return UNIMPLEMENTED();
}

EXPORT(int, __aeabi_uidivmod) {
    return UNIMPLEMENTED();
}

EXPORT(int, __aeabi_ulcmp) {
    return UNIMPLEMENTED();
}

EXPORT(int, __memcpy_chk) {
    return UNIMPLEMENTED();
}

EXPORT(int, __memmove_chk) {
    return UNIMPLEMENTED();
}

EXPORT(int, __memset_chk) {
    return UNIMPLEMENTED();
}

EXPORT(int, __kstack_chk_fail) {
    return UNIMPLEMENTED();
}

EXPORT(int, __strncat_chk) {
    return UNIMPLEMENTED();
}

EXPORT(int, __strncpy_chk) {
    return UNIMPLEMENTED();
}

EXPORT(int, look_ctype_table) {
    return UNIMPLEMENTED();
}

EXPORT(int, kmemchr) {
    return UNIMPLEMENTED();
}

EXPORT(int, kmemcmp, Ptr<void> s1, Ptr<void> s2, SceSize len) {
    return memcmp(s1.get(emuenv.mem), s2.get(emuenv.mem), len);
}

EXPORT(Ptr<void>, kmemcpy, Ptr<void> dst, const void *src, SceSize len) {
    memcpy(dst.get(emuenv.mem), src, len);
    return dst;
}

EXPORT(Ptr<void>, kmemmove, Ptr<void> dst, const void *src, SceSize len) {
    memmove(dst.get(emuenv.mem), src, len);
    return dst;
}

EXPORT(Ptr<void>, kmemset, Ptr<void> dst, int ch, SceSize len) {
    memset(dst.get(emuenv.mem), ch, len);
    return dst;
}

EXPORT(int, rshift) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksnprintf) {
    return UNIMPLEMENTED();
}

EXPORT(int, kstrchr) {
    return UNIMPLEMENTED();
}

EXPORT(int, kstrcmp, Ptr<char> s1, Ptr<char> s2) {
    return strcmp(s1.get(emuenv.mem), s2.get(emuenv.mem));
}

EXPORT(int, strlcat) {
    return UNIMPLEMENTED();
}

EXPORT(int, strlcpy) {
    return UNIMPLEMENTED();
}

EXPORT(uint32_t, kstrlen, Ptr<char> s1, SceSize maxlen) {
    return static_cast<uint32_t>(strnlen(s1, maxlen));
}

EXPORT(int, kstrncat) {
    return UNIMPLEMENTED();
}

EXPORT(int, kstrncmp) {
    return UNIMPLEMENTED();
}

EXPORT(Ptr<char>,kstrncpy, Ptr<char> destination, Ptr<char> source, SceSize size) {
    strncpy(destination.get(emuenv.mem), source.get(emuenv.mem), size);
    return destination;
}

EXPORT(int, strnlen, Ptr<char> str) {
    return static_cast<int>(strlen(str.get(emuenv.mem)));
}

EXPORT(int, kstrrchr) {
    return UNIMPLEMENTED();
}

EXPORT(int, kstrstr) {
    return UNIMPLEMENTED();
}

EXPORT(int, kstrtol) {
    return UNIMPLEMENTED();
}

EXPORT(int, kstrtoll) {
    return UNIMPLEMENTED();
}

EXPORT(int, kstrtoul) {
    return UNIMPLEMENTED();
}

EXPORT(int, ktolower) {
    return UNIMPLEMENTED();
}

EXPORT(int, ktoupper) {
    return UNIMPLEMENTED();
}

EXPORT(int, kvsnprintf) {
    return UNIMPLEMENTED();
}
