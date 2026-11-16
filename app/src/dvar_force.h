// Engine dvar overrides for measurements (BO2MP_DVARS). See dvar_force.cpp.
#pragma once

#include <cstdint>

/** Starts the forcing thread when BO2MP_DVARS is set; does nothing otherwise. */
void StartDvarForcing(uint8_t* guest_base);

/** Writes the loaded guest image to BO2MP_DUMP_IMAGE when set. */
void DumpGuestImage(uint8_t* guest_base);
