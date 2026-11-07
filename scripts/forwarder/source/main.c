#include <switch.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "capabilities.h"

#define HBMENU_NRO "sdmc:/hbmenu.nro"
#define DEFAULT_NRO "sdmc:/switch/SwitchFS.nro"
#define APP_STORE_NRO "sdmc:/switch/SwitchFS/SwitchFS.nro"

const char g_noticeText[] =
    "Switch FS Forwarder " VERSION
    "\0"
    "Switch FS — FreeShop Client for Nintendo Switch";

static char g_argv[2048];
static char g_nextArgv[2048];
static char g_nextNroPath[512];
u64 g_nroAddr = 0;
static u64 g_nroSize = 0;
static NroHeader g_nroHeader;

static u128 g_userIdStorage;
static u8 g_savedTls[0x100];

u32 __nx_fs_num_sessions = 1;
u32 __nx_fsdev_direntry_cache_size = 1;
bool __nx_fsdev_support_cwd = false;

Result g_lastRet = 0;

void NX_NORETURN nroEntrypointTrampoline(const ConfigEntry* entries, u64 handle, u64 entrypoint);

void __libnx_initheap(void) {
    static char g_innerheap[0x4000];
    extern char* fake_heap_start;
    extern char* fake_heap_end;
    fake_heap_start = &g_innerheap[0];
    fake_heap_end = &g_innerheap[sizeof g_innerheap];
}

static Result readSetting(const char* key, void* buf, size_t size) {
    Result rc;
    u64 actual_size;
    const char* const section_name = "hbloader";
    rc = setsysGetSettingsItemValueSize(section_name, key, &actual_size);
    if (R_SUCCEEDED(rc) && actual_size != size) rc = MAKERESULT(Module_Libnx, LibnxError_BadInput);
    if (R_SUCCEEDED(rc)) rc = setsysGetSettingsItemValue(section_name, key, buf, size, &actual_size);
    if (R_SUCCEEDED(rc) && actual_size != size) rc = MAKERESULT(Module_Libnx, LibnxError_BadInput);
    if (R_FAILED(rc)) memset(buf, 0, size);
    return rc;
}

void __appInit(void) {
    Result rc;
    Handle dummy;
    rc = svcConnectToNamedPort(&dummy, "ams");
    u32 ams_flag = (R_VALUE(rc) != KERNELRESULT(NotFound)) ? BIT(31) : 0;
    if (R_SUCCEEDED(rc)) svcCloseHandle(dummy);

    rc = smInitialize();
    if (R_FAILED(rc)) diagAbortWithResult(MAKERESULT(Module_HomebrewLoader, 1));

    rc = setsysInitialize();
    if (R_SUCCEEDED(rc)) {
        SetSysFirmwareVersion fw;
        rc = setsysGetFirmwareVersion(&fw);
        if (R_SUCCEEDED(rc)) hosversionSet(ams_flag | MAKEHOSVERSION(fw.major, fw.minor, fw.micro));
        readSetting("applet_heap_size", &g_appletHeapSize, sizeof(g_appletHeapSize));
        readSetting("applet_heap_reservation_size", &g_appletHeapReservationSize, sizeof(g_appletHeapReservationSize));
        setsysExit();
    }

    rc = fsInitialize();
    if (R_FAILED(rc)) diagAbortWithResult(MAKERESULT(Module_HomebrewLoader, 2));
}

void __wrap_exit(void) {
    diagAbortWithResult(MAKERESULT(Module_HomebrewLoader, 39));
}

static void loadNro(void) {
    NroHeader* header = NULL;
    size_t rw_size = 0;
    Result rc;

    rc = fsdevMountSdmc();
    if (R_FAILED(rc)) diagAbortWithResult(MAKERESULT(Module_HomebrewLoader, 404));

    if (g_nextNroPath[0] == '\0') {
        if (access(DEFAULT_NRO, F_OK) != -1) {
            memcpy(g_nextNroPath, DEFAULT_NRO, sizeof(DEFAULT_NRO));
            memcpy(g_nextArgv, DEFAULT_NRO, sizeof(DEFAULT_NRO));
        } else if (access(APP_STORE_NRO, F_OK) != -1) {
            memcpy(g_nextNroPath, APP_STORE_NRO, sizeof(APP_STORE_NRO));
            memcpy(g_nextArgv, APP_STORE_NRO, sizeof(APP_STORE_NRO));
        } else {
            memcpy(g_nextNroPath, HBMENU_NRO, sizeof(HBMENU_NRO));
            memcpy(g_nextArgv, HBMENU_NRO, sizeof(HBMENU_NRO));
        }
    }

    memcpy(g_argv, g_nextArgv, sizeof g_argv);

    uint8_t* nrobuf = (uint8_t*)g_heapAddr;
    NroStart* start = (NroStart*)(nrobuf + 0);
    header = (NroHeader*)(nrobuf + sizeof(NroStart));
    uint8_t* rest = (uint8_t*)(nrobuf + sizeof(NroStart) + sizeof(NroHeader));

    int fd = open(g_nextNroPath, O_RDONLY);
    if (fd < 0) diagAbortWithResult(MAKERESULT(Module_HomebrewLoader, 3));

    g_nextNroPath[0] = '\0';
    if (read(fd, start, sizeof(*start)) != sizeof(*start)) diagAbortWithResult(MAKERESULT(Module_HomebrewLoader, 4));
    if (read(fd, header, sizeof(*header)) != sizeof(*header)) diagAbortWithResult(MAKERESULT(Module_HomebrewLoader, 4));
    if (header->magic != NROHEADER_MAGIC) diagAbortWithResult(MAKERESULT(Module_HomebrewLoader, 5));

    size_t rest_size = header->size - (sizeof(NroStart) + sizeof(NroHeader));
    if (read(fd, rest, rest_size) != rest_size) diagAbortWithResult(MAKERESULT(Module_HomebrewLoader, 7));

    close(fd);
    fsdevUnmountAll();

    size_t total_size = (header->size + header->bss_size + 0xFFF) & ~0xFFF;
    rw_size = (header->segments[2].size + header->bss_size + 0xFFF) & ~0xFFF;

    memcpy(&g_nroHeader, header, sizeof(g_nroHeader));
    header = &g_nroHeader;

    virtmemLock();
    void* map_addr = virtmemFindCodeMemory(total_size, 0);
    rc = svcMapProcessCodeMemory(g_procHandle, (u64)map_addr, (u64)nrobuf, total_size);
    virtmemUnlock();
    if (R_FAILED(rc)) diagAbortWithResult(MAKERESULT(Module_HomebrewLoader, 18));

    svcSetProcessMemoryPermission(g_procHandle, (u64)map_addr + header->segments[0].file_off, header->segments[0].size, Perm_R | Perm_X);
    svcSetProcessMemoryPermission(g_procHandle, (u64)map_addr + header->segments[1].file_off, header->segments[1].size, Perm_R);
    svcSetProcessMemoryPermission(g_procHandle, (u64)map_addr + header->segments[2].file_off, rw_size, Perm_Rw);

    u64 nro_size = header->segments[2].file_off + rw_size;
    u64 nro_heap_start = ((u64)g_heapAddr) + nro_size;
    u64 nro_heap_size = g_heapSize + (u64)g_heapAddr - (u64)nro_heap_start;

#define M EntryFlag_IsMandatory
    static ConfigEntry entries[] = {
        {EntryType_MainThreadHandle, 0, {0, 0}},
        {EntryType_ProcessHandle, 0, {0, 0}},
        {EntryType_AppletType, 0, {AppletType_SystemApplication, EnvAppletFlags_ApplicationOverride}},
        {EntryType_OverrideHeap, M, {0, 0}},
        {EntryType_Argv, 0, {0, 0}},
        {EntryType_NextLoadPath, 0, {0, 0}},
        {EntryType_LastLoadResult, 0, {0, 0}},
        {EntryType_SyscallAvailableHint, 0, {UINT64_MAX, UINT64_MAX}},
        {EntryType_SyscallAvailableHint2, 0, {UINT64_MAX, 0}},
        {EntryType_RandomSeed, 0, {0, 0}},
        {EntryType_UserIdStorage, 0, {(u64)(uintptr_t)&g_userIdStorage, 0}},
        {EntryType_HosVersion, 0, {0, 0}},
        {EntryType_EndOfList, 0, {(u64)(uintptr_t)g_noticeText, sizeof(g_noticeText)}}
    };

    entries[0].Value[0] = envGetMainThreadHandle();
    entries[1].Value[0] = g_procHandle;
    entries[3].Value[0] = nro_heap_start;
    entries[3].Value[1] = nro_heap_size;
    entries[4].Value[1] = (u64)(uintptr_t)&g_argv[0];
    entries[5].Value[0] = (u64)(uintptr_t)&g_nextNroPath[0];
    entries[5].Value[1] = (u64)(uintptr_t)&g_nextArgv[0];
    entries[6].Value[0] = g_lastRet;
    entries[9].Value[0] = randomGet64();
    entries[9].Value[1] = randomGet64();
    entries[11].Value[0] = hosversionGet();
    entries[11].Value[1] = hosversionIsAtmosphere() ? 0x41544d4f53504852UL : 0;

    g_nroAddr = (u64)map_addr;
    g_nroSize = nro_size;

    nroEntrypointTrampoline(&entries[0], -1, g_nroAddr);
}

int main(int argc, char** argv) {
    memcpy(g_savedTls, (u8*)armGetTls() + 0x100, 0x100);
    getIsApplication();
    getIsAutomaticGameplayRecording();
    smExit();
    setupHbHeap();
    getOwnProcessHandle();
    getCodeMemoryCapability();
    loadNro();

    diagAbortWithResult(MAKERESULT(Module_HomebrewLoader, 8));
    return 0;
}
