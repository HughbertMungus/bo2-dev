// Engine dvar overrides for measurements (bo2-dev P2; from bo2-bisect's WatchMaxFps / DvarControl).
//
// BO2MP_DVARS="com_maxfps=i:1000,r_vsync=i:0" forces each listed dvar. A dvar_t lives on the guest
// heap, its first word points at the name string in the image, and the current value sits at +0x18
// (+0x14 is the "modified" flag the engine checks before applying render dvars). A background
// thread finds the name string, then the struct, and re-writes the value whenever the game
// changes it. Without BO2MP_DVARS nothing runs. Windows only (VirtualQuery for the heap scan).

#include "dvar_force.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <chrono>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace {

struct ForcedDvar {
  std::string name;
  char kind = 'i';  // b: byte, i: u32, f: float
  uint32_t bits = 0;
  uint32_t name_ptr = 0;
  uint32_t struct_addr = 0;
};

uint32_t Load32(const uint8_t* base, uint32_t addr) {
  const uint8_t* p = base + addr;
  return (uint32_t{p[0]} << 24) | (uint32_t{p[1]} << 16) | (uint32_t{p[2]} << 8) | p[3];
}

void Store32(uint8_t* base, uint32_t addr, uint32_t value) {
  uint8_t* p = base + addr;
  p[0] = static_cast<uint8_t>(value >> 24);
  p[1] = static_cast<uint8_t>(value >> 16);
  p[2] = static_cast<uint8_t>(value >> 8);
  p[3] = static_cast<uint8_t>(value);
}

std::vector<ForcedDvar> ParseSpec(const char* spec) {
  std::vector<ForcedDvar> out;
  std::string all(spec);
  size_t at = 0;
  while (at < all.size()) {
    size_t end = all.find(',', at);
    if (end == std::string::npos) end = all.size();
    const std::string item = all.substr(at, end - at);
    at = end + 1;
    const size_t eq = item.find('=');
    const size_t colon = item.find(':', eq == std::string::npos ? 0 : eq);
    if (eq == std::string::npos || colon == std::string::npos || colon != eq + 2) continue;
    ForcedDvar d;
    d.name = item.substr(0, eq);
    d.kind = item[eq + 1];
    const std::string value = item.substr(colon + 1);
    if (d.kind == 'f') {
      const float f = static_cast<float>(std::atof(value.c_str()));
      std::memcpy(&d.bits, &f, sizeof(f));
    } else {
      d.bits = static_cast<uint32_t>(std::strtoul(value.c_str(), nullptr, 0));
    }
    out.push_back(d);
  }
  return out;
}

// The NUL-terminated string in the loaded image (0x82000000-0x82400000), or 0.
uint32_t FindImageString(const uint8_t* base, const char* name, size_t len) {
  constexpr uint32_t kBegin = 0x82000000u;
  constexpr uint32_t kEnd = 0x82400000u;
  for (uint32_t a = kBegin + 1; a + len + 1 < kEnd; ++a) {
    if (base[a] == static_cast<uint8_t>(name[0]) && base[a - 1] == 0 && base[a + len] == 0 &&
        std::memcmp(base + a, name, len) == 0) {
      return a;
    }
  }
  return 0;
}

#if defined(_WIN32)
// The first committed, readable guest word equal to `target` in 0x82000000-0x8A000000, or 0.
uint32_t FindPointerTo(const uint8_t* base, uint32_t target) {
  constexpr uint32_t kBegin = 0x82000000u;
  constexpr uint32_t kEnd = 0x8A000000u;
  const uint8_t needle[4] = {static_cast<uint8_t>(target >> 24), static_cast<uint8_t>(target >> 16),
                             static_cast<uint8_t>(target >> 8), static_cast<uint8_t>(target)};
  const uintptr_t host_base = reinterpret_cast<uintptr_t>(base);
  uintptr_t p = host_base + kBegin;
  const uintptr_t end = host_base + kEnd;
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
      const uint8_t* hay = reinterpret_cast<const uint8_t*>(p);
      for (size_t i = 0; i + 4 <= static_cast<size_t>(region_end - p); i += 4) {
        if (std::memcmp(hay + i, needle, 4) == 0) {
          return static_cast<uint32_t>((p + i) - host_base);
        }
      }
    }
    if (region_end <= p) break;
    p = region_end;
  }
  return 0;
}

// Guest memory is released when the title unloads, which can happen while this detached thread
// still runs (a quit used to fault here, 0x83A97D90 read after TitleRuntime::Unload). Every guest
// access goes through these SEH guards, which return false on a fault so the thread just ends.
// They hold no objects with destructors, as __try requires.
bool SafeFindString(const uint8_t* base, const char* name, size_t len, uint32_t* out) {
  __try {
    *out = FindImageString(base, name, len);
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

bool SafeFindPointer(const uint8_t* base, uint32_t target, uint32_t* out) {
  __try {
    *out = FindPointerTo(base, target);
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

bool SafeLoad32(const uint8_t* base, uint32_t addr, uint32_t* out) {
  __try {
    *out = Load32(base, addr);
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

// Re-writes the value when the game changed it.
bool SafeForce(uint8_t* base, uint32_t struct_addr, char kind, uint32_t bits) {
  __try {
    const uint32_t value_addr = struct_addr + 0x18;
    const bool differs = kind == 'b' ? base[value_addr] != static_cast<uint8_t>(bits)
                                     : Load32(base, value_addr) != bits;
    if (differs) {
      if (kind == 'b') {
        base[value_addr] = static_cast<uint8_t>(bits);
      } else {
        Store32(base, value_addr, bits);
      }
      base[struct_addr + 0x14] = 1;  // modified: the engine applies it
    }
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}
#endif

void ForceLoop(uint8_t* base, std::vector<ForcedDvar> dvars) {
#if defined(_WIN32)
  std::FILE* log = std::fopen("mp_dvars.txt", "a");
  auto note = [log](const std::string& text) {
    if (log) {
      std::fprintf(log, "%s\n", text.c_str());
      std::fflush(log);
    }
  };
  // The dvars register after startup, which can take minutes on a cold pipeline cache: look for
  // up to 10 minutes, then keep forcing every 200 ms.
  for (int tick = 0;; ++tick) {
    bool all_found = true;
    for (ForcedDvar& d : dvars) {
      if (d.struct_addr != 0) continue;
      all_found = false;
      if (tick % 5 != 0 || tick > 3000) continue;
      if (d.name_ptr == 0 && !SafeFindString(base, d.name.data(), d.name.size(), &d.name_ptr)) {
        return;
      }
      if (d.name_ptr != 0 && !SafeFindPointer(base, d.name_ptr, &d.struct_addr)) {
        return;
      }
      if (d.struct_addr != 0) {
        uint32_t value = 0;
        if (!SafeLoad32(base, d.struct_addr + 0x18, &value)) {
          return;
        }
        // The raw struct too (its domain, the value range, follows the values).
        std::string raw;
        for (uint32_t off = 0; off < 0x50; off += 4) {
          uint32_t word = 0;
          if (!SafeLoad32(base, d.struct_addr + off, &word)) {
            return;
          }
          char hex[12];
          std::snprintf(hex, sizeof(hex), "%08X ", word);
          raw += hex;
        }
        note("dvar " + d.name + ": struct " + std::to_string(d.struct_addr) + " value " +
             std::to_string(value) + " -> forcing " + std::to_string(d.bits) + " raw " + raw);
      }
    }
    for (ForcedDvar& d : dvars) {
      if (d.struct_addr == 0) continue;
      if (!SafeForce(base, d.struct_addr, d.kind, d.bits)) {
        return;  // Guest memory is gone: the title unloaded.
      }
    }
    (void)all_found;
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
#else
  (void)base;
  (void)dvars;
#endif
}

}  // namespace

void DumpGuestImage(uint8_t* guest_base) {
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
  // BO2MP_DUMP_IMAGE=<path>: writes the loaded image (0x82000000-0x849B0000) once, for offline
  // searches of strings and data (as bo2-bisect's app did).
  const char* path = std::getenv("BO2MP_DUMP_IMAGE");
  if (path == nullptr || path[0] == 0 || guest_base == nullptr) {
    return;
  }
  if (std::FILE* f = std::fopen(path, "wb")) {
    std::fwrite(guest_base + 0x82000000u, 1, 0x849B0000u - 0x82000000u, f);
    std::fclose(f);
  }
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
}

void StartDvarForcing(uint8_t* guest_base) {
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
  const char* spec = std::getenv("BO2MP_DVARS");
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
  if (spec == nullptr || spec[0] == 0 || guest_base == nullptr) {
    return;
  }
  std::vector<ForcedDvar> dvars = ParseSpec(spec);
  if (dvars.empty()) {
    return;
  }
  std::thread(ForceLoop, guest_base, std::move(dvars)).detach();
}
