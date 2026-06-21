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
    TRACY_FUNC(ksceIoChstatAsync);
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

EXPORT(int, ksceIoClose, const SceUID fd) {
    TRACY_FUNC(ksceIoClose, fd);
    return close_file(emuenv.io, fd, export_name);
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
          
EXPORT(int, ksceIoDclose, const SceUID fd) {
    TRACY_FUNC(ksceIoDclose, fd);
    return close_dir(emuenv.io, fd, export_name);
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
          
EXPORT(int, ksceIoDopen, const char *dir) {
    TRACY_FUNC(ksceIoDopen, dir);
    return open_dir(emuenv.io, dir, emuenv.pref_path, export_name);
}
          
EXPORT(int, ksceIoDopenAsync) {
    TRACY_FUNC(ksceIoDopenAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoDread, const SceUID fd, SceIoDirent *dir) {
    TRACY_FUNC(ksceIoDread, fd, dir);
    if (dir == nullptr) {
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_ADDR);
    }
    return read_dir(emuenv.io, fd, dir, emuenv.pref_path, export_name);
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
    TRACY_FUNC(ksceIoGetThreadDefaultPriorityForSystem);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoGetstat, const char *file, SceIoStat *stat) {
    TRACY_FUNC(ksceIoGetstat, file, stat);
    return stat_file(emuenv.io, file, stat, emuenv.pref_path, export_name);
}

EXPORT(int, ksceIoGetstat2) {
    TRACY_FUNC(ksceIoGetstat2);
    return UNIMPLEMENTED();
}
          
EXPORT(int, ksceIoGetstatAsync) {
    TRACY_FUNC(ksceIoGetstatAsync);
    return UNIMPLEMENTED();
} 

EXPORT(int, ksceIoGetstatByFd, const SceUID fd, SceIoStat *stat) {
    TRACY_FUNC(ksceIoGetstatByFd, fd, stat);
    return stat_file_by_fd(emuenv.io, fd, stat, emuenv.pref_path, export_name);
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

EXPORT(SceOff, ksceIoLseek, const SceUID fd, Ptr<_sceIoLseekOpt> opt) {
    TRACY_FUNC(ksceIoLseek, fd, opt);
    return seek_file(fd, opt.get(emuenv.mem)->offset, opt.get(emuenv.mem)->whence, emuenv.io, export_name);
}

EXPORT(int, ksceIoLseekAsync) {
    TRACY_FUNC(ksceIoLseekAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoMkdir, const char *dir, const SceMode mode) {
    TRACY_FUNC(ksceIoMkdir, dir, mode);
    return create_dir(emuenv.io, dir, mode, emuenv.pref_path, export_name);
}

EXPORT(int, ksceIoMkdirAsync) {
    TRACY_FUNC(ksceIoMkdirAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoMount) {
    TRACY_FUNC(ksceIoMount);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoOpen, const char *file, const int flags, const SceMode mode) {
    TRACY_FUNC(ksceIoOpen, file, flags, mode);
    if (file == nullptr) {
        return RET_ERROR(SCE_ERROR_ERRNO_EINVAL);
    }
    LOG_INFO("Opening file: {}", file);
    return open_file(emuenv.io, file, flags, emuenv.pref_path, export_name);
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

EXPORT(int, ksceIoSetProcessDefaultPriorityForSystem) {
    TRACY_FUNC(ksceIoSetProcessDefaultPriorityForSystem);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoSetThreadDefaultPriorityForSystem) {
    TRACY_FUNC(ksceIoSetThreadDefaultPriorityForSystem);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoSync) {
    TRACY_FUNC(ksceIoSync);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoSyncAsync) {
    TRACY_FUNC(ksceIoSyncAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoSyncByFd) {
    TRACY_FUNC(ksceIoSyncByFd);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoSyncByFd2) {
    TRACY_FUNC(ksceIoSyncByFd2);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoSyncByFdAsync) {
    TRACY_FUNC(ksceIoSyncByFdAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceIoUmount) {
    TRACY_FUNC(ksceIoUmount);
    return UNIMPLEMENTED();
}
          
EXPORT(int, ksceIoWrite, const SceUID fd, const void *data, const SceSize size) {
    TRACY_FUNC(ksceIoWrite, fd, data, size);
    return write_file(fd, data, size, emuenv.io, export_name);
}

EXPORT(int, ksceIoWriteAsync) {
    TRACY_FUNC(ksceIoWriteAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, kscePfsMgrVfsMount) {
    TRACY_FUNC(kscePfsMgrVfsMount);
    return UNIMPLEMENTED();
}

EXPORT(int, kscePfsMgrVfsUmount) {
    TRACY_FUNC(kscePfsMgrVfsUmount);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceVfsAddVfs) {
    TRACY_FUNC(ksceVfsAddVfs);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceVfsDeleteVfs) {
    TRACY_FUNC(ksceVfsDeleteVfs);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceVfsFreeVnode) {
    TRACY_FUNC(ksceVfsFreeVnode);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceVfsGetNewNode) {
    TRACY_FUNC(ksceVfsGetNewNode);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceVfsLockMnt) {
    TRACY_FUNC(ksceVfsLockMnt);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceVfsMount) {
    TRACY_FUNC(ksceVfsMount);
    return UNIMPLEMENTED();
}
          
EXPORT(int, ksceVfsNodeSetEventFlag) {
    TRACY_FUNC(ksceVfsNodeSetEventFlag);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceVfsNodeWaitEventFlag) {
    TRACY_FUNC(ksceVfsNodeWaitEventFlag);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceVfsOpDecodePathElem) {
    TRACY_FUNC(ksceVfsOpDecodePathElem);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceVfsOpDevctl) {
    TRACY_FUNC(ksceVfsOpDevctl);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceVfsUnlockMnt) {
    TRACY_FUNC(ksceVfsUnlockMnt);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceVfsUnmount) {
    TRACY_FUNC(ksceVfsUnmount);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceVopChstat) {
    TRACY_FUNC(ksceVopChstat);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceVopClose) {
    TRACY_FUNC(ksceVopClose);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceVopCreate) {
    TRACY_FUNC(ksceVopCreate);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceVopDclose) {
    TRACY_FUNC(ksceVopDclose);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceVopDopen) {
    TRACY_FUNC(ksceVopDopen);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceVopDread) {
    TRACY_FUNC(ksceVopDread);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceVopGetstat) {
    TRACY_FUNC(ksceVopGetstat);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceVopInactive) {
    TRACY_FUNC(ksceVopInactive);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceVopIoctl) {
    TRACY_FUNC(ksceVopIoctl);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceVopLseek) {
    TRACY_FUNC(ksceVopLseek);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceVopOpen) {
    TRACY_FUNC(ksceVopOpen);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceVopPread) {
    TRACY_FUNC(ksceVopPread);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceVopPwrite) {
    TRACY_FUNC(ksceVopPwrite);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceVopRead) {
    TRACY_FUNC(ksceVopRead);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceVopRename) {
    TRACY_FUNC(ksceVopRename);
    return UNIMPLEMENTED();
}          
    
EXPORT(int, ksceVopRmdir) {
    TRACY_FUNC(ksceVopRmdir);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceVopSync) {
    TRACY_FUNC(ksceVopSync);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceVopWrite) {
    TRACY_FUNC(ksceVopWrite);
    return UNIMPLEMENTED();
}
