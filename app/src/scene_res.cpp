// Native 1280x720 single-player scene for BO2 MP (bo2-dev). Addresses are title update 18's
// (the disc version's + 0x14AF48: the table moved as a whole, its records are unchanged).
//
// BO2 renders the single-player scene at 880x720 with 2x MSAA and resamples it to the 1280x720
// front buffer (the Xbox 360's 10 MB EDRAM fits 880x720 2x colour + depth; 1280x720 2x does not).
// The sizes come from a static render-target table in the image (52-byte records at
// 0x82A9A22C..0x82A9C620: +0 width, +2 height (u16), +8 colour format, +12 depth format,
// +16 MSAA (0 none, 1 2x, 2 4x), +22 id, +26/+28 resolve texture width/height).
//
// BO2MP_SCENE_1280=1 rewrites the single-player 880-based family before the guest starts:
// the 880x720 scene targets become 1280x720 without MSAA (colour + depth ~7.4 MB, fits EDRAM),
// and every alias / derived target keeps its relation (same sample footprint for the 4x "AS_4xMSAA"
// aliases, quarter and half sizes scaled by 1280/880). Each record is checked against its expected
// contents first, so nothing is written if the table differs. Split-screen targets are unchanged.

#include "scene_res.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <vector>

namespace {

struct Patch {
  uint32_t address;
  uint16_t id;
  uint16_t width, height;
  uint32_t msaa;
  uint16_t new_width, new_height;
  uint32_t new_msaa;
};

constexpr Patch kPatches[] = {
    // Scene targets (R_RENDERTARGET_SINGLE_PLAYER_FULLSCENE_*): 880x720 2x -> 1280x720 1x.
    {0x82A9A8AC, 0x112F, 880, 720, 1, 1280, 720, 0},
    {0x82A9A914, 0x1131, 880, 720, 1, 1280, 720, 0},
    {0x82A9A948, 0x1132, 880, 720, 1, 1280, 720, 0},
    {0x82A9A97C, 0x1133, 880, 720, 1, 1280, 720, 0},
    // Its 4x alias (same EDRAM samples): 440x720 4x -> 320x720 4x.
    {0x82A9A8E0, 0x0130, 440, 720, 2, 320, 720, 2},
    // Post-effect targets at scene size.
    {0x82A9AA4C, 0x0137, 880, 720, 0, 1280, 720, 0},
    {0x82A9B098, 0x017F, 880, 720, 0, 1280, 720, 0},
    {0x82A9B0CC, 0x0199, 880, 720, 0, 1280, 720, 0},
    // 4x alias of 0x0137: 440x360 4x -> 640x360 4x.
    {0x82A9AA80, 0x0138, 440, 360, 2, 640, 360, 2},
    // Quarter size.
    {0x82A9AAB4, 0x0139, 220, 180, 0, 320, 180, 0},
    {0x82A9AAE8, 0x013A, 220, 180, 0, 320, 180, 0},
    {0x82A9AB1C, 0x013B, 220, 180, 0, 320, 180, 0},
    {0x82A9B100, 0x01AD, 110, 90, 0, 160, 90, 0},
    // Half size.
    {0x82A9A844, 0x1113, 440, 360, 0, 640, 360, 0},
    {0x82A9A878, 0x1114, 440, 360, 0, 640, 360, 0},
};

uint16_t Load16(const uint8_t* p) { return static_cast<uint16_t>((p[0] << 8) | p[1]); }
uint32_t Load32(const uint8_t* p) {
  return (uint32_t{p[0]} << 24) | (uint32_t{p[1]} << 16) | (uint32_t{p[2]} << 8) | p[3];
}
void Store16(uint8_t* p, uint16_t v) {
  p[0] = static_cast<uint8_t>(v >> 8);
  p[1] = static_cast<uint8_t>(v);
}
void Store32(uint8_t* p, uint32_t v) {
  p[0] = static_cast<uint8_t>(v >> 24);
  p[1] = static_cast<uint8_t>(v >> 16);
  p[2] = static_cast<uint8_t>(v >> 8);
  p[3] = static_cast<uint8_t>(v);
}

}  // namespace

namespace {

// Split-screen: the per-view targets are 832x352 (2 players, each half of 1280x720) and 448x352
// (4 players, each quarter) with 2x/4x MSAA, plus derived half/quarter/eighth/sixteenth targets.
// Native view sizes are 1280x360 and 640x360. Scene-size targets lose MSAA (EDRAM); the 4x alias
// views keep 4x at the matching footprint (e.g. 640x180 4x = 1280x360 1x).
struct SizeMap {
  uint16_t w, h, new_w, new_h;
  bool scene;  // MSAA -> none
};
constexpr SizeMap kSplitSizes[] = {
    {832, 352, 1280, 360, true}, {416, 176, 640, 180, false}, {208, 88, 320, 90, false},
    {104, 44, 160, 45, false},   {52, 22, 80, 22, false},     {448, 352, 640, 360, true},
    {224, 176, 320, 180, false}, {112, 88, 160, 90, false},   {56, 44, 80, 45, false},
};
constexpr uint32_t kTableBegin = 0x82A98F48;
constexpr uint32_t kTableEnd = 0x82A9CF48;
constexpr int kExpectedRecords = 167;
constexpr int kExpectedSplitRecords = 106;

bool IsRecord(const uint8_t* r) {
  const uint16_t w = Load16(r), h = Load16(r + 2);
  const uint32_t pad = Load32(r + 4), color = Load32(r + 8), msaa = Load32(r + 16);
  const uint8_t family = static_cast<uint8_t>(color >> 24);
  return w >= 16 && w <= 4096 && h >= 8 && h <= 4096 && pad == 0 &&
         (family == 0x18 || family == 0x1A) && msaa <= 2 && Load16(r + 26) == w &&
         Load16(r + 28) == h;
}

void ApplySplitScreenResolution(uint8_t* guest_base, std::FILE* log) {
  std::vector<uint32_t> records;
  for (uint32_t a = kTableBegin; a + 52 <= kTableEnd;) {
    if (IsRecord(guest_base + a)) {
      records.push_back(a);
      a += 52;
    } else {
      a += 2;
    }
  }
  std::vector<std::pair<uint32_t, const SizeMap*>> patches;
  for (uint32_t a : records) {
    const uint8_t* r = guest_base + a;
    for (const SizeMap& m : kSplitSizes) {
      if (Load16(r) == m.w && Load16(r + 2) == m.h) {
        patches.emplace_back(a, &m);
      }
    }
  }
  if (static_cast<int>(records.size()) != kExpectedRecords ||
      static_cast<int>(patches.size()) != kExpectedSplitRecords) {
    if (log) {
      std::fprintf(log, "split-screen: %zu records, %zu matches (want %d, %d) - no change\n",
                   records.size(), patches.size(), kExpectedRecords, kExpectedSplitRecords);
    }
    return;
  }
  for (const auto& [a, m] : patches) {
    uint8_t* r = guest_base + a;
    const uint32_t msaa = Load32(r + 16);
    const uint32_t new_msaa = m->scene ? 0 : msaa;
    Store16(r + 0, m->new_w);
    Store16(r + 2, m->new_h);
    Store32(r + 16, new_msaa);
    Store16(r + 26, m->new_w);
    Store16(r + 28, m->new_h);
    if (log) {
      std::fprintf(log, "split id %04X %ux%u msaa %u -> %ux%u msaa %u\n", Load16(r + 22), m->w,
                   m->h, msaa, m->new_w, m->new_h, new_msaa);
    }
  }
}

}  // namespace

void ApplySceneResolution(uint8_t* guest_base) {
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
  const char* env = std::getenv("BO2MP_SCENE_1280");
  const char* split_env = std::getenv("BO2MP_SPLIT_NATIVE");
  const bool split = split_env != nullptr && split_env[0] == '1';
  std::FILE* log = ((env != nullptr && env[0] == '1') || split) ? std::fopen("mp_scene.txt", "w")
                                                                 : nullptr;
  if (split && guest_base != nullptr) {
    // BO2MP_SPLIT_NATIVE=1: native 1280x360 / 640x360 split-screen views without MSAA.
    ApplySplitScreenResolution(guest_base, log);
  }
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
  if (env == nullptr || env[0] != '1' || guest_base == nullptr) {
    if (log) {
      std::fclose(log);
    }
    return;
  }
  // Verify every record first; patch only if all match (an all-or-nothing change keeps the
  // aliases consistent).
  for (const Patch& p : kPatches) {
    const uint8_t* r = guest_base + p.address;
    const bool ok = Load16(r + 0) == p.width && Load16(r + 2) == p.height &&
                    Load32(r + 16) == p.msaa && Load16(r + 22) == p.id &&
                    Load16(r + 26) == p.width && Load16(r + 28) == p.height;
    if (!ok) {
      if (log) {
        std::fprintf(log, "record %08X (id %04X) differs: %ux%u msaa %u id %04X - no change\n",
                     p.address, p.id, Load16(r), Load16(r + 2), Load32(r + 16), Load16(r + 22));
        std::fclose(log);
      }
      return;
    }
  }
  for (const Patch& p : kPatches) {
    uint8_t* r = guest_base + p.address;
    Store16(r + 0, p.new_width);
    Store16(r + 2, p.new_height);
    Store32(r + 16, p.new_msaa);
    Store16(r + 26, p.new_width);
    Store16(r + 28, p.new_height);
    if (log) {
      std::fprintf(log, "id %04X %ux%u msaa %u -> %ux%u msaa %u\n", p.id, p.width, p.height,
                   p.msaa, p.new_width, p.new_height, p.new_msaa);
    }
  }
  if (log) {
    std::fclose(log);
  }
}
