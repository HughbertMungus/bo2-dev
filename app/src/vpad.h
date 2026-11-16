// Virtual pads ("bo2vpad" input driver) and the player 1 pad mirror. See vpad.cpp.
#pragma once

namespace rex {
class IModule;
namespace hid {
class IInputSystem;
}
}  // namespace rex

/** The code module that serves the bo2vpad input driver; add it in App::OnRegisterModules. */
rex::IModule& VirtualPadModuleInstance();

/** Starts the threads that mirror player 1's Y / X / RB onto the virtual pads and follow mp_cmd.txt. */
void StartPadMirror(rex::hid::IInputSystem& input);
