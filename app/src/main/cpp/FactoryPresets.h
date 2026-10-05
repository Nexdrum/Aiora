#pragma once
#include <array>
#include "AioraTypes.h"

namespace aiora {

enum class FactoryPreset : int32_t {
    SpectrachordInit = 0,
    Subula = 1,
    Spectrello = 2,
    Nebular = 3,
    Nexdrum = 4,
    Count = 5
};

Patch makeDefaultPatch(const char* name = "Spectrachord Init");
Patch makeFactoryPatch(FactoryPreset preset);
const std::array<Patch, static_cast<size_t>(FactoryPreset::Count)>& factoryBank();

} // namespace aiora
