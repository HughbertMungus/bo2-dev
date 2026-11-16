// Native 1280x720 single-player scene (BO2MP_SCENE_1280=1). See scene_res.cpp.
#pragma once

#include <cstdint>

/** Rewrites BO2's single-player render-target table for a 1280x720 scene without MSAA. */
void ApplySceneResolution(uint8_t* guest_base);
