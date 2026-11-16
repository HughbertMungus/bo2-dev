// defaultmp - BO2 multiplayer on rexglue-sdk development (bo2-dev P0 baseline).
//
// Upstream's title app as `rexglue init` writes it (resources/templates/init/main_cpp.inja). The
// bo2-bisect DefaultMpApp (probes, settings system, pad mirror, crash tracing, display defaults)
// is deliberately not carried over in P0; see PORT_PLAN.md.

#include <rex/app/title_app.h>
#include <rex/result.h>
#include <rex/settings_registry.h>
#include <rex/kernel/title_runtime.h>

#include "dvar_force.h"
#include "gsc_patches.h"
#include "vpad.h"
#include "scene_res.h"
#include "live_settings.h"

#include <cstdlib>
#include <vector>

#include "generated/default_mp/defaultmp_init.h"

class DefaultmpApp : public rex::app::TitleApp {
 public:
  DefaultmpApp() : TitleApp("defaultmp", kPPCImageConfig) {}

 protected:
  rex::Result<void> OnConfigure(rex::SettingsRegistry& settings) override {
    // title.entry defaults to default.xex, which is the campaign binary in the BO2 folder and does
    // not match the image generated from default_mp.xex (bo2-bisect: guest entry returned at once,
    // the "silent exit"). Title layer, so the command line can still override it.
    REX_TRY_VOID(settings.SetText(rex::SettingLayer::kTitle, "title.entry", "default_mp.xex"));
    return rex::Ok();
  }

  void OnRegisterModules(std::vector<rex::IModule*>& modules) override {
    TitleApp::OnRegisterModules(modules);
    // Serves the "bo2vpad" input driver; it runs only when hid.drivers lists it (vpad.cpp).
    modules.push_back(&VirtualPadModuleInstance());
  }

  rex::Result<void> OnTitleLaunch(rex::kernel::TitleRuntime& title) override {
    // Measurement only: BO2MP_DVARS forces engine dvars such as com_maxfps (dvar_force.cpp).
    DumpGuestImage(title.GetMembase().GetBase());
    // BO2MP_SCENE_1280=1: native 1280x720 single-player scene without MSAA (scene_res.cpp).
    ApplySceneResolution(title.GetMembase().GetBase());
    // live.txt (working folder) applies settings once a second, for in-match A/B (live_settings.cpp).
    StartLiveSettings();
    StartDvarForcing(title.GetMembase().GetBase());
    // BO2MP_LMP_BOT_LIMIT: Local Multiplayer bot limits (gsc_patches.cpp, lua_patches.cpp).
    StartGscPatches(title.GetMembase().GetBase());
    // BO2MP_VPADS=1 (with hid.drivers listing bo2vpad): mirror player 1's Y/X/RB to the virtual pads.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
    const char* vpads = std::getenv("BO2MP_VPADS");
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
    if (vpads != nullptr && vpads[0] == '1' && !mirror_started_) {
      mirror_started_ = true;
      StartPadMirror(GetInput());
    }
    return TitleApp::OnTitleLaunch(title);
  }

  // The default asks "Restart / Exit" when the title exits (on the 360, BO2 MP leaving to the
  // campaign or Zombies launches another executable). Nothing else runs here, so close the app:
  // streaming hosts (Sunshine/Moonlight) and launch scripts expect the process to end.
  void OnTitleExit(rex::kernel::TitleExitReason) override { rex::app::RequestShutdown(0); }

 private:
  bool mirror_started_ = false;
};

REX_TITLE_APP(DefaultmpApp)
