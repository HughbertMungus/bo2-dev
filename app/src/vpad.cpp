// Virtual pads and the player 1 pad mirror (bo2-dev; from bo2-bisect input/0002, mp_control.cpp).
//
// A one-controller 4 player bots setup: an input driver "bo2vpad" adds three virtual pads, which
// the input system assigns to the free user slots like any pad. While the mirror is on, a rising
// edge of player 1's
//   Y  -> every virtual pad presses A (join),
//   X  -> every virtual pad presses B (leave class select -> spectate),
//   RB -> virtual pad N presses RB N times (each spectates a different bot).
// The input system turns the presses into keystrokes too, so menus react as to a real pad.
//
// Enable: hid.drivers must list bo2vpad (run_local.cmd: BO2MP_VPADS=1). The mirror is on from launch
// with BO2MP_PAD_MIRROR=1, and padmirror.txt in the working folder ("0"/"1", read twice a second)
// switches it live - turn it off in a match, where Y/X/RB would otherwise press on the spectators.
//
// Scripted presses (bo2-bisect mp_control.cpp's command file): lines appended to mp_cmd.txt in the
// working folder, "<pad> <button> [hold_ms]" with pad p2-p4 (the virtual pads) and button one of
// a b x y up down left right start back lb rb ls rs, e.g. "p2 a" or "p3 start 300" (run\pad.cmd).
// Lines already in the file at launch are skipped. Player 1 is the real pad and is not scripted.

#include "vpad.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <mutex>
#include <thread>

#include <rex/driver_module.h>
#include <rex/driver_registration.h>
#include <rex/hid/input_driver.h>
#include <rex/hid/input_system.h>
#include <rex/module.h>

namespace {

using Clock = std::chrono::steady_clock;
constexpr int kVirtualPads = 3;
constexpr rex::u64 kIdBase = 0xB02A0000;

struct VirtualPadState {
  std::mutex mutex;
  std::array<rex::u16, kVirtualPads> held{};
  std::array<Clock::time_point, kVirtualPads> release_at{};
};

VirtualPadState& Pads() {
  static VirtualPadState state;
  return state;
}

rex::Error NotAVirtualPad() {
  return rex::Error(rex::ErrorCategory::kNotFound, "not a virtual pad");
}

int IndexOf(rex::hid::PadDeviceID id) {
  const rex::u64 index = id.value - kIdBase;
  return id.value >= kIdBase && index < kVirtualPads ? static_cast<int>(index) : -1;
}

class VirtualPadDriver final : public rex::hid::IInputDriver {
 public:
  std::string_view GetName() const override { return "bo2vpad"; }
  rex::Result<void> Initialize(rex::IDriverContext&) override { return rex::Ok(); }
  void Shutdown() override {}

  rex::u64 GetDeviceRevision() const override { return 1; }

  void EnumerateDevices(std::vector<rex::hid::PadDevice>& out) override {
    out.clear();
    for (int i = 0; i < kVirtualPads; ++i) {
      rex::hid::PadDevice device;
      device.id = rex::hid::PadDeviceID{kIdBase + static_cast<rex::u64>(i)};
      device.name = "Virtual Pad " + std::to_string(i + 2);
      device.info.type = rex::hid::PadType::kX360;
      // A real pad connecting later (e.g. one streamed in by Sunshine) still becomes player 1.
      device.yields_slot = true;
      out.push_back(std::move(device));
    }
  }

  rex::Result<rex::hid::PadState> GetDeviceState(rex::hid::PadDeviceID id) override {
    const int index = IndexOf(id);
    if (index < 0) {
      return std::unexpected(NotAVirtualPad());
    }
    VirtualPadState& pads = Pads();
    std::lock_guard lock(pads.mutex);
    rex::hid::PadState state;
    if (pads.held[index] != 0 && Clock::now() >= pads.release_at[index]) {
      pads.held[index] = 0;
    }
    state.buttons = pads.held[index];
    return state;
  }

  rex::Result<rex::hid::PadCapabilities> GetDeviceCapabilities(
      rex::hid::PadDeviceID id) override {
    if (IndexOf(id) < 0) {
      return std::unexpected(NotAVirtualPad());
    }
    rex::hid::PadCapabilities caps;
    caps.subtype = 0x01;
    caps.supported.buttons = 0xFBFF;  // everything but the guide button
    caps.supported.left_trigger = 0xFF;
    caps.supported.right_trigger = 0xFF;
    caps.supported.thumb_lx = 0x7FFF;
    caps.supported.thumb_ly = 0x7FFF;
    caps.supported.thumb_rx = 0x7FFF;
    caps.supported.thumb_ry = 0x7FFF;
    return caps;
  }

  rex::Result<void> SetDeviceVibration(rex::hid::PadDeviceID id,
                                       const rex::hid::PadVibration&) override {
    if (IndexOf(id) < 0) {
      return std::unexpected(NotAVirtualPad());
    }
    return rex::Ok();
  }
};

std::unique_ptr<rex::IDriver> CreateVirtualPadDriver() {
  return std::make_unique<VirtualPadDriver>();
}

constexpr std::array<rex::DriverRegistration, 1> kDrivers = {
    rex::DriverRegistration{rex::DriverType::kInput, "bo2vpad", rex::GraphicsFamily::kNone,
                            &CreateVirtualPadDriver}};

class VirtualPadModule final : public rex::IModule, public rex::IDriverModule {
 public:
  REX_MODULE_SERVES(rex::IDriverModule)

  std::string_view GetName() const override { return "defaultmp-vpad"; }
  rex::Result<void> Connect(rex::IInterfaceFactory&) override { return rex::Ok(); }
  void Disconnect() override {}
  rex::Result<void> Init() override { return rex::Ok(); }
  void Shutdown() override {}
  rex::IInterface* QueryInterface(std::string_view name, rex::u32 version) override {
    return rex::FindModuleInterface<rex::IDriverModule>(*this, name, version);
  }
  std::span<const rex::DriverRegistration> GetDrivers() const override { return kDrivers; }
};

// Presses `button` on virtual pad `pad` `times` times: held 150 ms, 250 ms apart.
void PressVirtual(int pad, rex::u16 button, int times) {
  std::thread([pad, button, times] {
    for (int i = 0; i < times; ++i) {
      if (i) std::this_thread::sleep_for(std::chrono::milliseconds(250));
      VirtualPadState& pads = Pads();
      {
        std::lock_guard lock(pads.mutex);
        pads.held[pad] = button;
        pads.release_at[pad] = Clock::now() + std::chrono::milliseconds(150);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }
  }).detach();
}

rex::u16 ButtonByName(const char* name) {
  static constexpr struct {
    const char* name;
    rex::u16 bit;
  } kButtons[] = {{"a", rex::hid::kPadA},
                  {"b", rex::hid::kPadB},
                  {"x", rex::hid::kPadX},
                  {"y", rex::hid::kPadY},
                  {"up", rex::hid::kPadDPadUp},
                  {"down", rex::hid::kPadDPadDown},
                  {"left", rex::hid::kPadDPadLeft},
                  {"right", rex::hid::kPadDPadRight},
                  {"start", rex::hid::kPadStart},
                  {"back", rex::hid::kPadBack},
                  {"lb", rex::hid::kPadLeftShoulder},
                  {"rb", rex::hid::kPadRightShoulder},
                  {"ls", rex::hid::kPadLeftThumb},
                  {"rs", rex::hid::kPadRightThumb}};
  for (const auto& b : kButtons) {
    if (std::strcmp(b.name, name) == 0) return b.bit;
  }
  return 0;
}

// Follows mp_cmd.txt from its size at launch; a truncated file is read again from the start.
void CommandFileLoop() {
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
  long offset = -1;
  for (;;) {
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    std::FILE* f = std::fopen("mp_cmd.txt", "rb");
    if (!f) {
      if (offset < 0) offset = 0;
      continue;
    }
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    if (offset < 0 || size < offset) {
      offset = offset < 0 ? size : 0;
    }
    if (size == offset) {
      std::fclose(f);
      continue;
    }
    std::fseek(f, offset, SEEK_SET);
    std::string chunk(static_cast<size_t>(size - offset), '\0');
    chunk.resize(std::fread(chunk.data(), 1, chunk.size(), f));
    std::fclose(f);
    // Keep a partial last line for the next read.
    const size_t end = chunk.rfind('\n');
    if (end == std::string::npos) continue;
    offset += static_cast<long>(end + 1);
    size_t at = 0;
    while (at <= end) {
      const size_t nl = chunk.find('\n', at);
      const std::string line = chunk.substr(at, nl - at);
      at = nl + 1;
      char pad[8] = {};
      char button[16] = {};
      int hold_ms = 250;
      if (std::sscanf(line.c_str(), "%7s %15s %d", pad, button, &hold_ms) < 2) continue;
      const int index = (pad[0] == 'p' && pad[1] >= '2' && pad[1] <= '4' && pad[2] == 0)
                            ? pad[1] - '2'
                            : -1;
      const rex::u16 bit = ButtonByName(button);
      if (index < 0 || bit == 0 || hold_ms <= 0 || hold_ms > 10000) continue;
      VirtualPadState& pads = Pads();
      std::lock_guard lock(pads.mutex);
      pads.held[index] = bit;
      pads.release_at[index] = Clock::now() + std::chrono::milliseconds(hold_ms);
    }
  }
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
}

bool MirrorEnabled(bool launch_default) {
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
  static Clock::time_point last_check{};
  static int file_state = -1;  // -1 no file, 0 off, 1 on
  const Clock::time_point now = Clock::now();
  if (now - last_check >= std::chrono::milliseconds(500)) {
    last_check = now;
    file_state = -1;
    if (std::FILE* f = std::fopen("padmirror.txt", "r")) {
      const int ch = std::fgetc(f);
      std::fclose(f);
      if (ch == '0' || ch == '1') file_state = ch - '0';
    }
  }
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
  return file_state >= 0 ? file_state == 1 : launch_default;
}

}  // namespace

rex::IModule& VirtualPadModuleInstance() {
  static VirtualPadModule module;
  return module;
}

void StartPadMirror(rex::hid::IInputSystem& input) {
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
  const char* env = std::getenv("BO2MP_PAD_MIRROR");
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
  const bool launch_default = env != nullptr && env[0] == '1';
  std::thread(CommandFileLoop).detach();
  std::thread([&input, launch_default] {
    rex::u16 previous = 0;
    for (;;) {
      std::this_thread::sleep_for(std::chrono::milliseconds(8));
      auto reading = input.GetState(0);
      const rex::u16 buttons = reading ? reading->state.buttons : 0;
      const rex::u16 pressed = static_cast<rex::u16>(buttons & ~previous);
      previous = buttons;
      constexpr rex::u16 kWatched =
          rex::hid::kPadY | rex::hid::kPadX | rex::hid::kPadRightShoulder;
      if (!(pressed & kWatched) || !MirrorEnabled(launch_default)) {
        continue;
      }
      for (int p = 0; p < kVirtualPads; ++p) {
        if (pressed & rex::hid::kPadY) PressVirtual(p, rex::hid::kPadA, 1);
        if (pressed & rex::hid::kPadX) PressVirtual(p, rex::hid::kPadB, 1);
        if (pressed & rex::hid::kPadRightShoulder) {
          PressVirtual(p, rex::hid::kPadRightShoulder, p + 1);
        }
      }
    }
  }).detach();
}
