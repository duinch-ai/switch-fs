#include "capabilities.h"
#include <string.h>

Handle g_procHandle;
void* g_heapAddr;
size_t g_heapSize;
bool g_isApplication = 0;
bool g_isAutomaticGameplayRecording = 0;
u64 g_appletHeapSize = 0;
u64 g_appletHeapReservationSize = 0;
int g_codeMemoryCapability = 0;

static u64 calculateMaxHeapSize(void) {
    u64 size = 0;
    u64 mem_available = 0, mem_used = 0;

    svcGetInfo(&mem_available, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
    svcGetInfo(&mem_used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0);

    if (mem_available > mem_used + 0x200000) size = (mem_available - mem_used - 0x200000) & ~0x1FFFFF;
    if (size == 0) size = 0x2000000 * 16;
    if (size > 0x6000000 && g_isAutomaticGameplayRecording) size -= 0x6000000;

    return size;
}

void setupHbHeap(void) {
    void* addr = NULL;
    u64 size = calculateMaxHeapSize();

    if (!g_isApplication) {
        if (g_appletHeapSize) {
            u64 requested_size = (g_appletHeapSize + 0x1FFFFF) & ~0x1FFFFF;
            if (requested_size < size) size = requested_size;
        } else if (g_appletHeapReservationSize) {
            u64 reserved_size = (g_appletHeapReservationSize + 0x1FFFFF) & ~0x1FFFFF;
            if (reserved_size < size) size -= reserved_size;
        }
    }

    Result rc = svcSetHeapSize(&addr, size);
    if (R_FAILED(rc) || addr == NULL) diagAbortWithResult(MAKERESULT(Module_HomebrewLoader, 9));

    g_heapAddr = addr;
    g_heapSize = size;
}

static void procHandleReceiveThread(void* arg) {
    Handle session = (Handle)(uintptr_t)arg;
    Result rc;

    void* base = armGetTls();
    hipcMakeRequestInline(base);

    s32 idx = 0;
    rc = svcReplyAndReceive(&idx, &session, 1, INVALID_HANDLE, UINT64_MAX);
    if (R_FAILED(rc)) diagAbortWithResult(MAKERESULT(Module_HomebrewLoader, 15));

    HipcParsedRequest r = hipcParseRequest(base);
    if (r.meta.num_copy_handles != 1) diagAbortWithResult(MAKERESULT(Module_HomebrewLoader, 17));

    g_procHandle = r.data.copy_handles[0];
    svcCloseHandle(session);
}

void getIsApplication(void) {
    Result rc;
    u64 flag = 0;
    rc = svcGetInfo(&flag, InfoType_IsApplication, CUR_PROCESS_HANDLE, 0);
    if (R_SUCCEEDED(rc)) {
        g_isApplication = flag != 0;
        return;
    }

    u64 cur_pid = 0;
    rc = svcGetProcessId(&cur_pid, CUR_PROCESS_HANDLE);
    if (R_FAILED(rc)) diagAbortWithResult(rc);

    rc = pmshellInitialize();
    if (R_SUCCEEDED(rc)) {
        u64 app_pid = 0;
        rc = pmshellGetApplicationProcessIdForShell(&app_pid);
        pmshellExit();
        if (cur_pid == app_pid) g_isApplication = 1;
    }
}

void getIsAutomaticGameplayRecording(void) {
    Result rc;
    if (hosversionBefore(4, 0, 0) || !g_isApplication) return;

    u64 cur_progid = 0;
    rc = svcGetInfo(&cur_progid, InfoType_ProgramId, CUR_PROCESS_HANDLE, 0);
    if (R_FAILED(rc)) diagAbortWithResult(rc);

    rc = nsInitialize();
    if (R_SUCCEEDED(rc)) {
        NsApplicationControlData data;
        u64 size = 0;
        rc = nsGetApplicationControlData(NsApplicationControlSource_Storage, cur_progid, &data, sizeof(data), &size);
        nsExit();
        if (R_SUCCEEDED(rc) && data.nacp.video_capture == 2) g_isAutomaticGameplayRecording = 1;
    }
}

void getOwnProcessHandle(void) {
    Result rc;
    Handle server_handle, client_handle;
    rc = svcCreateSession(&server_handle, &client_handle, 0, 0);
    if (R_FAILED(rc)) diagAbortWithResult(MAKERESULT(Module_HomebrewLoader, 12));

    Thread t;
    rc = threadCreate(&t, &procHandleReceiveThread, (void*)(uintptr_t)server_handle, NULL, 0x1000, 0x20, 0);
    if (R_FAILED(rc)) diagAbortWithResult(MAKERESULT(Module_HomebrewLoader, 10));

    rc = threadStart(&t);
    if (R_FAILED(rc)) diagAbortWithResult(MAKERESULT(Module_HomebrewLoader, 13));

    hipcMakeRequestInline(armGetTls(), .num_copy_handles = 1, ).copy_handles[0] = CUR_PROCESS_HANDLE;
    svcSendSyncRequest(client_handle);
    svcCloseHandle(client_handle);

    threadWaitForExit(&t);
    threadClose(&t);
}

static bool isKernel5xOrLater(void) {
    u64 dummy = 0;
    Result rc = svcGetInfo(&dummy, InfoType_UserExceptionContextAddress, INVALID_HANDLE, 0);
    return R_VALUE(rc) != KERNELRESULT(InvalidEnumValue);
}

void getCodeMemoryCapability(void) {
    if (detectMesosphere()) {
        g_codeMemoryCapability = 3; // CodeMemorySameProcess
    } else if (isKernel5xOrLater()) {
        Handle code;
        Result rc = svcCreateCodeMemory(&code, g_heapAddr, 0x1000);
        if (R_SUCCEEDED(rc)) {
            rc = svcControlCodeMemory(code, CodeMemoryOperation_MapOwner, 0, 0x1000, Perm_None);
            if (R_VALUE(rc) == KERNELRESULT(InvalidEnumValue)) {
                g_codeMemoryCapability = 3;
            } else {
                g_codeMemoryCapability = 1;
            }
            svcCloseHandle(code);
        } else {
            g_codeMemoryCapability = 1;
        }
    } else {
        g_codeMemoryCapability = 3;
    }
}
