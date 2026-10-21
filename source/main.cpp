#define NO_MALLOC_OVERRIDE
#include <GarrysMod/Lua/Interface.h>
#include <detouring/hook.hpp>
#include <windows.h>
#include <array>
#include <atomic>
#include <mutex>
#include "waveform.h"

namespace {
// SteamUser023, verified against the engine and the bundled Steam SDK.
using GetVoice = int (*)(void*, bool, void*, unsigned, unsigned*, bool, void*, unsigned, unsigned*, unsigned);
using DecompressVoice = int (*)(void*, const void*, unsigned, void*, unsigned, unsigned*, unsigned);
Detouring::Hook captureHook;
void* steamUser = nullptr;
DecompressVoice decompress = nullptr;
std::atomic<bool> enabled{false};
std::mutex waveMutex;
Waveform wave;
ULONGLONG lastCapture = 0;

int Capture(void* self, bool compressed, void* dest, unsigned capacity, unsigned* written,
            bool raw, void* rawDest, unsigned rawCapacity, unsigned* rawWritten, unsigned rate) {
    auto original = captureHook.GetTrampoline<GetVoice>();
    const int result = original(self, compressed, dest, capacity, written,
                                raw, rawDest, rawCapacity, rawWritten, rate);
    // Observe the game's existing capture. Never fetch/consume voice independently,
    // start the microphone, change its packets, or write audio to disk.
    if (self != steamUser || result != 0 || !enabled.load()) return result;
    std::array<int16_t, 48000> decoded{};
    unsigned bytes = 0;
    if (compressed && dest && written && *written && *written <= capacity &&
        decompress(self, dest, *written, decoded.data(), sizeof(decoded), &bytes, 48000) == 0 &&
        bytes > 0 && bytes <= sizeof(decoded)) {
        std::lock_guard<std::mutex> lock(waveMutex);
        const auto now = GetTickCount64();
        if (now - lastCapture > 200) wave.Clear();
        wave.Append(decoded.data(), bytes / sizeof(int16_t));
        lastCapture = now;
    }
    return result;
}
}

LUA_FUNCTION_STATIC(SetEnabled) {
    enabled.store(LUA->GetBool(1));
    if (!enabled.load()) {
        std::lock_guard<std::mutex> lock(waveMutex);
        wave.Clear(); lastCapture = 0;
    }
    return 0;
}
LUA_FUNCTION_STATIC(GetSamples) {
    LUA->CheckType(1, GarrysMod::Lua::Type::Table);
    std::array<int16_t, 240> points{};
    bool fresh = false;
    {
        std::lock_guard<std::mutex> lock(waveMutex);
        fresh = enabled.load() && lastCapture && GetTickCount64() - lastCapture < 200;
        if (fresh) for (size_t i = 0; i < points.size(); ++i) points[i] = wave.Point(i);
    }
    for (size_t i = 0; i < points.size(); ++i) {
        LUA->PushNumber(i + 1); LUA->PushNumber(points[i]); LUA->RawSet(1);
    }
    LUA->PushBool(fresh);
    return 1;
}
GMOD_MODULE_OPEN() {
    auto steam = GetModuleHandleW(L"steam_api64.dll");
    auto getUser = steam ? reinterpret_cast<void* (*)()>(GetProcAddress(steam, "SteamAPI_SteamUser_v023")) : nullptr;
    steamUser = getUser ? getUser() : nullptr;
    if (!steamUser) LUA->ThrowError("Sub Rosa microphone: SteamUser023 unavailable");
    auto table = *reinterpret_cast<void***>(steamUser);
    decompress = reinterpret_cast<DecompressVoice>(table[11]);
    if (!captureHook.Create(Detouring::Hook::Target(table[10]), reinterpret_cast<void*>(&Capture)) || !captureHook.Enable()) {
        captureHook.Destroy();
        LUA->ThrowError("Sub Rosa microphone: could not observe game voice capture");
    }
    LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
    LUA->CreateTable();
    LUA->PushCFunction(SetEnabled); LUA->SetField(-2, "SetEnabled");
    LUA->PushCFunction(GetSamples); LUA->SetField(-2, "GetSamples");
    LUA->PushNumber(1); LUA->SetField(-2, "API_VERSION");
    LUA->SetField(-2, "subrosa_mic"); LUA->Pop();
    return 0;
}
GMOD_MODULE_CLOSE() {
    enabled.store(false);
    captureHook.Destroy();
    return 0;
}
