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
    Violin = 5,
    Viola = 6,
    Cello = 7,
    Contrabass = 8,
    Flute = 9,
    Clarinet = 10,
    Oboe = 11,
    Bassoon = 12,
    FrenchHorn = 13,
    Trumpet = 14,
    Trombone = 15,
    Tuba = 16,
    Timpani = 17,
    Count = 18
};

Patch makeDefaultPatch(const char* name = "Spectrachord Init");
Patch makeFactoryPatch(FactoryPreset preset);
const std::array<Patch, static_cast<size_t>(FactoryPreset::Count)>& factoryBank();

} // namespace aiora
