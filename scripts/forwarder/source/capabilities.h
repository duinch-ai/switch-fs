#pragma once

#include <switch.h>

extern Handle g_procHandle;
extern void* g_heapAddr;
extern size_t g_heapSize;
extern bool g_isApplication;
extern bool g_isAutomaticGameplayRecording;
extern u64 g_appletHeapSize;
extern u64 g_appletHeapReservationSize;
extern int g_codeMemoryCapability;

void setupHbHeap(void);
void getIsApplication(void);
void getIsAutomaticGameplayRecording(void);
void getOwnProcessHandle(void);
void getCodeMemoryCapability(void);
