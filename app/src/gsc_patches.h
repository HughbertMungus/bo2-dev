// In-memory patches to the title's GSC game scripts. See gsc_patches.cpp.
#pragma once

#include <cstdint>

/** Starts the patch thread when a patch is enabled (BO2MP_LMP_BOT_LIMIT); does nothing otherwise. */
void StartGscPatches(uint8_t* guest_base);
