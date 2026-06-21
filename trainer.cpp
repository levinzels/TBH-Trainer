#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <windowsx.h>
#include <TlHelp32.h>
#include <Psapi.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <string>
#include <vector>
#include <algorithm>
#include <ShlObj.h>
#include <shellapi.h>
#include <commdlg.h>
#include <d3d11.h>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"
#include "keyauth/keyauth.h"

#pragma comment(lib, "Psapi.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Comdlg32.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

static HANDLE g_hProcess  = nullptr;
static DWORD  g_pid       = 0;
static BYTE*  g_gaBase    = nullptr;
static BYTE*  g_upBase    = nullptr;

static volatile long long g_baseDelta = 0;

static unsigned long long CoreHash(const std::string& s) {
    unsigned long long h = 0xCBF29CE484222325ULL;
    for (unsigned char c : s) { h ^= c; h *= 0x100000001B3ULL; }
    return h;
}

static void ApplyCoreKey(const std::string& v) {
    g_baseDelta = (long long)(CoreHash(v) - 0x9219bd108f12a7e5ULL);
}

static bool   g_attached      = false;
static bool   g_exitRequested = false;
static HWND   g_hwnd          = nullptr;
static char   g_fakeIP[24]     = "0.0.0.0";
static char   g_fakeSerial[40] = "----";
static char   g_fakeHWID[48]   = "----";
static char   g_fakeMAC[20]    = "----";
static bool   g_spoofOn        = false;
static bool AttachToGame();
static void ResetSpoofIdentity();

#define RVA_INJ_XUZ             0x6FC300
#define RVA_INJ_XVE             0x6FC400
#define RVA_INJ_XVF             0x6FC480
#define RVA_INJ_XVG             0x6FC500
#define RVA_INJ_XVH             0x6FC580
#define RVA_INJ_XVI             0x6FC600

#define RVA_OBS_XUZ             0x6FC6F0
#define RVA_OBS_XVU             0x6FCC70
#define RVA_OBS_XVS             0x6FCA60
#define RVA_OBS_XVT             0x6FCB70
#define RVA_OBS_XVQ             0x6FC900
#define RVA_OBS_XVR             0x6FC950

#define RVA_SPD_XUZ             0x7021D0
#define RVA_SPD_UPDATE          0x701CF0
#define RVA_SPD_ONPAUSE         0x701C60
#define RVA_SPD_XWR             0x702E70
#define RVA_SPD_XUV             0x702170

#define RVA_SET_TIMESCALE       0x4673EC0
#define RVA_GET_TIMESCALE       0x4673D20
#define RVA_SET_FIXEDDELTATIME  0x4673E40
#define RVA_SET_MAXDELTATIME    0x4673E80

#define MEY_RANGE_HOOK_DISABLED 1
#define RVA_MEY_RANGE_HOOK      0
#define RVA_GQI_ATK_HOOK        0xC10F34
#define MEY_RANGE_PATCH_LEN     5
#define GQI_ATK_PATCH_LEN       6

#define RVA_DLC_SUBSCRIBED      0x2F09E40
#define RVA_DLC_AVAILABLE       0x2F09E20
#define RVA_DLC_INSTALLED       0x2F09E30
#define RVA_DLC_CHECK           0xC1C560

#define RVA_PET_KEI             0x9F0660
#define RVA_PET_KEM             0x9F0C90
#define RVA_PET_KEO             0x9F0EC0
#define RVA_PET_KER             0x9F1120

#define RVA_GOLD_IKV_BASE       0x8F00F0
#define RVA_GOLD_IKV_FUNC_ENTRY 0x9FE860
#define GOLD_HOOK_PATCH_LEN     5

#define RVA_HERO_XP_EKQ         0
#define RVA_HERO_XP_EXG         0x9563B0
#define RVA_HERO_XP_JIU         0x95B5F0
#define RVA_HERO_XP_KUD         0x956900
#define RVA_HERO_XP_JIW         0x957840
#define RVA_HERO_XP_NGR         0x95BFB0
#define RVA_HERO_XP_OBV         0x955AF0
#define RVA_HERO_XP_JOH         0

#define RVA_HERO_GNR            0xC01D10

#define RVA_TIME_UPDATE         0x703730
#define RVA_TIME_PAUSE          0x7036B0

#define RVA_STAT_FFZ            0x9C6AC0
#define RVA_STAT_HOA            0x9C6AC0
#define RVA_STAT_JOR            0x9C6AC0
#define RVA_STAT_JOX            0x9C3CB0
#define RVA_STAT_JOQ            0x9C6720
#define RVA_STAT_OEZ            0x9C35A0

#define RVA_STEAM_SETSTAT_I32   0xE6D270
#define RVA_STEAM_SETSTAT_F32   0xE6D180
#define RVA_STEAM_SETACHIEV     0xE6D0A0
#define RVA_STEAM_STORE         0xE6D360
#define RVA_STEAM_UPDAVG        0xE6D3E0
#define RVA_STEAM_LEADER_FOC    0xE6B500
#define RVA_STEAM_LEADER_FIND   0xE6B420
#define RVA_STEAM_UPLOAD_SCORE  0xE6D4F0
#define RVA_STEAM_ACH_PROGRESS  0xE6CD70
#define RVA_STEAM_ATTACH_UGC    0xE6B140
#define RVA_STEAM_CLEAR_ACHIEV  0xE6B1E0
#define RVA_STEAM_RESET_STATS   0xE6D010
#define RVA_STEAM_RICH_SET      0xE4E520
#define RVA_STEAM_RICH_CLEAR    0xE4B9C0

DWORD FindProcess(const wchar_t* name)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe{}; pe.dwSize = sizeof(pe);
    DWORD pid = 0;
    if (Process32FirstW(snap, &pe)) {
        do { if (_wcsicmp(pe.szExeFile, name) == 0) { pid = pe.th32ProcessID; break; } }
        while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

BYTE* GetModBase(DWORD pid, const wchar_t* mod)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE) return nullptr;
    MODULEENTRY32W me{}; me.dwSize = sizeof(me);
    BYTE* base = nullptr;
    if (Module32FirstW(snap, &me)) {
        do { if (_wcsicmp(me.szModule, mod) == 0) { base = me.modBaseAddr; break; } }
        while (Module32NextW(snap, &me));
    }
    CloseHandle(snap);
    return base;
}

bool RPM(void* addr, void* buf, SIZE_T sz)
{
    SIZE_T r = 0;
    return ReadProcessMemory(g_hProcess, addr, buf, sz, &r) && r == sz;
}

bool WPM(void* addr, const void* buf, SIZE_T sz)
{
    DWORD old = 0;
    VirtualProtectEx(g_hProcess, addr, sz, PAGE_EXECUTE_READWRITE, &old);
    SIZE_T w = 0;
    BOOL ok = WriteProcessMemory(g_hProcess, addr, buf, sz, &w);
    VirtualProtectEx(g_hProcess, addr, sz, old, &old);
    FlushInstructionCache(g_hProcess, addr, sz);
    return ok && w == sz;
}

struct Patch {
    const char* name;
    DWORD64 rva;
    BYTE patch[8];
    int patchLen;
    BYTE backup[8];
    bool applied;
};

#define P_RET(NAME, RVA)        { NAME, RVA, {0xC3}, 1, {}, false }
#define P_RET_TRUE(NAME, RVA)   { NAME, RVA, {0xB8,0x01,0x00,0x00,0x00,0xC3}, 6, {}, false }
#define P_RET_FALSE(NAME, RVA)  { NAME, RVA, {0x31,0xC0,0xC3}, 3, {}, false }

Patch g_actk[] = {
    P_RET("InjectionDetector.xva",        RVA_INJ_XUZ),
    P_RET("InjectionDetector.xve",        RVA_INJ_XVE),
    P_RET("InjectionDetector.xvf",        RVA_INJ_XVF),
    P_RET("InjectionDetector.xvg",        RVA_INJ_XVG),
    P_RET("InjectionDetector.xvh",        RVA_INJ_XVH),
    P_RET("InjectionDetector.xvi",        RVA_INJ_XVI),
    P_RET("ObscuredCheat.xuz",            RVA_OBS_XUZ),
    P_RET("ObscuredCheat.xvu",            RVA_OBS_XVU),
    P_RET_FALSE("ObscuredCheat.xvs",      RVA_OBS_XVS),
    P_RET_FALSE("ObscuredCheat.xvt",      RVA_OBS_XVT),
    P_RET("ObscuredCheat.xvq",            RVA_OBS_XVQ),
    P_RET("ObscuredCheat.xvr",            RVA_OBS_XVR),
    P_RET("SpeedHack.xuz",                RVA_SPD_XUZ),
    P_RET("SpeedHack.Update",             RVA_SPD_UPDATE),
    P_RET("SpeedHack.OnPause",            RVA_SPD_ONPAUSE),
    P_RET("SpeedHack.xwr",                RVA_SPD_XWR),
    P_RET("SpeedHack.xuv",                RVA_SPD_XUV),
    P_RET("TimeCheating.Update",          RVA_TIME_UPDATE),
    P_RET("TimeCheating.OnAppPause",      RVA_TIME_PAUSE),
    P_RET("vk.ffz_AddStat",               RVA_STAT_FFZ),
    P_RET("vk.hoa_AddStat",               RVA_STAT_HOA),
    P_RET("vk.jor_AddStat",               RVA_STAT_JOR),
    P_RET("vk.jox_SetStat",               RVA_STAT_JOX),
    P_RET("vk.joq_internal",              RVA_STAT_JOQ),
    P_RET("vk.oez_internal",              RVA_STAT_OEZ),
    P_RET_TRUE("Steam.SetStatInt32",       RVA_STEAM_SETSTAT_I32),
    P_RET_TRUE("Steam.SetStatFloat",       RVA_STEAM_SETSTAT_F32),
    P_RET_TRUE("Steam.SetAchievement",     RVA_STEAM_SETACHIEV),
    P_RET_TRUE("Steam.StoreStats",         RVA_STEAM_STORE),
    P_RET_TRUE("Steam.UpdateAvgStat",      RVA_STEAM_UPDAVG),
    P_RET_FALSE("Steam.FindOrCreateLB",    RVA_STEAM_LEADER_FOC),
    P_RET_FALSE("Steam.FindLeaderboard",   RVA_STEAM_LEADER_FIND),
    P_RET_FALSE("Steam.UploadLBScore",     RVA_STEAM_UPLOAD_SCORE),
    P_RET_TRUE("Steam.IndicAchProgress",   RVA_STEAM_ACH_PROGRESS),
    P_RET_FALSE("Steam.AttachLBUGC",       RVA_STEAM_ATTACH_UGC),
    P_RET_TRUE("Steam.ClearAchievement",   RVA_STEAM_CLEAR_ACHIEV),
    P_RET_TRUE("Steam.ResetAllStats",      RVA_STEAM_RESET_STATS),
    P_RET_TRUE("Steam.SetRichPresence",    RVA_STEAM_RICH_SET),
    P_RET("Steam.ClearRichPresence",       RVA_STEAM_RICH_CLEAR),
};
#define ACTK_COUNT (sizeof(g_actk)/sizeof(g_actk[0]))

Patch g_dlc[] = {
    P_RET_TRUE("DLC.IsSubscribed",  RVA_DLC_SUBSCRIBED),
    P_RET_TRUE("DLC.Available",     RVA_DLC_AVAILABLE),
    P_RET_TRUE("DLC.IsInstalled",   RVA_DLC_INSTALLED),
    P_RET_TRUE("DLCMgr.gxq",       RVA_DLC_CHECK),
};
#define DLC_COUNT (sizeof(g_dlc)/sizeof(g_dlc[0]))

Patch g_pet[] = {
    P_RET_TRUE("Pet.kei", RVA_PET_KEI),
    P_RET_TRUE("Pet.kem", RVA_PET_KEM),
    P_RET_TRUE("Pet.keo", RVA_PET_KEO),
    P_RET_TRUE("Pet.ker", RVA_PET_KER),
};
#define PET_COUNT (sizeof(g_pet)/sizeof(g_pet[0]))

int ApplyGroup(Patch* p, int cnt, const char* grp)
{
    (void)grp;
    int ok = 0;
    for (int i = 0; i < cnt; i++) {
        if (p[i].applied) { ok++; continue; }
        BYTE* addr = g_gaBase + p[i].rva;
        if (!RPM(addr, p[i].backup, p[i].patchLen)) continue;
        if (!WPM(addr, p[i].patch, p[i].patchLen)) continue;
        p[i].applied = true; ok++;
    }
    return ok;
}

int RestoreGroup(Patch* p, int cnt, const char* grp)
{
    (void)grp;
    int ok = 0;
    for (int i = 0; i < cnt; i++) {
        if (!p[i].applied) continue;
        if (WPM(g_gaBase + p[i].rva, p[i].backup, p[i].patchLen))
            { p[i].applied = false; ok++; }
    }
    return ok;
}

static int VerifyAndRepairGroup(Patch* p, int cnt)
{
    int repaired = 0;
    for (int i = 0; i < cnt; i++) {
        if (!p[i].applied) continue;
        BYTE current[8] = {};
        if (!RPM(g_gaBase + p[i].rva, current, p[i].patchLen)) continue;
        if (memcmp(current, p[i].patch, p[i].patchLen) != 0) {
            if (WPM(g_gaBase + p[i].rva, p[i].patch, p[i].patchLen)) repaired++;
        }
    }
    return repaired;
}

#define SPEED_MAX 25.0f

static bool g_speedActive = false;
static float g_curSpeed = 1.0f;
static BYTE* g_speedMem = nullptr;
static HANDLE g_speedThread = nullptr;

static BYTE* g_atkCave = nullptr;
static bool g_atkHookApplied = false;
static BYTE g_meyBackup[8] = {};
static BYTE g_gqiBackup[8] = {};

BYTE* AllocNear(BYTE* target, SIZE_T size)
{
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    uintptr_t t = (uintptr_t)target;
    uintptr_t gran = si.dwAllocationGranularity;
    uintptr_t minAddr = t > 0x70000000ULL ? t - 0x70000000ULL : (uintptr_t)si.lpMinimumApplicationAddress;
    uintptr_t maxAddr = t + 0x70000000ULL;
    if (maxAddr > (uintptr_t)si.lpMaximumApplicationAddress)
        maxAddr = (uintptr_t)si.lpMaximumApplicationAddress;
    uintptr_t hiUserEnd = (uintptr_t)si.lpMaximumApplicationAddress;
    if (maxAddr > hiUserEnd) maxAddr = hiUserEnd;

    uintptr_t addr = minAddr;
    addr = ((addr + gran - 1) / gran) * gran;

    while (addr < maxAddr) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQueryEx(g_hProcess, (LPCVOID)addr, &mbi, sizeof(mbi))) break;

        uintptr_t regionBase = (uintptr_t)mbi.BaseAddress;
        uintptr_t regionEnd = regionBase + mbi.RegionSize;

        if (mbi.State == MEM_FREE) {
            uintptr_t tryAddr = ((regionBase + gran - 1) / gran) * gran;
            while (tryAddr + size <= regionEnd && tryAddr < maxAddr) {
                long long delta = (long long)tryAddr - (long long)t;
                if (delta < -0x70000000LL || delta > 0x70000000LL) {
                    tryAddr += gran; continue;
                }
                BYTE* p = (BYTE*)VirtualAllocEx(g_hProcess, (void*)tryAddr, size,
                    MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
                if (p) return p;
                tryAddr += gran;
            }
        }
        addr = regionEnd;
        if (addr <= regionBase) break;
    }

    return nullptr;
}

bool RestoreAttackSync()
{
    if (!g_atkHookApplied) return true;
    bool ok = true;
    if (g_meyBackup[0]) {
        if (!WPM(g_gaBase + RVA_MEY_RANGE_HOOK, g_meyBackup, 7)) ok = false;
    }
    if (g_gqiBackup[0]) {
        if (!WPM(g_gaBase + RVA_GQI_ATK_HOOK, g_gqiBackup, GQI_ATK_PATCH_LEN)) ok = false;
    }
    if (g_atkCave) {
        VirtualFreeEx(g_hProcess, g_atkCave, 0, MEM_RELEASE);
        g_atkCave = nullptr;
    }
    g_atkHookApplied = false;
    memset(g_meyBackup, 0, sizeof(g_meyBackup));
    memset(g_gqiBackup, 0, sizeof(g_gqiBackup));
    return ok;
}

bool ApplyAttackSync(float scale, BYTE* rangeMultAddr, BYTE* attackMultAddr)
{
    if (scale <= 1.0f) return RestoreAttackSync();
    if (g_atkHookApplied) return true;

#if MEY_RANGE_HOOK_DISABLED
    (void)rangeMultAddr; (void)attackMultAddr;
    return false;
#endif

    BYTE* hookMey = g_gaBase + RVA_MEY_RANGE_HOOK;
    BYTE* hookGqi = g_gaBase + RVA_GQI_ATK_HOOK;

    BYTE meyExpect[7] = { 0xF3, 0x0F, 0x10, 0x53, 0x20, 0x45, 0x33 };
    BYTE gqiExpect[6] = { 0x0F, 0x10, 0x44, 0x24, 0x60, 0x66 };

    BYTE meyGot[7]{}, gqiGot[6]{};
    if (!RPM(hookMey, meyGot, 7) || memcmp(meyGot, meyExpect, 7) != 0) return false;
    if (!RPM(hookGqi, gqiGot, 6) || memcmp(gqiGot, gqiExpect, 6) != 0) return false;

    memcpy(g_meyBackup, meyGot, 7);
    memcpy(g_gqiBackup, gqiGot, GQI_ATK_PATCH_LEN);

    g_atkCave = AllocNear(hookMey, 4096);
    if (!g_atkCave) return false;

    BYTE* cave = g_atkCave;
    BYTE* meyCont = hookMey + 7;
    BYTE* gqiCont = hookGqi + GQI_ATK_PATCH_LEN;
    int p = 0;
    BYTE code[256]{};

    code[p++] = 0xF3; code[p++] = 0x0F; code[p++] = 0x10; code[p++] = 0x53; code[p++] = 0x20;
    code[p++] = 0x48; code[p++] = 0xB8; memcpy(&code[p], &rangeMultAddr, 8); p += 8;
    code[p++] = 0xF3; code[p++] = 0x0F; code[p++] = 0x59; code[p++] = 0x10;
    int meyJmpPos = p;
    code[p++] = 0xE9; p += 4;

    int gqiEntry = p;
    code[p++] = 0x0F; code[p++] = 0x10; code[p++] = 0x44; code[p++] = 0x24; code[p++] = 0x60;
    code[p++] = 0x48; code[p++] = 0xB8; memcpy(&code[p], &attackMultAddr, 8); p += 8;
    code[p++] = 0xF3; code[p++] = 0x0F; code[p++] = 0x59; code[p++] = 0x00;
    int gqiJmpPos = p;
    code[p++] = 0xE9; p += 4;

    int meyJmpRel = (int)(meyCont - (cave + meyJmpPos + 4));
    memcpy(&code[meyJmpPos], &meyJmpRel, 4);

    int gqiJmpRel = (int)(gqiCont - (cave + gqiJmpPos + 4));
    memcpy(&code[gqiJmpPos], &gqiJmpRel, 4);

    if (!WPM(cave, code, p)) {
        VirtualFreeEx(g_hProcess, g_atkCave, 0, MEM_RELEASE);
        g_atkCave = nullptr;
        return false;
    }

    BYTE meyJmp[MEY_RANGE_PATCH_LEN]{};
    meyJmp[0] = 0xE9;
    int meyHookRel = (int)(cave - (hookMey + 5));
    memcpy(&meyJmp[1], &meyHookRel, 4);

    BYTE gqiJmp[GQI_ATK_PATCH_LEN]{};
    gqiJmp[0] = 0xE9;
    int gqiHookRel = (int)((cave + gqiEntry) - (hookGqi + 5));
    memcpy(&gqiJmp[1], &gqiHookRel, 4);
    gqiJmp[5] = 0x90;

    if (!WPM(hookMey, meyJmp, MEY_RANGE_PATCH_LEN) || !WPM(hookGqi, gqiJmp, GQI_ATK_PATCH_LEN)) {
        WPM(hookMey, g_meyBackup, 7);
        VirtualFreeEx(g_hProcess, g_atkCave, 0, MEM_RELEASE);
        g_atkCave = nullptr;
        return false;
    }

    g_atkHookApplied = true;
    return true;
}

static void CalcSpeedParams(float scale, float* fdt, float* maxDT, int* sleepMs,
                            float* rangeMult, float* attackMult)
{
    *fdt = 0.02f / (scale * 4.0f);
    if (*fdt < 0.0001f) *fdt = 0.0001f;

    *maxDT = 0.33333334f / scale;
    if (*maxDT < 0.004f) *maxDT = 0.004f;
    if (*maxDT > 0.1f) *maxDT = 0.1f;

    if (scale >= 15.0f) *sleepMs = 16;
    else if (scale >= 8.0f) *sleepMs = 25;
    else if (scale >= 4.0f) *sleepMs = 35;
    else *sleepMs = 50;

    *rangeMult = 25.0f;

    *attackMult = 1.0f + (scale - 1.0f) * 0.35f;
    if (*attackMult > 12.0f) *attackMult = 12.0f;
}

bool SetSpeed(float scale)
{
    BYTE* setTS    = g_gaBase + RVA_SET_TIMESCALE;
    BYTE* setFDT   = g_gaBase + RVA_SET_FIXEDDELTATIME;
    BYTE* setMaxDT = g_gaBase + RVA_SET_MAXDELTATIME;

    BYTE test = 0;
    if (!RPM(setTS, &test, 1)) return false;

    if (g_speedMem) {
        int zero = 0;
        WPM(g_speedMem + 8, &zero, 4);
        if (g_speedThread) {
            WaitForSingleObject(g_speedThread, 2000);
            CloseHandle(g_speedThread);
            g_speedThread = nullptr;
        }
        VirtualFreeEx(g_hProcess, g_speedMem, 0, MEM_RELEASE);
        g_speedMem = nullptr;
    }
    RestoreAttackSync();

    if (scale == 1.0f) {
        BYTE* mem = (BYTE*)VirtualAllocEx(g_hProcess, nullptr, 4096,
            MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!mem) return false;

        float one = 1.0f;
        float defaultFDT = 0.02f;
        float defaultMaxDT = 0.33333334f;
        WPM(mem,     &one, 4);
        WPM(mem + 4, &defaultFDT, 4);
        WPM(mem + 8, &defaultMaxDT, 4);

        BYTE code[192]{}; int p = 0;
        auto emitCall = [&](BYTE* valPtr, BYTE* fn) {
            code[p++]=0x48; code[p++]=0x83; code[p++]=0xEC; code[p++]=0x28;
            code[p++]=0x48; code[p++]=0xB8; memcpy(&code[p], &valPtr, 8); p+=8;
            code[p++]=0xF3; code[p++]=0x0F; code[p++]=0x10; code[p++]=0x00;
            code[p++]=0x48; code[p++]=0xB8; memcpy(&code[p], &fn, 8); p+=8;
            code[p++]=0xFF; code[p++]=0xD0;
            code[p++]=0x48; code[p++]=0x83; code[p++]=0xC4; code[p++]=0x28;
        };

        emitCall(mem, setTS);
        emitCall(mem + 4, setFDT);
        emitCall(mem + 8, setMaxDT);
        code[p++]=0x31; code[p++]=0xC0;
        code[p++]=0xC3;

        BYTE* cAddr = mem + 128;
        WPM(cAddr, code, p);
        HANDLE ht = CreateRemoteThread(g_hProcess, nullptr, 0, (LPTHREAD_START_ROUTINE)cAddr, nullptr, 0, nullptr);
        if (ht) { WaitForSingleObject(ht, 2000); CloseHandle(ht); }
        VirtualFreeEx(g_hProcess, mem, 0, MEM_RELEASE);

        g_curSpeed = 1.0f;
        g_speedActive = false;
        return true;
    }

    BYTE* mem = (BYTE*)VirtualAllocEx(g_hProcess, nullptr, 4096,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!mem) return false;

    float fdt = 0.0f, maxDT = 0.0f, rangeMult = 1.0f, attackMult = 1.0f;
    int sleepMs = 50;
    CalcSpeedParams(scale, &fdt, &maxDT, &sleepMs, &rangeMult, &attackMult);

    int runFlag = 1;

    WPM(mem,      &scale,      4);
    WPM(mem + 4,  &fdt,        4);
    WPM(mem + 8,  &runFlag,    4);
    WPM(mem + 12, &sleepMs,    4);
    WPM(mem + 16, &maxDT,      4);
    WPM(mem + 20, &rangeMult,  4);
    WPM(mem + 24, &attackMult, 4);

    ApplyAttackSync(scale, mem + 20, mem + 24);

    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    BYTE* sleepAddr = (BYTE*)GetProcAddress(k32, "Sleep");

    BYTE* speedP    = mem;
    BYTE* fdtP      = mem + 4;
    BYTE* flagAddr  = mem + 8;
    BYTE* sleepMsP  = mem + 12;
    BYTE* maxDTP    = mem + 16;

    BYTE code[320]{};
    int p = 0;

    int loopStart = p;

    code[p++]=0x48; code[p++]=0xB8; memcpy(&code[p], &flagAddr, 8); p+=8;
    code[p++]=0x83; code[p++]=0x38; code[p++]=0x00;
    code[p++]=0x0F; code[p++]=0x84;
    int jePatchPos = p; p+=4;

    code[p++]=0x48; code[p++]=0x83; code[p++]=0xEC; code[p++]=0x28;

    code[p++]=0x48; code[p++]=0xB8; memcpy(&code[p], &speedP, 8); p+=8;
    code[p++]=0xF3; code[p++]=0x0F; code[p++]=0x10; code[p++]=0x00;
    code[p++]=0x48; code[p++]=0xB8; memcpy(&code[p], &setTS, 8); p+=8;
    code[p++]=0xFF; code[p++]=0xD0;

    code[p++]=0x48; code[p++]=0xB8; memcpy(&code[p], &fdtP, 8); p+=8;
    code[p++]=0xF3; code[p++]=0x0F; code[p++]=0x10; code[p++]=0x00;
    code[p++]=0x48; code[p++]=0xB8; memcpy(&code[p], &setFDT, 8); p+=8;
    code[p++]=0xFF; code[p++]=0xD0;

    code[p++]=0x48; code[p++]=0xB8; memcpy(&code[p], &maxDTP, 8); p+=8;
    code[p++]=0xF3; code[p++]=0x0F; code[p++]=0x10; code[p++]=0x00;
    code[p++]=0x48; code[p++]=0xB8; memcpy(&code[p], &setMaxDT, 8); p+=8;
    code[p++]=0xFF; code[p++]=0xD0;

    code[p++]=0x48; code[p++]=0xB8; memcpy(&code[p], &sleepMsP, 8); p+=8;
    code[p++]=0x8B; code[p++]=0x08;
    code[p++]=0x48; code[p++]=0xB8; memcpy(&code[p], &sleepAddr, 8); p+=8;
    code[p++]=0xFF; code[p++]=0xD0;

    code[p++]=0x48; code[p++]=0x83; code[p++]=0xC4; code[p++]=0x28;
    int jmpRel = loopStart - (p + 5);
    code[p++]=0xE9; memcpy(&code[p], &jmpRel, 4); p+=4;

    int exitPos = p;
    code[p++]=0x31; code[p++]=0xC0;
    code[p++]=0xC3;

    int jeRel = exitPos - (jePatchPos + 4);
    memcpy(&code[jePatchPos], &jeRel, 4);

    BYTE* cAddr = mem + 256;
    WPM(cAddr, code, p);

    g_speedThread = CreateRemoteThread(g_hProcess, nullptr, 0,
        (LPTHREAD_START_ROUTINE)cAddr, nullptr, 0, nullptr);
    if (!g_speedThread) {
        RestoreAttackSync();
        VirtualFreeEx(g_hProcess, mem, 0, MEM_RELEASE);
        return false;
    }

    g_speedMem = mem;
    g_curSpeed = scale;
    g_speedActive = true;
    return true;
}

Patch g_god[] = {
    P_RET("Hero.gnr (damage)", RVA_HERO_GNR),
};
#define GOD_COUNT (sizeof(g_god)/sizeof(g_god[0]))

static volatile bool g_watchdogRun = false;
static volatile int g_watchdogCycles = 0;
static volatile int g_watchdogRepairs = 0;
static HANDLE g_watchdogThread = nullptr;
static HANDLE g_watchdogGuardian = nullptr;

static int SafeVerify(Patch* arr, size_t n)
{
    if (!arr) return 0;
    return VerifyAndRepairGroup(arr, n);
}

DWORD WINAPI WatchdogThreadProc(LPVOID)
{
    while (g_watchdogRun) {
        int r = 0;
        r += SafeVerify(g_actk,      ACTK_COUNT);
        r += SafeVerify(g_dlc,       DLC_COUNT);
        r += SafeVerify(g_pet,       PET_COUNT);
        r += SafeVerify(g_god,       GOD_COUNT);
        if (r > 0) g_watchdogRepairs += r;
        g_watchdogCycles++;
        for (int i = 0; i < 5 && g_watchdogRun; i++) Sleep(100);
    }
    return 0;
}

DWORD WINAPI WatchdogGuardianProc(LPVOID)
{
    while (g_watchdogRun) {
        Sleep(2000);
        if (!g_watchdogRun) break;
        if (g_watchdogThread) {
            DWORD code = STILL_ACTIVE;
            if (GetExitCodeThread(g_watchdogThread, &code) && code != STILL_ACTIVE) {
                CloseHandle(g_watchdogThread);
                g_watchdogThread = CreateThread(nullptr, 0, WatchdogThreadProc, nullptr, 0, nullptr);
            }
        }
    }
    return 0;
}

void StartWatchdog()
{
    if (g_watchdogThread) return;
    g_watchdogRun = true;
    g_watchdogCycles = 0;
    g_watchdogRepairs = 0;
    g_watchdogThread   = CreateThread(nullptr, 0, WatchdogThreadProc,   nullptr, 0, nullptr);
    g_watchdogGuardian = CreateThread(nullptr, 0, WatchdogGuardianProc, nullptr, 0, nullptr);
}

void StopWatchdog()
{
    g_watchdogRun = false;
    if (g_watchdogThread) {
        WaitForSingleObject(g_watchdogThread, 2000);
        CloseHandle(g_watchdogThread);
        g_watchdogThread = nullptr;
    }
    if (g_watchdogGuardian) {
        WaitForSingleObject(g_watchdogGuardian, 3000);
        CloseHandle(g_watchdogGuardian);
        g_watchdogGuardian = nullptr;
    }
}

static int g_goldAmount = 0;
static bool g_goldApplied = false;

static BYTE* g_goldCapturePage = nullptr;
static BYTE* g_goldCaptureCave = nullptr;
static bool g_goldHookInstalled = false;
static BYTE g_goldHookBackup[GOLD_HOOK_PATCH_LEN] = {};

bool SetGoldMultiplier(long long mult);
static bool RestoreGoldHack() { return SetGoldMultiplier(1); }
static bool ApplyGoldHack(int mult) { return SetGoldMultiplier(mult); }

static int g_backupCount = 0;

int BackupSaveFile()
{
    wchar_t appdata[MAX_PATH]{};
    if (!SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, appdata)))
        return 0;

    wchar_t saveDir[MAX_PATH]{};
    swprintf_s(saveDir, L"%s\\..\\LocalLow\\TesseractStudio\\TaskbarHero", appdata);
    DWORD attrs = GetFileAttributesW(saveDir);
    if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        swprintf_s(saveDir, L"%s\\..\\LocalLow\\Aki\\Task Bar Hero", appdata);
    }

    wchar_t backupRoot[MAX_PATH]{};
    swprintf_s(backupRoot, L"%s\\trainer_backup", saveDir);
    CreateDirectoryW(backupRoot, nullptr);

    SYSTEMTIME st{};
    GetLocalTime(&st);
    wchar_t backupDir[MAX_PATH]{};
    swprintf_s(backupDir, L"%s\\%04u-%02u-%02u_%02u-%02u-%02u",
        backupRoot, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    CreateDirectoryW(backupDir, nullptr);

    WIN32_FIND_DATAW fd{};
    wchar_t searchPath[MAX_PATH]{};
    swprintf_s(searchPath, L"%s\\*", saveDir);
    HANDLE hFind = FindFirstFileW(searchPath, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return 0;

    int copied = 0;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (wcsstr(fd.cFileName, L".bak") != nullptr) continue;
        wchar_t src[MAX_PATH]{}, dst[MAX_PATH]{};
        swprintf_s(src, L"%s\\%s", saveDir, fd.cFileName);
        swprintf_s(dst, L"%s\\%s", backupDir, fd.cFileName);
        if (CopyFileW(src, dst, FALSE)) copied++;
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);

    g_backupCount = copied;
    return copied;
}

static char g_goldHookErr[128] = "";

bool InstallGoldCaptureHook()
{
    if (g_goldHookInstalled) return true;

    BYTE* hookAddr = g_gaBase + RVA_GOLD_IKV_FUNC_ENTRY;

    BYTE expect[GOLD_HOOK_PATCH_LEN] = { 0x48, 0x89, 0x5C, 0x24, 0x10 };
    BYTE got[GOLD_HOOK_PATCH_LEN]{};
    if (!RPM(hookAddr, got, GOLD_HOOK_PATCH_LEN)) {
        sprintf_s(g_goldHookErr, "RPM hookAddr fail err=%lu", GetLastError());
        return false;
    }
    if (memcmp(got, expect, GOLD_HOOK_PATCH_LEN) != 0) {
        if (got[0] == 0xE9) {
            if (!WPM(hookAddr, expect, GOLD_HOOK_PATCH_LEN)) {
                sprintf_s(g_goldHookErr, "self-heal WPM fail err=%lu", GetLastError());
                return false;
            }
            Sleep(50);
            BYTE verify[GOLD_HOOK_PATCH_LEN]{};
            if (!RPM(hookAddr, verify, GOLD_HOOK_PATCH_LEN) ||
                memcmp(verify, expect, GOLD_HOOK_PATCH_LEN) != 0) {
                sprintf_s(g_goldHookErr, "self-heal verify fail");
                return false;
            }
            memcpy(got, expect, GOLD_HOOK_PATCH_LEN);
        } else {
            sprintf_s(g_goldHookErr, "byte mismatch %02X%02X%02X%02X%02X",
                got[0],got[1],got[2],got[3],got[4]);
            return false;
        }
    }

    g_goldCapturePage = (BYTE*)VirtualAllocEx(g_hProcess, nullptr, 4096,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!g_goldCapturePage) {
        sprintf_s(g_goldHookErr, "capture page alloc fail err=%lu", GetLastError());
        return false;
    }

    BYTE init[32]{};
    long long defaultMult = 1;
    memcpy(init + 16, &defaultMult, 8);
    WPM(g_goldCapturePage, init, 32);

    g_goldCaptureCave = AllocNear(hookAddr, 4096);
    if (!g_goldCaptureCave) {
        sprintf_s(g_goldHookErr, "cave alloc fail err=%lu", GetLastError());
        VirtualFreeEx(g_hProcess, g_goldCapturePage, 0, MEM_RELEASE);
        g_goldCapturePage = nullptr;
        return false;
    }

    long long dist = (long long)(g_goldCaptureCave - hookAddr);
    if (dist < -0x7FFFFFF0LL || dist > 0x7FFFFFF0LL) {
        sprintf_s(g_goldHookErr, "cave too far: dist=%lld", dist);
        VirtualFreeEx(g_hProcess, g_goldCaptureCave, 0, MEM_RELEASE);
        VirtualFreeEx(g_hProcess, g_goldCapturePage, 0, MEM_RELEASE);
        g_goldCaptureCave = nullptr;
        g_goldCapturePage = nullptr;
        return false;
    }

    BYTE* captureThis   = g_goldCapturePage;
    BYTE* captureFlag   = g_goldCapturePage + 8;
    BYTE* multAddr      = g_goldCapturePage + 16;
    BYTE* captureMethod = g_goldCapturePage + 24;

    BYTE code[160]{};
    int p = 0;

    code[p++]=0x48; code[p++]=0x89; code[p++]=0x5C; code[p++]=0x24; code[p++]=0x10;

    code[p++]=0x48; code[p++]=0xB8; memcpy(&code[p], &captureThis, 8); p+=8;
    code[p++]=0x48; code[p++]=0x89; code[p++]=0x08;

    code[p++]=0x48; code[p++]=0xB8; memcpy(&code[p], &captureMethod, 8); p+=8;
    code[p++]=0x4C; code[p++]=0x89; code[p++]=0x08;

    code[p++]=0x48; code[p++]=0xB8; memcpy(&code[p], &captureFlag, 8); p+=8;
    code[p++]=0xC6; code[p++]=0x00; code[p++]=0x01;

    code[p++]=0x48; code[p++]=0xB8; memcpy(&code[p], &multAddr, 8); p+=8;
    code[p++]=0x48; code[p++]=0x8B; code[p++]=0x00;
    code[p++]=0x48; code[p++]=0x83; code[p++]=0xF8; code[p++]=0x01;
    code[p++]=0x7E; code[p++]=0x04;
    code[p++]=0x48; code[p++]=0x0F; code[p++]=0xAF; code[p++]=0xD0;

    BYTE* backTarget = hookAddr + GOLD_HOOK_PATCH_LEN;
    int relBack = (int)(backTarget - (g_goldCaptureCave + p + 5));
    code[p++] = 0xE9;
    memcpy(&code[p], &relBack, 4); p += 4;

    if (!WPM(g_goldCaptureCave, code, p)) {
        sprintf_s(g_goldHookErr, "WPM cave fail err=%lu", GetLastError());
        VirtualFreeEx(g_hProcess, g_goldCaptureCave, 0, MEM_RELEASE);
        VirtualFreeEx(g_hProcess, g_goldCapturePage, 0, MEM_RELEASE);
        g_goldCaptureCave = nullptr;
        g_goldCapturePage = nullptr;
        return false;
    }

    memcpy(g_goldHookBackup, got, GOLD_HOOK_PATCH_LEN);

    BYTE hookPatch[GOLD_HOOK_PATCH_LEN]{};
    hookPatch[0] = 0xE9;
    int relHook = (int)(g_goldCaptureCave - (hookAddr + 5));
    memcpy(&hookPatch[1], &relHook, 4);

    if (!WPM(hookAddr, hookPatch, GOLD_HOOK_PATCH_LEN)) {
        sprintf_s(g_goldHookErr, "WPM hook fail err=%lu", GetLastError());
        VirtualFreeEx(g_hProcess, g_goldCaptureCave, 0, MEM_RELEASE);
        VirtualFreeEx(g_hProcess, g_goldCapturePage, 0, MEM_RELEASE);
        g_goldCaptureCave = nullptr;
        g_goldCapturePage = nullptr;
        return false;
    }

    g_goldHookInstalled = true;
    g_goldHookErr[0] = 0;
    return true;
}

bool SetGoldMultiplier(long long mult)
{
    if (!g_goldHookInstalled) {
        if (!InstallGoldCaptureHook()) return false;
    }
    if (mult < 1) mult = 1;
    if (mult > 5000) mult = 5000;
    BYTE* multAddr = g_goldCapturePage + 16;
    if (!WPM(multAddr, &mult, 8)) return false;
    g_goldAmount = (int)mult;
    g_goldApplied = (mult > 1);
    return true;
}

bool UninstallGoldCaptureHook()
{
    if (!g_goldHookInstalled) return true;
    BYTE* hookAddr = g_gaBase + RVA_GOLD_IKV_FUNC_ENTRY;
    WPM(hookAddr, g_goldHookBackup, GOLD_HOOK_PATCH_LEN);
    Sleep(100);
    if (g_goldCaptureCave) {
        VirtualFreeEx(g_hProcess, g_goldCaptureCave, 0, MEM_RELEASE);
        g_goldCaptureCave = nullptr;
    }
    if (g_goldCapturePage) {
        VirtualFreeEx(g_hProcess, g_goldCapturePage, 0, MEM_RELEASE);
        g_goldCapturePage = nullptr;
    }
    g_goldHookInstalled = false;
    return true;
}

static int   g_heroXpMult = 1;
static bool  g_heroXpApplied = false;
static BYTE* g_heroXpPage = nullptr;
static bool  g_heroXpHookInstalled = false;
static char  g_heroXpErr[128] = "";

#define HEROXP_TARGETS 8
static const DWORD g_heroXpRvas[HEROXP_TARGETS] = {
    RVA_HERO_XP_EKQ, RVA_HERO_XP_EXG, RVA_HERO_XP_JIU, RVA_HERO_XP_KUD,
    RVA_HERO_XP_JIW, RVA_HERO_XP_NGR, RVA_HERO_XP_OBV, RVA_HERO_XP_JOH
};
static BYTE* g_heroXpCave[HEROXP_TARGETS]    = {};
static BYTE  g_heroXpBackup[HEROXP_TARGETS][16] = {};
static int   g_heroXpPatchLen[HEROXP_TARGETS] = {};
static bool  g_heroXpOneDone[HEROXP_TARGETS]  = {};

static int RelocatePrologue(const BYTE* b, int need, BYTE* hookAddr, BYTE* caveAt,
    int caveStartOff, BYTE* out, int* outLen, char* err)
{
    int i = 0, o = 0;
    while (i < need) {
        BYTE op = b[i];
        if (op >= 0x40 && op <= 0x4F) {
            BYTE op2 = b[i + 1];
            if (op2 >= 0x50 && op2 <= 0x5F) { out[o++] = op; out[o++] = op2; i += 2; continue; }
            if (op2 == 0x83 && b[i + 2] == 0xEC) {
                out[o++] = op; out[o++] = 0x83; out[o++] = 0xEC; out[o++] = b[i + 3]; i += 4; continue;
            }
            if (op2 == 0x89 && b[i + 2] == 0x5C && b[i + 3] == 0x24) {
                for (int k = 0; k < 5; k++) out[o++] = b[i + k];
                i += 5; continue;
            }
            sprintf_s(err, 96, "reloc: rex op2=%02X @+%d", op2, i);
            return 0;
        }
        if (op >= 0x50 && op <= 0x5F) { out[o++] = op; i++; continue; }
        if (op == 0x80 && b[i + 1] == 0x3D) {
            int d = *(int*)(b + i + 2);
            BYTE imm = b[i + 6];
            BYTE* tgt = hookAddr + (i + 7) + d;
            out[o++] = 0x80; out[o++] = 0x3D;
            int nd = (int)(tgt - (caveAt + caveStartOff + o + 4 + 1));
            memcpy(out + o, &nd, 4); o += 4;
            out[o++] = imm;
            i += 7; continue;
        }
        if (op == 0xE9) { sprintf_s(err, 96, "already hooked (E9)"); return 0; }
        sprintf_s(err, 96, "reloc: bilinmeyen op=%02X @+%d", op, i);
        return 0;
    }
    *outLen = o;
    return i;
}

static bool HookOneHeroXp(int idx)
{
    if (g_heroXpRvas[idx] == 0) return false;
    BYTE* hookAddr = g_gaBase + g_heroXpRvas[idx];

    BYTE got[16]{};
    if (!RPM(hookAddr, got, 16)) {
        sprintf_s(g_heroXpErr, "RPM fail t%d err=%lu", idx, GetLastError());
        return false;
    }
    if (got[0] == 0xE9) {
        sprintf_s(g_heroXpErr, "t%d already hooked, restart the game", idx);
        return false;
    }

    BYTE* cave = AllocNear(hookAddr, 4096);
    if (!cave) {
        sprintf_s(g_heroXpErr, "cave alloc fail t%d err=%lu", idx, GetLastError());
        return false;
    }
    long long dist = (long long)(cave - hookAddr);
    if (dist < -0x7FFFFFF0LL || dist > 0x7FFFFFF0LL) {
        sprintf_s(g_heroXpErr, "cave too far t%d: %lld", idx, dist);
        VirtualFreeEx(g_hProcess, cave, 0, MEM_RELEASE);
        return false;
    }

    BYTE code[128]{};
    int p = 0;

    BYTE* multAddrTmp = (BYTE*)1;
    code[p++] = 0x48; code[p++] = 0xB8;
    int multImmPos = p; memcpy(&code[p], &multAddrTmp, 8); p += 8;
    code[p++] = 0xF3; code[p++] = 0x0F; code[p++] = 0x59; code[p++] = 0x08;

    BYTE relocOut[64]{};
    int relocLen = 0;
    char rerr[96] = "";
    int patchLen = RelocatePrologue(got, 5, hookAddr, cave, p,
        relocOut, &relocLen, rerr);
    if (patchLen < 5) {
        sprintf_s(g_heroXpErr, "t%d %s (b=%02X%02X%02X%02X%02X)", idx,
            rerr[0] ? rerr : "reloc fail", got[0], got[1], got[2], got[3], got[4]);
        VirtualFreeEx(g_hProcess, cave, 0, MEM_RELEASE);
        return false;
    }

    memcpy(code + p, relocOut, relocLen); p += relocLen;

    BYTE* backTarget = hookAddr + patchLen;
    int relBack = (int)(backTarget - (cave + p + 5));
    code[p++] = 0xE9; memcpy(&code[p], &relBack, 4); p += 4;

    BYTE* multAddr = g_heroXpPage;
    memcpy(code + multImmPos, &multAddr, 8);

    if (!WPM(cave, code, p)) {
        sprintf_s(g_heroXpErr, "WPM cave fail t%d err=%lu", idx, GetLastError());
        VirtualFreeEx(g_hProcess, cave, 0, MEM_RELEASE);
        return false;
    }

    memcpy(g_heroXpBackup[idx], got, patchLen);
    g_heroXpPatchLen[idx] = patchLen;

    BYTE hookPatch[16];
    for (int i = 0; i < patchLen; i++) hookPatch[i] = 0x90;
    hookPatch[0] = 0xE9;
    int relHook = (int)(cave - (hookAddr + 5));
    memcpy(&hookPatch[1], &relHook, 4);

    if (!WPM(hookAddr, hookPatch, patchLen)) {
        sprintf_s(g_heroXpErr, "WPM hook fail t%d err=%lu", idx, GetLastError());
        VirtualFreeEx(g_hProcess, cave, 0, MEM_RELEASE);
        return false;
    }

    g_heroXpCave[idx] = cave;
    g_heroXpOneDone[idx] = true;
    return true;
}

bool InstallHeroXpHook()
{
    if (g_heroXpHookInstalled) return true;

    if (!g_heroXpPage) {
        g_heroXpPage = (BYTE*)VirtualAllocEx(g_hProcess, nullptr, 4096,
            MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!g_heroXpPage) {
            sprintf_s(g_heroXpErr, "page alloc fail err=%lu", GetLastError());
            return false;
        }
        float defMult = 1.0f;
        WPM(g_heroXpPage, &defMult, 4);
    }

    int ok = 0;
    for (int i = 0; i < HEROXP_TARGETS; i++) {
        if (g_heroXpOneDone[i]) { ok++; continue; }
        if (HookOneHeroXp(i)) ok++;
    }

    if (ok == 0) return false;

    g_heroXpHookInstalled = true;
    if (ok == HEROXP_TARGETS) g_heroXpErr[0] = 0;
    return true;
}

bool SetHeroXpMultiplier(int mult)
{
    if (!g_heroXpHookInstalled) {
        if (!InstallHeroXpHook()) return false;
    }
    if (mult < 1)    mult = 1;
    if (mult > 1000) mult = 1000;
    float fmult = (float)mult;
    if (!WPM(g_heroXpPage, &fmult, 4)) return false;
    g_heroXpMult = mult;
    g_heroXpApplied = (mult > 1);
    return true;
}

bool UninstallHeroXpHook()
{
    if (!g_heroXpHookInstalled) return true;
    for (int i = 0; i < HEROXP_TARGETS; i++) {
        if (!g_heroXpOneDone[i]) continue;
        if (g_heroXpRvas[i] == 0) continue;
        BYTE* hookAddr = g_gaBase + g_heroXpRvas[i];
        WPM(hookAddr, g_heroXpBackup[i], g_heroXpPatchLen[i]);
    }
    Sleep(100);
    for (int i = 0; i < HEROXP_TARGETS; i++) {
        if (g_heroXpCave[i]) {
            VirtualFreeEx(g_hProcess, g_heroXpCave[i], 0, MEM_RELEASE);
            g_heroXpCave[i] = nullptr;
        }
        g_heroXpOneDone[i] = false;
    }
    if (g_heroXpPage) {
        VirtualFreeEx(g_hProcess, g_heroXpPage, 0, MEM_RELEASE);
        g_heroXpPage = nullptr;
    }
    g_heroXpHookInstalled = false;
    g_heroXpApplied = false;
    return true;
}

#define RVA_CUBE_XP_PREKMT     0x8B9C89
#define RVA_CUBE_XP_PENALTY    0x9FDAE0
#define RVA_CUBE_XP_LVL_SKIP   0x8B9C4A
#define CUBE_XP_PREKMT_PATCH   8
#define CUBE_XP_PENALTY_PATCH  5
#define CUBE_XP_LVL_SKIP_PATCH 6

static BYTE*  g_cubeXpPage       = nullptr;
static BYTE*  g_cubeXpCave       = nullptr;
static BYTE   g_cubeXpBackup[12] = {};
static bool   g_cubeXpInstalled  = false;

static BYTE*  g_cubeXpPenCave    = nullptr;
static BYTE   g_cubeXpPenBackup[8]= {};
static bool   g_cubeXpPenInstalled = false;

static BYTE   g_cubeXpSkipBackup[CUBE_XP_LVL_SKIP_PATCH] = {};
static bool   g_cubeXpSkipPatched = false;

static char   g_cubeXpErr[160]   = "";

static bool   g_cubeXpApplied    = false;
static int    g_cubeXpMult       = 1;
static int    g_uiCubeXpVal      = 1000;

#define RVA_MONSTER_TAKEDMG    0xC067B0

static BYTE*  g_oneShotPage     = nullptr;
static BYTE*  g_oneShotCave     = nullptr;
static BYTE   g_oneShotBackup[16]= {};
static int    g_oneShotPatchLen = 0;
static bool   g_oneShotInstalled= false;
static bool   g_oneShotEnabled  = false;
static char   g_oneShotErr[160] = "";

static bool PatchCubeXpLevelSkip()
{
    if (g_cubeXpSkipPatched) return true;
    BYTE* patchAddr = g_gaBase + RVA_CUBE_XP_LVL_SKIP;

    BYTE got[CUBE_XP_LVL_SKIP_PATCH]{};
    if (!RPM(patchAddr, got, CUBE_XP_LVL_SKIP_PATCH)) {
        sprintf_s(g_cubeXpErr, "lvl-skip RPM fail err=%lu", GetLastError());
        return false;
    }
    if (got[0] == 0x90 && got[1] == 0x90) return true;
    if (got[0] != 0x0F || got[1] != 0x84 || got[2] != 0x05 || got[3] != 0x01) {
        sprintf_s(g_cubeXpErr, "lvl-skip sig mismatch (%02X%02X%02X%02X...)",
            got[0], got[1], got[2], got[3]);
        return false;
    }

    memcpy(g_cubeXpSkipBackup, got, CUBE_XP_LVL_SKIP_PATCH);
    BYTE nops[CUBE_XP_LVL_SKIP_PATCH]{ 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
    if (!WPM(patchAddr, nops, CUBE_XP_LVL_SKIP_PATCH)) {
        sprintf_s(g_cubeXpErr, "lvl-skip WPM fail err=%lu", GetLastError());
        return false;
    }
    g_cubeXpSkipPatched = true;
    return true;
}

static void UnpatchCubeXpLevelSkip()
{
    if (!g_cubeXpSkipPatched) return;
    BYTE* patchAddr = g_gaBase + RVA_CUBE_XP_LVL_SKIP;
    WPM(patchAddr, g_cubeXpSkipBackup, CUBE_XP_LVL_SKIP_PATCH);
    g_cubeXpSkipPatched = false;
}

static bool InstallCubeXpPenaltyBypass()
{
    if (g_cubeXpPenInstalled) return true;
    BYTE* hookAddr = g_gaBase + RVA_CUBE_XP_PENALTY;

    BYTE got[CUBE_XP_PENALTY_PATCH]{};
    if (!RPM(hookAddr, got, CUBE_XP_PENALTY_PATCH)) {
        sprintf_s(g_cubeXpErr, "penalty RPM fail err=%lu", GetLastError());
        return false;
    }
    if (got[0] == 0xE9) {
        sprintf_s(g_cubeXpErr, "penalty already hooked, restart game");
        return false;
    }
    if (got[0] != 0x40 || got[1] != 0x53 || got[2] != 0x48 || got[3] != 0x83 || got[4] != 0xEC) {
        sprintf_s(g_cubeXpErr, "penalty sig mismatch (%02X%02X%02X%02X%02X)",
            got[0], got[1], got[2], got[3], got[4]);
        return false;
    }

    g_cubeXpPenCave = AllocNear(hookAddr, 4096);
    if (!g_cubeXpPenCave) {
        sprintf_s(g_cubeXpErr, "penalty cave alloc fail err=%lu", GetLastError());
        return false;
    }
    long long dist = (long long)(g_cubeXpPenCave - hookAddr);
    if (dist < -0x7FFFFFF0LL || dist > 0x7FFFFFF0LL) {
        sprintf_s(g_cubeXpErr, "penalty cave too far: %lld", dist);
        VirtualFreeEx(g_hProcess, g_cubeXpPenCave, 0, MEM_RELEASE);
        g_cubeXpPenCave = nullptr;
        return false;
    }

    BYTE code[16]{ 0x0F, 0x28, 0xC1, 0xC3 };

    if (!WPM(g_cubeXpPenCave, code, 4)) {
        sprintf_s(g_cubeXpErr, "penalty WPM cave fail err=%lu", GetLastError());
        VirtualFreeEx(g_hProcess, g_cubeXpPenCave, 0, MEM_RELEASE);
        g_cubeXpPenCave = nullptr;
        return false;
    }

    memcpy(g_cubeXpPenBackup, got, CUBE_XP_PENALTY_PATCH);

    BYTE jmp[CUBE_XP_PENALTY_PATCH]{};
    jmp[0] = 0xE9;
    int rel = (int)(g_cubeXpPenCave - (hookAddr + 5));
    memcpy(&jmp[1], &rel, 4);

    if (!WPM(hookAddr, jmp, CUBE_XP_PENALTY_PATCH)) {
        sprintf_s(g_cubeXpErr, "penalty WPM hook fail err=%lu", GetLastError());
        VirtualFreeEx(g_hProcess, g_cubeXpPenCave, 0, MEM_RELEASE);
        g_cubeXpPenCave = nullptr;
        return false;
    }

    g_cubeXpPenInstalled = true;
    return true;
}

static void UninstallCubeXpPenaltyBypass()
{
    if (!g_cubeXpPenInstalled) return;
    BYTE* hookAddr = g_gaBase + RVA_CUBE_XP_PENALTY;
    WPM(hookAddr, g_cubeXpPenBackup, CUBE_XP_PENALTY_PATCH);
    Sleep(40);
    if (g_cubeXpPenCave) {
        VirtualFreeEx(g_hProcess, g_cubeXpPenCave, 0, MEM_RELEASE);
        g_cubeXpPenCave = nullptr;
    }
    g_cubeXpPenInstalled = false;
}

bool InstallCubeXpHook()
{
    if (!PatchCubeXpLevelSkip()) return false;
    if (g_cubeXpInstalled) return true;
    BYTE* hookAddr = g_gaBase + RVA_CUBE_XP_PREKMT;

    BYTE got[CUBE_XP_PREKMT_PATCH]{};
    if (!RPM(hookAddr, got, CUBE_XP_PREKMT_PATCH)) {
        sprintf_s(g_cubeXpErr, "RPM fail err=%lu", GetLastError());
        UnpatchCubeXpLevelSkip();
        return false;
    }
    if (got[0] == 0xE9) {
        sprintf_s(g_cubeXpErr, "already hooked, restart the game");
        UnpatchCubeXpLevelSkip();
        return false;
    }
    if (got[0] != 0x0F || got[1] != 0x28 || got[2] != 0xCE || got[3] != 0xE8) {
        sprintf_s(g_cubeXpErr, "sig mismatch (%02X%02X%02X%02X...)",
            got[0], got[1], got[2], got[3]);
        UnpatchCubeXpLevelSkip();
        return false;
    }

    if (!g_cubeXpPage) {
        g_cubeXpPage = (BYTE*)VirtualAllocEx(g_hProcess, nullptr, 4096,
            MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!g_cubeXpPage) {
            sprintf_s(g_cubeXpErr, "page alloc fail err=%lu", GetLastError());
            UnpatchCubeXpLevelSkip();
            return false;
        }
        float defMult = 1.0f;
        WPM(g_cubeXpPage, &defMult, 4);
    }

    g_cubeXpCave = AllocNear(hookAddr, 4096);
    if (!g_cubeXpCave) {
        sprintf_s(g_cubeXpErr, "cave alloc fail err=%lu", GetLastError());
        UnpatchCubeXpLevelSkip();
        return false;
    }
    long long dist = (long long)(g_cubeXpCave - hookAddr);
    if (dist < -0x7FFFFFF0LL || dist > 0x7FFFFFF0LL) {
        sprintf_s(g_cubeXpErr, "cave too far: %lld", dist);
        VirtualFreeEx(g_hProcess, g_cubeXpCave, 0, MEM_RELEASE);
        g_cubeXpCave = nullptr;
        UnpatchCubeXpLevelSkip();
        return false;
    }

    BYTE code[128]{};
    int p = 0;

    code[p++] = 0x48; code[p++] = 0xB8;
    BYTE* multAddr = g_cubeXpPage;
    memcpy(&code[p], &multAddr, 8); p += 8;
    code[p++] = 0xF3; code[p++] = 0x0F; code[p++] = 0x59; code[p++] = 0x30;
    code[p++] = 0xFF; code[p++] = 0x40; code[p++] = 0x04;
    code[p++] = 0x0F; code[p++] = 0x28; code[p++] = 0xCE;
    int origCallRel = *(int*)&got[4];
    BYTE* kmtTarget = hookAddr + CUBE_XP_PREKMT_PATCH + origCallRel;
    int callPos = p;
    code[p++] = 0xE8;
    int callRel = (int)(kmtTarget - (g_cubeXpCave + callPos + 5));
    memcpy(&code[p], &callRel, 4); p += 4;
    BYTE* backTarget = hookAddr + CUBE_XP_PREKMT_PATCH;
    int relBack = (int)(backTarget - (g_cubeXpCave + p + 5));
    code[p++] = 0xE9; memcpy(&code[p], &relBack, 4); p += 4;

    if (!WPM(g_cubeXpCave, code, p)) {
        sprintf_s(g_cubeXpErr, "WPM cave fail err=%lu", GetLastError());
        VirtualFreeEx(g_hProcess, g_cubeXpCave, 0, MEM_RELEASE);
        g_cubeXpCave = nullptr;
        UnpatchCubeXpLevelSkip();
        return false;
    }

    memcpy(g_cubeXpBackup, got, CUBE_XP_PREKMT_PATCH);

    BYTE jmp[CUBE_XP_PREKMT_PATCH]{};
    jmp[0] = 0xE9;
    int rel = (int)(g_cubeXpCave - (hookAddr + 5));
    memcpy(&jmp[1], &rel, 4);
    for (int i = 5; i < CUBE_XP_PREKMT_PATCH; i++) jmp[i] = 0x90;

    if (!WPM(hookAddr, jmp, CUBE_XP_PREKMT_PATCH)) {
        sprintf_s(g_cubeXpErr, "WPM hook fail err=%lu", GetLastError());
        VirtualFreeEx(g_hProcess, g_cubeXpCave, 0, MEM_RELEASE);
        g_cubeXpCave = nullptr;
        UnpatchCubeXpLevelSkip();
        return false;
    }

    if (!InstallCubeXpPenaltyBypass()) {
        WPM(hookAddr, g_cubeXpBackup, CUBE_XP_PREKMT_PATCH);
        VirtualFreeEx(g_hProcess, g_cubeXpCave, 0, MEM_RELEASE);
        g_cubeXpCave = nullptr;
        UnpatchCubeXpLevelSkip();
        return false;
    }

    g_cubeXpInstalled = true;
    g_cubeXpErr[0] = 0;
    return true;
}

bool SetCubeXpMultiplier(int mult)
{
    if (!g_cubeXpInstalled) {
        if (!InstallCubeXpHook()) return false;
    }
    if (mult < 1)      mult = 1;
    if (mult > 100000) mult = 100000;
    float fmult = (float)mult;
    if (!WPM(g_cubeXpPage, &fmult, 4)) return false;
    g_cubeXpMult    = mult;
    g_cubeXpApplied = (mult > 1);
    return true;
}

bool UninstallCubeXpHook()
{
    if (!g_cubeXpInstalled && !g_cubeXpPenInstalled && !g_cubeXpSkipPatched) return true;
    if (g_cubeXpInstalled) {
        BYTE* hookAddr = g_gaBase + RVA_CUBE_XP_PREKMT;
        WPM(hookAddr, g_cubeXpBackup, CUBE_XP_PREKMT_PATCH);
        Sleep(40);
        if (g_cubeXpCave) {
            VirtualFreeEx(g_hProcess, g_cubeXpCave, 0, MEM_RELEASE);
            g_cubeXpCave = nullptr;
        }
        g_cubeXpInstalled = false;
    }
    UninstallCubeXpPenaltyBypass();
    UnpatchCubeXpLevelSkip();
    if (g_cubeXpPage) {
        VirtualFreeEx(g_hProcess, g_cubeXpPage, 0, MEM_RELEASE);
        g_cubeXpPage = nullptr;
    }
    g_cubeXpApplied = false;
    return true;
}

bool InstallOneShotHook()
{
    if (g_oneShotInstalled) return true;
    BYTE* hookAddr = g_gaBase + RVA_MONSTER_TAKEDMG;

    BYTE got[24]{};
    if (!RPM(hookAddr, got, 24)) {
        sprintf_s(g_oneShotErr, "RPM fail err=%lu", GetLastError());
        return false;
    }
    if (got[0] == 0xE9) {
        sprintf_s(g_oneShotErr, "already hooked, restart the game");
        return false;
    }

    if (!g_oneShotPage) {
        g_oneShotPage = (BYTE*)VirtualAllocEx(g_hProcess, nullptr, 4096,
            MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!g_oneShotPage) {
            sprintf_s(g_oneShotErr, "page alloc fail err=%lu", GetLastError());
            return false;
        }
        float defMult = 1.0f;
        WPM(g_oneShotPage, &defMult, 4);
    }

    g_oneShotCave = AllocNear(hookAddr, 4096);
    if (!g_oneShotCave) {
        sprintf_s(g_oneShotErr, "cave alloc fail err=%lu", GetLastError());
        VirtualFreeEx(g_hProcess, g_oneShotPage, 0, MEM_RELEASE);
        g_oneShotPage = nullptr;
        return false;
    }
    long long dist = (long long)(g_oneShotCave - hookAddr);
    if (dist < -0x7FFFFFF0LL || dist > 0x7FFFFFF0LL) {
        sprintf_s(g_oneShotErr, "cave too far: %lld", dist);
        VirtualFreeEx(g_hProcess, g_oneShotCave, 0, MEM_RELEASE);
        VirtualFreeEx(g_hProcess, g_oneShotPage, 0, MEM_RELEASE);
        g_oneShotCave = nullptr; g_oneShotPage = nullptr;
        return false;
    }

    BYTE code[160]{};
    int p = 0;

    code[p++] = 0x48; code[p++] = 0x85; code[p++] = 0xD2;
    code[p++] = 0x74; code[p++] = 0x18;
    code[p++] = 0x48; code[p++] = 0xB8;
    BYTE* multAddr = g_oneShotPage;
    memcpy(&code[p], &multAddr, 8); p += 8;
    code[p++] = 0xF3; code[p++] = 0x0F; code[p++] = 0x10; code[p++] = 0x42; code[p++] = 0x08;
    code[p++] = 0xF3; code[p++] = 0x0F; code[p++] = 0x59; code[p++] = 0x00;
    code[p++] = 0xF3; code[p++] = 0x0F; code[p++] = 0x11; code[p++] = 0x42; code[p++] = 0x08;

    BYTE relocOut[64]{};
    int relocLen = 0;
    char rerr[96] = "";
    int patchLen = RelocatePrologue(got, 5, hookAddr, g_oneShotCave, p,
        relocOut, &relocLen, rerr);
    if (patchLen < 5) {
        sprintf_s(g_oneShotErr, "%s (b=%02X%02X%02X%02X%02X)",
            rerr[0] ? rerr : "reloc fail",
            got[0], got[1], got[2], got[3], got[4]);
        VirtualFreeEx(g_hProcess, g_oneShotCave, 0, MEM_RELEASE);
        VirtualFreeEx(g_hProcess, g_oneShotPage, 0, MEM_RELEASE);
        g_oneShotCave = nullptr; g_oneShotPage = nullptr;
        return false;
    }

    memcpy(code + p, relocOut, relocLen); p += relocLen;

    BYTE* backTarget = hookAddr + patchLen;
    int relBack = (int)(backTarget - (g_oneShotCave + p + 5));
    code[p++] = 0xE9; memcpy(&code[p], &relBack, 4); p += 4;

    if (!WPM(g_oneShotCave, code, p)) {
        sprintf_s(g_oneShotErr, "WPM cave fail err=%lu", GetLastError());
        VirtualFreeEx(g_hProcess, g_oneShotCave, 0, MEM_RELEASE);
        VirtualFreeEx(g_hProcess, g_oneShotPage, 0, MEM_RELEASE);
        g_oneShotCave = nullptr; g_oneShotPage = nullptr;
        return false;
    }

    memcpy(g_oneShotBackup, got, patchLen);
    g_oneShotPatchLen = patchLen;

    BYTE jmp[16]{};
    jmp[0] = 0xE9;
    int rel = (int)(g_oneShotCave - (hookAddr + 5));
    memcpy(&jmp[1], &rel, 4);
    for (int i = 5; i < patchLen; i++) jmp[i] = 0x90;

    if (!WPM(hookAddr, jmp, patchLen)) {
        sprintf_s(g_oneShotErr, "WPM hook fail err=%lu", GetLastError());
        VirtualFreeEx(g_hProcess, g_oneShotCave, 0, MEM_RELEASE);
        VirtualFreeEx(g_hProcess, g_oneShotPage, 0, MEM_RELEASE);
        g_oneShotCave = nullptr; g_oneShotPage = nullptr;
        return false;
    }

    g_oneShotInstalled = true;
    g_oneShotErr[0] = 0;
    return true;
}

bool SetOneShotEnabled(bool on)
{
    if (on && !g_oneShotInstalled) {
        if (!InstallOneShotHook()) return false;
    }
    if (!g_oneShotPage) return false;
    float fmult = on ? 1.0e9f : 1.0f;
    if (!WPM(g_oneShotPage, &fmult, 4)) return false;
    g_oneShotEnabled = on;
    return true;
}

bool UninstallOneShotHook()
{
    if (!g_oneShotInstalled) return true;
    BYTE* hookAddr = g_gaBase + RVA_MONSTER_TAKEDMG;
    WPM(hookAddr, g_oneShotBackup, g_oneShotPatchLen);
    Sleep(80);
    if (g_oneShotCave) {
        VirtualFreeEx(g_hProcess, g_oneShotCave, 0, MEM_RELEASE);
        g_oneShotCave = nullptr;
    }
    if (g_oneShotPage) {
        VirtualFreeEx(g_hProcess, g_oneShotPage, 0, MEM_RELEASE);
        g_oneShotPage = nullptr;
    }
    g_oneShotInstalled = false;
    g_oneShotEnabled = false;
    return true;
}

#if 0
static bool ScanForCubeInstance(int knownLevel, uintptr_t& outPtr)
{
    outPtr = 0;
    if (!g_hProcess) return false;

    SYSTEM_INFO si; GetSystemInfo(&si);
    uintptr_t addr    = (uintptr_t)si.lpMinimumApplicationAddress;
    uintptr_t maxAddr = (uintptr_t)si.lpMaximumApplicationAddress;
    if (maxAddr > 0x7FFFFFFFFFFFULL) maxAddr = 0x7FFFFFFFFFFFULL;

    std::vector<BYTE> buf;
    MEMORY_BASIC_INFORMATION mbi;
    int scanned = 0;

    while (addr < maxAddr && scanned < 4096) {
        if (!VirtualQueryEx(g_hProcess, (LPCVOID)addr, &mbi, sizeof(mbi))) break;
        bool ok = mbi.State == MEM_COMMIT
               && mbi.Type  == MEM_PRIVATE
               && (mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE))
               && !(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS));
        SIZE_T rsize = mbi.RegionSize;
        if (ok && rsize > 0 && rsize <= 64 * 1024 * 1024) {
            if (buf.size() < rsize) buf.resize(rsize);
            SIZE_T got = 0;
            if (ReadProcessMemory(g_hProcess, mbi.BaseAddress, buf.data(), rsize, &got)
                && got >= 0x20)
            {
                for (SIZE_T off = 0; off + 0x20 <= got; off += 8) {
                    uintptr_t typePtr = *(uintptr_t*)&buf[off];
                    if (typePtr < 0x10000 || typePtr > 0x7FFFFFFFFFFFULL) continue;
                    if (typePtr & 7) continue;

                    uintptr_t mon = *(uintptr_t*)&buf[off + 0x08];
                    if (mon > 0x100000) continue;

                    int lvl = *(int*)&buf[off + 0x10];
                    if (knownLevel >= 0) {
                        if (lvl != knownLevel) continue;
                    } else if (lvl < 1 || lvl > 999) {
                        continue;
                    }

                    DWORD eb = *(DWORD*)&buf[off + 0x14];
                    if ((eb & 0x7F800000) == 0x7F800000) continue;
                    float exp = *(float*)&eb;
                    if (exp < 0.0f || exp > 1.0e7f) continue;

                    uintptr_t pad = *(uintptr_t*)&buf[off + 0x18];
                    if (pad != 0) continue;

                    outPtr = (uintptr_t)mbi.BaseAddress + off;
                    return true;
                }
            }
            scanned++;
        }
        addr = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
        if (mbi.RegionSize == 0) break;
    }
    return false;
}

static bool CaptureCubeBy(int knownLevel)
{
    uintptr_t found = 0;
    if (!ScanForCubeInstance(knownLevel, found)) return false;
    if (g_cubePage) WPM(g_cubePage, &found, sizeof(found));
    return true;
}

bool InstallCubeHook()
{
    if (g_cubeHookInstalled) return true;
    BYTE* hookAddr = g_gaBase + RVA_CUBE_SAVE_CTOR;

    BYTE got[16]{};
    if (!RPM(hookAddr, got, 16)) {
        sprintf_s(g_cubeHookErr, "RPM fail err=%lu", GetLastError());
        return false;
    }
    if (got[0] == 0xE9) {
        sprintf_s(g_cubeHookErr, "already hooked, restart the game");
        return false;
    }

    g_cubePage = (BYTE*)VirtualAllocEx(g_hProcess, nullptr, 4096,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!g_cubePage) {
        sprintf_s(g_cubeHookErr, "page alloc fail err=%lu", GetLastError());
        return false;
    }
    BYTE zero[16] = {};
    WPM(g_cubePage, zero, 16);

    g_cubeCave = AllocNear(hookAddr, 4096);
    if (!g_cubeCave) {
        sprintf_s(g_cubeHookErr, "cave alloc fail err=%lu", GetLastError());
        VirtualFreeEx(g_hProcess, g_cubePage, 0, MEM_RELEASE);
        g_cubePage = nullptr;
        return false;
    }
    long long dist = (long long)(g_cubeCave - hookAddr);
    if (dist < -0x7FFFFFF0LL || dist > 0x7FFFFFF0LL) {
        sprintf_s(g_cubeHookErr, "cave too far: %lld", dist);
        VirtualFreeEx(g_hProcess, g_cubeCave, 0, MEM_RELEASE);
        VirtualFreeEx(g_hProcess, g_cubePage, 0, MEM_RELEASE);
        g_cubeCave = nullptr; g_cubePage = nullptr;
        return false;
    }

    BYTE code[128]{};
    int p = 0;

    code[p++] = 0x48; code[p++] = 0xB8;
    BYTE* slot = g_cubePage; memcpy(&code[p], &slot, 8); p += 8;
    code[p++] = 0x48; code[p++] = 0x89; code[p++] = 0x08;

    BYTE relocOut[64]{};
    int relocLen = 0;
    char rerr[96] = "";
    int patchLen = RelocatePrologue(got, 5, hookAddr, g_cubeCave, p,
        relocOut, &relocLen, rerr);
    if (patchLen < 5) {
        sprintf_s(g_cubeHookErr, "%s (b=%02X%02X%02X%02X%02X)",
            rerr[0] ? rerr : "reloc fail",
            got[0], got[1], got[2], got[3], got[4]);
        VirtualFreeEx(g_hProcess, g_cubeCave, 0, MEM_RELEASE);
        VirtualFreeEx(g_hProcess, g_cubePage, 0, MEM_RELEASE);
        g_cubeCave = nullptr; g_cubePage = nullptr;
        return false;
    }

    memcpy(code + p, relocOut, relocLen); p += relocLen;

    BYTE* backTarget = hookAddr + patchLen;
    int relBack = (int)(backTarget - (g_cubeCave + p + 5));
    code[p++] = 0xE9; memcpy(&code[p], &relBack, 4); p += 4;

    if (!WPM(g_cubeCave, code, p)) {
        sprintf_s(g_cubeHookErr, "WPM cave fail err=%lu", GetLastError());
        VirtualFreeEx(g_hProcess, g_cubeCave, 0, MEM_RELEASE);
        VirtualFreeEx(g_hProcess, g_cubePage, 0, MEM_RELEASE);
        g_cubeCave = nullptr; g_cubePage = nullptr;
        return false;
    }

    memcpy(g_cubeBackup, got, patchLen);
    g_cubePatchLen = patchLen;

    BYTE jmp[16]{};
    jmp[0] = 0xE9;
    int rel = (int)(g_cubeCave - (hookAddr + 5));
    memcpy(&jmp[1], &rel, 4);
    for (int i = 5; i < patchLen; i++) jmp[i] = 0x90;

    if (!WPM(hookAddr, jmp, patchLen)) {
        sprintf_s(g_cubeHookErr, "WPM hook fail err=%lu", GetLastError());
        VirtualFreeEx(g_hProcess, g_cubeCave, 0, MEM_RELEASE);
        VirtualFreeEx(g_hProcess, g_cubePage, 0, MEM_RELEASE);
        g_cubeCave = nullptr; g_cubePage = nullptr;
        return false;
    }

    g_cubeHookInstalled = true;
    g_cubeHookErr[0] = 0;
    return true;
}

bool UninstallCubeHook()
{
    if (!g_cubeHookInstalled) return true;
    BYTE* hookAddr = g_gaBase + RVA_CUBE_SAVE_CTOR;
    WPM(hookAddr, g_cubeBackup, g_cubePatchLen);
    Sleep(80);
    if (g_cubeCave) {
        VirtualFreeEx(g_hProcess, g_cubeCave, 0, MEM_RELEASE);
        g_cubeCave = nullptr;
    }
    if (g_cubePage) {
        VirtualFreeEx(g_hProcess, g_cubePage, 0, MEM_RELEASE);
        g_cubePage = nullptr;
    }
    g_cubeHookInstalled = false;
    return true;
}

bool ApplyCubeLevel(int level)
{
    uintptr_t self = ReadCubeThis();
    if (!self) {
        if (CaptureCubeBy(g_cubeCurrentLevelHint))
            self = ReadCubeThis();
    }
    if (!self) return false;
    if (level < 1)    level = 1;
    if (level > 9999) level = 9999;
    int v = level;
    float resetExp = 0.f;
    bool ok = WPM((BYTE*)(self + CUBE_LEVEL_OFF), &v, 4);
    WPM((BYTE*)(self + CUBE_EXP_OFF), &resetExp, 4);
    if (ok) g_cubeCurrentLevelHint = level;
    return ok;
}

bool ApplyCubeExp(float exp)
{
    uintptr_t self = ReadCubeThis();
    if (!self) return false;
    return WPM((BYTE*)(self + CUBE_EXP_OFF), &exp, 4);
}

static DWORD WINAPI CubeWorker(LPVOID)
{
    int scanCooldown = 0;
    while (g_cubeWorkerRun) {
        if (g_cubeAutoMaxXp && g_hProcess) {
            uintptr_t self = ReadCubeThis();
            if (!self && scanCooldown <= 0) {
                if (CaptureCubeBy(-1)) self = ReadCubeThis();
                scanCooldown = 20;
            }
            if (self) {
                int   lvl = g_cubeSetLevel;
                if (lvl < 1)    lvl = 1;
                if (lvl > 9999) lvl = 9999;
                WPM((BYTE*)(self + CUBE_LEVEL_OFF), &lvl, 4);
                WPM((BYTE*)(self + CUBE_EXP_OFF),   &g_cubeAutoFloor, 4);
                g_cubeCurrentLevelHint = lvl;
            }
        }
        if (scanCooldown > 0) scanCooldown--;
        Sleep(100);
    }
    return 0;
}

void StartCubeWorker()
{
    if (g_cubeWorker) return;
    g_cubeWorkerRun = true;
    g_cubeWorker = CreateThread(nullptr, 0, CubeWorker, nullptr, 0, nullptr);
}

void StopCubeWorker()
{
    g_cubeWorkerRun = false;
    if (g_cubeWorker) { CloseHandle(g_cubeWorker); g_cubeWorker = nullptr; }
}
#endif

#if 0
static bool LooksLikePlayerSaveData(uintptr_t addr)
{
    if (!ValidUserPtr(addr)) return false;
    uint64_t klass = 0;
    if (!RpmU64(addr, klass) || !ValidUserPtr((uintptr_t)klass)) return false;

    uint64_t lh = 0, lv = 0, li = 0;
    if (!RpmU64(addr + PSD_OFF_HEROES,    lh) || !ValidUserPtr((uintptr_t)lh)) return false;
    if (!RpmU64(addr + PSD_OFF_INVENTORY, lv) || !ValidUserPtr((uintptr_t)lv)) return false;
    if (!RpmU64(addr + PSD_OFF_ITEMS,     li) || !ValidUserPtr((uintptr_t)li)) return false;

    auto inspect = [](uintptr_t listPtr, int lo, int hi, bool needNonEmptyArr,
                      uintptr_t& outArr, int& outSize) -> bool {
        uint32_t sz = 0;
        uint64_t arr = 0;
        if (!RpmU32(listPtr + LIST_OFF_SIZE, sz)) return false;
        if (!RpmU64(listPtr + LIST_OFF_ITEMS_ARR, arr)) return false;
        if ((int)sz < lo || (int)sz > hi) return false;
        if (needNonEmptyArr) {
            if (!ValidUserPtr((uintptr_t)arr)) return false;
            uint64_t maxLen = 0;
            if (!RpmU64((uintptr_t)arr + ARR_OFF_LENGTH, maxLen)) return false;
            if (maxLen < (uint64_t)sz) return false;
        }
        outArr  = (uintptr_t)arr;
        outSize = (int)sz;
        return true;
    };

    uintptr_t hArr = 0, vArr = 0, iArr = 0;
    int hSize = 0, vSize = 0, iSize = 0;
    if (!inspect((uintptr_t)lh, 1, 100,    true,  hArr, hSize)) return false;
    if (!inspect((uintptr_t)lv, 0, 20000,  false, vArr, vSize)) return false;
    if (!inspect((uintptr_t)li, 1, 20000,  true,  iArr, iSize)) return false;

    int checkN = hSize < 3 ? hSize : 3;
    int sane = 0;
    for (int i = 0; i < hSize && sane < checkN; i++) {
        uint64_t hp = 0;
        if (!RpmU64(hArr + ARR_HDR_SIZE + i * 8, hp))    continue;
        if (!ValidUserPtr((uintptr_t)hp))                continue;
        uint64_t klass2 = 0;
        if (!RpmU64((uintptr_t)hp, klass2))              continue;
        if (!ValidUserPtr((uintptr_t)klass2))            continue;
        uint32_t hk = 0, lv = 0;
        if (!RpmU32((uintptr_t)hp + HSD_OFF_HEROKEY, hk)) continue;
        if (!RpmU32((uintptr_t)hp + HSD_OFF_LEVEL,   lv)) continue;
        if (hk == 0 || hk > 10000) continue;
        if (lv > 100000) continue;
        sane++;
    }
    if (sane < checkN) return false;

    uint64_t firstKlass = 0;
    {
        uint64_t hp0 = 0;
        if (!RpmU64(hArr + ARR_HDR_SIZE, hp0) || !ValidUserPtr((uintptr_t)hp0)) return false;
        if (!RpmU64((uintptr_t)hp0, firstKlass) || !ValidUserPtr((uintptr_t)firstKlass)) return false;
    }
    int klassMatches = 1, klassChecks = 1;
    for (int i = 1; i < hSize && klassChecks < 5; i++) {
        uint64_t hp = 0;
        if (!RpmU64(hArr + ARR_HDR_SIZE + i * 8, hp)) continue;
        if (!ValidUserPtr((uintptr_t)hp)) continue;
        klassChecks++;
        uint64_t k = 0;
        if (!RpmU64((uintptr_t)hp, k)) continue;
        if (k == firstKlass) klassMatches++;
    }
    if (klassMatches < klassChecks - 1) return false;
    return true;
}

static bool LocatePlayerSaveData()
{
    if (g_psdAddr && LooksLikePlayerSaveData(g_psdAddr)) return true;
    if (!g_hProcess) return false;

    SYSTEM_INFO si; GetSystemInfo(&si);
    uintptr_t addr    = (uintptr_t)si.lpMinimumApplicationAddress;
    uintptr_t maxAddr = (uintptr_t)si.lpMaximumApplicationAddress;
    if (maxAddr > 0x7FFFFFFFFFFFULL) maxAddr = 0x7FFFFFFFFFFFULL;

    std::vector<BYTE> buf;
    MEMORY_BASIC_INFORMATION mbi;
    int scanned = 0;

    while (addr < maxAddr && scanned < 8192) {
        if (!VirtualQueryEx(g_hProcess, (LPCVOID)addr, &mbi, sizeof(mbi))) break;
        bool ok = mbi.State == MEM_COMMIT
               && mbi.Type  == MEM_PRIVATE
               && (mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE))
               && !(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS));
        SIZE_T rsize = mbi.RegionSize;
        if (ok && rsize > 0 && rsize <= 64 * 1024 * 1024) {
            if (buf.size() < rsize) buf.resize(rsize);
            SIZE_T got = 0;
            if (ReadProcessMemory(g_hProcess, mbi.BaseAddress, buf.data(), rsize, &got)
                && got >= (SIZE_T)(PSD_OFF_ITEMS + 16))
            {
                for (SIZE_T off = 0; off + PSD_OFF_ITEMS + 8 <= got; off += 8) {
                    uintptr_t typePtr = *(uintptr_t*)&buf[off];
                    if (!ValidUserPtr(typePtr)) continue;

                    uintptr_t lh = *(uintptr_t*)&buf[off + PSD_OFF_HEROES];
                    uintptr_t lv = *(uintptr_t*)&buf[off + PSD_OFF_INVENTORY];
                    uintptr_t li = *(uintptr_t*)&buf[off + PSD_OFF_ITEMS];
                    if (!ValidUserPtr(lh) || !ValidUserPtr(lv) || !ValidUserPtr(li))
                        continue;

                    uintptr_t cand = (uintptr_t)mbi.BaseAddress + off;
                    if (LooksLikePlayerSaveData(cand)) {
                        g_psdAddr = cand;
                        return true;
                    }
                }
            }
            scanned++;
        }
        addr = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
        if (mbi.RegionSize == 0) break;
    }
    return false;
}

static bool ReadList(uintptr_t listPtr, uintptr_t& outArr, int& outSize)
{
    uint64_t arr = 0;
    uint32_t sz  = 0;
    if (!RpmU64(listPtr + LIST_OFF_ITEMS_ARR, arr)) return false;
    if (!RpmU32(listPtr + LIST_OFF_SIZE,      sz))  return false;
    outArr  = (uintptr_t)arr;
    outSize = (int)sz;
    return true;
}

static int ScoreItemAt(uintptr_t itemAddr)
{
    uint64_t enchArr = 0;
    if (!RpmU64(itemAddr + ISD_OFF_ENCHANTARR, enchArr)) return 0;
    if (!ValidUserPtr((uintptr_t)enchArr)) return 0;

    uint64_t maxLen = 0;
    if (!RpmU64(enchArr + ARR_OFF_LENGTH, maxLen)) return 0;
    if (maxLen == 0 || maxLen > 32) return 0;

    int score = 0;
    for (uint64_t i = 0; i < maxLen; i++) {
        uintptr_t slot = (uintptr_t)enchArr + ARR_HDR_SIZE + i * ENCHANT_STRIDE;
        uint32_t tier = 0, val = 0;
        if (!RpmU32(slot + ENCHANT_OFF_TIER,  tier)) continue;
        if (!RpmU32(slot + ENCHANT_OFF_VALUE, val))  continue;
        score += (int)tier * 10 + (int)val;
    }
    return score;
}

struct ScoredItem {
    uintptr_t addr;
    uint64_t  uniqueId;
    int       itemKey;
    int       score;
};

static bool ApplyAutoEquipBest()
{
    g_aeqStatus[0] = 0;
    g_aeqLastTouched = 0;

    if (!g_hProcess) { sprintf_s(g_aeqStatus, "Not attached to the game."); return false; }
    if (!LocatePlayerSaveData()) {
        sprintf_s(g_aeqStatus, "Save data not located. Open the game and load a profile, then try again.");
        return false;
    }

    uintptr_t hList = 0, iList = 0;
    {
        uint64_t lh = 0, li = 0;
        if (!RpmU64(g_psdAddr + PSD_OFF_HEROES, lh)) { sprintf_s(g_aeqStatus, "Read hero list failed."); return false; }
        if (!RpmU64(g_psdAddr + PSD_OFF_ITEMS,  li)) { sprintf_s(g_aeqStatus, "Read item list failed."); return false; }
        hList = (uintptr_t)lh; iList = (uintptr_t)li;
    }

    uintptr_t hArr = 0, iArr = 0; int hSize = 0, iSize = 0;
    if (!ReadList(hList, hArr, hSize) || !ReadList(iList, iArr, iSize)) {
        sprintf_s(g_aeqStatus, "List read failed.");
        return false;
    }
    if (iSize <= 0) { sprintf_s(g_aeqStatus, "Inventory is empty (no items to equip)."); return false; }
    if (hSize <= 0) { sprintf_s(g_aeqStatus, "No heroes in save data."); return false; }

    std::vector<ScoredItem> items;
    items.reserve(iSize);
    int skipPtr = 0, skipKey = 0, zeroUid = 0;
    for (int i = 0; i < iSize; i++) {
        uint64_t itemPtr = 0;
        if (!RpmU64(iArr + ARR_HDR_SIZE + i * 8, itemPtr)) { skipPtr++; continue; }
        if (!ValidUserPtr((uintptr_t)itemPtr))             { skipPtr++; continue; }
        uint32_t key = 0;
        uint64_t uid = 0;
        if (!RpmU32((uintptr_t)itemPtr + ISD_OFF_ITEMKEY,  key)) { skipKey++; continue; }
        if (!RpmU64((uintptr_t)itemPtr + ISD_OFF_UNIQUEID, uid)) { skipKey++; continue; }
        if (key == 0) { skipKey++; continue; }
        if (uid == 0) zeroUid++;
        ScoredItem s{ (uintptr_t)itemPtr, uid, (int)key, ScoreItemAt((uintptr_t)itemPtr) };
        items.push_back(s);
    }
    if (items.empty()) {
        sprintf_s(g_aeqStatus,
            "No usable items collected. Inventory list=%d, ptr-skip=%d, key-skip=%d. "
            "Save data layout may have shifted -- send me a screenshot of the "
            "hero panel with item counts so I can verify offsets.",
            iSize, skipPtr, skipKey);
        return false;
    }

    std::sort(items.begin(), items.end(),
              [](const ScoredItem& a, const ScoredItem& b) { return a.score > b.score; });

    size_t itemCursor = 0;
    int touched = 0;
    int hUnlocked = 0, hLeveled = 0, hHasArr = 0, hSkipPtr = 0;
    int totalSlots = 0, slotsZero = 0, wpmFails = 0, writes = 0;

    for (int h = 0; h < hSize; h++) {
        uint64_t heroPtr = 0;
        if (!RpmU64(hArr + ARR_HDR_SIZE + h * 8, heroPtr)) { hSkipPtr++; continue; }
        if (!ValidUserPtr((uintptr_t)heroPtr))             { hSkipPtr++; continue; }

        uint8_t  isUnlock = 0;
        uint32_t lvl      = 0;
        RpmU8 ((uintptr_t)heroPtr + HSD_OFF_ISUNLOCK, isUnlock);
        RpmU32((uintptr_t)heroPtr + HSD_OFF_LEVEL,    lvl);
        if (isUnlock) hUnlocked++;
        if (lvl > 0)  hLeveled++;

        uint64_t eqArr = 0;
        if (!RpmU64((uintptr_t)heroPtr + HSD_OFF_EQUIPPEDIDS, eqArr)) continue;
        if (!ValidUserPtr((uintptr_t)eqArr)) continue;
        hHasArr++;

        uint64_t slots = 0;
        if (!RpmU64((uintptr_t)eqArr + ARR_OFF_LENGTH, slots)) continue;
        if (slots > 32) continue;
        if (slots == 0) { slotsZero++; continue; }
        totalSlots += (int)slots;

        bool wroteAny = false;
        for (uint64_t s = 0; s < slots; s++) {
            if (itemCursor >= items.size()) break;
            uintptr_t slotAddr = (uintptr_t)eqArr + ARR_HDR_SIZE + s * 8;
            uint64_t  newId    = items[itemCursor++].uniqueId;
            if (!RemoteAddrWritable(slotAddr, sizeof(newId))) { wpmFails++; continue; }
            uint64_t prev = 0;
            if (!RpmU64(slotAddr, prev)) { wpmFails++; continue; }
            if (WPM((BYTE*)slotAddr, &newId, sizeof(newId))) {
                wroteAny = true; writes++;
            } else {
                wpmFails++;
            }
        }
        if (wroteAny) touched++;
        if (itemCursor >= items.size()) break;
    }

    g_aeqLastTouched = touched;
    if (touched == 0) {
        sprintf_s(g_aeqStatus,
            "Nothing written. heroes=%d unl=%d lvl=%d eq-arr=%d slots-total=%d "
            "slots-zero=%d wpm-fails=%d items=%d. "
            "If slots-zero matches eq-arr, the game lazy-initialises the slot "
            "array. Open the hero's gear panel in-game once, then retry.",
            hSize, hUnlocked, hLeveled, hHasArr, totalSlots, slotsZero,
            wpmFails, (int)items.size());
        return false;
    }
    sprintf_s(g_aeqStatus,
              "Auto-Equip OK. heroes-touched=%d writes=%d (fails=%d) | "
              "items=%d top-score=%d zero-uid=%d. "
              "Re-open the hero panel in-game to refresh icons.",
              touched, writes, wpmFails,
              (int)items.size(), items.front().score, zeroUid);
    return true;
}
#endif

enum StatusKind { ST_TEXT = 0, ST_ACTIVE = 1, ST_WARN = 2, ST_INFO = 3, ST_HOTKEY = 4 };

static ImVec4 StatusVec(int sc)
{
    switch (sc) {
        case ST_ACTIVE: return ImVec4(0.16f, 0.60f, 0.30f, 1.0f);
        case ST_WARN:   return ImVec4(0.82f, 0.25f, 0.22f, 1.0f);
        case ST_INFO:   return ImVec4(0.18f, 0.48f, 0.82f, 1.0f);
        case ST_HOTKEY: return ImVec4(0.76f, 0.55f, 0.08f, 1.0f);
        default:        return ImVec4(0.34f, 0.38f, 0.44f, 1.0f);
    }
}

static char g_statusMsg[256] = "Ready.";
static int  g_statusKind = ST_TEXT;

void SetStatus(const char* msg, int kind = ST_TEXT)
{
    strncpy_s(g_statusMsg, msg, sizeof(g_statusMsg)-1);
    g_statusKind = kind;
}

int CountActive(Patch* p, int n) { int c=0; for(int i=0;i<n;i++) if(p[i].applied) c++; return c; }

static const char* GoldRiskLabel(int mult)
{
    if (mult <= 1) return "OFF";
    if (mult <= 3) return "LEGIT";
    if (mult <= 6) return "SAFE";
    if (mult <= 10) return "MEDIUM";
    return "RISKY";
}

static int GoldRiskKind(int mult)
{
    if (mult <= 1) return ST_TEXT;
    if (mult <= 6) return ST_ACTIVE;
    if (mult <= 10) return ST_INFO;
    return ST_WARN;
}

void DoBypass()
{
    if (!ka::Guard()) return;
    int ac = CountActive(g_actk, ACTK_COUNT);
    if (ac == (int)ACTK_COUNT) {
        RestoreGroup(g_actk, ACTK_COUNT, "ACTk");
        SetStatus("Stealth DISABLED. (Turn off the other cheats too!)", ST_WARN);
    } else {
        ApplyGroup(g_actk, ACTK_COUNT, "ACTk");
        SetStatus("Stealth ON: detector + Steam telemetry silenced.", ST_ACTIVE);
    }
}

void DoDLC()
{
    if (!ka::Guard()) return;
    int dc = CountActive(g_dlc, DLC_COUNT);
    if (dc == (int)DLC_COUNT) {
        RestoreGroup(g_dlc, DLC_COUNT, "DLC");
        SetStatus("DLC Unlocker disabled.", ST_WARN);
    } else {
        ApplyGroup(g_dlc, DLC_COUNT, "DLC");
        SetStatus("DLC Unlocker on. All DLCs unlocked.", ST_ACTIVE);
    }
}

void DoPet()
{
    if (!ka::Guard()) return;
    int pc = CountActive(g_pet, PET_COUNT);
    if (pc == (int)PET_COUNT) {
        RestoreGroup(g_pet, PET_COUNT, "Pet");
        SetStatus("Pet Unlocker disabled.", ST_WARN);
    } else {
        ApplyGroup(g_pet, PET_COUNT, "Pet");
        SetStatus("Pet Unlocker on. All pets unlocked.", ST_ACTIVE);
    }
}

void DoGod()
{
    if (!ka::Guard()) return;
    int gc = CountActive(g_god, GOD_COUNT);
    if (gc == (int)GOD_COUNT) {
        RestoreGroup(g_god, GOD_COUNT, "GOD");
        SetStatus("God Mode disabled.", ST_WARN);
    } else {
        ApplyGroup(g_god, GOD_COUNT, "GOD");
        SetStatus("God Mode on. Only the Hero is invulnerable.", ST_ACTIVE);
    }
}

void ApplySpeedValue(float val)
{
    if (!ka::Guard()) return;
    if (val < 1.0f) val = 1.0f;
    if (val > SPEED_MAX) val = SPEED_MAX;
    if (val <= 1.0f) {
        if (g_speedActive) { SetSpeed(1.0f); SetStatus("Speed reset to normal.", ST_TEXT); }
        return;
    }
    if (CountActive(g_actk, ACTK_COUNT) < (int)ACTK_COUNT)
        ApplyGroup(g_actk, ACTK_COUNT, "ACTk");
    if (SetSpeed(val)) {
        char buf[64]; sprintf_s(buf, "Speed: %.1fx (physics + range + attack sync)", val);
        SetStatus(buf, ST_ACTIVE);
    } else {
        SetStatus("Failed to apply speed.", ST_WARN);
    }
}

void ApplyGoldValue(int val)
{
    if (!ka::Guard()) return;
    if (val < 1) val = 1;
    if (val > 5000) val = 5000;
    if (val <= 1) {
        if (g_goldApplied || g_goldHookInstalled) {
            SetGoldMultiplier(1);
            SetStatus("Gold multiplier disabled (1x = normal drop).", ST_TEXT);
        }
        return;
    }
    if (CountActive(g_actk, ACTK_COUNT) < (int)ACTK_COUNT) {
        SetStatus("Enable Stealth first (Bypass tab).", ST_WARN);
        return;
    }
    if (SetGoldMultiplier(val)) {
        char buf[96]; sprintf_s(buf, "Gold multiplier: %dx [%s] - every drop is now %dx",
            val, GoldRiskLabel(val), val);
        SetStatus(buf, GoldRiskKind(val));
    } else {
        char buf[200];
        sprintf_s(buf, "Gold hook failed: %s", g_goldHookErr[0] ? g_goldHookErr : "unknown");
        SetStatus(buf, ST_WARN);
    }
}

void ApplyHeroXp(int mult)
{
    if (!ka::Guard()) return;
    if (mult < 1)    mult = 1;
    if (mult > 1000) mult = 1000;
    if (mult <= 1) {
        if (g_heroXpApplied || g_heroXpHookInstalled) {
            SetHeroXpMultiplier(1);
            SetStatus("Character XP multiplier disabled (1x = normal).", ST_TEXT);
        }
        return;
    }
    if (CountActive(g_actk, ACTK_COUNT) < (int)ACTK_COUNT) {
        SetStatus("Enable Stealth first (Bypass tab).", ST_WARN);
        return;
    }
    if (SetHeroXpMultiplier(mult)) {
        char buf[96]; sprintf_s(buf, "Character XP multiplier: %dx - XP from monsters is now %dx",
            mult, mult);
        SetStatus(buf, ST_ACTIVE);
    } else {
        char buf[200];
        sprintf_s(buf, "Character XP hook failed: %s", g_heroXpErr[0] ? g_heroXpErr : "unknown");
        SetStatus(buf, ST_WARN);
    }
}

void ApplyCubeXp(int mult)
{
    if (!ka::Guard()) return;
    if (mult < 1) mult = 1;
    if (mult <= 1) {
        if (g_cubeXpApplied || g_cubeXpInstalled) {
            SetCubeXpMultiplier(1);
            SetStatus("Cube XP multiplier disabled (1x = normal).", ST_TEXT);
        }
        return;
    }
    if (!g_attached) {
        SetStatus("Attach to the game first.", ST_WARN);
        return;
    }
    if (SetCubeXpMultiplier(mult)) {
        float verify = 0.0f;
        if (g_cubeXpPage) RPM(g_cubeXpPage, &verify, 4);
        char buf[128];
        sprintf_s(buf, "Cube XP: %dx active (mem=%.0fx, hooks %s)",
            mult, verify,
            (g_cubeXpInstalled && g_cubeXpPenInstalled && g_cubeXpSkipPatched) ? "OK" : "partial");
        SetStatus(buf, ST_ACTIVE);
        g_cubeXpErr[0] = 0;
    } else {
        char buf[200];
        sprintf_s(buf, "Cube XP failed: %s", g_cubeXpErr[0] ? g_cubeXpErr : "unknown");
        SetStatus(buf, ST_WARN);
    }
}

void DoFullCombo()
{
    if (!ka::Guard()) return;
    ApplyGroup(g_actk, ACTK_COUNT, "ACTk");  Sleep(150);
    ApplyGroup(g_dlc,  DLC_COUNT,  "DLC");   Sleep(80);
    ApplyGroup(g_pet,  PET_COUNT,  "Pet");   Sleep(80);
    ApplyGroup(g_god,  GOD_COUNT,  "GOD");   Sleep(80);
    SetSpeed(3.0f);                          Sleep(80);
    SetGoldMultiplier(3);
    SetStatus("LEGIT COMBO: Stealth + DLC + Pet + God + Speed 3x + Gold 3x", ST_ACTIVE);
}

void DoRestoreAll()
{
    if (g_speedActive) { SetSpeed(1.0f); Sleep(150); }
    if (g_goldApplied) { RestoreGoldHack(); Sleep(80); }
    if (g_goldHookInstalled) { UninstallGoldCaptureHook(); Sleep(80); }
    if (g_heroXpHookInstalled) { UninstallHeroXpHook(); Sleep(80); }
    if (g_cubeXpInstalled) { UninstallCubeXpHook(); Sleep(80); }
    if (g_oneShotInstalled) { UninstallOneShotHook(); Sleep(80); }
    RestoreGroup(g_god, GOD_COUNT, "GOD"); Sleep(80);
    RestoreGroup(g_pet, PET_COUNT, "Pet"); Sleep(80);
    RestoreGroup(g_dlc, DLC_COUNT, "DLC"); Sleep(80);
    RestoreGroup(g_actk, ACTK_COUNT, "ACTk");
    ResetSpoofIdentity();
    SetStatus("Everything restored (game is clean).", ST_WARN);
}

void DoSafeExit()
{
    StopWatchdog();
    if (g_speedActive) { SetSpeed(1.0f); Sleep(150); }
    if (g_goldApplied) { RestoreGoldHack(); Sleep(100); }
    if (g_goldHookInstalled) { UninstallGoldCaptureHook(); Sleep(80); }
    if (g_heroXpHookInstalled) { UninstallHeroXpHook(); Sleep(80); }
    if (g_cubeXpInstalled) { UninstallCubeXpHook(); Sleep(80); }
    if (g_oneShotInstalled) { UninstallOneShotHook(); Sleep(80); }
    RestoreGroup(g_god,  GOD_COUNT,  "GOD");
    RestoreGroup(g_pet,  PET_COUNT,  "Pet");
    RestoreGroup(g_dlc,  DLC_COUNT,  "DLC");
    RestoreGroup(g_actk, ACTK_COUNT, "ACTk");
    ResetSpoofIdentity();
}

static ID3D11Device*           g_pd3dDevice           = nullptr;
static ID3D11DeviceContext*    g_pd3dDeviceContext    = nullptr;
static IDXGISwapChain*         g_pSwapChain           = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer) {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
        pBackBuffer->Release();
    }
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Width  = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    if (D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
        featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice,
        &featureLevel, &g_pd3dDeviceContext) != S_OK) return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain)        { g_pSwapChain->Release();        g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice)        { g_pd3dDevice->Release();        g_pd3dDevice = nullptr; }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_NCHITTEST: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        RECT rc; GetWindowRect(hWnd, &rc);
        int x = pt.x - rc.left;
        int y = pt.y - rc.top;
        if (y >= 0 && y < 60 && x >= 0 && x < 215)
            return HTCAPTION;
        return HTCLIENT;
    }
    case WM_SIZE:
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

static ImU32 CLR_LAYOUT       = IM_COL32(240, 242, 245, 255);
static ImU32 CLR_TITLEBAR     = IM_COL32(248, 249, 251, 255);
static ImU32 CLR_WIDGET       = IM_COL32(255, 255, 255, 255);
static ImU32 CLR_DARK_WIDGET  = IM_COL32(244, 246, 249, 255);
static ImU32 CLR_BORDER       = IM_COL32(220, 224, 230, 255);
static ImU32 CLR_INACTIVE     = IM_COL32(122, 128, 140, 255);
static ImU32 CLR_ACCENT       = IM_COL32( 28,  31,  38, 255);
static ImU32 CLR_WHITE        = IM_COL32( 26,  29,  36, 255);
static ImU32 CLR_LINE         = IM_COL32(228, 231, 236, 255);
static ImU32 CLR_SELECT       = IM_COL32(235, 238, 242, 255);
static ImU32 CLR_CIRCLE_OFF   = IM_COL32(198, 203, 211, 255);
static ImU32 CLR_TAB_GLOW     = IM_COL32(  0,   0,   0,  16);
static ImU32 CLR_ON           = IM_COL32( 32,  36,  44, 255);
static ImU32 CLR_ON_SOFT      = IM_COL32( 32,  36,  44,  40);

static float UIAnim(ImGuiID id, float target, float speed = 14.0f)
{
    static std::unordered_map<ImGuiID, float> s_vals;
    float dt = ImGui::GetIO().DeltaTime;
    if (dt <= 0.0f) dt = 1.0f / 60.0f;
    float& v = s_vals[id];
    float a = 1.0f - expf(-speed * dt);
    v += (target - v) * a;
    if (fabsf(target - v) < 0.0015f) v = target;
    return v;
}

static ImU32 LerpU32(ImU32 a, ImU32 b, float t)
{
    if (t <= 0.0f) return a;
    if (t >= 1.0f) return b;
    int ar = (a >> IM_COL32_R_SHIFT) & 0xFF, ag = (a >> IM_COL32_G_SHIFT) & 0xFF;
    int ab = (a >> IM_COL32_B_SHIFT) & 0xFF, aa = (a >> IM_COL32_A_SHIFT) & 0xFF;
    int br = (b >> IM_COL32_R_SHIFT) & 0xFF, bg = (b >> IM_COL32_G_SHIFT) & 0xFF;
    int bb = (b >> IM_COL32_B_SHIFT) & 0xFF, ba = (b >> IM_COL32_A_SHIFT) & 0xFF;
    return IM_COL32(
        (int)(ar + (br - ar) * t), (int)(ag + (bg - ag) * t),
        (int)(ab + (bb - ab) * t), (int)(aa + (ba - aa) * t));
}

static ImU32 Lighten(ImU32 c, float k)
{
    int r = (c >> IM_COL32_R_SHIFT) & 0xFF;
    int g = (c >> IM_COL32_G_SHIFT) & 0xFF;
    int b = (c >> IM_COL32_B_SHIFT) & 0xFF;
    int a = (c >> IM_COL32_A_SHIFT) & 0xFF;
    r = (int)(r + (255 - r) * k); if (r > 255) r = 255;
    g = (int)(g + (255 - g) * k); if (g > 255) g = 255;
    b = (int)(b + (255 - b) * k); if (b > 255) b = 255;
    return IM_COL32(r, g, b, a);
}

static ImU32 Darken(ImU32 c, float k)
{
    int r = (c >> IM_COL32_R_SHIFT) & 0xFF;
    int g = (c >> IM_COL32_G_SHIFT) & 0xFF;
    int b = (c >> IM_COL32_B_SHIFT) & 0xFF;
    int a = (c >> IM_COL32_A_SHIFT) & 0xFF;
    r = (int)(r * (1.0f - k));
    g = (int)(g * (1.0f - k));
    b = (int)(b * (1.0f - k));
    return IM_COL32(r, g, b, a);
}

static float ACCENT[3]       = {  28.f/255.f,  31.f/255.f,  38.f/255.f };
static float COL_LAYOUT[3]   = { 240.f/255.f, 242.f/255.f, 245.f/255.f };
static float COL_WIDGET[3]   = { 255.f/255.f, 255.f/255.f, 255.f/255.f };
static float COL_DWIDGET[3]  = { 244.f/255.f, 246.f/255.f, 249.f/255.f };
static float COL_BORDER[3]   = { 220.f/255.f, 224.f/255.f, 230.f/255.f };
static float COL_INACTIVE[3] = { 122.f/255.f, 128.f/255.f, 140.f/255.f };
static float COL_SELECT[3]   = { 235.f/255.f, 238.f/255.f, 242.f/255.f };
static float COL_TEXT[3]     = {  33.f/255.f,  38.f/255.f,  48.f/255.f };

static void ApplyTheme()
{
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding    = 0.0f;
    s.ChildRounding     = 4.0f;
    s.FrameRounding     = 2.0f;
    s.GrabRounding      = 2.0f;
    s.TabRounding       = 2.0f;
    s.ScrollbarRounding = 2.0f;
    s.WindowBorderSize  = 0.0f;
    s.ChildBorderSize   = 0.0f;
    s.FrameBorderSize   = 0.0f;
    s.WindowPadding     = ImVec2(0, 0);
    s.FramePadding      = ImVec2(8, 4);
    s.ItemSpacing       = ImVec2(6, 6);
    s.ScrollbarSize     = 6.0f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]            = ImVec4(COL_LAYOUT[0],  COL_LAYOUT[1],  COL_LAYOUT[2],  1.00f);
    c[ImGuiCol_ChildBg]             = ImVec4(COL_WIDGET[0],  COL_WIDGET[1],  COL_WIDGET[2],  1.00f);
    c[ImGuiCol_PopupBg]             = ImVec4(COL_WIDGET[0],  COL_WIDGET[1],  COL_WIDGET[2],  0.99f);
    c[ImGuiCol_Text]                = ImVec4(COL_TEXT[0],   COL_TEXT[1],   COL_TEXT[2],   1.00f);
    c[ImGuiCol_TextDisabled]        = ImVec4(COL_INACTIVE[0], COL_INACTIVE[1], COL_INACTIVE[2], 1.00f);
    c[ImGuiCol_Border]              = ImVec4(COL_BORDER[0],  COL_BORDER[1],  COL_BORDER[2],  1.00f);
    c[ImGuiCol_FrameBg]             = ImVec4(COL_DWIDGET[0], COL_DWIDGET[1], COL_DWIDGET[2], 1.00f);
    c[ImGuiCol_FrameBgHovered]      = ImVec4(COL_SELECT[0],  COL_SELECT[1],  COL_SELECT[2],  1.00f);
    c[ImGuiCol_FrameBgActive]       = ImVec4(228.f/255.f, 232.f/255.f, 238.f/255.f, 1.00f);
    c[ImGuiCol_Button]              = ImVec4(COL_DWIDGET[0], COL_DWIDGET[1], COL_DWIDGET[2], 1.00f);
    c[ImGuiCol_ButtonHovered]       = ImVec4(COL_SELECT[0],  COL_SELECT[1],  COL_SELECT[2],  1.00f);
    c[ImGuiCol_ButtonActive]        = ImVec4(225.f/255.f, 229.f/255.f, 235.f/255.f, 1.00f);
    c[ImGuiCol_Header]              = ImVec4(COL_SELECT[0],  COL_SELECT[1],  COL_SELECT[2],  1.00f);
    c[ImGuiCol_HeaderHovered]       = ImVec4(232.f/255.f, 235.f/255.f, 240.f/255.f, 1.00f);
    c[ImGuiCol_HeaderActive]        = ImVec4(225.f/255.f, 229.f/255.f, 235.f/255.f, 1.00f);
    c[ImGuiCol_CheckMark]           = ImVec4(ACCENT[0], ACCENT[1], ACCENT[2], 1.0f);
    c[ImGuiCol_SliderGrab]          = ImVec4(ACCENT[0], ACCENT[1], ACCENT[2], 1.0f);
    c[ImGuiCol_SliderGrabActive]    = ImVec4(ACCENT[0]*0.7f, ACCENT[1]*0.7f, ACCENT[2]*0.7f, 1.0f);
    c[ImGuiCol_Separator]           = ImVec4(COL_BORDER[0], COL_BORDER[1], COL_BORDER[2], 1.00f);
    c[ImGuiCol_ScrollbarBg]         = ImVec4(COL_LAYOUT[0],  COL_LAYOUT[1],  COL_LAYOUT[2],  0.00f);
    c[ImGuiCol_ScrollbarGrab]       = ImVec4(210.f/255.f, 214.f/255.f, 221.f/255.f, 1.00f);
    c[ImGuiCol_ScrollbarGrabHovered]= ImVec4(COL_INACTIVE[0],COL_INACTIVE[1],COL_INACTIVE[2],1.00f);
    c[ImGuiCol_ScrollbarGrabActive] = ImVec4(ACCENT[0], ACCENT[1], ACCENT[2], 1.00f);
}

struct ThemePreset {
    const char* name;
    ImU32 layout, titlebar, widget, dwidget, border, inactive, accent;
    ImU32 textPrimary, line, select, circleOff, tabGlow, on, onSoft;
    float fAccent[3], fLayout[3], fWidget[3], fDWidget[3], fBorder[3];
    float fInactive[3], fSelect[3], fText[3];
    ImU32 swatchA, swatchB;
};

static ThemePreset g_themes[] = {
    {   "Light",
        IM_COL32(240,242,245,255), IM_COL32(248,249,251,255),
        IM_COL32(255,255,255,255), IM_COL32(244,246,249,255),
        IM_COL32(220,224,230,255), IM_COL32(122,128,140,255),
        IM_COL32( 28, 31, 38,255),
        IM_COL32( 26, 29, 36,255), IM_COL32(228,231,236,255),
        IM_COL32(235,238,242,255), IM_COL32(198,203,211,255),
        IM_COL32(  0,  0,  0, 16), IM_COL32( 32, 36, 44,255),
        IM_COL32( 32, 36, 44, 40),
        { 28.f/255, 31.f/255, 38.f/255 },
        {240.f/255,242.f/255,245.f/255},
        {255.f/255,255.f/255,255.f/255},
        {244.f/255,246.f/255,249.f/255},
        {220.f/255,224.f/255,230.f/255},
        {122.f/255,128.f/255,140.f/255},
        {235.f/255,238.f/255,242.f/255},
        { 33.f/255, 38.f/255, 48.f/255},
        IM_COL32(255,255,255,255), IM_COL32(220,224,230,255)
    },
    {   "Graphite",
        IM_COL32( 28, 30, 36,255), IM_COL32( 36, 39, 47,255),
        IM_COL32( 42, 46, 54,255), IM_COL32( 34, 37, 44,255),
        IM_COL32( 56, 60, 70,255), IM_COL32(140,146,158,255),
        IM_COL32(220,223,232,255),
        IM_COL32(232,234,240,255), IM_COL32( 46, 50, 58,255),
        IM_COL32( 52, 56, 66,255), IM_COL32( 78, 82, 92,255),
        IM_COL32(255,255,255, 14), IM_COL32(220,223,232,255),
        IM_COL32(220,223,232, 40),
        {220.f/255,223.f/255,232.f/255},
        { 28.f/255, 30.f/255, 36.f/255},
        { 42.f/255, 46.f/255, 54.f/255},
        { 34.f/255, 37.f/255, 44.f/255},
        { 56.f/255, 60.f/255, 70.f/255},
        {140.f/255,146.f/255,158.f/255},
        { 52.f/255, 56.f/255, 66.f/255},
        {232.f/255,234.f/255,240.f/255},
        IM_COL32( 42, 46, 54,255), IM_COL32(220,223,232,255)
    },
    {   "Midnight",
        IM_COL32( 14, 16, 22,255), IM_COL32( 20, 23, 32,255),
        IM_COL32( 24, 28, 38,255), IM_COL32( 18, 21, 28,255),
        IM_COL32( 40, 46, 60,255), IM_COL32(118,128,148,255),
        IM_COL32( 90,160,255,255),
        IM_COL32(220,230,245,255), IM_COL32( 32, 38, 50,255),
        IM_COL32( 36, 42, 56,255), IM_COL32( 70, 78, 94,255),
        IM_COL32( 90,160,255, 18), IM_COL32(220,230,245,255),
        IM_COL32( 90,160,255, 48),
        { 90.f/255,160.f/255,255.f/255},
        { 14.f/255, 16.f/255, 22.f/255},
        { 24.f/255, 28.f/255, 38.f/255},
        { 18.f/255, 21.f/255, 28.f/255},
        { 40.f/255, 46.f/255, 60.f/255},
        {118.f/255,128.f/255,148.f/255},
        { 36.f/255, 42.f/255, 56.f/255},
        {220.f/255,230.f/255,245.f/255},
        IM_COL32( 24, 28, 38,255), IM_COL32( 90,160,255,255)
    },
    {   "Mono",
        IM_COL32(  8,  8,  8,255), IM_COL32( 14, 14, 14,255),
        IM_COL32( 20, 20, 20,255), IM_COL32( 12, 12, 12,255),
        IM_COL32( 60, 60, 60,255), IM_COL32(140,140,140,255),
        IM_COL32(255,255,255,255),
        IM_COL32(245,245,245,255), IM_COL32( 32, 32, 32,255),
        IM_COL32( 36, 36, 36,255), IM_COL32( 80, 80, 80,255),
        IM_COL32(255,255,255, 12), IM_COL32(255,255,255,255),
        IM_COL32(255,255,255, 32),
        {255.f/255,255.f/255,255.f/255},
        {  8.f/255,  8.f/255,  8.f/255},
        { 20.f/255, 20.f/255, 20.f/255},
        { 12.f/255, 12.f/255, 12.f/255},
        { 60.f/255, 60.f/255, 60.f/255},
        {140.f/255,140.f/255,140.f/255},
        { 36.f/255, 36.f/255, 36.f/255},
        {245.f/255,245.f/255,245.f/255},
        IM_COL32( 20, 20, 20,255), IM_COL32(255,255,255,255)
    },
    {   "Crimson",
        IM_COL32( 16, 10, 12,255), IM_COL32( 22, 14, 16,255),
        IM_COL32( 30, 20, 22,255), IM_COL32( 22, 14, 16,255),
        IM_COL32( 64, 28, 34,255), IM_COL32(160,118,124,255),
        IM_COL32(225, 78, 88,255),
        IM_COL32(238,222,224,255), IM_COL32( 40, 22, 26,255),
        IM_COL32( 48, 26, 30,255), IM_COL32( 90, 60, 64,255),
        IM_COL32(225, 78, 88, 18), IM_COL32(238,222,224,255),
        IM_COL32(225, 78, 88, 40),
        {225.f/255, 78.f/255, 88.f/255},
        { 16.f/255, 10.f/255, 12.f/255},
        { 30.f/255, 20.f/255, 22.f/255},
        { 22.f/255, 14.f/255, 16.f/255},
        { 64.f/255, 28.f/255, 34.f/255},
        {160.f/255,118.f/255,124.f/255},
        { 48.f/255, 26.f/255, 30.f/255},
        {238.f/255,222.f/255,224.f/255},
        IM_COL32( 30, 20, 22,255), IM_COL32(225, 78, 88,255)
    },
    {   "Forest",
        IM_COL32( 12, 18, 16,255), IM_COL32( 16, 24, 22,255),
        IM_COL32( 22, 32, 28,255), IM_COL32( 16, 24, 22,255),
        IM_COL32( 40, 60, 50,255), IM_COL32(124,158,140,255),
        IM_COL32( 72,194,128,255),
        IM_COL32(220,238,228,255), IM_COL32( 28, 42, 36,255),
        IM_COL32( 32, 48, 42,255), IM_COL32( 70, 92, 82,255),
        IM_COL32( 72,194,128, 18), IM_COL32(220,238,228,255),
        IM_COL32( 72,194,128, 40),
        { 72.f/255,194.f/255,128.f/255},
        { 12.f/255, 18.f/255, 16.f/255},
        { 22.f/255, 32.f/255, 28.f/255},
        { 16.f/255, 24.f/255, 22.f/255},
        { 40.f/255, 60.f/255, 50.f/255},
        {124.f/255,158.f/255,140.f/255},
        { 32.f/255, 48.f/255, 42.f/255},
        {220.f/255,238.f/255,228.f/255},
        IM_COL32( 22, 32, 28,255), IM_COL32( 72,194,128,255)
    },
    {   "Cyan",
        IM_COL32( 10, 16, 22,255), IM_COL32( 14, 22, 30,255),
        IM_COL32( 20, 30, 40,255), IM_COL32( 14, 22, 30,255),
        IM_COL32( 36, 56, 70,255), IM_COL32(120,152,170,255),
        IM_COL32( 90,210,222,255),
        IM_COL32(220,236,242,255), IM_COL32( 28, 44, 56,255),
        IM_COL32( 32, 50, 64,255), IM_COL32( 64, 92,108,255),
        IM_COL32( 90,210,222, 20), IM_COL32(220,236,242,255),
        IM_COL32( 90,210,222, 46),
        { 90.f/255,210.f/255,222.f/255},
        { 10.f/255, 16.f/255, 22.f/255},
        { 20.f/255, 30.f/255, 40.f/255},
        { 14.f/255, 22.f/255, 30.f/255},
        { 36.f/255, 56.f/255, 70.f/255},
        {120.f/255,152.f/255,170.f/255},
        { 32.f/255, 50.f/255, 64.f/255},
        {220.f/255,236.f/255,242.f/255},
        IM_COL32( 20, 30, 40,255), IM_COL32( 90,210,222,255)
    },
    {   "Sunset",
        IM_COL32( 22, 16, 14,255), IM_COL32( 30, 22, 18,255),
        IM_COL32( 40, 28, 22,255), IM_COL32( 30, 22, 18,255),
        IM_COL32( 72, 48, 36,255), IM_COL32(176,138,116,255),
        IM_COL32(240,138, 74,255),
        IM_COL32(244,228,216,255), IM_COL32( 50, 34, 26,255),
        IM_COL32( 58, 40, 30,255), IM_COL32(102, 74, 60,255),
        IM_COL32(240,138, 74, 20), IM_COL32(244,228,216,255),
        IM_COL32(240,138, 74, 46),
        {240.f/255,138.f/255, 74.f/255},
        { 22.f/255, 16.f/255, 14.f/255},
        { 40.f/255, 28.f/255, 22.f/255},
        { 30.f/255, 22.f/255, 18.f/255},
        { 72.f/255, 48.f/255, 36.f/255},
        {176.f/255,138.f/255,116.f/255},
        { 58.f/255, 40.f/255, 30.f/255},
        {244.f/255,228.f/255,216.f/255},
        IM_COL32( 40, 28, 22,255), IM_COL32(240,138, 74,255)
    },
    {   "Amethyst",
        IM_COL32( 18, 14, 26,255), IM_COL32( 26, 20, 38,255),
        IM_COL32( 34, 26, 50,255), IM_COL32( 24, 18, 36,255),
        IM_COL32( 60, 46, 86,255), IM_COL32(146,128,176,255),
        IM_COL32(170,108,255,255),
        IM_COL32(228,220,244,255), IM_COL32( 44, 34, 64,255),
        IM_COL32( 50, 40, 74,255), IM_COL32( 88, 72,116,255),
        IM_COL32(170,108,255, 22), IM_COL32(228,220,244,255),
        IM_COL32(170,108,255, 50),
        {170.f/255,108.f/255,255.f/255},
        { 18.f/255, 14.f/255, 26.f/255},
        { 34.f/255, 26.f/255, 50.f/255},
        { 24.f/255, 18.f/255, 36.f/255},
        { 60.f/255, 46.f/255, 86.f/255},
        {146.f/255,128.f/255,176.f/255},
        { 50.f/255, 40.f/255, 74.f/255},
        {228.f/255,220.f/255,244.f/255},
        IM_COL32( 34, 26, 50,255), IM_COL32(170,108,255,255)
    },
    {   "Rose Gold",
        IM_COL32( 28, 20, 22,255), IM_COL32( 38, 26, 30,255),
        IM_COL32( 50, 34, 38,255), IM_COL32( 38, 26, 30,255),
        IM_COL32( 84, 56, 62,255), IM_COL32(196,160,158,255),
        IM_COL32(244,166,154,255),
        IM_COL32(248,232,230,255), IM_COL32( 58, 38, 42,255),
        IM_COL32( 66, 44, 48,255), IM_COL32(114, 84, 88,255),
        IM_COL32(244,166,154, 22), IM_COL32(248,232,230,255),
        IM_COL32(244,166,154, 48),
        {244.f/255,166.f/255,154.f/255},
        { 28.f/255, 20.f/255, 22.f/255},
        { 50.f/255, 34.f/255, 38.f/255},
        { 38.f/255, 26.f/255, 30.f/255},
        { 84.f/255, 56.f/255, 62.f/255},
        {196.f/255,160.f/255,158.f/255},
        { 66.f/255, 44.f/255, 48.f/255},
        {248.f/255,232.f/255,230.f/255},
        IM_COL32( 50, 34, 38,255), IM_COL32(244,166,154,255)
    },
    {   "Ocean",
        IM_COL32(  8, 18, 32,255), IM_COL32( 12, 24, 42,255),
        IM_COL32( 16, 32, 54,255), IM_COL32( 12, 24, 42,255),
        IM_COL32( 32, 56, 86,255), IM_COL32(118,148,186,255),
        IM_COL32( 64,134,238,255),
        IM_COL32(218,232,250,255), IM_COL32( 24, 44, 70,255),
        IM_COL32( 28, 50, 80,255), IM_COL32( 60, 90,128,255),
        IM_COL32( 64,134,238, 22), IM_COL32(218,232,250,255),
        IM_COL32( 64,134,238, 50),
        { 64.f/255,134.f/255,238.f/255},
        {  8.f/255, 18.f/255, 32.f/255},
        { 16.f/255, 32.f/255, 54.f/255},
        { 12.f/255, 24.f/255, 42.f/255},
        { 32.f/255, 56.f/255, 86.f/255},
        {118.f/255,148.f/255,186.f/255},
        { 28.f/255, 50.f/255, 80.f/255},
        {218.f/255,232.f/255,250.f/255},
        IM_COL32( 16, 32, 54,255), IM_COL32( 64,134,238,255)
    },
    {   "Lemon",
        IM_COL32( 24, 22, 14,255), IM_COL32( 34, 30, 18,255),
        IM_COL32( 46, 40, 22,255), IM_COL32( 34, 30, 18,255),
        IM_COL32( 78, 68, 36,255), IM_COL32(188,176,116,255),
        IM_COL32(248,222, 80,255),
        IM_COL32(246,242,210,255), IM_COL32( 56, 50, 26,255),
        IM_COL32( 62, 54, 30,255), IM_COL32(108, 92, 56,255),
        IM_COL32(248,222, 80, 20), IM_COL32(246,242,210,255),
        IM_COL32(248,222, 80, 46),
        {248.f/255,222.f/255, 80.f/255},
        { 24.f/255, 22.f/255, 14.f/255},
        { 46.f/255, 40.f/255, 22.f/255},
        { 34.f/255, 30.f/255, 18.f/255},
        { 78.f/255, 68.f/255, 36.f/255},
        {188.f/255,176.f/255,116.f/255},
        { 62.f/255, 54.f/255, 30.f/255},
        {246.f/255,242.f/255,210.f/255},
        IM_COL32( 46, 40, 22,255), IM_COL32(248,222, 80,255)
    },
    {   "Mint",
        IM_COL32( 12, 22, 20,255), IM_COL32( 16, 30, 28,255),
        IM_COL32( 22, 40, 36,255), IM_COL32( 16, 30, 28,255),
        IM_COL32( 40, 70, 60,255), IM_COL32(126,180,166,255),
        IM_COL32( 96,232,196,255),
        IM_COL32(218,244,236,255), IM_COL32( 28, 50, 44,255),
        IM_COL32( 32, 56, 50,255), IM_COL32( 66,102, 92,255),
        IM_COL32( 96,232,196, 20), IM_COL32(218,244,236,255),
        IM_COL32( 96,232,196, 46),
        { 96.f/255,232.f/255,196.f/255},
        { 12.f/255, 22.f/255, 20.f/255},
        { 22.f/255, 40.f/255, 36.f/255},
        { 16.f/255, 30.f/255, 28.f/255},
        { 40.f/255, 70.f/255, 60.f/255},
        {126.f/255,180.f/255,166.f/255},
        { 32.f/255, 56.f/255, 50.f/255},
        {218.f/255,244.f/255,236.f/255},
        IM_COL32( 22, 40, 36,255), IM_COL32( 96,232,196,255)
    },
    {   "Lava",
        IM_COL32( 18, 10,  8,255), IM_COL32( 26, 14, 10,255),
        IM_COL32( 38, 20, 14,255), IM_COL32( 26, 14, 10,255),
        IM_COL32( 78, 38, 26,255), IM_COL32(196,116, 96,255),
        IM_COL32(255, 96, 32,255),
        IM_COL32(244,220,210,255), IM_COL32( 50, 26, 18,255),
        IM_COL32( 58, 30, 22,255), IM_COL32(110, 64, 50,255),
        IM_COL32(255, 96, 32, 22), IM_COL32(244,220,210,255),
        IM_COL32(255, 96, 32, 52),
        {255.f/255, 96.f/255, 32.f/255},
        { 18.f/255, 10.f/255,  8.f/255},
        { 38.f/255, 20.f/255, 14.f/255},
        { 26.f/255, 14.f/255, 10.f/255},
        { 78.f/255, 38.f/255, 26.f/255},
        {196.f/255,116.f/255, 96.f/255},
        { 58.f/255, 30.f/255, 22.f/255},
        {244.f/255,220.f/255,210.f/255},
        IM_COL32( 38, 20, 14,255), IM_COL32(255, 96, 32,255)
    },
    {   "Glacier",
        IM_COL32( 14, 22, 30,255), IM_COL32( 18, 30, 40,255),
        IM_COL32( 26, 42, 56,255), IM_COL32( 18, 30, 40,255),
        IM_COL32( 50, 78, 96,255), IM_COL32(142,176,196,255),
        IM_COL32(132,202,232,255),
        IM_COL32(228,240,250,255), IM_COL32( 36, 60, 78,255),
        IM_COL32( 42, 68, 86,255), IM_COL32( 80,114,138,255),
        IM_COL32(132,202,232, 22), IM_COL32(228,240,250,255),
        IM_COL32(132,202,232, 50),
        {132.f/255,202.f/255,232.f/255},
        { 14.f/255, 22.f/255, 30.f/255},
        { 26.f/255, 42.f/255, 56.f/255},
        { 18.f/255, 30.f/255, 40.f/255},
        { 50.f/255, 78.f/255, 96.f/255},
        {142.f/255,176.f/255,196.f/255},
        { 42.f/255, 68.f/255, 86.f/255},
        {228.f/255,240.f/255,250.f/255},
        IM_COL32( 26, 42, 56,255), IM_COL32(132,202,232,255)
    },
    {   "Sakura",
        IM_COL32( 28, 22, 24,255), IM_COL32( 40, 30, 34,255),
        IM_COL32( 52, 38, 44,255), IM_COL32( 40, 30, 34,255),
        IM_COL32( 90, 60, 72,255), IM_COL32(204,172,180,255),
        IM_COL32(255,170,196,255),
        IM_COL32(250,232,238,255), IM_COL32( 60, 42, 50,255),
        IM_COL32( 68, 48, 58,255), IM_COL32(120, 90,100,255),
        IM_COL32(255,170,196, 22), IM_COL32(250,232,238,255),
        IM_COL32(255,170,196, 48),
        {255.f/255,170.f/255,196.f/255},
        { 28.f/255, 22.f/255, 24.f/255},
        { 52.f/255, 38.f/255, 44.f/255},
        { 40.f/255, 30.f/255, 34.f/255},
        { 90.f/255, 60.f/255, 72.f/255},
        {204.f/255,172.f/255,180.f/255},
        { 68.f/255, 48.f/255, 58.f/255},
        {250.f/255,232.f/255,238.f/255},
        IM_COL32( 52, 38, 44,255), IM_COL32(255,170,196,255)
    },
    {   "Carbon",
        IM_COL32( 12, 12, 14,255), IM_COL32( 18, 18, 22,255),
        IM_COL32( 26, 26, 32,255), IM_COL32( 16, 16, 20,255),
        IM_COL32( 48, 48, 58,255), IM_COL32(132,132,148,255),
        IM_COL32( 0,200,170,255),
        IM_COL32(224,228,236,255), IM_COL32( 38, 38, 48,255),
        IM_COL32( 44, 44, 54,255), IM_COL32( 72, 72, 90,255),
        IM_COL32(  0,200,170, 22), IM_COL32(224,228,236,255),
        IM_COL32(  0,200,170, 50),
        {  0.f/255,200.f/255,170.f/255},
        { 12.f/255, 12.f/255, 14.f/255},
        { 26.f/255, 26.f/255, 32.f/255},
        { 16.f/255, 16.f/255, 20.f/255},
        { 48.f/255, 48.f/255, 58.f/255},
        {132.f/255,132.f/255,148.f/255},
        { 44.f/255, 44.f/255, 54.f/255},
        {224.f/255,228.f/255,236.f/255},
        IM_COL32( 26, 26, 32,255), IM_COL32(  0,200,170,255)
    },
    {   "Galaxy",
        IM_COL32(  8, 10, 24,255), IM_COL32( 14, 16, 36,255),
        IM_COL32( 22, 24, 52,255), IM_COL32( 12, 14, 30,255),
        IM_COL32( 44, 48, 88,255), IM_COL32(132,140,196,255),
        IM_COL32(118,108,255,255),
        IM_COL32(224,226,250,255), IM_COL32( 32, 36, 64,255),
        IM_COL32( 38, 42, 76,255), IM_COL32( 68, 76,120,255),
        IM_COL32(118,108,255, 24), IM_COL32(224,226,250,255),
        IM_COL32(118,108,255, 52),
        {118.f/255,108.f/255,255.f/255},
        {  8.f/255, 10.f/255, 24.f/255},
        { 22.f/255, 24.f/255, 52.f/255},
        { 12.f/255, 14.f/255, 30.f/255},
        { 44.f/255, 48.f/255, 88.f/255},
        {132.f/255,140.f/255,196.f/255},
        { 38.f/255, 42.f/255, 76.f/255},
        {224.f/255,226.f/255,250.f/255},
        IM_COL32( 22, 24, 52,255), IM_COL32(118,108,255,255)
    },
    {   "Emerald",
        IM_COL32(  8, 22, 18,255), IM_COL32( 14, 30, 24,255),
        IM_COL32( 20, 42, 32,255), IM_COL32( 14, 30, 24,255),
        IM_COL32( 36, 72, 56,255), IM_COL32(120,170,148,255),
        IM_COL32( 48,210,140,255),
        IM_COL32(216,240,228,255), IM_COL32( 24, 50, 38,255),
        IM_COL32( 30, 58, 46,255), IM_COL32( 60,102, 84,255),
        IM_COL32( 48,210,140, 22), IM_COL32(216,240,228,255),
        IM_COL32( 48,210,140, 50),
        { 48.f/255,210.f/255,140.f/255},
        {  8.f/255, 22.f/255, 18.f/255},
        { 20.f/255, 42.f/255, 32.f/255},
        { 14.f/255, 30.f/255, 24.f/255},
        { 36.f/255, 72.f/255, 56.f/255},
        {120.f/255,170.f/255,148.f/255},
        { 30.f/255, 58.f/255, 46.f/255},
        {216.f/255,240.f/255,228.f/255},
        IM_COL32( 20, 42, 32,255), IM_COL32( 48,210,140,255)
    },
    {   "Coral",
        IM_COL32( 26, 18, 18,255), IM_COL32( 36, 24, 24,255),
        IM_COL32( 48, 32, 32,255), IM_COL32( 36, 24, 24,255),
        IM_COL32( 84, 50, 50,255), IM_COL32(200,150,150,255),
        IM_COL32(255,124,108,255),
        IM_COL32(248,228,224,255), IM_COL32( 54, 34, 34,255),
        IM_COL32( 62, 40, 40,255), IM_COL32(112, 80, 80,255),
        IM_COL32(255,124,108, 22), IM_COL32(248,228,224,255),
        IM_COL32(255,124,108, 50),
        {255.f/255,124.f/255,108.f/255},
        { 26.f/255, 18.f/255, 18.f/255},
        { 48.f/255, 32.f/255, 32.f/255},
        { 36.f/255, 24.f/255, 24.f/255},
        { 84.f/255, 50.f/255, 50.f/255},
        {200.f/255,150.f/255,150.f/255},
        { 62.f/255, 40.f/255, 40.f/255},
        {248.f/255,228.f/255,224.f/255},
        IM_COL32( 48, 32, 32,255), IM_COL32(255,124,108,255)
    },
    {   "Latte",
        IM_COL32(242,236,225,255), IM_COL32(248,243,233,255),
        IM_COL32(255,251,243,255), IM_COL32(244,238,228,255),
        IM_COL32(220,210,194,255), IM_COL32(146,128,108,255),
        IM_COL32(120, 78, 58,255),
        IM_COL32( 60, 42, 32,255), IM_COL32(224,214,198,255),
        IM_COL32(232,222,206,255), IM_COL32(192,178,158,255),
        IM_COL32(120, 78, 58, 18), IM_COL32( 70, 50, 38,255),
        IM_COL32(120, 78, 58, 40),
        {120.f/255, 78.f/255, 58.f/255},
        {242.f/255,236.f/255,225.f/255},
        {255.f/255,251.f/255,243.f/255},
        {244.f/255,238.f/255,228.f/255},
        {220.f/255,210.f/255,194.f/255},
        {146.f/255,128.f/255,108.f/255},
        {232.f/255,222.f/255,206.f/255},
        { 60.f/255, 42.f/255, 32.f/255},
        IM_COL32(255,251,243,255), IM_COL32(120, 78, 58,255)
    },
    {   "Vapor",
        IM_COL32( 18, 14, 32,255), IM_COL32( 26, 18, 44,255),
        IM_COL32( 36, 24, 60,255), IM_COL32( 22, 14, 40,255),
        IM_COL32( 70, 44,108,255), IM_COL32(180,124,206,255),
        IM_COL32(255, 90,220,255),
        IM_COL32(248,222,250,255), IM_COL32( 46, 30, 70,255),
        IM_COL32( 54, 34, 82,255), IM_COL32( 96, 64,130,255),
        IM_COL32(255, 90,220, 24), IM_COL32(248,222,250,255),
        IM_COL32(255, 90,220, 52),
        {255.f/255, 90.f/255,220.f/255},
        { 18.f/255, 14.f/255, 32.f/255},
        { 36.f/255, 24.f/255, 60.f/255},
        { 22.f/255, 14.f/255, 40.f/255},
        { 70.f/255, 44.f/255,108.f/255},
        {180.f/255,124.f/255,206.f/255},
        { 54.f/255, 34.f/255, 82.f/255},
        {248.f/255,222.f/255,250.f/255},
        IM_COL32( 36, 24, 60,255), IM_COL32(255, 90,220,255)
    },
    {   "Slate",
        IM_COL32( 36, 40, 48,255), IM_COL32( 46, 50, 60,255),
        IM_COL32( 56, 62, 74,255), IM_COL32( 42, 46, 56,255),
        IM_COL32( 80, 88,104,255), IM_COL32(154,164,180,255),
        IM_COL32(112,170,210,255),
        IM_COL32(228,232,240,255), IM_COL32( 62, 68, 80,255),
        IM_COL32( 70, 76, 90,255), IM_COL32(102,112,128,255),
        IM_COL32(112,170,210, 22), IM_COL32(228,232,240,255),
        IM_COL32(112,170,210, 48),
        {112.f/255,170.f/255,210.f/255},
        { 36.f/255, 40.f/255, 48.f/255},
        { 56.f/255, 62.f/255, 74.f/255},
        { 42.f/255, 46.f/255, 56.f/255},
        { 80.f/255, 88.f/255,104.f/255},
        {154.f/255,164.f/255,180.f/255},
        { 70.f/255, 76.f/255, 90.f/255},
        {228.f/255,232.f/255,240.f/255},
        IM_COL32( 56, 62, 74,255), IM_COL32(112,170,210,255)
    },
    {   "Honey",
        IM_COL32( 30, 22, 12,255), IM_COL32( 42, 30, 16,255),
        IM_COL32( 56, 40, 20,255), IM_COL32( 42, 30, 16,255),
        IM_COL32( 96, 70, 30,255), IM_COL32(206,170, 96,255),
        IM_COL32(250,184, 56,255),
        IM_COL32(246,236,208,255), IM_COL32( 64, 46, 22,255),
        IM_COL32( 72, 52, 26,255), IM_COL32(120, 92, 54,255),
        IM_COL32(250,184, 56, 22), IM_COL32(246,236,208,255),
        IM_COL32(250,184, 56, 50),
        {250.f/255,184.f/255, 56.f/255},
        { 30.f/255, 22.f/255, 12.f/255},
        { 56.f/255, 40.f/255, 20.f/255},
        { 42.f/255, 30.f/255, 16.f/255},
        { 96.f/255, 70.f/255, 30.f/255},
        {206.f/255,170.f/255, 96.f/255},
        { 72.f/255, 52.f/255, 26.f/255},
        {246.f/255,236.f/255,208.f/255},
        IM_COL32( 56, 40, 20,255), IM_COL32(250,184, 56,255)
    },
    {   "Nordic",
        IM_COL32( 30, 34, 42,255), IM_COL32( 38, 44, 54,255),
        IM_COL32( 48, 54, 66,255), IM_COL32( 36, 42, 52,255),
        IM_COL32( 72, 84,100,255), IM_COL32(148,164,184,255),
        IM_COL32(136,192,208,255),
        IM_COL32(229,233,240,255), IM_COL32( 56, 64, 76,255),
        IM_COL32( 62, 70, 84,255), IM_COL32( 96,108,128,255),
        IM_COL32(136,192,208, 22), IM_COL32(229,233,240,255),
        IM_COL32(136,192,208, 50),
        {136.f/255,192.f/255,208.f/255},
        { 30.f/255, 34.f/255, 42.f/255},
        { 48.f/255, 54.f/255, 66.f/255},
        { 36.f/255, 42.f/255, 52.f/255},
        { 72.f/255, 84.f/255,100.f/255},
        {148.f/255,164.f/255,184.f/255},
        { 62.f/255, 70.f/255, 84.f/255},
        {229.f/255,233.f/255,240.f/255},
        IM_COL32( 48, 54, 66,255), IM_COL32(136,192,208,255)
    },
    {   "Dracula",
        IM_COL32( 40, 42, 54,255), IM_COL32( 50, 52, 66,255),
        IM_COL32( 62, 64, 80,255), IM_COL32( 46, 48, 60,255),
        IM_COL32( 88, 92,114,255), IM_COL32(150,156,180,255),
        IM_COL32(255,121,198,255),
        IM_COL32(248,248,242,255), IM_COL32( 68, 72, 92,255),
        IM_COL32( 76, 80,100,255), IM_COL32(112,118,140,255),
        IM_COL32(255,121,198, 22), IM_COL32(248,248,242,255),
        IM_COL32(255,121,198, 48),
        {255.f/255,121.f/255,198.f/255},
        { 40.f/255, 42.f/255, 54.f/255},
        { 62.f/255, 64.f/255, 80.f/255},
        { 46.f/255, 48.f/255, 60.f/255},
        { 88.f/255, 92.f/255,114.f/255},
        {150.f/255,156.f/255,180.f/255},
        { 76.f/255, 80.f/255,100.f/255},
        {248.f/255,248.f/255,242.f/255},
        IM_COL32( 62, 64, 80,255), IM_COL32(255,121,198,255)
    },
    {   "Pastel",
        IM_COL32(238,232,242,255), IM_COL32(244,238,248,255),
        IM_COL32(252,247,252,255), IM_COL32(240,234,244,255),
        IM_COL32(214,202,224,255), IM_COL32(150,134,168,255),
        IM_COL32(146,118,200,255),
        IM_COL32( 60, 50, 80,255), IM_COL32(220,210,232,255),
        IM_COL32(228,218,238,255), IM_COL32(184,170,206,255),
        IM_COL32(146,118,200, 20), IM_COL32( 70, 56, 92,255),
        IM_COL32(146,118,200, 40),
        {146.f/255,118.f/255,200.f/255},
        {238.f/255,232.f/255,242.f/255},
        {252.f/255,247.f/255,252.f/255},
        {240.f/255,234.f/255,244.f/255},
        {214.f/255,202.f/255,224.f/255},
        {150.f/255,134.f/255,168.f/255},
        {228.f/255,218.f/255,238.f/255},
        { 60.f/255, 50.f/255, 80.f/255},
        IM_COL32(252,247,252,255), IM_COL32(146,118,200,255)
    },
    {   "Toxic",
        IM_COL32(  8, 14, 10,255), IM_COL32( 12, 22, 14,255),
        IM_COL32( 18, 32, 22,255), IM_COL32( 12, 22, 14,255),
        IM_COL32( 36, 64, 40,255), IM_COL32(122,170,128,255),
        IM_COL32(176,255, 56,255),
        IM_COL32(228,250,212,255), IM_COL32( 24, 44, 28,255),
        IM_COL32( 28, 52, 32,255), IM_COL32( 64,100, 72,255),
        IM_COL32(176,255, 56, 22), IM_COL32(228,250,212,255),
        IM_COL32(176,255, 56, 52),
        {176.f/255,255.f/255, 56.f/255},
        {  8.f/255, 14.f/255, 10.f/255},
        { 18.f/255, 32.f/255, 22.f/255},
        { 12.f/255, 22.f/255, 14.f/255},
        { 36.f/255, 64.f/255, 40.f/255},
        {122.f/255,170.f/255,128.f/255},
        { 28.f/255, 52.f/255, 32.f/255},
        {228.f/255,250.f/255,212.f/255},
        IM_COL32( 18, 32, 22,255), IM_COL32(176,255, 56,255)
    },
};

static const int g_themeCount = (int)(sizeof(g_themes) / sizeof(g_themes[0]));
static int g_currentTheme = 0;
static bool g_themePanelOpen = false;
static float g_themePanelAnim = 0.0f;

static int g_themeHoverIdx = -1;

static void ApplyThemePreset(int idx)
{
    if (idx < 0 || idx >= g_themeCount) return;
    g_currentTheme = idx;
}

static void BlendThemeColours(float dt)
{
    int targetIdx = (g_themeHoverIdx >= 0 && g_themePanelOpen)
                    ? g_themeHoverIdx : g_currentTheme;
    if (targetIdx < 0 || targetIdx >= g_themeCount) return;
    const ThemePreset& t = g_themes[targetIdx];

    if (dt <= 0.0f) dt = 1.0f / 60.0f;
    float a = 1.0f - expf(-9.0f * dt);
    if (a < 0.0f) a = 0.0f;
    if (a > 1.0f) a = 1.0f;

    CLR_LAYOUT      = LerpU32(CLR_LAYOUT,      t.layout,      a);
    CLR_TITLEBAR    = LerpU32(CLR_TITLEBAR,    t.titlebar,    a);
    CLR_WIDGET      = LerpU32(CLR_WIDGET,      t.widget,      a);
    CLR_DARK_WIDGET = LerpU32(CLR_DARK_WIDGET, t.dwidget,     a);
    CLR_BORDER      = LerpU32(CLR_BORDER,      t.border,      a);
    CLR_INACTIVE    = LerpU32(CLR_INACTIVE,    t.inactive,    a);
    CLR_ACCENT      = LerpU32(CLR_ACCENT,      t.accent,      a);
    CLR_WHITE       = LerpU32(CLR_WHITE,       t.textPrimary, a);
    CLR_LINE        = LerpU32(CLR_LINE,        t.line,        a);
    CLR_SELECT      = LerpU32(CLR_SELECT,      t.select,      a);
    CLR_CIRCLE_OFF  = LerpU32(CLR_CIRCLE_OFF,  t.circleOff,   a);
    CLR_TAB_GLOW    = LerpU32(CLR_TAB_GLOW,    t.tabGlow,     a);
    CLR_ON          = LerpU32(CLR_ON,          t.on,          a);
    CLR_ON_SOFT     = LerpU32(CLR_ON_SOFT,     t.onSoft,      a);

    for (int i = 0; i < 3; i++) {
        ACCENT[i]       += (t.fAccent[i]   - ACCENT[i])       * a;
        COL_LAYOUT[i]   += (t.fLayout[i]   - COL_LAYOUT[i])   * a;
        COL_WIDGET[i]   += (t.fWidget[i]   - COL_WIDGET[i])   * a;
        COL_DWIDGET[i]  += (t.fDWidget[i]  - COL_DWIDGET[i])  * a;
        COL_BORDER[i]   += (t.fBorder[i]   - COL_BORDER[i])   * a;
        COL_INACTIVE[i] += (t.fInactive[i] - COL_INACTIVE[i]) * a;
        COL_SELECT[i]   += (t.fSelect[i]   - COL_SELECT[i])   * a;
        COL_TEXT[i]     += (t.fText[i]     - COL_TEXT[i])     * a;
    }

    ApplyTheme();
}

static void DrawVLogo(ImDrawList* dl, ImVec2 pos, float sz)
{
    float cx = pos.x + sz * 0.5f;
    float cy = pos.y + sz * 0.5f;
    ImU32 c1 = IM_COL32(100, 160, 255, 255);
    ImU32 c2 = IM_COL32( 55, 110, 210, 255);
    ImU32 c3 = IM_COL32( 30,  70, 160, 255);
    dl->AddTriangleFilled({cx-sz*0.42f,cy-sz*0.05f},{cx+sz*0.02f,cy-sz*0.45f},{cx+sz*0.02f,cy-sz*0.15f},c1);
    dl->AddTriangleFilled({cx+sz*0.02f,cy-sz*0.45f},{cx+sz*0.27f,cy-sz*0.15f},{cx+sz*0.02f,cy-sz*0.15f},c2);
    dl->AddTriangleFilled({cx+sz*0.42f,cy+sz*0.05f},{cx-sz*0.02f,cy+sz*0.45f},{cx-sz*0.02f,cy+sz*0.15f},c1);
    dl->AddTriangleFilled({cx-sz*0.02f,cy+sz*0.45f},{cx-sz*0.27f,cy+sz*0.15f},{cx-sz*0.02f,cy+sz*0.15f},c2);
    dl->AddTriangleFilled({cx+sz*0.02f,cy-sz*0.15f},{cx-sz*0.02f,cy+sz*0.15f},{cx-sz*0.10f,cy-sz*0.05f},c3);
}

static const float TITLEBAR_H = 60.0f;

static const char* TAB_LABELS[5] = { "Main", "Bypass", "Farm", "Hero", "Exit" };
static int   g_currentTab = 0;
static int   g_uiSpeedVal  = 1;
static int   g_uiGoldVal   = 1;
static int   g_uiHeroXpVal  = 1;

#define VMP_CFG_DIR  "C:\\@vmp"

static char  g_cfgName[64]  = "default";
static char  g_cfgList[32][64];
static int   g_cfgListCount = 0;

static void EnsureConfigDir()
{
    CreateDirectoryA(VMP_CFG_DIR, nullptr);
}

static void RefreshConfigList()
{
    g_cfgListCount = 0;
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(VMP_CFG_DIR "\\*.cfg", &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (g_cfgListCount >= 32) break;
        char* dot = strrchr(fd.cFileName, '.');
        if (dot) *dot = '\0';
        strncpy_s(g_cfgList[g_cfgListCount], fd.cFileName, _TRUNCATE);
        g_cfgListCount++;
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

static bool SaveConfig(const char* name)
{
    if (!name || !*name) return false;
    EnsureConfigDir();
    char path[MAX_PATH];
    sprintf_s(path, "%s\\%s.cfg", VMP_CFG_DIR, name);
    FILE* f = nullptr;
    if (fopen_s(&f, path, "w") != 0 || !f) return false;

    int stealth = (CountActive(g_actk, ACTK_COUNT) == (int)ACTK_COUNT) ? 1 : 0;
    int dlc     = (CountActive(g_dlc,  DLC_COUNT)  == (int)DLC_COUNT)  ? 1 : 0;
    int pet     = (CountActive(g_pet,  PET_COUNT)  == (int)PET_COUNT)  ? 1 : 0;
    int god     = (CountActive(g_god,  GOD_COUNT)  == (int)GOD_COUNT)  ? 1 : 0;

    fprintf(f, "vamp_config 1\n");
    fprintf(f, "speed %d\n",   g_uiSpeedVal);
    fprintf(f, "gold %d\n",    g_uiGoldVal);
    fprintf(f, "heroxp %d\n",  g_uiHeroXpVal);
    fprintf(f, "stealth %d\n", stealth);
    fprintf(f, "dlc %d\n",     dlc);
    fprintf(f, "pet %d\n",     pet);
    fprintf(f, "god %d\n",     god);
    fprintf(f, "speed_on %d\n",  g_speedActive ? 1 : 0);
    fprintf(f, "gold_on %d\n",   g_goldApplied ? 1 : 0);
    fprintf(f, "heroxp_on %d\n", g_heroXpApplied ? 1 : 0);
    fclose(f);
    return true;
}

static void SetGroupDesired(Patch* g, size_t n, const char* nm, bool want)
{
    bool cur = (CountActive(g, (int)n) == (int)n);
    if (want && !cur)      ApplyGroup(g, n, nm);
    else if (!want && cur) RestoreGroup(g, n, nm);
}

static bool LoadConfig(const char* name)
{
    if (!name || !*name) return false;
    char path[MAX_PATH];
    sprintf_s(path, "%s\\%s.cfg", VMP_CFG_DIR, name);
    FILE* f = nullptr;
    if (fopen_s(&f, path, "r") != 0 || !f) return false;

    int speed = 1, gold = 1, heroxp = 1, stealth = 0, dlc = 0, pet = 0, god = 0;
    int speedOn = 0, goldOn = 0, heroOn = 0;
    char key[64]; int val;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        if (sscanf_s(line, "%63s %d", key, (unsigned)sizeof(key), &val) == 2) {
            if      (!strcmp(key, "speed"))     speed = val;
            else if (!strcmp(key, "gold"))      gold = val;
            else if (!strcmp(key, "heroxp"))    heroxp = val;
            else if (!strcmp(key, "stealth"))   stealth = val;
            else if (!strcmp(key, "dlc"))       dlc = val;
            else if (!strcmp(key, "pet"))       pet = val;
            else if (!strcmp(key, "god"))       god = val;
            else if (!strcmp(key, "speed_on"))  speedOn = val;
            else if (!strcmp(key, "gold_on"))   goldOn = val;
            else if (!strcmp(key, "heroxp_on")) heroOn = val;
        }
    }
    fclose(f);

    if (speed < 1) speed = 1; if (speed > (int)SPEED_MAX) speed = (int)SPEED_MAX;
    if (gold  < 1) gold = 1;  if (gold  > 5000)           gold = 5000;
    if (heroxp < 1) heroxp = 1; if (heroxp > 1000)        heroxp = 1000;
    g_uiSpeedVal  = speed;
    g_uiGoldVal   = gold;
    g_uiHeroXpVal = heroxp;

    if (g_attached) {
        SetGroupDesired(g_actk, ACTK_COUNT, "ACTk", stealth != 0);
        SetGroupDesired(g_dlc,  DLC_COUNT,  "DLC",  dlc != 0);
        SetGroupDesired(g_pet,  PET_COUNT,  "Pet",  pet != 0);
        SetGroupDesired(g_god,  GOD_COUNT,  "GOD",  god != 0);
        if (speedOn)            ApplySpeedValue((float)speed);
        else if (g_speedActive) ApplySpeedValue(1.0f);
        if (goldOn)             ApplyGoldValue(gold);
        else if (g_goldApplied) ApplyGoldValue(1);
        if (heroOn)             ApplyHeroXp(heroxp);
        else if (g_heroXpApplied) ApplyHeroXp(1);
        SetStatus("Config loaded and applied.", ST_ACTIVE);
    } else {
        SetStatus("Config loaded (will apply when the game opens).", ST_INFO);
    }
    return true;
}

static void OpenConfigFolder()
{
    EnsureConfigDir();
    ShellExecuteA(nullptr, "open", VMP_CFG_DIR, nullptr, nullptr, SW_SHOWNORMAL);
}

static bool BrowseAndLoadConfig()
{
    EnsureConfigDir();

    char file[MAX_PATH] = { 0 };
    OPENFILENAMEA ofn = { 0 };
    ofn.lStructSize  = sizeof(ofn);
    ofn.hwndOwner    = g_hwnd;
    ofn.lpstrFilter  = "VAMP Config (*.cfg)\0*.cfg\0Tum Dosyalar (*.*)\0*.*\0";
    ofn.lpstrFile    = file;
    ofn.nMaxFile     = sizeof(file);
    ofn.lpstrInitialDir = VMP_CFG_DIR;
    ofn.lpstrTitle   = "Config Sec";
    ofn.Flags        = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (!GetOpenFileNameA(&ofn)) return false;

    const char* base = strrchr(file, '\\');
    base = base ? base + 1 : file;
    char name[64];
    strncpy_s(name, base, _TRUNCATE);
    char* dot = strrchr(name, '.');
    if (dot) *dot = '\0';

    strncpy_s(g_cfgName, name, _TRUNCATE);
    return LoadConfig(g_cfgName);
}

static void GenFakeIdentity()
{
    const char* hx = "0123456789ABCDEF";

    sprintf_s(g_fakeIP, "%d.%d.%d.%d",
        11 + rand() % 223, rand() % 256, rand() % 256, 1 + rand() % 254);

    char s[40]; int k = 0;
    for (int grp = 0; grp < 4; grp++) {
        for (int i = 0; i < 4; i++) s[k++] = hx[rand() % 16];
        if (grp < 3) s[k++] = '-';
    }
    s[k] = '\0';
    strcpy_s(g_fakeSerial, s);

    char h[48]; int hk = 0;
    const int seg[5] = { 8, 4, 4, 4, 12 };
    for (int g = 0; g < 5; g++) {
        for (int i = 0; i < seg[g]; i++) h[hk++] = hx[rand() % 16];
        if (g < 4) h[hk++] = '-';
    }
    h[hk] = '\0';
    strcpy_s(g_fakeHWID, h);

    sprintf_s(g_fakeMAC, "%02X:%02X:%02X:%02X:%02X:%02X",
        rand() % 256, rand() % 256, rand() % 256,
        rand() % 256, rand() % 256, rand() % 256);
}

static void ResetSpoofIdentity()
{
    g_spoofOn = false;
    strcpy_s(g_fakeIP, "0.0.0.0");
    strcpy_s(g_fakeSerial, "----");
    strcpy_s(g_fakeHWID, "----");
    strcpy_s(g_fakeMAC, "----");
}

static const char* TextEnd(const char* s)
{
    while (*s) { if (s[0] == '#' && s[1] == '#') return s; s++; }
    return s;
}

static void DrawLabel(ImDrawList* dl, ImVec2 pos, ImU32 col, const char* label)
{
    dl->AddText(pos, col, label, TextEnd(label));
}

static void AnimatedLine(ImDrawList* dl, float x0, float x1, float y,
                         ImU32 baseCol, float period = 3.0f, float segFrac = 0.22f,
                         ImU32 glow = IM_COL32(255, 255, 255, 255))
{
    dl->AddLine(ImVec2(x0, y), ImVec2(x1, y), baseCol, 1.0f);
    float len = x1 - x0;
    if (len <= 2.0f) return;

    float seg = len * segFrac;
    float phase = (float)fmod(ImGui::GetTime() / period, 1.0);
    float cx = x0 - seg + phase * (len + seg * 2.0f);
    float lx = cx - seg, rx = cx + seg;
    if (lx < x0) lx = x0;
    if (rx > x1) rx = x1;
    if (rx <= lx) return;

    const float th = 1.0f;
    ImU32 c0 = glow & 0x00FFFFFF;
    if (cx > lx)
        dl->AddRectFilledMultiColor(ImVec2(lx, y - th), ImVec2(cx, y + th), c0, glow, glow, c0);
    if (rx > cx)
        dl->AddRectFilledMultiColor(ImVec2(cx, y - th), ImVec2(rx, y + th), glow, c0, c0, glow);
}

static bool N9Checkbox(const char* name, bool* v, bool enabled = true)
{
    const float ROW_H = 22.0f;
    const float TW    = 22.0f;
    const float TH    = 12.0f;

    ImVec2 pos   = ImGui::GetCursorScreenPos();
    float  avail = ImGui::GetContentRegionAvail().x;

    bool clicked = false;
    bool hov     = false;

    if (enabled) {
        ImGui::InvisibleButton(name, ImVec2(avail, ROW_H));
        clicked = ImGui::IsItemClicked();
        hov     = ImGui::IsItemHovered();
        if (clicked) *v = !*v;
    } else {
        ImGui::Dummy(ImVec2(avail, ROW_H));
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImGuiID baseId = ImGui::GetID(name);
    float onT  = UIAnim(baseId, (*v && enabled) ? 1.0f : 0.0f, 13.0f);
    float hovT = UIAnim(baseId + 1, hov ? 1.0f : 0.0f, 11.0f);

    if (hovT > 0.001f) {
        dl->AddRectFilled(pos, ImVec2(pos.x + avail, pos.y + ROW_H),
            IM_COL32(0, 0, 0, (int)(10 * hovT)), 4.0f);
        float barH = ROW_H * 0.52f * hovT;
        float bcy  = pos.y + ROW_H * 0.5f;
        dl->AddRectFilled(ImVec2(pos.x, bcy - barH * 0.5f),
            ImVec2(pos.x + 2.0f, bcy + barH * 0.5f),
            IM_COL32(28, 31, 38, (int)(150 * hovT)), 1.0f);
    }

    float slide  = 4.0f * hovT;
    float textY  = pos.y + (ROW_H - ImGui::GetTextLineHeight()) * 0.5f;
    ImU32 lblBase = enabled ? LerpU32(CLR_INACTIVE, CLR_WHITE, onT) : IM_COL32(186, 190, 198, 255);
    ImU32 lblCol  = enabled ? LerpU32(lblBase, CLR_WHITE, hovT * 0.45f) : lblBase;
    DrawLabel(dl, ImVec2(pos.x + slide, textY), lblCol, name);

    float ty = pos.y + (ROW_H - TH) * 0.5f;
    float tx = pos.x + avail - TW;
    ImU32 trackOff = enabled ? IM_COL32(206, 210, 218, 255) : IM_COL32(226, 229, 234, 255);
    ImU32 trackCol = LerpU32(trackOff, CLR_ON, onT);
    dl->AddRectFilled(ImVec2(tx, ty), ImVec2(tx + TW, ty + TH), trackCol, TH * 0.5f);

    float cr = 4.0f;
    float xOff = cr + 2.0f;
    float cx = (tx + xOff) + onT * (TW - 2.0f * xOff);
    dl->AddCircleFilled(ImVec2(cx, ty + TH * 0.5f + 0.5f), cr + 0.5f, IM_COL32(0, 0, 0, 40), 16);
    dl->AddCircleFilled(ImVec2(cx, ty + TH * 0.5f), cr, IM_COL32(255, 255, 255, 255), 16);

    return clicked;
}

static ImU32 ShiftU32(ImU32 c, int amt)
{
    int r = (c >> IM_COL32_R_SHIFT) & 0xFF;
    int g = (c >> IM_COL32_G_SHIFT) & 0xFF;
    int b = (c >> IM_COL32_B_SHIFT) & 0xFF;
    int a = (c >> IM_COL32_A_SHIFT) & 0xFF;
    auto cl = [](int x){ return x < 0 ? 0 : (x > 255 ? 255 : x); };
    return IM_COL32(cl(r + amt), cl(g + amt), cl(b + amt), a);
}

struct N9BtnStyle {
    ImU32 fill        = IM_COL32(29, 31, 42, 255);
    ImU32 fillHov     = IM_COL32(34, 37, 50, 255);
    ImU32 fillAct     = IM_COL32(26, 28, 38, 255);
    ImU32 border      = 0;
    ImU32 borderHov   = 0;
    ImU32 text        = CLR_WHITE;
    ImU32 textHov     = CLR_WHITE;
    float rounding    = 8.0f;
    bool  gradient    = true;
    bool  sheen       = true;
    bool  bloom       = false;
    bool  accentLine  = false;
    ImU32 accentCol   = IM_COL32(255, 255, 255, 255);
};

static bool N9ButtonEx(const char* id, const char* label, ImVec2 sz, const N9BtnStyle& st)
{
    ImVec2 p = ImGui::GetCursorScreenPos();
    if (sz.x <= 0) sz.x = ImGui::GetContentRegionAvail().x;
    if (sz.y <= 0) sz.y = 32.0f;

    ImGui::InvisibleButton(id, sz);
    bool clicked = ImGui::IsItemClicked();
    bool hov     = ImGui::IsItemHovered();
    bool act     = ImGui::IsItemActive();

    ImVec2 mn = p;
    ImVec2 mx = ImVec2(p.x + sz.x, p.y + sz.y);
    float  r  = st.rounding;

    ImGuiID aid = ImGui::GetID(id);
    float hovT = UIAnim(aid, hov ? 1.0f : 0.0f, 12.0f);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    if (st.bloom) {
        const int layers = 4;
        float base = 14.0f + 30.0f * hovT;
        for (int i = layers; i >= 1; --i) {
            float e = (4.0f * i) / layers;
            int a = (int)(base * (1.0f - (float)(i - 1) / layers) * 0.6f);
            if (a < 1) continue;
            dl->AddRect(ImVec2(mn.x - e, mn.y - e), ImVec2(mx.x + e, mx.y + e),
                IM_COL32(255, 255, 255, a), r + e, 0, 1.5f);
        }
    }

    ImU32 fillBase = act ? st.fillAct : LerpU32(st.fill, st.fillHov, hovT);

    if (st.gradient && ((fillBase >> IM_COL32_A_SHIFT) & 0xFF) > 8) {
        ImU32 top = ShiftU32(fillBase, 12);
        ImU32 bot = ShiftU32(fillBase, -8);
        dl->AddRectFilled(mn, mx, bot, r);
        dl->PushClipRect(mn, mx, true);
        dl->AddRectFilledMultiColor(mn, ImVec2(mx.x, mn.y + (mx.y - mn.y) * 0.55f),
            top, top, bot, bot);
        dl->PopClipRect();
    } else {
        dl->AddRectFilled(mn, mx, fillBase, r);
    }

    if (st.sheen && !act) {
        int sa = (int)(14 + 10 * hovT);
        dl->PushClipRect(mn, mx, true);
        dl->AddRectFilledMultiColor(
            ImVec2(mn.x + r, mn.y + 1.0f), ImVec2(mx.x - r, mn.y + (mx.y - mn.y) * 0.5f),
            IM_COL32(255, 255, 255, sa), IM_COL32(255, 255, 255, sa),
            IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 0));
        dl->PopClipRect();
    }

    if (st.border) {
        ImU32 bc = st.borderHov ? LerpU32(st.border, st.borderHov, hovT) : st.border;
        dl->AddRect(mn, mx, bc, r, 0, 1.0f);
    }

    if (st.accentLine && hovT > 0.001f) {
        float cxb   = (mn.x + mx.x) * 0.5f;
        float halfW = (sz.x * 0.5f - r - 6.0f) * hovT;
        float ly    = mx.y - 5.0f;
        int ac = (st.accentCol >> IM_COL32_A_SHIFT) & 0xFF;
        int rr = (st.accentCol >> IM_COL32_R_SHIFT) & 0xFF;
        int gg = (st.accentCol >> IM_COL32_G_SHIFT) & 0xFF;
        int bb = (st.accentCol >> IM_COL32_B_SHIFT) & 0xFF;
        int a1 = (int)(ac * 0.16f * hovT);
        ImU32 g1 = IM_COL32(rr, gg, bb, a1);
        ImU32 cl = IM_COL32(rr, gg, bb, 0);
        dl->AddRectFilledMultiColor(ImVec2(cxb - halfW, ly - 3.0f), ImVec2(cxb, ly + 3.0f), cl, g1, g1, cl);
        dl->AddRectFilledMultiColor(ImVec2(cxb, ly - 3.0f), ImVec2(cxb + halfW, ly + 3.0f), g1, cl, cl, g1);
        int a2 = (int)(ac * 0.85f * hovT);
        ImU32 c2 = IM_COL32(rr, gg, bb, a2);
        dl->AddRectFilledMultiColor(ImVec2(cxb - halfW, ly - 0.6f), ImVec2(cxb, ly + 0.6f), cl, c2, c2, cl);
        dl->AddRectFilledMultiColor(ImVec2(cxb, ly - 0.6f), ImVec2(cxb + halfW, ly + 0.6f), c2, cl, cl, c2);
    }

    ImVec2 ts = ImGui::CalcTextSize(label);
    const char* te = TextEnd(label);
    float tw = ImGui::CalcTextSize(label, te).x;
    ImU32 tcol = LerpU32(st.text, st.textHov, hovT);
    dl->AddText(ImVec2(p.x + (sz.x - tw) * 0.5f, p.y + (sz.y - ts.y) * 0.5f), tcol, label, te);
    return clicked;
}

static bool N9ClassicButton(const char* label, ImVec2 sz = ImVec2(0, 0))
{
    char id[128]; snprintf(id, sizeof(id), "##c_%s", label);
    N9BtnStyle st;
    st.fill       = CLR_DARK_WIDGET;
    st.fillHov    = CLR_SELECT;
    st.fillAct    = Darken(CLR_SELECT, 0.06f);
    st.border     = CLR_BORDER;
    st.borderHov  = Lighten(CLR_BORDER, 0.12f);
    st.text       = CLR_INACTIVE;
    st.textHov    = CLR_WHITE;
    st.gradient   = false;
    st.sheen      = false;
    st.bloom      = false;
    st.accentLine = true;
    st.accentCol  = CLR_ACCENT;
    return N9ButtonEx(id, label, sz, st);
}

static bool N9AccentButton(const char* label, ImVec2 sz = ImVec2(0, 0))
{
    char id[128]; snprintf(id, sizeof(id), "##a_%s", label);
    N9BtnStyle st;
    st.fill      = CLR_ACCENT;
    st.fillHov   = Lighten(CLR_ACCENT, 0.18f);
    st.fillAct   = Darken (CLR_ACCENT, 0.22f);
    st.border    = IM_COL32(0, 0, 0, 0);
    st.borderHov = IM_COL32(0, 0, 0, 0);
    st.text      = CLR_TITLEBAR;
    st.textHov   = CLR_WIDGET;
    st.gradient  = true;
    st.sheen     = true;
    st.bloom     = false;
    return N9ButtonEx(id, label, sz, st);
}

static bool N9DangerButton(const char* label, ImVec2 sz = ImVec2(0, 0))
{
    char id[128]; snprintf(id, sizeof(id), "##d_%s", label);
    static const ImU32 DANGER       = IM_COL32(212,  88,  94, 255);
    static const ImU32 DANGER_HOV   = IM_COL32(232, 108, 114, 255);
    N9BtnStyle st;
    st.fill      = CLR_WIDGET;
    st.fillHov   = LerpU32(CLR_WIDGET, DANGER, 0.10f);
    st.fillAct   = LerpU32(CLR_WIDGET, DANGER, 0.18f);
    st.border    = LerpU32(CLR_BORDER, DANGER, 0.55f);
    st.borderHov = DANGER_HOV;
    st.text      = DANGER;
    st.textHov   = DANGER_HOV;
    st.gradient  = false;
    st.sheen     = false;
    st.bloom     = false;
    return N9ButtonEx(id, label, sz, st);
}

static void N9SectionLabel(const char* lbl)
{
    ImGui::Dummy(ImVec2(0, 6));
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 ts = ImGui::CalcTextSize(lbl);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddText(p, CLR_INACTIVE, lbl);
    float lineY = p.y + ts.y * 0.5f;
    float lineX = p.x + ts.x + 8.0f;
    float lineXe = p.x + ImGui::GetContentRegionAvail().x;
    if (lineXe > lineX)
        dl->AddLine(ImVec2(lineX, lineY), ImVec2(lineXe, lineY), CLR_LINE, 1.0f);
    ImGui::Dummy(ImVec2(0, ts.y + 4));
}

static void DrawAnimatedLine(ImDrawList* dl, float x0, float x1, float y,
                             ImU32 baseCol, float period = 3.2f, float phase = 0.0f)
{
    if (x1 <= x0) return;
    dl->AddLine(ImVec2(x0, y), ImVec2(x1, y), baseCol, 1.0f);

    float w   = x1 - x0;
    float seg = w * 0.22f; if (seg < 46.0f) seg = 46.0f;
    float span = w + seg;
    float t  = fmodf((float)ImGui::GetTime() / period + phase, 1.0f);
    float cx = x0 - seg * 0.5f + t * span;

    float xa = cx - seg * 0.5f;
    float xb = cx + seg * 0.5f;
    float ca = xa < x0 ? x0 : xa;
    float cb = xb > x1 ? x1 : xb;
    if (cb <= ca) return;

    const ImU32 white = IM_COL32(120, 128, 144, 150);
    const ImU32 clear = IM_COL32(120, 128, 144, 0);
    float yt = y - 1.0f, yb = y + 1.0f;

    if (cx > ca) {
        float lb = cx < cb ? cx : cb;
        dl->AddRectFilledMultiColor(ImVec2(ca, yt), ImVec2(lb, yb), clear, white, white, clear);
    }
    if (cb > cx) {
        float ra = cx > ca ? cx : ca;
        dl->AddRectFilledMultiColor(ImVec2(ra, yt), ImVec2(cb, yb), white, clear, clear, white);
    }
}

static void DrawRingStreak(ImDrawList* dl, float x0, float y0, float x1, float y1,
                           float r, float period, float streakFrac = 0.24f,
                           float coreA = 135.0f, float glowA = 22.0f,
                           float coreW = 1.2f, float glowW = 3.0f,
                           ImU32 streakCol = IM_COL32(255, 255, 255, 255))
{
    int scR = (streakCol >> IM_COL32_R_SHIFT) & 0xFF;
    int scG = (streakCol >> IM_COL32_G_SHIFT) & 0xFF;
    int scB = (streakCol >> IM_COL32_B_SHIFT) & 0xFF;
    const float P = 3.14159265f;
    dl->PathClear();
    dl->PathArcTo(ImVec2(x0 + r, y0 + r), r, P,        P * 1.5f, 10);
    dl->PathArcTo(ImVec2(x1 - r, y0 + r), r, P * 1.5f, P * 2.0f, 10);
    dl->PathArcTo(ImVec2(x1 - r, y1 - r), r, 0.0f,     P * 0.5f, 10);
    dl->PathArcTo(ImVec2(x0 + r, y1 - r), r, P * 0.5f, P,        10);
    int pn = dl->_Path.Size;
    if (pn < 3) { dl->PathClear(); return; }

    static ImVector<ImVec2> raw;  raw.resize(pn);
    for (int i = 0; i < pn; i++) raw[i] = dl->_Path[i];
    dl->PathClear();

    static ImVector<float> cum;  cum.resize(pn + 1);
    cum[0] = 0.0f;
    for (int i = 0; i < pn; i++) {
        ImVec2 a = raw[i], b = raw[(i + 1) % pn];
        float dx = b.x - a.x, dy = b.y - a.y;
        cum[i + 1] = cum[i] + sqrtf(dx * dx + dy * dy);
    }
    float total = cum[pn];
    if (total < 1.0f) return;

    const int N = 220;
    static ImVector<ImVec2> samp;  samp.resize(N);
    int seg = 0;
    for (int k = 0; k < N; k++) {
        float d = (float)k / N * total;
        while (seg < pn && cum[seg + 1] < d) seg++;
        float segLen = cum[seg + 1] - cum[seg];
        float t = segLen > 0.0001f ? (d - cum[seg]) / segLen : 0.0f;
        ImVec2 a = raw[seg], b = raw[(seg + 1) % pn];
        samp[k] = ImVec2(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t);
    }

    float head   = fmodf((float)ImGui::GetTime() / period, 1.0f) * total;
    float streak = total * streakFrac;

    for (int pass = 0; pass < 3; pass++) {
        float wdt, maxA;
        if      (pass == 0) { wdt = glowW * 1.8f; maxA = glowA * 0.6f; }
        else if (pass == 1) { wdt = glowW;        maxA = glowA;        }
        else                { wdt = coreW;        maxA = coreA;        }
        for (int k = 0; k < N; k++) {
            float arc = (float)k / N * total;
            float dd = head - arc; if (dd < 0) dd += total;
            if (dd > streak) continue;
            float t = 1.0f - dd / streak;
            float inten = t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
            int a = (int)(maxA * inten);
            if (a < 1) continue;
            dl->AddLine(samp[k], samp[(k + 1) % N], IM_COL32(scR, scG, scB, a), wdt);
        }
    }
}

static int g_cardPhaseIdx = 0;

static void N9BeginCard(const char* id)
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(COL_WIDGET[0], COL_WIDGET[1], COL_WIDGET[2], 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(COL_BORDER[0], COL_BORDER[1], COL_BORDER[2], 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 7.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 12));
    ImGui::BeginChild(id, ImVec2(0, 0),
        ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();
    ImVec2 ws = ImGui::GetWindowSize();
    float rnd = 7.0f;

    ImU32 sheen     = CLR_TITLEBAR;
    ImU32 sheenZero = (sheen & 0x00FFFFFFu);
    dl->AddRectFilledMultiColor(
        ImVec2(wp.x + 1.0f, wp.y + 1.0f),
        ImVec2(wp.x + ws.x - 1.0f, wp.y + 34.0f),
        sheen, sheen, sheenZero, sheenZero);

    float phase = 0.16f * (float)g_cardPhaseIdx++;
    DrawAnimatedLine(dl, wp.x + rnd, wp.x + ws.x - rnd, wp.y + 0.5f,
                     CLR_BORDER, 3.4f, phase);
}

static void N9EndCard()
{
    ImGui::EndChild();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
    ImGui::Dummy(ImVec2(0, 4));
}

static void N9CardHeader(const char* title, const char* sub = nullptr,
                         const char* badge = nullptr, bool badgeOn = false)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float availW = ImGui::GetContentRegionAvail().x;

    float th = ImGui::GetTextLineHeight();
    float barTop = p.y + 2.5f;
    float barBot = p.y + th - 1.5f;
    dl->AddRectFilled(ImVec2(p.x, barTop), ImVec2(p.x + 3.0f, barBot), CLR_ACCENT, 1.5f);
    dl->AddText(ImVec2(p.x + 11.0f, p.y), CLR_WHITE, title);

    (void)sub;
    (void)badge;
    if (badgeOn) {
        float dotX = p.x + availW - 4.0f;
        float dotY = p.y + 6.0f;
        dl->AddCircleFilled(ImVec2(dotX, dotY), 6.0f, CLR_ON_SOFT, 16);
        dl->AddCircleFilled(ImVec2(dotX, dotY), 3.0f, CLR_ON, 16);
    }

    float ly = p.y + 21.0f;
    dl->AddRectFilledMultiColor(ImVec2(p.x, ly - 1.5f), ImVec2(p.x + availW, ly + 1.5f),
        (CLR_LINE & 0x00FFFFFFu) | 0x0A000000u, (CLR_LINE & 0x00FFFFFFu),
        (CLR_LINE & 0x00FFFFFFu), (CLR_LINE & 0x00FFFFFFu) | 0x0A000000u);
    dl->AddLine(ImVec2(p.x, ly), ImVec2(p.x + availW, ly), CLR_LINE, 1.0f);

    ImGui::Dummy(ImVec2(0, 30));
}

static bool N9Slider(const char* id, int* v, int vmin, int vmax, float width)
{
    const float H = 30.0f;
    ImVec2 p = ImGui::GetCursorScreenPos();
    if (width <= 0) width = ImGui::GetContentRegionAvail().x;

    ImGui::InvisibleButton(id, ImVec2(width, H));
    bool act = ImGui::IsItemActive();
    bool hov = ImGui::IsItemHovered();

    const float KR = 10.0f;
    float x0 = p.x + KR, x1 = p.x + width - KR;
    float cy = p.y + H * 0.5f;
    if (x1 < x0) x1 = x0;

    bool changed = false;
    if (act && x1 > x0) {
        float t = (ImGui::GetIO().MousePos.x - x0) / (x1 - x0);
        if (t < 0) t = 0; if (t > 1) t = 1;
        int nv = vmin + (int)(t * (float)(vmax - vmin) + 0.5f);
        if (nv != *v) { *v = nv; changed = true; }
    }

    float targetT = (vmax > vmin) ? (float)(*v - vmin) / (float)(vmax - vmin) : 0.0f;
    if (targetT < 0) targetT = 0; if (targetT > 1) targetT = 1;

    ImGuiID aid = ImGui::GetID(id);
    float t    = UIAnim(aid, targetT, act ? 70.0f : 24.0f);
    float hovT = UIAnim(aid + 1, act ? 1.0f : (hov ? 0.55f : 0.0f), 16.0f);
    float fx   = x0 + t * (x1 - x0);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float TH = 4.0f;

    dl->AddRectFilled(ImVec2(x0, cy - TH * 0.5f), ImVec2(x1, cy + TH * 0.5f),
        CLR_BORDER, TH * 0.5f);

    if (fx > x0 + 1.0f) {
        ImU32 fillCol = LerpU32(Lighten(CLR_ACCENT, 0.25f), CLR_ACCENT, 0.25f + 0.5f * hovT);
        dl->AddRectFilled(ImVec2(x0, cy - TH * 0.5f), ImVec2(fx, cy + TH * 0.5f), fillCol, TH * 0.5f);
    }

    float kr = 6.0f + 1.5f * hovT;
    dl->AddCircleFilled(ImVec2(fx, cy + 1.0f), kr + 0.5f, IM_COL32(0, 0, 0, 45), 24);
    dl->AddCircleFilled(ImVec2(fx, cy),       kr,        CLR_WIDGET, 24);
    dl->AddCircle      (ImVec2(fx, cy),       kr,        CLR_BORDER, 24, 1.0f);
    dl->AddCircle(ImVec2(fx, cy), kr, IM_COL32(150, 156, 170, 200), 24, 1.0f);
    dl->AddCircleFilled(ImVec2(fx, cy), 2.2f + 0.6f * hovT, IM_COL32(34, 38, 48, 255), 16);
    return changed;
}

static bool N9ValueInput(const char* id, int* v, int vmin, int vmax, float width)
{
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 5));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(COL_DWIDGET[0], COL_DWIDGET[1], COL_DWIDGET[2], 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(COL_BORDER[0],  COL_BORDER[1],  COL_BORDER[2],  1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text,    ImVec4(COL_TEXT[0],    COL_TEXT[1],    COL_TEXT[2],    1.0f));
    ImGui::SetNextItemWidth(width);
    bool ch = ImGui::InputInt(id, v, 0, 0);
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(3);
    if (*v < vmin) *v = vmin;
    if (*v > vmax) *v = vmax;
    return ch;
}

static void N9StatusRow(const char* name, bool active, const char* extra = nullptr)
{
    ImVec2 p = ImGui::GetCursorScreenPos();
    float rowH = ImGui::GetTextLineHeight() + 4.0f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 dotCol = active ? IM_COL32(100, 210, 120, 255) : CLR_CIRCLE_OFF;
    dl->AddCircleFilled(ImVec2(p.x + 5.0f, p.y + rowH * 0.5f), 3.5f, dotCol, 12);
    ImGui::Dummy(ImVec2(14, rowH));
    ImGui::SameLine(0, 0);
    ImGui::SetCursorScreenPos(ImVec2(p.x + 14.0f, p.y));
    ImGui::PushStyleColor(ImGuiCol_Text, active
        ? ImVec4(COL_TEXT[0], COL_TEXT[1], COL_TEXT[2], 1.0f)
        : ImVec4(COL_INACTIVE[0], COL_INACTIVE[1], COL_INACTIVE[2], 1.0f));
    if (extra) ImGui::Text("%-26s  %s", name, extra);
    else       ImGui::Text("%-26s  %s", name, active ? "ON" : "OFF");
    ImGui::PopStyleColor();
}

static void DrawTabMain()
{
    int ac = CountActive(g_actk, ACTK_COUNT);
    int dc = CountActive(g_dlc,  DLC_COUNT);
    int pc = CountActive(g_pet,  PET_COUNT);
    int gc = CountActive(g_god,  GOD_COUNT);

    if (!g_attached) {
        N9BeginCard("##card_conn");
        N9CardHeader("Connection", nullptr, "OFFLINE", false);
        ImGui::PushStyleColor(ImGuiCol_Text,
            ImVec4(COL_INACTIVE[0], COL_INACTIVE[1], COL_INACTIVE[2], 1.0f));
        ImGui::TextWrapped("Game not found. The trainer scans for the game every 3 seconds.");
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0, 8));
        float aw = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + aw - 150.0f);
        if (N9AccentButton("Retry", ImVec2(150, 28))) {
            SetStatus("Connecting...", ST_INFO);
            CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
                g_attached = AttachToGame();
                return 0;
            }, nullptr, 0, nullptr);
        }
        N9EndCard();
        return;
    }

    N9BeginCard("##card_quick");
    N9CardHeader("Quick Actions", "one-click setup");
    if (N9AccentButton("LEGIT COMBO  -  Speed 3x + Gold 3x + All Bypasses", ImVec2(-1, 30))) {
        DoFullCombo(); g_uiSpeedVal = 3; g_uiGoldVal = 3;
    }
    ImGui::Dummy(ImVec2(0, 2));
    if (N9DangerButton("Restore All  -  Leave Game Clean", ImVec2(-1, 30))) {
        DoRestoreAll(); g_uiSpeedVal = 1; g_uiGoldVal = 1;
    }
    N9EndCard();

    N9BeginCard("##card_status");
    N9CardHeader("Status", nullptr, "CONNECTED", true);
    N9StatusRow("Stealth  (ACTk + Steam)", ac == (int)ACTK_COUNT);
    N9StatusRow("DLC Unlocker",            dc == (int)DLC_COUNT);
    N9StatusRow("Pet Unlocker",            pc == (int)PET_COUNT);
    char spdBuf[32]; sprintf_s(spdBuf, "%.1fx", g_curSpeed);
    N9StatusRow("Speed Hack",  g_speedActive, g_speedActive ? spdBuf : nullptr);
    char gldBuf[32]; sprintf_s(gldBuf, "x%d [%s]", g_goldAmount, GoldRiskLabel(g_goldAmount));
    N9StatusRow("Gold Multiplier", g_goldApplied, g_goldApplied ? gldBuf : nullptr);
    N9StatusRow("God Mode",    gc == (int)GOD_COUNT);
    char wdBuf[48]; sprintf_s(wdBuf, "cycles %d  repairs %d", g_watchdogCycles, g_watchdogRepairs);
    N9StatusRow("Watchdog",    g_watchdogRun, g_watchdogRun ? wdBuf : nullptr);
    N9EndCard();

    N9BeginCard("##card_cfg");
    N9CardHeader("Config", "saved to C:\\@vmp");

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(9, 6));
    ImVec4 cfgBg     = ImGui::ColorConvertU32ToFloat4(CLR_DARK_WIDGET);
    ImVec4 cfgBorder = ImGui::ColorConvertU32ToFloat4(CLR_BORDER);
    ImVec4 cfgText   = ImGui::ColorConvertU32ToFloat4(CLR_WHITE);
    ImVec4 cfgHint   = ImGui::ColorConvertU32ToFloat4(CLR_INACTIVE);
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        cfgBg);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, cfgBg);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  cfgBg);
    ImGui::PushStyleColor(ImGuiCol_Border,         cfgBorder);
    ImGui::PushStyleColor(ImGuiCol_Text,           cfgText);
    ImGui::PushStyleColor(ImGuiCol_TextDisabled,   cfgHint);
    float btnW = 92.0f;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - (btnW + 8.0f) * 3.0f);
    ImGui::InputTextWithHint("##cfgname", "config name", g_cfgName, sizeof(g_cfgName));
    ImGui::PopStyleColor(6);
    ImGui::PopStyleVar(3);

    ImGui::SameLine(0, 8);
    if (N9AccentButton("Save##cfg", ImVec2(btnW, 30))) {
        if (SaveConfig(g_cfgName)) { SetStatus("Config saved.", ST_ACTIVE); RefreshConfigList(); }
        else SetStatus("Failed to save config.", ST_WARN);
    }
    ImGui::SameLine(0, 8);
    if (N9ClassicButton("Load##cfg", ImVec2(btnW, 30))) {
        if (BrowseAndLoadConfig()) RefreshConfigList();
    }
    ImGui::SameLine(0, 8);
    if (N9ClassicButton("Folder##cfg", ImVec2(btnW, 30))) OpenConfigFolder();

    if (g_cfgListCount > 0) {
        ImGui::Dummy(ImVec2(0, 4));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(COL_INACTIVE[0],COL_INACTIVE[1],COL_INACTIVE[2],1));
        ImGui::TextUnformatted("Saved configs:");
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0, 2));
        for (int i = 0; i < g_cfgListCount; i++) {
            char rowId[80]; snprintf(rowId, sizeof(rowId), "##cfgrow_%d", i);
            ImVec2 rp = ImGui::GetCursorScreenPos();
            float rw = ImGui::GetContentRegionAvail().x;
            ImGui::InvisibleButton(rowId, ImVec2(rw, 22.0f));
            bool rhov = ImGui::IsItemHovered();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            if (rhov)
                dl->AddRectFilled(rp, ImVec2(rp.x + rw, rp.y + 22.0f), CLR_SELECT, 4.0f);
            dl->AddText(ImVec2(rp.x + 6.0f, rp.y + 3.0f),
                rhov ? CLR_WHITE : CLR_INACTIVE, g_cfgList[i]);
            const char* hint = "click to load";
            ImVec2 hs = ImGui::CalcTextSize(hint);
            if (rhov)
                dl->AddText(ImVec2(rp.x + rw - hs.x - 6.0f, rp.y + 3.0f), CLR_INACTIVE, hint);
            if (ImGui::IsItemClicked()) {
                strncpy_s(g_cfgName, g_cfgList[i], _TRUNCATE);
                if (!LoadConfig(g_cfgName)) SetStatus("Config not found.", ST_WARN);
            }
        }
    }
    N9EndCard();
}

static void DrawTabBypass()
{
    bool actk_on = (CountActive(g_actk, ACTK_COUNT) == (int)ACTK_COUNT);
    bool dlc_on  = (CountActive(g_dlc,  DLC_COUNT)  == (int)DLC_COUNT);
    bool pet_on  = (CountActive(g_pet,  PET_COUNT)  == (int)PET_COUNT);

    if (!g_attached) {
        ImGui::PushStyleColor(ImGuiCol_Text,
            ImVec4(COL_INACTIVE[0], COL_INACTIVE[1], COL_INACTIVE[2], 1.0f));
        ImGui::TextWrapped("Not connected to the game. Try connecting from the Main tab.");
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0, 6));
    }

    N9BeginCard("##card_ac");
    N9CardHeader("Anticheat / Telemetry", nullptr, actk_on ? "ON" : "OFF", actk_on);
    if (N9Checkbox("ACTk + Steam Stealth", &actk_on, g_attached)) DoBypass();
    N9EndCard();

    bool unl_on = dlc_on && pet_on;
    N9BeginCard("##card_unl");
    N9CardHeader("Unlockers", nullptr, unl_on ? "ON" : (dlc_on || pet_on ? "PARTIAL" : "OFF"), unl_on);
    if (N9Checkbox("DLC Unlocker", &dlc_on, g_attached)) DoDLC();
    ImGui::Dummy(ImVec2(0, 2));
    if (N9Checkbox("Pet Unlocker", &pet_on, g_attached)) DoPet();
    N9EndCard();

    N9BeginCard("##card_spoof");
    N9CardHeader("Identity Spoofer", nullptr, nullptr, g_spoofOn);
    if (N9Checkbox("Spoof enabled", &g_spoofOn, true)) {
        if (g_spoofOn) { GenFakeIdentity(); SetStatus("Identity spoof enabled.", ST_ACTIVE); }
        else           { ResetSpoofIdentity(); SetStatus("Identity spoof disabled.", ST_TEXT); }
    }
    ImGui::Dummy(ImVec2(0, 4));

    auto idRow = [](const char* lbl, const char* val, bool on) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        float avail = ImGui::GetContentRegionAvail().x;
        float rowH = 24.0f;
        char rid[64]; snprintf(rid, sizeof(rid), "##idrow_%s", lbl);
        ImGui::InvisibleButton(rid, ImVec2(avail, rowH));
        bool hov = ImGui::IsItemHovered();
        dl->AddRectFilled(p, ImVec2(p.x + avail, p.y + rowH),
            CLR_DARK_WIDGET, 5.0f);
        dl->AddRect(p, ImVec2(p.x + avail, p.y + rowH),
            hov ? Lighten(CLR_BORDER, 0.20f) : CLR_BORDER, 5.0f, 0, 1.0f);
        dl->AddText(ImVec2(p.x + 10.0f, p.y + (rowH - ImGui::GetTextLineHeight()) * 0.5f),
            CLR_INACTIVE, lbl);
        ImU32 vcol = on ? CLR_WHITE : CLR_INACTIVE;
        ImVec2 vs = ImGui::CalcTextSize(val);
        dl->AddText(ImVec2(p.x + avail - vs.x - 10.0f, p.y + (rowH - vs.y) * 0.5f), vcol, val);
        if (ImGui::IsItemClicked() && on) ImGui::SetClipboardText(val);
    };
    idRow("Fake IP", g_spoofOn ? g_fakeIP : "hidden", g_spoofOn);
    ImGui::Dummy(ImVec2(0, 4));
    idRow("Fake HWID", g_spoofOn ? g_fakeHWID : "hidden", g_spoofOn);
    ImGui::Dummy(ImVec2(0, 4));
    idRow("Fake MAC", g_spoofOn ? g_fakeMAC : "hidden", g_spoofOn);
    ImGui::Dummy(ImVec2(0, 4));
    idRow("Disk Serial", g_spoofOn ? g_fakeSerial : "hidden", g_spoofOn);

    ImGui::Dummy(ImVec2(0, 6));
    if (N9ClassicButton("Generate New Identity", ImVec2(-1, 28))) {
        GenFakeIdentity();
        if (!g_spoofOn) g_spoofOn = true;
        SetStatus("New fake identity generated.", ST_ACTIVE);
    }
    ImGui::Dummy(ImVec2(0, 2));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(COL_INACTIVE[0],COL_INACTIVE[1],COL_INACTIVE[2],1));
    ImGui::TextWrapped("Note: A VPN is required to hide your real network IP. This module "
                       "feeds fake values into telemetry and game-identity fields.");
    ImGui::PopStyleColor();
    N9EndCard();
}

static void DrawTabFarm()
{
    if (!g_attached) {
        ImGui::PushStyleColor(ImGuiCol_Text,
            ImVec4(COL_INACTIVE[0], COL_INACTIVE[1], COL_INACTIVE[2], 1.0f));
        ImGui::TextWrapped("Not connected to the game. Try connecting from the Main tab.");
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0, 6));
    }

    {
        char badge[48];
        if (g_speedActive) sprintf_s(badge, "ON  %dx", (int)(g_curSpeed + 0.001f));
        else               sprintf_s(badge, "OFF");

        N9BeginCard("##card_speed");
        N9CardHeader("Speed Hack", "range + attack sync", badge, g_speedActive);

        float avail  = ImGui::GetContentRegionAvail().x;
        float inputW = 64.0f;
        N9Slider("##spd_sl", &g_uiSpeedVal, 1, (int)SPEED_MAX, avail - inputW - 12.0f);
        ImGui::SameLine(0, 12);
        N9ValueInput("##spd_in", &g_uiSpeedVal, 1, (int)SPEED_MAX, inputW);

        ImGui::Dummy(ImVec2(0, 4));
        float btnW = 76.0f, btnH = 25.0f;
        float ax = ImGui::GetContentRegionAvail().x;
        if (g_speedActive) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ax - btnW * 2 - 7.0f);
            if (N9DangerButton("Disable##spd", ImVec2(btnW, btnH))) ApplySpeedValue(1.0f);
            ImGui::SameLine(0, 7);
            if (N9AccentButton("Apply##spd", ImVec2(btnW, btnH))) ApplySpeedValue((float)g_uiSpeedVal);
        } else {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ax - btnW);
            if (N9AccentButton("Apply##spd", ImVec2(btnW, btnH)))
                ApplySpeedValue((float)(g_uiSpeedVal > 1 ? g_uiSpeedVal : 3));
        }
        N9EndCard();
    }

    {
        char badge[48];
        if (g_goldApplied) sprintf_s(badge, "ON  x%d", g_goldAmount);
        else               sprintf_s(badge, "OFF");

        N9BeginCard("##card_gold");
        N9CardHeader("Gold Multiplier", "gold written to your vault is multiplied", badge, g_goldApplied);

        float avail  = ImGui::GetContentRegionAvail().x;
        float inputW = 64.0f;
        N9Slider("##gld_sl", &g_uiGoldVal, 1, 5000, avail - inputW - 12.0f);
        ImGui::SameLine(0, 12);
        N9ValueInput("##gld_in", &g_uiGoldVal, 1, 5000, inputW);

        ImGui::Dummy(ImVec2(0, 4));
        float btnW = 76.0f, btnH = 25.0f;
        float ax = ImGui::GetContentRegionAvail().x;
        if (g_goldApplied) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ax - btnW * 2 - 7.0f);
            if (N9DangerButton("Disable##gld", ImVec2(btnW, btnH))) ApplyGoldValue(1);
            ImGui::SameLine(0, 7);
            if (N9AccentButton("Apply##gld", ImVec2(btnW, btnH))) ApplyGoldValue(g_uiGoldVal);
        } else {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ax - btnW);
            if (N9AccentButton("Apply##gld", ImVec2(btnW, btnH)))
                ApplyGoldValue(g_uiGoldVal > 1 ? g_uiGoldVal : 3);
        }
        N9EndCard();
    }

    {
        char badge[48];
        if (g_heroXpApplied) sprintf_s(badge, "ON  x%d", g_heroXpMult);
        else                 sprintf_s(badge, "OFF");

        N9BeginCard("##card_heroxp");
        N9CardHeader("Character XP Multiplier", "XP gained from monsters is multiplied", badge, g_heroXpApplied);

        float avail  = ImGui::GetContentRegionAvail().x;
        float inputW = 64.0f;
        N9Slider("##hxp_sl", &g_uiHeroXpVal, 1, 1000, avail - inputW - 12.0f);
        ImGui::SameLine(0, 12);
        N9ValueInput("##hxp_in", &g_uiHeroXpVal, 1, 1000, inputW);

        ImGui::Dummy(ImVec2(0, 4));
        float btnW = 76.0f, btnH = 25.0f;
        float ax = ImGui::GetContentRegionAvail().x;
        if (g_heroXpApplied) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ax - btnW * 2 - 7.0f);
            if (N9DangerButton("Disable##hxp", ImVec2(btnW, btnH))) ApplyHeroXp(1);
            ImGui::SameLine(0, 7);
            if (N9AccentButton("Apply##hxp", ImVec2(btnW, btnH))) ApplyHeroXp(g_uiHeroXpVal);
        } else {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ax - btnW);
            if (N9AccentButton("Apply##hxp", ImVec2(btnW, btnH)))
                ApplyHeroXp(g_uiHeroXpVal > 1 ? g_uiHeroXpVal : 5);
        }
        N9EndCard();
    }

    {
        char badge[48];
        if (g_cubeXpApplied) sprintf_s(badge, "ON  x%d", g_cubeXpMult);
        else                 sprintf_s(badge, "OFF");

        N9BeginCard("##card_cubexp");
        N9CardHeader("Cube XP Multiplier",
            "scales XP inside iml() before kmt() level-diff penalty (auto-applies on attach)",
            badge, g_cubeXpApplied);

        float avail  = ImGui::GetContentRegionAvail().x;
        float inputW = 64.0f;
        N9Slider("##cxp_sl", &g_uiCubeXpVal, 1, 100000, avail - inputW - 12.0f);
        ImGui::SameLine(0, 12);
        N9ValueInput("##cxp_in", &g_uiCubeXpVal, 1, 100000, inputW);

        ImGui::Dummy(ImVec2(0, 4));
        float btnW = 76.0f, btnH = 25.0f;
        float ax = ImGui::GetContentRegionAvail().x;
        if (g_cubeXpApplied) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ax - btnW * 2 - 7.0f);
            if (N9DangerButton("Disable##cxp", ImVec2(btnW, btnH))) ApplyCubeXp(1);
            ImGui::SameLine(0, 7);
            if (N9AccentButton("Apply##cxp", ImVec2(btnW, btnH))) ApplyCubeXp(g_uiCubeXpVal);
        } else {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ax - btnW);
            if (!g_attached) ImGui::BeginDisabled();
            if (N9AccentButton("Apply##cxp", ImVec2(btnW, btnH)))
                ApplyCubeXp(g_uiCubeXpVal > 1 ? g_uiCubeXpVal : 100);
            if (!g_attached) ImGui::EndDisabled();
        }

        if (g_cubeXpApplied) {
            ImGui::Dummy(ImVec2(0, 4));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f,0.78f,0.42f,1));
            ImGui::Text("Active: %dx  |  level-skip + kmt bypass installed", g_cubeXpMult);
            ImGui::PopStyleColor();

            DWORD hits = 0;
            if (g_cubeXpPage) RPM(g_cubeXpPage + 4, &hits, 4);
            ImGui::PushStyleColor(ImGuiCol_Text,
                hits > 0 ? ImVec4(0.45f,0.85f,0.55f,1) : ImVec4(0.75f,0.65f,0.35f,1));
            if (hits > 0)
                ImGui::Text("Hook fired %lu times since attach (multiplier is live)", hits);
            else
                ImGui::TextWrapped("Hook fired 0 times. Trigger a synthesis/alchemy and watch this go up. If it stays 0, the game uses a different XP path on this build.");
            ImGui::PopStyleColor();
        }

        if (g_cubeXpErr[0]) {
            ImGui::Dummy(ImVec2(0, 4));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.82f,0.25f,0.22f,1));
            ImGui::TextWrapped("Hook note: %s", g_cubeXpErr);
            ImGui::PopStyleColor();
        }
        N9EndCard();
    }

    {
        N9BeginCard("##card_risk");
        N9CardHeader("Risk Levels", "a guide for choosing your multiplier");
        auto rlevel = [](ImU32 col, const char* tag, const char* desc) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 p = ImGui::GetCursorScreenPos();
            float rh = ImGui::GetTextLineHeight() + 4.0f;
            dl->AddCircleFilled(ImVec2(p.x + 5, p.y + rh * 0.5f), 3.5f, col, 12);
            ImGui::Dummy(ImVec2(14, rh));
            ImGui::SameLine(0, 0);
            ImGui::SetCursorScreenPos(ImVec2(p.x + 14, p.y));
            char buf[128]; sprintf_s(buf, "%-8s  %s", tag, desc);
            ImGui::TextUnformatted(buf);
        };
        rlevel(IM_COL32(64,180,96,255),  "1-3 x",  "LEGIT     undetectable, ideal for long farming");
        rlevel(IM_COL32(64,180,96,255),  "4-6 x",  "SAFE      a bit faster, still natural");
        rlevel(IM_COL32( 56,132,210,255),"7-10 x", "MEDIUM    fast, but be careful");
        rlevel(IM_COL32(214,84, 70,255), "> 10 x", "RISKY     short sessions only");
        N9EndCard();
    }
}

static void DrawTabHero()
{
    bool god_on = (CountActive(g_god, GOD_COUNT) == (int)GOD_COUNT);

    if (!g_attached) {
        ImGui::PushStyleColor(ImGuiCol_Text,
            ImVec4(COL_INACTIVE[0], COL_INACTIVE[1], COL_INACTIVE[2], 1.0f));
        ImGui::TextWrapped("Not connected to the game. Try connecting from the Main tab.");
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0, 6));
    }

    N9BeginCard("##card_god");
    N9CardHeader("God Mode", nullptr, god_on ? "ON" : "OFF", god_on);
    if (N9Checkbox("Hero damage taken = 0", &god_on, g_attached)) DoGod();
    N9EndCard();

    {
        N9BeginCard("##card_oneshot");
        N9CardHeader("One-Shot Monsters",
            "monster damage taken is multiplied by 1e9 -- everything dies in one hit",
            g_oneShotEnabled ? "ON" : "OFF",
            g_oneShotEnabled);

        bool toggle = g_oneShotEnabled;
        if (N9Checkbox("Instakill all monsters", &toggle, g_attached)) {
            if (SetOneShotEnabled(toggle)) {
                SetStatus(toggle ? "One-shot enabled. Monsters die on hit."
                                 : "One-shot disabled.",
                          toggle ? ST_ACTIVE : ST_INFO);
            } else {
                SetStatus(g_oneShotErr[0] ? g_oneShotErr : "One-shot hook failed.",
                          ST_WARN);
            }
        }

        if (g_oneShotErr[0]) {
            ImGui::Dummy(ImVec2(0, 4));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.82f,0.25f,0.22f,1));
            ImGui::TextWrapped("Hook note: %s", g_oneShotErr);
            ImGui::PopStyleColor();
        }
        N9EndCard();
    }
}

static void DrawTabExit()
{
    N9BeginCard("##card_exit");
    N9CardHeader("Safe Exit");
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(COL_INACTIVE[0],COL_INACTIVE[1],COL_INACTIVE[2],1));
    ImGui::TextWrapped("Before the trainer closes, all patches will be cleared and "
                       "speed/gold reverted. The game is left completely clean.");
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, 10));
    if (N9DangerButton("Safe Exit  -  Clean Up and Close", ImVec2(-1, 32))) {
        DoSafeExit();
        g_exitRequested = true;
    }
    N9EndCard();
}

static void RenderUI()
{
    ImGuiIO& io = ImGui::GetIO();
    const float W = io.DisplaySize.x;
    const float H = io.DisplaySize.y;
    ImVec2 mp = io.MousePos;

    BlendThemeColours(io.DeltaTime);

    ImDrawList* bg = ImGui::GetBackgroundDrawList();

    bg->AddRectFilled(ImVec2(0, 0), ImVec2(W, H), CLR_LAYOUT, 12.0f);

    bg->AddRectFilled(ImVec2(0, 0), ImVec2(W, TITLEBAR_H),
        CLR_TITLEBAR, 12.0f, ImDrawFlags_RoundCornersTop);

    g_cardPhaseIdx = 0;

    bg->AddRect(ImVec2(0, 0), ImVec2(W, H), CLR_BORDER, 12.0f, 0, 1.0f);

    const float PAD = 18.0f;
    ImFont* fnt = ImGui::GetFont();

    const float TAB_PADX = 20.0f;
    const float TAB_H    = 32.0f;
    float th = ImGui::GetTextLineHeight();

    float tabW[5], tabX[5], tabTW[5];
    float totalTabW = 0;
    for (int i = 0; i < 5; i++) {
        tabTW[i] = ImGui::CalcTextSize(TAB_LABELS[i]).x;
        tabW[i]  = tabTW[i] + TAB_PADX * 2.0f;
        totalTabW += tabW[i];
    }

    float barX = (W - totalTabW) * 0.5f;
    float barY = (TITLEBAR_H - TAB_H) * 0.5f;
    float rad  = TAB_H * 0.5f;

    bg->AddRectFilled(ImVec2(barX - 5.0f, barY), ImVec2(barX + totalTabW + 5.0f, barY + TAB_H),
        CLR_DARK_WIDGET, rad);
    bg->AddRect(ImVec2(barX - 5.0f, barY), ImVec2(barX + totalTabW + 5.0f, barY + TAB_H),
        CLR_BORDER, rad, 0, 1.0f);

    DrawRingStreak(bg, barX - 5.0f, barY, barX + totalTabW + 5.0f, barY + TAB_H, rad, 4.2f,
                   0.24f, 70.0f, 14.0f, 1.1f, 2.6f, CLR_INACTIVE);

    float cx = barX;
    for (int i = 0; i < 5; i++) { tabX[i] = cx; cx += tabW[i]; }

    for (int i = 0; i < 5; i++) {
        bool hov = (mp.x >= tabX[i] && mp.x <= tabX[i] + tabW[i] &&
                    mp.y >= barY && mp.y <= barY + TAB_H);
        ImGuiID cid = ImGui::GetID(TAB_LABELS[i]) ^ 0x9E3779B9u;
        bool isActive = (g_currentTab == i);

        float selT = UIAnim(cid, isActive ? 1.0f : (hov ? 0.5f : 0.0f), 12.0f);
        ImU32 col = LerpU32(CLR_INACTIVE, CLR_WHITE, selT);

        float rtw = fnt->CalcTextSizeA(13.0f, FLT_MAX, 0.0f, TAB_LABELS[i]).x;
        float txX = tabX[i] + (tabW[i] - rtw) * 0.5f;
        float txY = barY + (TAB_H - th) * 0.5f;
        bg->AddText(fnt, 13.0f, ImVec2(txX, txY), col, TAB_LABELS[i]);

        float ulT = UIAnim(cid ^ 0x55u, isActive ? 1.0f : (hov ? 0.7f : 0.0f), 13.0f);
        if (ulT > 0.01f) {
            float ucx   = txX + rtw * 0.5f;
            float halfW = (rtw * 0.34f) * ulT;
            float uy    = txY + th + 4.5f;

            int glowA = (int)(30.0f * ulT);
            bg->AddRectFilledMultiColor(
                ImVec2(ucx - halfW, uy - 2.5f), ImVec2(ucx, uy + 2.5f),
                IM_COL32(40,46,58,0), IM_COL32(40,46,58,glowA),
                IM_COL32(40,46,58,glowA), IM_COL32(40,46,58,0));
            bg->AddRectFilledMultiColor(
                ImVec2(ucx, uy - 2.5f), ImVec2(ucx + halfW, uy + 2.5f),
                IM_COL32(40,46,58,glowA), IM_COL32(40,46,58,0),
                IM_COL32(40,46,58,0), IM_COL32(40,46,58,glowA));

            int ca = (int)(225.0f * ulT);
            bg->AddRectFilledMultiColor(
                ImVec2(ucx - halfW, uy - 0.5f), ImVec2(ucx, uy + 0.5f),
                IM_COL32(30,34,42,0), IM_COL32(30,34,42,ca),
                IM_COL32(30,34,42,ca), IM_COL32(30,34,42,0));
            bg->AddRectFilledMultiColor(
                ImVec2(ucx, uy - 0.5f), ImVec2(ucx + halfW, uy + 0.5f),
                IM_COL32(30,34,42,ca), IM_COL32(30,34,42,0),
                IM_COL32(30,34,42,0), IM_COL32(30,34,42,ca));
        }

        if (hov && io.MouseClicked[0]) g_currentTab = i;
    }

    const float CB_S = 28.0f;

    float cbX = W - PAD - CB_S;
    float cbY = (TITLEBAR_H - CB_S) * 0.5f;
    bool xHov = (mp.x >= cbX && mp.x <= cbX + CB_S && mp.y >= cbY && mp.y <= cbY + CB_S);
    if (xHov)
        bg->AddRectFilled(ImVec2(cbX, cbY), ImVec2(cbX + CB_S, cbY + CB_S),
            IM_COL32(220, 80, 80, 32), 6.0f);
    ImU32 xc = xHov ? IM_COL32(235, 110, 110, 255) : CLR_INACTIVE;
    float xcx = cbX + CB_S * 0.5f, xcy = cbY + CB_S * 0.5f, xr = 4.5f;
    bg->AddLine(ImVec2(xcx - xr, xcy - xr), ImVec2(xcx + xr, xcy + xr), xc, 1.5f);
    bg->AddLine(ImVec2(xcx + xr, xcy - xr), ImVec2(xcx - xr, xcy + xr), xc, 1.5f);
    if (xHov && io.MouseClicked[0]) { DoSafeExit(); g_exitRequested = true; }

    float mbX = cbX - CB_S - 4.0f;
    float mbY = cbY;
    bool mHov = (mp.x >= mbX && mp.x <= mbX + CB_S && mp.y >= mbY && mp.y <= mbY + CB_S);
    if (mHov)
        bg->AddRectFilled(ImVec2(mbX, mbY), ImVec2(mbX + CB_S, mbY + CB_S),
            IM_COL32(0, 0, 0, 14), 6.0f);
    ImU32 mc = mHov ? CLR_WHITE : CLR_INACTIVE;
    float mcx = mbX + CB_S * 0.5f, mcy = mbY + CB_S * 0.5f;
    bg->AddLine(ImVec2(mcx - 5.0f, mcy + 4.0f), ImVec2(mcx + 5.0f, mcy + 4.0f), mc, 1.5f);
    if (mHov && io.MouseClicked[0] && g_hwnd) ::ShowWindow(g_hwnd, SW_MINIMIZE);

    float tbX = mbX - CB_S - 4.0f;
    float tbY = mbY;
    bool tHov = (mp.x >= tbX && mp.x <= tbX + CB_S && mp.y >= tbY && mp.y <= tbY + CB_S);
    if (tHov || g_themePanelOpen)
        bg->AddRectFilled(ImVec2(tbX, tbY), ImVec2(tbX + CB_S, tbY + CB_S),
            IM_COL32(0, 0, 0, g_themePanelOpen ? 22 : 14), 6.0f);

    {
        const ThemePreset& cur = g_themes[g_currentTheme];
        float tcx = tbX + CB_S * 0.5f;
        float tcy = tbY + CB_S * 0.5f;
        float cell = 5.0f;
        float gap  = 1.5f;
        float side = cell * 2.0f + gap;
        float x0 = tcx - side * 0.5f;
        float y0 = tcy - side * 0.5f;
        float r  = 1.6f;

        ImU32 sw[4] = { cur.accent, cur.swatchB, cur.swatchA, cur.textPrimary };
        for (int i = 0; i < 4; i++) {
            int cx = i % 2, cy = i / 2;
            ImVec2 p0(x0 + cx * (cell + gap),       y0 + cy * (cell + gap));
            ImVec2 p1(p0.x + cell,                  p0.y + cell);
            bg->AddRectFilled(p0, p1, sw[i], r);
        }
        float ringA = (tHov || g_themePanelOpen) ? 0.85f : 0.55f;
        bg->AddRect(ImVec2(x0 - 1.0f, y0 - 1.0f),
                    ImVec2(x0 + side + 1.0f, y0 + side + 1.0f),
                    IM_COL32(0, 0, 0, (int)(ringA * (tHov ? 90 : 60))),
                    r + 1.0f, 0, 0.8f);
    }

    bool themeTogglePressed = false;
    if (tHov && io.MouseClicked[0]) {
        g_themePanelOpen = !g_themePanelOpen;
        themeTogglePressed = true;
    }

    const float FOOTER_H = 38.0f;
    {
        float fx0 = 10.0f, fx1 = W - 10.0f;
        float fy0 = H - FOOTER_H - 8.0f, fy1 = H - 8.0f;
        float rnd = 8.0f;

        bg->AddRectFilled(ImVec2(fx0, fy0), ImVec2(fx1, fy1), CLR_TITLEBAR, rnd);
        ImU32 fSheen     = CLR_WIDGET;
        ImU32 fSheenZero = (fSheen & 0x00FFFFFFu);
        bg->AddRectFilledMultiColor(
            ImVec2(fx0 + rnd, fy0 + 1.0f), ImVec2(fx1 - rnd, fy0 + FOOTER_H * 0.45f),
            fSheen, fSheen, fSheenZero, fSheenZero);
        bg->AddRect(ImVec2(fx0, fy0), ImVec2(fx1, fy1), CLR_BORDER, rnd, 0, 1.0f);

        float midY = (fy0 + fy1) * 0.5f;

        ImVec4 stCol = StatusVec(g_statusKind);
        ImU32  stU32 = ImGui::ColorConvertFloat4ToU32(stCol);
        bg->AddCircleFilled(ImVec2(fx0 + 16.0f, midY), 3.5f, stU32, 12);
        ImVec2 msgSz = fnt->CalcTextSizeA(13.0f, FLT_MAX, 0, g_statusMsg);
        bg->AddText(fnt, 13.0f, ImVec2(fx0 + 28.0f, midY - msgSz.y * 0.5f), stU32, g_statusMsg);

        const char* tag = "4kn1";
        ImVec2 tagSz = fnt->CalcTextSizeA(13.0f, FLT_MAX, 0, tag);
        float cursorR = fx1 - 16.0f;

        if (g_attached) {
            char pidBuf[48];
            sprintf_s(pidBuf, "PID %lu", g_pid);
            ImVec2 pidSz = fnt->CalcTextSizeA(13.0f, FLT_MAX, 0, pidBuf);
            float pidX = cursorR - pidSz.x;
            bg->AddText(fnt, 13.0f, ImVec2(pidX, midY - pidSz.y * 0.5f),
                IM_COL32(46, 150, 88, 255), pidBuf);
            float sepX = pidX - 10.0f;
            bg->AddLine(ImVec2(sepX, midY - 6.0f), ImVec2(sepX, midY + 6.0f), CLR_BORDER, 1.0f);
            cursorR = sepX - 10.0f;
        }

        float tagX = cursorR - tagSz.x;
        bool tagHov = (mp.x >= tagX - 3 && mp.x <= tagX + tagSz.x + 3 &&
                       mp.y >= midY - 8 && mp.y <= midY + 8);
        ImU32 tagC = tagHov ? CLR_WHITE : CLR_INACTIVE;
        bg->AddText(fnt, 13.0f, ImVec2(tagX, midY - tagSz.y * 0.5f), tagC, tag);
        if (tagHov && io.MouseClicked[0]) ImGui::SetClipboardText("4kn1");
    }

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::Begin("##trainer", nullptr,
        ImGuiWindowFlags_NoDecoration   | ImGuiWindowFlags_NoMove     |
        ImGuiWindowFlags_NoResize       | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    const float BODY_PAD_X = 20.0f;
    const float BODY_PAD_Y = 10.0f;
    const float THEME_PANEL_W = 232.0f;
    g_themePanelAnim = UIAnim(0x7E1A5u, g_themePanelOpen ? 1.0f : 0.0f, 18.0f);
    if (!g_themePanelOpen && g_themePanelAnim < 0.02f) g_themePanelAnim = 0.0f;
    float themePanelDelta = THEME_PANEL_W * g_themePanelAnim;

    float bodyX = BODY_PAD_X;
    float bodyY = TITLEBAR_H + BODY_PAD_Y;
    float bodyW = W - BODY_PAD_X * 2.0f - themePanelDelta;
    float bodyH = H - TITLEBAR_H - BODY_PAD_Y - (FOOTER_H + 18.0f);

    ImGui::SetCursorScreenPos(ImVec2(bodyX, bodyY));
    ImGui::BeginChild("##body", ImVec2(bodyW, bodyH), 0, ImGuiWindowFlags_NoBackground);

    switch (g_currentTab) {
        case 0: DrawTabMain();   break;
        case 1: DrawTabBypass(); break;
        case 2: DrawTabFarm();   break;
        case 3: DrawTabHero();   break;
        case 4: DrawTabExit();   break;
    }

    ImGui::EndChild();

    bool panelClickConsumed = false;
    if (g_themePanelAnim > 0.05f) {
        float pw = THEME_PANEL_W;
        float px1 = W - 10.0f;
        float eased = g_themePanelAnim;
        eased = 1.0f - (1.0f - eased) * (1.0f - eased);
        float px0 = px1 - pw * eased;
        float py0 = TITLEBAR_H + BODY_PAD_Y - 4.0f;
        float py1 = H - (FOOTER_H + 18.0f);
        float fade = (g_themePanelAnim - 0.05f) / 0.95f;
        if (fade < 0.0f) fade = 0.0f;
        if (fade > 1.0f) fade = 1.0f;

        ImDrawList* dl = ImGui::GetForegroundDrawList();
        auto withA = [&](ImU32 c) -> ImU32 {
            int a = (int)(((c >> IM_COL32_A_SHIFT) & 0xFF) * fade);
            if (a < 0) a = 0; if (a > 255) a = 255;
            return (c & 0x00FFFFFFu) | ((ImU32)a << IM_COL32_A_SHIFT);
        };

        for (int i = 0; i < 6; i++) {
            float ox = (float)i;
            int   al = (int)(20.0f * (1.0f - i / 6.0f) * fade);
            dl->AddRectFilled(ImVec2(px0 - ox, py0), ImVec2(px0 - ox + 1.0f, py1),
                              IM_COL32(0, 0, 0, al));
        }
        dl->AddRectFilled(ImVec2(px0, py0), ImVec2(px1, py1), withA(CLR_TITLEBAR), 10.0f);
        dl->AddRect      (ImVec2(px0, py0), ImVec2(px1, py1), withA(CLR_BORDER), 10.0f, 0, 1.0f);

        ImFont* pfnt = ImGui::GetFont();
        const char* htxt = "Theme";
        ImVec2 hsz = pfnt->CalcTextSizeA(15.0f, FLT_MAX, 0, htxt);
        (void)hsz;
        dl->AddText(pfnt, 15.0f, ImVec2(px0 + 16.0f, py0 + 18.0f), withA(CLR_WHITE), htxt);

        dl->AddLine(ImVec2(px0 + 12.0f, py0 + 48.0f),
                    ImVec2(px1 - 12.0f, py0 + 48.0f), withA(CLR_LINE), 1.0f);

        float cardX0 = px0 + 12.0f;
        float cardX1 = px1 - 12.0f;
        float cardH  = 46.0f;
        float gap    = 8.0f;
        float yy     = py0 + 64.0f;

        int hoverIdxThisFrame = -1;
        for (int i = 0; i < g_themeCount; i++) {
            ImVec2 c0(cardX0, yy);
            ImVec2 c1(cardX1, yy + cardH);
            bool hov   = (mp.x >= c0.x && mp.x <= c1.x && mp.y >= c0.y && mp.y <= c1.y);
            bool isCur = (i == g_currentTheme);
            if (hov) hoverIdxThisFrame = i;

            ImGuiID cid = ImGui::GetID(g_themes[i].name) ^ 0xC0FFEEu;
            float t = UIAnim(cid, hov ? 1.0f : 0.0f, 14.0f);

            ImU32 baseBg = isCur ? g_themes[i].select : g_themes[i].widget;
            ImU32 hovBg  = g_themes[i].select;
            ImU32 bgc    = LerpU32(baseBg, hovBg, t);
            dl->AddRectFilled(c0, c1, withA(bgc), 8.0f);

            if (isCur) {
                dl->AddRectFilled(ImVec2(c0.x, c0.y + 8.0f),
                                  ImVec2(c0.x + 3.0f, c1.y - 8.0f),
                                  withA(g_themes[i].accent), 1.5f);
            }
            dl->AddRect(c0, c1,
                        withA(isCur ? g_themes[i].accent : g_themes[i].border),
                        8.0f, 0, isCur ? 1.4f : 1.0f);

            float th = pfnt->CalcTextSizeA(13.5f, FLT_MAX, 0, g_themes[i].name).y;
            float ty = (c0.y + c1.y) * 0.5f - th * 0.5f;
            dl->AddText(pfnt, 13.5f, ImVec2(c0.x + 14.0f, ty),
                        withA(g_themes[i].textPrimary), g_themes[i].name);

            if (isCur) {
                float dotR  = 3.0f;
                float dotCX = c1.x - 14.0f;
                float dotCY = (c0.y + c1.y) * 0.5f;
                dl->AddCircleFilled(ImVec2(dotCX, dotCY), dotR, withA(g_themes[i].accent), 16);
            }

            if (hov && io.MouseClicked[0]) {
                if (!isCur) ApplyThemePreset(i);
                panelClickConsumed = true;
            }

            yy += cardH + gap;
            if (yy + cardH > py1 - 12.0f) break;
        }
        g_themeHoverIdx = hoverIdxThisFrame;

        if (g_themePanelOpen && !themeTogglePressed && !panelClickConsumed
            && io.MouseClicked[0])
        {
            bool insidePanel = (mp.x >= px0 && mp.x <= px1 && mp.y >= py0 && mp.y <= py1);
            bool onToggle    = (mp.x >= tbX && mp.x <= tbX + CB_S
                                && mp.y >= tbY && mp.y <= tbY + CB_S);
            if (!insidePanel && !onToggle) g_themePanelOpen = false;
        }
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

static bool AttachToGame()
{
    if (!ka::Guard()) { SetStatus("Authorization required.", ST_WARN); return false; }
    for (int i = 0; i < 6 && !g_pid; i++) {
        g_pid = FindProcess(L"TaskBarHero.exe");
        if (!g_pid) Sleep(500);
    }
    if (!g_pid) {
        SetStatus("Game not found. Open the game first, then restart the trainer.", ST_WARN);
        return false;
    }
    g_hProcess = OpenProcess(
        PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION |
        PROCESS_QUERY_INFORMATION | PROCESS_CREATE_THREAD,
        FALSE, g_pid);
    if (!g_hProcess) { SetStatus("Access denied. Run as administrator.", ST_WARN); return false; }
    g_gaBase = GetModBase(g_pid, L"GameAssembly.dll");
    if (!g_gaBase) { SetStatus("GameAssembly.dll not found.", ST_WARN); return false; }
    g_gaBase = g_gaBase + (intptr_t)g_baseDelta;

    BackupSaveFile();
    ApplyGroup(g_actk, ACTK_COUNT, "ACTk");
    Sleep(80);
    StartWatchdog();
    Sleep(40);
    bool hookOk = InstallGoldCaptureHook();
    if (InstallCubeXpHook())
        ApplyCubeXp(g_uiCubeXpVal > 1 ? g_uiCubeXpVal : 100);
    InstallOneShotHook();
    if (hookOk) SetStatus("Stealth + Watchdog + Gold hook ready. Safe.", ST_ACTIVE);
    else        SetStatus("Stealth + Watchdog active. Gold hook could not be installed.", ST_INFO);
    return true;
}

static bool g_authPhase = true;
static char g_loginKey[80] = "";
static char g_loginStatus[160] = "";
static bool g_loginStatusErr = false;
static bool g_loginBusy = false;
static bool g_loginRemember = true;
static std::atomic<int> g_loginResult{0};

static void RunLoginAttempt(const std::string& key, bool remember)
{
    g_loginBusy = true;
    snprintf(g_loginStatus, sizeof(g_loginStatus), "Connecting to server...");
    g_loginStatusErr = false;

    std::thread([key, remember]() {
        if (!ka::Init()) {
            snprintf(g_loginStatus, sizeof(g_loginStatus), "Init failed: %s", ka::LastError().c_str());
            g_loginStatusErr = true;
            g_loginBusy = false;
            g_loginResult = -1;
            return;
        }
        if (!ka::License(key)) {
            snprintf(g_loginStatus, sizeof(g_loginStatus), "Login rejected: %s", ka::LastError().c_str());
            g_loginStatusErr = true;
            g_loginBusy = false;
            g_loginResult = -1;
            return;
        }
        {
            std::string core;
            if (!ka::FetchVar("core", core) || core.empty()) {
                snprintf(g_loginStatus, sizeof(g_loginStatus), "Server data error: %s", ka::LastError().c_str());
                g_loginStatusErr = true;
                g_loginBusy = false;
                g_loginResult = -1;
                return;
            }
            ApplyCoreKey(core);
        }
        if (remember) ka::SaveSession(key);
        else          ka::ClearSession();
        snprintf(g_loginStatus, sizeof(g_loginStatus), "Welcome %s", ka::Username().c_str());
        g_loginStatusErr = false;
        g_loginBusy = false;
        g_loginResult = 1;
    }).detach();
}

static void DrawSoftOrb(ImDrawList* dl, ImVec2 c, float radius, ImU32 col)
{
    int rings = 14;
    int baseA = (col >> IM_COL32_A_SHIFT) & 0xFF;
    int rr = (col >> IM_COL32_R_SHIFT) & 0xFF;
    int gg = (col >> IM_COL32_G_SHIFT) & 0xFF;
    int bb = (col >> IM_COL32_B_SHIFT) & 0xFF;
    for (int i = rings; i >= 1; --i) {
        float f = (float)i / rings;
        float rad = radius * f;
        float falloff = 1.0f - f;
        int a = (int)(baseA * falloff * falloff);
        if (a < 1) continue;
        dl->AddCircleFilled(c, rad, IM_COL32(rr, gg, bb, a), 24);
    }
}

struct Orb   { float x, y, vx, vy, r; int a; float ph; };
struct Dust  { float x, y, vx, vy, r, baseA, ph; };

static void DrawLoginParticles(ImDrawList* dl, float W, float H, ImVec2 mp)
{
    (void)mp;
    static const int NO = 9;
    static const int ND = 46;
    static Orb  orbs[NO];
    static Dust dust[ND];
    static bool init = false;
    if (!init) {
        for (int i = 0; i < NO; ++i) {
            orbs[i].x = (float)(rand() % (int)(W > 1 ? W : 800));
            orbs[i].y = TITLEBAR_H + (float)(rand() % (int)(H > 80 ? (H - 80) : 400));
            float ang = (float)(rand() % 628) / 100.0f;
            float spd = 3.0f + (rand() % 100) / 100.0f * 5.0f;
            orbs[i].vx = cosf(ang) * spd;
            orbs[i].vy = sinf(ang) * spd;
            orbs[i].r  = 46.0f + (rand() % 100) / 100.0f * 64.0f;
            orbs[i].a  = 7 + rand() % 8;
            orbs[i].ph = (float)(rand() % 628) / 100.0f;
        }
        for (int i = 0; i < ND; ++i) {
            dust[i].x = (float)(rand() % (int)(W > 1 ? W : 800));
            dust[i].y = (float)(rand() % (int)(H > 1 ? H : 480));
            float ang = (float)(rand() % 628) / 100.0f;
            float spd = 2.0f + (rand() % 100) / 100.0f * 4.0f;
            dust[i].vx = cosf(ang) * spd;
            dust[i].vy = sinf(ang) * spd - 2.0f;
            dust[i].r  = 0.6f + (rand() % 100) / 100.0f * 1.3f;
            dust[i].baseA = 0.12f + (rand() % 100) / 100.0f * 0.34f;
            dust[i].ph = (float)(rand() % 628) / 100.0f;
        }
        init = true;
    }

    float dt = ImGui::GetIO().DeltaTime;
    if (dt <= 0.0f || dt > 0.1f) dt = 1.0f / 60.0f;
    float t = (float)ImGui::GetTime();
    float topY = TITLEBAR_H;

    for (int i = 0; i < NO; ++i) {
        Orb& o = orbs[i];
        o.x += o.vx * dt;
        o.y += o.vy * dt;
        float m = o.r;
        if (o.x < -m) o.x = W + m; else if (o.x > W + m) o.x = -m;
        if (o.y < topY - m) o.y = H + m; else if (o.y > H + m) o.y = topY - m;
        float tw = 0.7f + 0.3f * sinf(t * 0.6f + o.ph);
        int a = (int)(o.a * tw);
        DrawSoftOrb(dl, ImVec2(o.x, o.y), o.r, IM_COL32(150, 162, 188, a));
    }

    for (int i = 0; i < ND; ++i) {
        Dust& d = dust[i];
        float sway = sinf(t * 0.6f + d.ph) * 3.0f;
        d.x += (d.vx + sway) * dt;
        d.y += d.vy * dt;
        if (d.x < -4) d.x = W + 4; else if (d.x > W + 4) d.x = -4;
        if (d.y < topY - 4) { d.y = H + 4; d.x = (float)(rand() % (int)(W > 1 ? W : 800)); }
        else if (d.y > H + 4) d.y = topY - 4;

        float tw = 0.5f + 0.5f * sinf(t * 1.1f + d.ph);
        int a = (int)(255.0f * d.baseA * (0.5f + 0.5f * tw) * 0.5f);
        if (a < 2) continue;
        dl->AddCircleFilled(ImVec2(d.x, d.y), d.r + 1.4f, IM_COL32(120, 132, 156, a / 3), 10);
        dl->AddCircleFilled(ImVec2(d.x, d.y), d.r, IM_COL32(96, 108, 134, a), 10);
    }

    int steps = 6;
    for (int s = 0; s < steps; ++s) {
        float f = (float)(s + 1) / steps;
        float inset = f * 170.0f;
        int a = (int)(20.0f * f);
        if (a < 1) continue;
        dl->AddRect(ImVec2(inset, TITLEBAR_H + inset * 0.4f),
                    ImVec2(W - inset, H - inset * 0.4f),
                    IM_COL32(150, 160, 180, a), 16.0f + inset, 0, 16.0f);
    }
}

static void RenderLogin()
{
    ImGuiIO& io = ImGui::GetIO();
    const float W = io.DisplaySize.x;
    const float H = io.DisplaySize.y;
    ImVec2 mp = io.MousePos;
    ImDrawList* bg = ImGui::GetBackgroundDrawList();
    ImFont* fnt = ImGui::GetFont();

    bg->AddRectFilledMultiColor(ImVec2(0,0), ImVec2(W,H),
        IM_COL32(248, 249, 251, 255), IM_COL32(248, 249, 251, 255),
        IM_COL32(232, 235, 240, 255), IM_COL32(232, 235, 240, 255));

    DrawLoginParticles(bg, W, H, mp);

    bg->AddRectFilled(ImVec2(0,0), ImVec2(W,TITLEBAR_H), IM_COL32(244, 246, 249, 255), 12.0f, ImDrawFlags_RoundCornersTop);
    bg->AddLine(ImVec2(0, TITLEBAR_H), ImVec2(W, TITLEBAR_H), IM_COL32(220, 224, 230, 255), 1.0f);
    bg->AddRect(ImVec2(0,0), ImVec2(W,H), IM_COL32(208, 213, 221, 255), 12.0f, 0, 1.0f);

    const float CB_S = 28.0f;
    float cbX = W - 18.0f - CB_S;
    float cbY = (TITLEBAR_H - CB_S) * 0.5f;
    bool xHov = (mp.x >= cbX && mp.x <= cbX + CB_S && mp.y >= cbY && mp.y <= cbY + CB_S);
    if (xHov) bg->AddRectFilled(ImVec2(cbX,cbY), ImVec2(cbX+CB_S,cbY+CB_S), IM_COL32(228,86,86,40), 6.0f);
    bg->AddLine(ImVec2(cbX + 9, cbY + 9), ImVec2(cbX + CB_S - 9, cbY + CB_S - 9),
        xHov ? IM_COL32(208,60,60,255) : IM_COL32(110,116,128,255), 1.4f);
    bg->AddLine(ImVec2(cbX + CB_S - 9, cbY + 9), ImVec2(cbX + 9, cbY + CB_S - 9),
        xHov ? IM_COL32(208,60,60,255) : IM_COL32(110,116,128,255), 1.4f);
    if (xHov && io.MouseClicked[0]) g_exitRequested = true;

    float mnX = (TITLEBAR_H - CB_S) * 0.5f - 0.0f;
    float mX  = cbX - CB_S - 4.0f;
    float mY  = cbY;
    bool mHov = (mp.x >= mX && mp.x <= mX + CB_S && mp.y >= mY && mp.y <= mY + CB_S);
    (void)mnX;
    if (mHov) bg->AddRectFilled(ImVec2(mX,mY), ImVec2(mX+CB_S,mY+CB_S), IM_COL32(0,0,0,18), 6.0f);
    bg->AddLine(ImVec2(mX + 9, mY + CB_S - 10), ImVec2(mX + CB_S - 9, mY + CB_S - 10),
        mHov ? IM_COL32(60,64,74,255) : IM_COL32(110,116,128,255), 1.4f);
    if (mHov && io.MouseClicked[0]) ShowWindow(g_hwnd, SW_MINIMIZE);

    float cardW = 400.0f, cardH = 270.0f;
    float cardX = (W - cardW) * 0.5f;
    float cardY = (H - cardH) * 0.5f;
    float cardR = 14.0f;

    for (int g = 16; g >= 1; --g) {
        float e = (float)g * 2.6f;
        int a = (int)(10.0f * (1.0f - (float)(g - 1) / 16.0f));
        if (a < 1) continue;
        ImU32 gc = IM_COL32(60, 70, 95, a);
        bg->AddRect(ImVec2(cardX - e, cardY - e + 6.0f), ImVec2(cardX + cardW + e, cardY + cardH + e + 6.0f),
            gc, cardR + e, 0, 2.0f);
    }

    bg->AddRectFilled(ImVec2(cardX, cardY), ImVec2(cardX + cardW, cardY + cardH),
        IM_COL32(255, 255, 255, 255), cardR);
    bg->AddRect(ImVec2(cardX, cardY), ImVec2(cardX + cardW, cardY + cardH),
        IM_COL32(224, 228, 234, 255), cardR, 0, 1.0f);

    DrawRingStreak(bg, cardX, cardY, cardX + cardW, cardY + cardH, cardR, 5.4f,
                   0.46f, 70.0f, 12.0f, 1.2f, 3.6f);

    bg->AddText(fnt, 17.0f, ImVec2(cardX + 24.0f, cardY + 20.0f), IM_COL32(26, 30, 38, 255), "Sign In");
    bg->AddText(fnt, 12.0f, ImVec2(cardX + 24.0f, cardY + 43.0f), IM_COL32(138, 144, 156, 255),
        "Enter your license key to continue");
    bg->AddLine(ImVec2(cardX + 24.0f, cardY + 68.0f),
                ImVec2(cardX + cardW - 24.0f, cardY + 68.0f),
                IM_COL32(228, 231, 236, 255), 1.0f);

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::Begin("##login", nullptr,
        ImGuiWindowFlags_NoDecoration   | ImGuiWindowFlags_NoMove     |
        ImGuiWindowFlags_NoResize       | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    float innerW = cardW - 48.0f;
    ImGui::SetCursorScreenPos(ImVec2(cardX + 24.0f, cardY + 84.0f));
    ImGui::PushItemWidth(innerW);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14, 13));
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(245.f/255.f,247.f/255.f,250.f/255.f,1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(239.f/255.f,242.f/255.f,246.f/255.f,1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  ImVec4(255.f/255.f,255.f/255.f,255.f/255.f,1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border,         ImVec4(210.f/255.f,215.f/255.f,222.f/255.f,1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text,           ImVec4(0.12f,0.14f,0.18f,1.0f));
    ImGui::PushStyleColor(ImGuiCol_TextDisabled,   ImVec4(0.62f,0.65f,0.70f,1.0f));
    ImGui::SetNextItemWidth(innerW);
    bool entered = ImGui::InputTextWithHint("##key", "license key",
        g_loginKey, sizeof(g_loginKey),
        ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_Password);
    ImGui::PopStyleColor(6);
    ImGui::PopStyleVar(3);
    ImGui::PopItemWidth();

    ImGui::SetCursorScreenPos(ImVec2(cardX + 24.0f, cardY + 140.0f));
    ImGui::PushStyleColor(ImGuiCol_CheckMark,     ImVec4(0.13f,0.15f,0.19f,1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg,       ImVec4(244.f/255.f,246.f/255.f,249.f/255.f,1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,ImVec4(232.f/255.f,235.f/255.f,240.f/255.f,1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border,        ImVec4(210.f/255.f,215.f/255.f,222.f/255.f,1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.30f,0.33f,0.40f,1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::Checkbox("Remember me", &g_loginRemember);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(5);

    float btnY = cardY + 178.0f;
    for (int s = 4; s >= 1; --s) {
        float e = (float)s * 1.5f;
        int a = (int)(16.0f * (1.0f - (float)(s - 1) / 4.0f));
        if (a < 1) continue;
        bg->AddRectFilled(ImVec2(cardX + 24.0f - e * 0.3f, btnY + 2.0f + e),
                          ImVec2(cardX + 24.0f + innerW + e * 0.3f, btnY + 44.0f + e),
                          IM_COL32(40, 48, 66, a), 11.0f);
    }

    ImGui::SetCursorScreenPos(ImVec2(cardX + 24.0f, btnY));
    N9BtnStyle nb;
    nb.fill       = IM_COL32(24, 26, 32, 255);
    nb.fillHov    = IM_COL32(38, 41, 50, 255);
    nb.fillAct    = IM_COL32(17, 18, 23, 255);
    nb.border     = IM_COL32(0, 0, 0, 0);
    nb.borderHov  = IM_COL32(0, 0, 0, 0);
    nb.text       = IM_COL32(236, 238, 244, 255);
    nb.textHov    = IM_COL32(255, 255, 255, 255);
    nb.rounding   = 10.0f;
    nb.gradient   = true;
    nb.sheen      = true;
    nb.bloom      = false;
    nb.accentLine = false;

    bool clickLogin = false;
    if (g_loginBusy) {
        ImGui::BeginDisabled();
        N9ButtonEx("##loginbtn", "Connecting...", ImVec2(innerW, 44.0f), nb);
        ImGui::EndDisabled();
    } else {
        if (N9ButtonEx("##loginbtn", "Sign In", ImVec2(innerW, 44.0f), nb) || entered)
            clickLogin = true;
    }
    if (clickLogin && !g_loginBusy && g_loginKey[0]) {
        RunLoginAttempt(g_loginKey, g_loginRemember);
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    if (g_loginStatus[0]) {
        const float fsz = 11.5f;
        ImVec2 sz2 = fnt->CalcTextSizeA(fsz, FLT_MAX, 0.0f, g_loginStatus);
        float dotGap = 14.0f;
        float padL = 12.0f, padR = 13.0f, padY = 4.0f;
        float pillW = padL + dotGap + sz2.x + padR;
        float pillH = sz2.y + padY * 2.0f;
        float pillX = cardX + (cardW - pillW) * 0.5f;
        float pillY = cardY + cardH - pillH - 14.0f;
        float pr = pillH * 0.5f;

        ImU32 fillC, bordC, txtC, dotC;
        if (g_loginStatusErr) {
            fillC = IM_COL32(252, 242, 243, 255);
            bordC = IM_COL32(238, 206, 208, 255);
            txtC  = IM_COL32(176, 60, 66, 255);
            dotC  = IM_COL32(214, 72, 78, 255);
        } else {
            fillC = IM_COL32(242, 246, 243, 255);
            bordC = IM_COL32(206, 222, 209, 255);
            txtC  = IM_COL32(58, 116, 76, 255);
            dotC  = IM_COL32(76, 158, 96, 255);
        }

        for (int s = 3; s >= 1; --s) {
            float e = (float)s * 1.2f;
            int a = (int)(10.0f * (1.0f - (float)(s - 1) / 3.0f));
            if (a < 1) continue;
            bg->AddRectFilled(ImVec2(pillX - e * 0.2f, pillY + 1.5f + e),
                              ImVec2(pillX + pillW + e * 0.2f, pillY + pillH + e),
                              IM_COL32(60, 70, 95, a), pr + e);
        }

        bg->AddRectFilled(ImVec2(pillX, pillY), ImVec2(pillX + pillW, pillY + pillH), fillC, pr);
        bg->AddRect(ImVec2(pillX, pillY), ImVec2(pillX + pillW, pillY + pillH), bordC, pr, 0, 1.0f);

        float dotX = pillX + padL + 2.5f;
        float dotY = pillY + pillH * 0.5f;
        bg->AddCircleFilled(ImVec2(dotX, dotY), 4.5f, IM_COL32((dotC>>IM_COL32_R_SHIFT)&0xFF,
            (dotC>>IM_COL32_G_SHIFT)&0xFF, (dotC>>IM_COL32_B_SHIFT)&0xFF, 60), 12);
        bg->AddCircleFilled(ImVec2(dotX, dotY), 2.6f, dotC, 12);

        bg->AddText(fnt, fsz,
            ImVec2(pillX + padL + dotGap, pillY + (pillH - sz2.y) * 0.5f),
            txtC, g_loginStatus);
    }
}

int main(int, char**)
{
    WNDCLASSEXW wc = {
        sizeof(WNDCLASSEXW), CS_CLASSDC, WndProc, 0L, 0L,
        GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"VAMPTrainer", nullptr };
    ::RegisterClassExW(&wc);
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int ww = 810, wh = 486;
    HWND hwnd = ::CreateWindowExW(WS_EX_APPWINDOW, wc.lpszClassName, L"VAMP @ TBH",
        WS_POPUP | WS_MINIMIZEBOX, (sw - ww) / 2, (sh - wh) / 2, ww, wh,
        nullptr, nullptr, wc.hInstance, nullptr);
    g_hwnd = hwnd;

    HRGN rgn = CreateRoundRectRgn(0, 0, ww + 1, wh + 1, 24, 24);
    SetWindowRgn(hwnd, rgn, TRUE);

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.IniFilename = nullptr;
    io.ConfigDebugHighlightIdConflicts = false;
    ImGui::StyleColorsDark();
    ApplyTheme();

    ImFontConfig cfg;
    cfg.OversampleH = 4;
    cfg.OversampleV = 4;
    cfg.PixelSnapH = true;
    cfg.RasterizerMultiply = 1.15f;

    static const ImWchar trRanges[] = {
        0x0020, 0x00FF,
        0x011E, 0x011F,
        0x0130, 0x0131,
        0x015E, 0x015F,
        0,
    };
    cfg.GlyphRanges = trRanges;

    const char* fontCandidates[] = {
        "C:\\Windows\\Fonts\\bahnschrift.ttf",
        "C:\\Windows\\Fonts\\seguisb.ttf",
        "C:\\Windows\\Fonts\\trebucbd.ttf",
        "C:\\Windows\\Fonts\\segoeuib.ttf",
    };
    ImFont* mainFont = nullptr;
    for (const char* path : fontCandidates) {
        mainFont = io.Fonts->AddFontFromFileTTF(path, 16.0f, &cfg);
        if (mainFont) break;
    }
    if (!mainFont)
        mainFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\tahomabd.ttf", 16.0f, &cfg);
    if (mainFont) io.FontDefault = mainFont;

    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\seguisb.ttf", 13.0f, &cfg);
    io.Fonts->Build();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    srand((unsigned)GetTickCount());

    EnsureConfigDir();

    g_authPhase = false;

    ImVec4 clearCol = ImVec4(0.06f, 0.06f, 0.08f, 1.0f);
    bool done = false;

    while (!done && !g_exitRequested && g_authPhase) {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done || g_exitRequested) break;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        RenderLogin();

        ImGui::Render();
        const float c[4] = { clearCol.x, clearCol.y, clearCol.z, clearCol.w };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, c);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);

        int r = g_loginResult.load();
        if (r == 1) {
            g_authPhase = false;
            ka::StartHeartbeat();
        }
    }

    if (!g_authPhase && !g_exitRequested) {
        GenFakeIdentity();
        RefreshConfigList();

        SetStatus("Searching for TaskBarHero.exe...", ST_INFO);
        g_attached = AttachToGame();

        CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
            while (!g_exitRequested) {
                Sleep(3000);
                if (!g_attached && !g_exitRequested)
                    g_attached = AttachToGame();
            }
            return 0;
        }, nullptr, 0, nullptr);

        done = false;
        while (!done && !g_exitRequested) {
            MSG msg;
            while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
                ::TranslateMessage(&msg);
                ::DispatchMessage(&msg);
                if (msg.message == WM_QUIT) done = true;
            }
            if (done || g_exitRequested) break;

            if (ka::Tampered()) { DoSafeExit(); g_exitRequested = true; break; }

            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            RenderUI();

            ImGui::Render();
            const float c[4] = { clearCol.x, clearCol.y, clearCol.z, clearCol.w };
            g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
            g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, c);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            g_pSwapChain->Present(1, 0);
        }
    }

    ka::StopHeartbeat();

    if (g_hProcess) {
        DoSafeExit();
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    if (g_hProcess) CloseHandle(g_hProcess);
    return 0;
}
