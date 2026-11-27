// Patches to the title's GSC game scripts in memory (bo2-dev).
//
// T6 runs GSC bytecode in place from the loaded script buffer, so rewriting the bytes before the
// code runs changes the game logic. A background thread finds each patch's bytes by their hashes
// (byte_signature.h) in the physical heap (guest 0xA0000000-0xC0000000, where fastfile zones load), patches it, then
// rechecks every few seconds in case the zone was reloaded. A different build of the scripts simply
// never matches.
//
// BO2MP_LMP_BOT_LIMIT set (see lua_patches.cpp): maps/mp/bots/_bot.gsc bot_local_think sets
// max_players = islocalgame() ? 10 : 18 and spawns bots only below it, so Local Multiplayer stops
// at 10 players. GetByte 10 (04 0a) -> GetByte 18, the System Link size.

#include "gsc_patches.h"

#include "byte_signature.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace {

struct GscPatch {
  const char* what;
  bo2::ByteSignature sig;
  std::vector<bo2::ByteEdit> edits;
  uint32_t found_at = 0;      // guest address of the last match, 0 when not found yet
  uint64_t patched_fnv = 0;   // hash of the stretch once patched
};

#if defined(_WIN32)
constexpr uint32_t kScanBegin = 0xA0000000u;
constexpr uint32_t kScanEnd = 0xC0000000u;

// Finds the signature in committed guest memory; returns the guest address or 0.
uint32_t FindSignature(const uint8_t* base, const bo2::ByteSignature& sig) {
  const uintptr_t host_base = reinterpret_cast<uintptr_t>(base);
  uintptr_t p = host_base + kScanBegin;
  const uintptr_t end = host_base + kScanEnd;
  while (p < end) {
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery(reinterpret_cast<LPCVOID>(p), &mbi, sizeof(mbi))) break;
    uintptr_t region_end = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
    if (region_end > end) region_end = end;
    const DWORD prot = mbi.Protect & 0xFFu;
    const bool readable = mbi.State == MEM_COMMIT &&
                          (prot == PAGE_READONLY || prot == PAGE_READWRITE ||
                           prot == PAGE_EXECUTE_READ || prot == PAGE_EXECUTE_READWRITE);
    if (readable && region_end > p) {
      const size_t at = bo2::Find(sig, reinterpret_cast<const uint8_t*>(p),
                                  static_cast<size_t>(region_end - p));
      if (at != SIZE_MAX) {
        return static_cast<uint32_t>((p + at) - host_base);
      }
    }
    if (region_end <= p) break;
    p = region_end;
  }
  return 0;
}

// Guest memory goes away when the title unloads; a fault just ends the thread (see dvar_force.cpp).
bool SafeFind(const uint8_t* base, const bo2::ByteSignature& sig, uint32_t* out) {
  __try {
    *out = FindSignature(base, sig);
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

bool SafeHash(const uint8_t* base, uint32_t at, size_t size, uint64_t* out) {
  __try {
    *out = bo2::Fnv1a64(base + at, size);
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

bool SafeEdit(uint8_t* base, uint32_t at, const std::vector<bo2::ByteEdit>& edits) {
  __try {
    bo2::ApplyEdits(base + at, edits);
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

void PatchLoop(uint8_t* base, std::vector<GscPatch> patches) {
  for (;;) {
    for (GscPatch& patch : patches) {
      if (patch.found_at) {
        uint64_t now = 0;
        if (!SafeHash(base, patch.found_at, patch.sig.length, &now)) {
          return;
        }
        if (now == patch.patched_fnv) {
          continue;
        }
      }
      uint32_t at = 0;
      if (!SafeFind(base, patch.sig, &at)) {
        return;
      }
      if (at && SafeEdit(base, at, patch.edits) &&
          SafeHash(base, at, patch.sig.length, &patch.patched_fnv)) {
        if (patch.found_at != at) {
          if (std::FILE* log = std::fopen("mp_gscpatch.txt", "a")) {
            std::fprintf(log, "patched %s at %08X\n", patch.what, at);
            std::fclose(log);
          }
        }
        patch.found_at = at;
      }
    }
    std::this_thread::sleep_for(std::chrono::seconds(3));
  }
}
#endif

}  // namespace

void StartGscPatches(uint8_t* guest_base) {
#if defined(_WIN32)
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
  const char* bots = std::getenv("BO2MP_LMP_BOT_LIMIT");
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
  std::vector<GscPatch> patches;
  if (bots && bots[0]) {
    // if (islocalgame()) max_players = 10; else max_players = 18;  (jump, GetByte 10, jump,
    // GetByte 18, store max_players): the first GetByte operand (offset 5) becomes 18.
    patches.push_back(GscPatch{"local multiplayer max players 10 -> 18",
                               {15, 0xf4af, 0x76630c99b0ec8367ull},
                               {{5, std::string(1, static_cast<char>(18))}}});
    // Title update 18: the same jump, GetByte 10, jump, GetByte 18, store, with another jump opcode.
    patches.push_back(GscPatch{"local multiplayer max players 10 -> 18 (TU18)",
                               {15, 0x879c, 0x2e3aaed9cd4373c8ull},
                               {{5, std::string(1, static_cast<char>(18))}}});
  }
  if (patches.empty() || guest_base == nullptr) {
    return;
  }
  std::thread(PatchLoop, guest_base, std::move(patches)).detach();
#else
  (void)guest_base;
#endif
}
