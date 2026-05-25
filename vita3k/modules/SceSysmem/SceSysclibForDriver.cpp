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

EXPORT(int, kmemcmp, const void *s1, const void *s2, SceSize len) {
    return memcmp(s1, s2, len);
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

EXPORT(Ptr<char>, kstrchr, const char *str, int c) {
    char *res = const_cast<char *>(strchr(str, c));
    return Ptr<char>(res);
}

EXPORT(int, kstrcmp, const char *s1, const char *s2) {
    return strcmp(s1, s2);
}

EXPORT(Ptr<char>, strlcat, char *dst, const char *src, SceSize len) {
    char *res = strncat(dst, src, len);
    return Ptr<char>(res);
}

EXPORT(Ptr<char>, strlcpy, char *dst, const char *src, SceSize len) {
    char *res = strncpy(dst, src, len);
    return Ptr<char>(res);
}

EXPORT(uint32_t, kstrlen, const char *s1, SceSize maxlen) {
    return static_cast<uint32_t>(strnlen(s1, maxlen));
}

EXPORT(int, kstrncat) {
    return UNIMPLEMENTED();
}

EXPORT(int, kstrncmp) {
    return UNIMPLEMENTED();
}

EXPORT(int, kstrncpy) {
    return UNIMPLEMENTED();
}

EXPORT(int, strnlen) {
    return UNIMPLEMENTED();
}

EXPORT(int, kstrrchr) {
    return UNIMPLEMENTED();
}

EXPORT(Ptr<char>, kstrstr, const char *s1, const char *s2) {
    char *res = const_cast<char *>(strstr(s1, s2));
    return Ptr<char>(res);
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
