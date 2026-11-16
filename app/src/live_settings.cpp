// Live settings file (bo2-dev P2; replaces bo2-bisect's per-switch control files).
//
// live.txt in the working folder is read once a second. Each line is `<setting> <value>`, the
// same text the console overlay takes (blank lines and lines starting with '#' are ignored). A
// value that differs from the last one applied is written at the runtime layer, which wins over
// the command line. Changes are logged to mp_live.txt. tools/ab_toggle.ps1 alternates two files
// into live.txt for in-match A/B tests, and every REX_FPS_LOG line carries the switches in effect.
// Removing a line leaves its last value in place.

#include "live_settings.h"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <thread>

#include <rex/settings_registry.h>

namespace {

void LiveLoop() {
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
  std::FILE* log = std::fopen("mp_live.txt", "a");
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
  std::map<std::string, std::string> applied;
  for (;;) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::ifstream file("live.txt");
    if (!file) {
      continue;
    }
    std::string line;
    while (std::getline(file, line)) {
      if (!line.empty() && line.back() == '\r') line.pop_back();
      std::istringstream in(line);
      std::string name, value;
      if (!(in >> name) || name[0] == '#' || !(in >> value)) {
        continue;
      }
      auto it = applied.find(name);
      if (it != applied.end() && it->second == value) {
        continue;
      }
      const auto result = REX_SETTINGS().SetText(rex::SettingLayer::kRuntime, name, value);
      applied[name] = value;
      if (log) {
        const std::time_t t = std::time(nullptr);
        std::tm tm_local{};
        localtime_s(&tm_local, &t);
        std::fprintf(log, "%02d:%02d:%02d %s = %s%s\n", tm_local.tm_hour, tm_local.tm_min,
                     tm_local.tm_sec, name.c_str(), value.c_str(),
                     result ? "" : " (refused)");
        std::fflush(log);
      }
    }
  }
}

}  // namespace

void StartLiveSettings() {
  static bool started = false;
  if (started) {
    return;
  }
  started = true;
  std::thread(LiveLoop).detach();
}
