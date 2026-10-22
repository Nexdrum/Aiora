#pragma once
#include <array>
#include "AioraTypes.h"

namespace aiora {
// Only four factory-selectable instruments. SpectrachordInit is an internal
// neutral patch used when creating/editing ordinary synth tracks.
enum class FactoryPreset : int32_t {
    Subula=0,
    Spectrello=1,
    Nebular=2,
    Nexdrum=3,
    SpectrachordInit=4,
    Count=4
};
Patch makeDefaultPatch(const char* name="Spectrachord Init");
Patch makeFactoryPatch(FactoryPreset preset);
const std::array<Patch,static_cast<size_t>(FactoryPreset::Count)>& factoryBank();
} // namespace aiora
