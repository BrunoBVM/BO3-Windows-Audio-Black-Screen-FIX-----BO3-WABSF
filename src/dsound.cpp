// dsound.cpp - logging / filtering proxy for dsound.dll (x64)
//
// PURPOSE
//   Step 1 (default build): log every DirectSound device enumeration / creation call the game
//   makes, with timestamps and thread ids, so we can see what it does when it stalls.
//   Step 2 (-DFILTER_TO_DEFAULT=1): additionally hide every device except the default
//   ("Primary Sound Driver") from the game. Nothing in Windows is changed.
//
// BUILD (x64 only, BlackOps3.exe is 64-bit)
//   MSVC  (x64 Native Tools Command Prompt):
//       cl /LD /O2 /EHsc dsound.cpp /link /DEF:dsound.def /OUT:dsound.dll
//       (filter build: add /DFILTER_TO_DEFAULT=1 before /link)
//   MinGW-w64:
//       x86_64-w64-mingw32-g++ -shared -O2 -static -o dsound.dll dsound.cpp dsound.def
//       (filter build: add -DFILTER_TO_DEFAULT=1)
//
// USE
//   Copy dsound.dll next to BlackOps3.exe. The log is written to dsound_proxy.log in that folder.
//   If no log appears, the game did not load this DLL. To remove: delete dsound.dll.
//   Offline / solo / LAN use only. Remove it before connecting to any online server.
//
// v2 ADDS a winmm device-count clamp (see InstallClamp). It patches the game's own import slots
//   for mixerGetNumDevs / waveOutGetNumDevs / waveInGetNumDevs so they never report more than
//   N devices (N from dsound_proxy.cfg, default 16). The log line "import slots patched" shows
//   whether the patch found anything; all zeros means the game does not call them that way.
//
// Built with MinGW-w64 g++ and tested on one machine only (Windows build 26100). Use at your own risk.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <unknwn.h>
#include <stdio.h>
#include <stdarg.h>
#include <wchar.h>

#ifndef FILTER_TO_DEFAULT
#define FILTER_TO_DEFAULT 0
#endif

static CRITICAL_SECTION g_cs;
static FILE* g_log = nullptr;

static void Log(const char* fmt, ...)
{
    if (!g_log) return;
    EnterCriticalSection(&g_cs);
    SYSTEMTIME t;
    GetLocalTime(&t);
    fprintf(g_log, "[%02u:%02u:%02u.%03u tid=%lu] ",
            (unsigned)t.wHour, (unsigned)t.wMinute, (unsigned)t.wSecond,
            (unsigned)t.wMilliseconds, (unsigned long)GetCurrentThreadId());
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fputc('\n', g_log);
    fflush(g_log);
    LeaveCriticalSection(&g_cs);
}

static void GuidStr(const GUID* g, char* out, size_t n)
{
    if (!g) { snprintf(out, n, "NULL(default)"); return; }
    snprintf(out, n, "{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
             (unsigned long)g->Data1, (unsigned)g->Data2, (unsigned)g->Data3,
             (unsigned)g->Data4[0], (unsigned)g->Data4[1], (unsigned)g->Data4[2],
             (unsigned)g->Data4[3], (unsigned)g->Data4[4], (unsigned)g->Data4[5],
             (unsigned)g->Data4[6], (unsigned)g->Data4[7]);
}

// ---- real dsound.dll from System32 ----------------------------------------------------------

static HMODULE LoadReal()
{
    static HMODULE h = []() -> HMODULE {
        wchar_t path[MAX_PATH] = L"";
        GetSystemDirectoryW(path, MAX_PATH);
        lstrcatW(path, L"\\dsound.dll");
        HMODULE m = LoadLibraryW(path);
        Log("loaded real dsound: %ls -> %p", path, (void*)m);
        return m;
    }();
    return h;
}

template <class T>
static T Real(const char* name)
{
    HMODULE m = LoadReal();
    FARPROC p = m ? GetProcAddress(m, name) : nullptr;
    if (!p) Log("!! real dsound has no export %s", name);
    return reinterpret_cast<T>(p);
}

typedef HRESULT (WINAPI *PFN_Create)(LPCGUID, LPVOID*, LPUNKNOWN);
typedef BOOL (CALLBACK *DSENUMCBW)(LPGUID, LPCWSTR, LPCWSTR, LPVOID);
typedef BOOL (CALLBACK *DSENUMCBA)(LPGUID, LPCSTR, LPCSTR, LPVOID);
typedef HRESULT (WINAPI *PFN_EnumW)(DSENUMCBW, LPVOID);
typedef HRESULT (WINAPI *PFN_EnumA)(DSENUMCBA, LPVOID);

// ---- enumeration wrappers -------------------------------------------------------------------

struct EnumCtx
{
    void* cb;
    void* ctx;
    int seen;
    int passed;
    const char* api;
};

static BOOL CALLBACK ThunkW(LPGUID g, LPCWSTR desc, LPCWSTR mod, LPVOID c)
{
    EnumCtx* e = static_cast<EnumCtx*>(c);
    char gs[64];
    GuidStr(g, gs, sizeof gs);
    e->seen++;
    Log("  %s device #%d guid=%s desc=\"%ls\" module=\"%ls\"",
        e->api, e->seen, gs, desc ? desc : L"", mod ? mod : L"");
#if FILTER_TO_DEFAULT
    if (g != nullptr) { Log("    (hidden from game)"); return TRUE; }
#endif
    e->passed++;
    return reinterpret_cast<DSENUMCBW>(e->cb)(g, desc, mod, e->ctx);
}

static BOOL CALLBACK ThunkA(LPGUID g, LPCSTR desc, LPCSTR mod, LPVOID c)
{
    EnumCtx* e = static_cast<EnumCtx*>(c);
    char gs[64];
    GuidStr(g, gs, sizeof gs);
    e->seen++;
    Log("  %s device #%d guid=%s desc=\"%s\" module=\"%s\"",
        e->api, e->seen, gs, desc ? desc : "", mod ? mod : "");
#if FILTER_TO_DEFAULT
    if (g != nullptr) { Log("    (hidden from game)"); return TRUE; }
#endif
    e->passed++;
    return reinterpret_cast<DSENUMCBA>(e->cb)(g, desc, mod, e->ctx);
}

static HRESULT EnumWCommon(const char* name, DSENUMCBW cb, LPVOID ctx)
{
    PFN_EnumW fn = Real<PFN_EnumW>(name);
    EnumCtx e = { reinterpret_cast<void*>(cb), ctx, 0, 0, name };
    Log("%s called", name);
    HRESULT hr = fn ? fn(ThunkW, &e) : E_FAIL;
    Log("%s returned hr=0x%08lX (%d devices seen, %d passed to game)",
        name, (unsigned long)hr, e.seen, e.passed);
    return hr;
}

static HRESULT EnumACommon(const char* name, DSENUMCBA cb, LPVOID ctx)
{
    PFN_EnumA fn = Real<PFN_EnumA>(name);
    EnumCtx e = { reinterpret_cast<void*>(cb), ctx, 0, 0, name };
    Log("%s called", name);
    HRESULT hr = fn ? fn(ThunkA, &e) : E_FAIL;
    Log("%s returned hr=0x%08lX (%d devices seen, %d passed to game)",
        name, (unsigned long)hr, e.seen, e.passed);
    return hr;
}

// ---- creation wrapper -----------------------------------------------------------------------

static HRESULT CreateCommon(const char* name, LPCGUID g, LPVOID* out, LPUNKNOWN outer)
{
    char gs[64];
    GuidStr(g, gs, sizeof gs);
    PFN_Create fn = Real<PFN_Create>(name);
    Log("%s(guid=%s) called", name, gs);
    HRESULT hr = fn ? fn(g, out, outer) : E_FAIL;
    Log("%s(guid=%s) returned hr=0x%08lX", name, gs, (unsigned long)hr);
    return hr;
}

// ---- exports (ordinals set in dsound.def) ---------------------------------------------------

extern "C" {

HRESULT WINAPI DirectSoundCreate(LPCGUID g, LPVOID* out, LPUNKNOWN outer)
{ return CreateCommon("DirectSoundCreate", g, out, outer); }

HRESULT WINAPI DirectSoundCreate8(LPCGUID g, LPVOID* out, LPUNKNOWN outer)
{ return CreateCommon("DirectSoundCreate8", g, out, outer); }

HRESULT WINAPI DirectSoundCaptureCreate(LPCGUID g, LPVOID* out, LPUNKNOWN outer)
{ return CreateCommon("DirectSoundCaptureCreate", g, out, outer); }

HRESULT WINAPI DirectSoundCaptureCreate8(LPCGUID g, LPVOID* out, LPUNKNOWN outer)
{ return CreateCommon("DirectSoundCaptureCreate8", g, out, outer); }

HRESULT WINAPI DirectSoundEnumerateW(DSENUMCBW cb, LPVOID ctx)
{ return EnumWCommon("DirectSoundEnumerateW", cb, ctx); }

HRESULT WINAPI DirectSoundEnumerateA(DSENUMCBA cb, LPVOID ctx)
{ return EnumACommon("DirectSoundEnumerateA", cb, ctx); }

HRESULT WINAPI DirectSoundCaptureEnumerateW(DSENUMCBW cb, LPVOID ctx)
{ return EnumWCommon("DirectSoundCaptureEnumerateW", cb, ctx); }

HRESULT WINAPI DirectSoundCaptureEnumerateA(DSENUMCBA cb, LPVOID ctx)
{ return EnumACommon("DirectSoundCaptureEnumerateA", cb, ctx); }

HRESULT WINAPI GetDeviceID(LPCGUID src, LPGUID dst)
{
    typedef HRESULT (WINAPI *F)(LPCGUID, LPGUID);
    F fn = Real<F>("GetDeviceID");
    return fn ? fn(src, dst) : E_FAIL;
}

HRESULT WINAPI DirectSoundFullDuplexCreate(LPCGUID a, LPCGUID b, LPCVOID c, LPCVOID d,
                                           HWND hwnd, DWORD level, LPVOID* e, LPVOID* f,
                                           LPVOID* g, LPUNKNOWN outer)
{
    typedef HRESULT (WINAPI *F)(LPCGUID, LPCGUID, LPCVOID, LPCVOID, HWND, DWORD,
                                LPVOID*, LPVOID*, LPVOID*, LPUNKNOWN);
    F fn = Real<F>("DirectSoundFullDuplexCreate");
    Log("DirectSoundFullDuplexCreate called");
    return fn ? fn(a, b, c, d, hwnd, level, e, f, g, outer) : E_FAIL;
}

HRESULT WINAPI DllCanUnloadNow(void)
{
    typedef HRESULT (WINAPI *F)(void);
    F fn = Real<F>("DllCanUnloadNow");
    return fn ? fn() : S_FALSE;
}

HRESULT WINAPI DllGetClassObject(const GUID& clsid, const GUID& iid, LPVOID* out)
{
    typedef HRESULT (WINAPI *F)(const GUID&, const GUID&, LPVOID*);
    F fn = Real<F>("DllGetClassObject");
    return fn ? fn(clsid, iid, out) : E_FAIL;
}

} // extern "C"

// ---- winmm device-count clamp ---------------------------------------------------------------
// Test for the "too many audio endpoints" theory. Caps the counts the game gets from
// mixerGetNumDevs / waveOutGetNumDevs / waveInGetNumDevs by patching the game's import slots.
// Windows audio settings are not touched. The cap is read from dsound_proxy.cfg (a single
// number, default 16, 0 = report no devices). Delete the .cfg to use the default.

static UINT g_cap = 16;
typedef UINT (WINAPI *PFN_Count)(void);
static PFN_Count g_realMixer = nullptr, g_realWaveOut = nullptr, g_realWaveIn = nullptr;

static UINT ClampCount(const char* name, PFN_Count real)
{
    UINT n = real ? real() : 0;
    UINT r = n > g_cap ? g_cap : n;
    Log("%s real=%u -> returned %u", name, n, r);
    return r;
}
static UINT WINAPI HookMixer()   { return ClampCount("mixerGetNumDevs",   g_realMixer); }
static UINT WINAPI HookWaveOut() { return ClampCount("waveOutGetNumDevs", g_realWaveOut); }
static UINT WINAPI HookWaveIn()  { return ClampCount("waveInGetNumDevs",  g_realWaveIn); }

// Replace every import slot in 'mod' that currently points at 'original'. Returns slots patched.
static int PatchImport(HMODULE mod, FARPROC original, void* replacement)
{
    BYTE* base = reinterpret_cast<BYTE*>(mod);
    IMAGE_DOS_HEADER* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    IMAGE_NT_HEADERS* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;
    BYTE* end = base + nt->OptionalHeader.SizeOfImage;
    IMAGE_DATA_DIRECTORY dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir.VirtualAddress || base + dir.VirtualAddress >= end) return 0;

    int patched = 0;
    IMAGE_IMPORT_DESCRIPTOR* imp = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress);
    for (; reinterpret_cast<BYTE*>(imp + 1) <= end && imp->Name; ++imp)
    {
        if (!imp->FirstThunk || base + imp->FirstThunk >= end) continue;
        ULONG_PTR* thunk = reinterpret_cast<ULONG_PTR*>(base + imp->FirstThunk);
        for (; reinterpret_cast<BYTE*>(thunk + 1) <= end && *thunk; ++thunk)
        {
            if (*thunk != reinterpret_cast<ULONG_PTR>(original)) continue;
            DWORD old = 0;
            if (VirtualProtect(thunk, sizeof(*thunk), PAGE_READWRITE, &old))
            {
                *thunk = reinterpret_cast<ULONG_PTR>(replacement);
                VirtualProtect(thunk, sizeof(*thunk), old, &old);
                ++patched;
            }
        }
    }
    return patched;
}

static void InstallClamp(const wchar_t* dir)
{
    wchar_t p[MAX_PATH];
    lstrcpyW(p, dir);
    lstrcatW(p, L"dsound_proxy.cfg");
    if (FILE* f = _wfopen(p, L"r"))
    {
        int v = 16;
        if (fscanf(f, "%d", &v) == 1 && v >= 0) g_cap = (UINT)v;
        fclose(f);
    }

    HMODULE winmm = GetModuleHandleW(L"winmm.dll");
    if (!winmm) { Log("winmm.dll not loaded at attach time; clamp NOT installed"); return; }

    g_realMixer   = (PFN_Count)GetProcAddress(winmm, "mixerGetNumDevs");
    g_realWaveOut = (PFN_Count)GetProcAddress(winmm, "waveOutGetNumDevs");
    g_realWaveIn  = (PFN_Count)GetProcAddress(winmm, "waveInGetNumDevs");

    HMODULE exe = GetModuleHandleW(nullptr);
    int a = g_realMixer   ? PatchImport(exe, (FARPROC)g_realMixer,   (void*)HookMixer)   : 0;
    int b = g_realWaveOut ? PatchImport(exe, (FARPROC)g_realWaveOut, (void*)HookWaveOut) : 0;
    int c = g_realWaveIn  ? PatchImport(exe, (FARPROC)g_realWaveIn,  (void*)HookWaveIn)  : 0;
    Log("clamp cap=%u; import slots patched: mixerGetNumDevs=%d waveOutGetNumDevs=%d waveInGetNumDevs=%d",
        g_cap, a, b, c);
}

// ---- DllMain --------------------------------------------------------------------------------

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(inst);
        InitializeCriticalSection(&g_cs);

        wchar_t path[MAX_PATH] = L"";
        GetModuleFileNameW(inst, path, MAX_PATH);
        wchar_t* slash = wcsrchr(path, L'\\');
        if (slash) *(slash + 1) = 0;
        wchar_t dir[MAX_PATH];
        lstrcpyW(dir, path);
        lstrcatW(path, L"dsound_proxy.log");
        g_log = _wfopen(path, L"w");

        wchar_t exe[MAX_PATH] = L"";
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        Log("proxy attached to %ls (FILTER_TO_DEFAULT=%d)", exe, (int)FILTER_TO_DEFAULT);
        InstallClamp(dir);
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        if (g_log) { fclose(g_log); g_log = nullptr; }
    }
    return TRUE;
}
