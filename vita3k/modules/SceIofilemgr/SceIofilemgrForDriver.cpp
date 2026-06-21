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

#include "SceIofilemgr.h"

#include <io/functions.h>
#include <kernel/types.h>

#include <util/tracy.h>
TRACY_MODULE_NAME(SceIofilemgrForDriver);

EXPORT(int, ksceIoCancel) {
    TRACY_FUNC(ksceIoCancel);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoChstat) {
    TRACY_FUNC(ksceIoChstat);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoChstat2) {
    TRACY_FUNC(ksceIoChstat2);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoChstatAsync) {
    TRACY_FUNC(_sceIoMkdirAsync);
    return UNIMPLEMENTED();
}
          
EXPORT(int, _sceIoMkdirAsync) {
    TRACY_FUNC(_sceIoMkdirAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoChstatByFd) {
    TRACY_FUNC(ksceIoChstatByFd);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoChstatByFdAsync) {
    TRACY_FUNC(ksceIoChstatByFdAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoClearErrorEvent) {
    TRACY_FUNC(ksceIoClearErrorEvent);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoClose) {
    TRACY_FUNC(ksceIoClose);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoCloseAsync) {
    TRACY_FUNC(ksceIoCloseAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoCreateErrorEvent) {
    TRACY_FUNC(ksceIoCreateErrorEvent);
    return UNIMPLEMENTED();
}
          
EXPORT(int, ksceIoCreateMountEvent) {
    TRACY_FUNC(ksceIoCreateMountEvent);
    return UNIMPLEMENTED();
}          
          
EXPORT(int, ksceIoDclose) {
    TRACY_FUNC(ksceIoDclose);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoDcloseAsync) {
    TRACY_FUNC(ksceIoDcloseAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoDeleteErrorEvent) {
    TRACY_FUNC(ksceIoDeleteErrorEvent);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoDeleteMountEvent) {
    TRACY_FUNC(ksceIoDeleteMountEvent);
    return UNIMPLEMENTED();
}
          
EXPORT(int, ksceIoDevctl) {
    TRACY_FUNC(ksceIoDevctl);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoDevctlAsync) {
    TRACY_FUNC(ksceIoDevctlAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoDevctlAsync) {
    TRACY_FUNC(ksceIoDevctlAsync);
    return UNIMPLEMENTED();
}
          
EXPORT(int, ksceIoDopen) {
    TRACY_FUNC(ksceIoDopen);
    return UNIMPLEMENTED();
}         
          
EXPORT(int, ksceIoDopenAsync) {
    TRACY_FUNC(ksceIoDopenAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoDread) {
    TRACY_FUNC(ksceIoDread);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoDread) {
    TRACY_FUNC(ksceIoDread);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoDread2) {
    TRACY_FUNC(ksceIoDread2);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoDreadAsync) {
    TRACY_FUNC(ksceIoDreadAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoFlock) {
    TRACY_FUNC(ksceIoFlock);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoGetFileInfo) {
    TRACY_FUNC(ksceIoGetFileInfo);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoGetGUIDFdListForDebugger) {
    TRACY_FUNC(ksceIoGetGUIDFdListForDebugger);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoGetMediaType) {
    TRACY_FUNC(ksceIoGetMediaType);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoGetPUIDFdListForDebugger) {
    TRACY_FUNC(ksceIoGetPUIDFdListForDebugger);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoGetProcessDefaultPriorityForSystem) {
    TRACY_FUNC(ksceIoGetProcessDefaultPriorityForSystem);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoGetRemoteKPLSData) {
    TRACY_FUNC(ksceIoGetRemoteKPLSData);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoGetThreadDefaultPriorityForSystem) {
    TRACY_FUNC(_sceIoMkdirAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoGetstat) {
    TRACY_FUNC(ksceIoGetstat);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoGetstat2) {
    TRACY_FUNC(ksceIoGetstat2);
    return UNIMPLEMENTED();
}
          
EXPORT(int, ksceIoGetstatAsync) {
    TRACY_FUNC(ksceIoGetstatAsync);
    return UNIMPLEMENTED();
} 

EXPORT(int, ksceIoGetstatByFd) {
    TRACY_FUNC(ksceIoGetstatByFd);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoGetstatByFdAsync) {
    TRACY_FUNC(ksceIoGetstatByFdAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoIoctl) {
    TRACY_FUNC(ksceIoIoctl);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoIoctlAsync) {
    TRACY_FUNC(ksceIoIoctlAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoLseek) {
    TRACY_FUNC(ksceIoLseek);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoLseekAsync) {
    TRACY_FUNC(ksceIoLseekAsync);
    return UNIMPLEMENTED();
}


EXPORT(int, ksceIoMkdir) {
    TRACY_FUNC(ksceIoMkdir);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoMkdirAsync) {
    TRACY_FUNC(ksceIoMkdirAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoMount) {
    TRACY_FUNC(ksceIoMount);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoOpen) {
    TRACY_FUNC(ksceIoOpen);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoOpenAsync) {
    TRACY_FUNC(ksceIoOpenAsync);
    return UNIMPLEMENTED();
}
          
EXPORT(int, ksceIoOpenForPid) {
    TRACY_FUNC(ksceIoOpenForPid);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoPread) {
    TRACY_FUNC(ksceIoPread);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoPreadAsync) {
    TRACY_FUNC(ksceIoPreadAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoPwrite) {
    TRACY_FUNC(ksceIoPwrite);
    return UNIMPLEMENTED();
}          


EXPORT(int, ksceIoPwriteAsync) {
    TRACY_FUNC(ksceIoPwriteAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoRead) {
    TRACY_FUNC(ksceIoRead);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoReadAsync) {
    TRACY_FUNC(ksceIoReadAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoRemove) {
    TRACY_FUNC(ksceIoRemove);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoRemoveAsync) {
    TRACY_FUNC(ksceIoRemoveAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoRename) {
    TRACY_FUNC(ksceIoRename);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoRenameAsync) {
    TRACY_FUNC(ksceIoRenameAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoRmdir) {
    TRACY_FUNC(ksceIoRmdir);
    return UNIMPLEMENTED();
}
          
EXPORT(int, ksceIoRmdirAsync) {
    TRACY_FUNC(ksceIoRmdirAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoSetPathMappingFunction) {
    TRACY_FUNC(ksceIoSetPathMappingFunction);
    return UNIMPLEMENTED();
}

          
          
          ksceIoSetProcessDefaultPriorityForSystem: 0xABE65071
          ksceIoSetThreadDefaultPriorityForSystem: 0x3F0FF9D5
          ksceIoSync: 0xDDF78594
          ksceIoSyncAsync: 0x4F9EA8B0
          ksceIoSyncByFd: 0x338DCD68
          ksceIoSyncByFd2: 0x43170575
          ksceIoSyncByFdAsync: 0x041209CF
          ksceIoUmount: 0x20574100
          ksceIoWrite: 0x21EE91F0
          ksceIoWriteAsync: 0xA1BD13D0
          kscePfsMgrVfsMount: 0xFEEE44A9
          kscePfsMgrVfsUmount: 0xD220539D
          ksceVfsAddVfs: 0x673D2FCD
          ksceVfsDeleteVfs: 0x9CBFA725
          ksceVfsFreeVnode: 0x21D57633
          ksceVfsGetNewNode: 0xD60B5C63
          ksceVfsLockMnt: 0x6B3CA9F7
          ksceVfsMount: 0xB62DE9A6
          ksceVfsNodeSetEventFlag: 0x6048F245
          ksceVfsNodeWaitEventFlag: 0xAA45010B
          ksceVfsOpDecodePathElem: 0xF7DAC0F5
          ksceVfsOpDevctl: 0xB07B307D
          ksceVfsUnlockMnt: 0xDC2D8BCE
          ksceVfsUnmount: 0x9C7E7B76
          ksceVopChstat: 0x1974FA92
          ksceVopClose: 0x40944C2E
          ksceVopCreate: 0x9E347C7D
          ksceVopDclose: 0x1350F5C7
          ksceVopDopen: 0x00C9C2DD
          ksceVopDread: 0x77584C8F
          ksceVopGetstat: 0x50A63ACF
          ksceVopInactive: 0x8FB94521
          ksceVopIoctl: 0x333C904D
          ksceVopLseek: 0xB2B13818
          ksceVopMkdir: 0x2F3F8C70
          ksceVopOpen: 0x76B79BEC
          ksceVopPread: 0xABBC80E3
          ksceVopPwrite: 0xA53C040D
          ksceVopRead: 0x570388A5
          ksceVopRename: 0x36A794C7
          ksceVopRmdir: 0x1D551105
          ksceVopSync: 0x9CD96406
          ksceVopWrite: 0x9A68378D
