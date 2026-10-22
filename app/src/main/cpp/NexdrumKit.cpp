#include "NexdrumKit.h"
#include "ApprovedNexdrumKit.h"
#include "ProjectCodec.h"
#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

namespace aiora {
const std::array<DrumPad,7>& nexdrumKit(){
    // Decode the exact approved audition kit, using the same path as Import Kit.
    // No re-authoring/approximation of the seven synth patches.
    static const std::array<DrumPad,7> kit=[] {
        std::vector<DrumPad> decoded;
        std::string error;
        if(!deserializeDrumKitJson(factory_kit_data::kNexdrumApprovedKitV12_1,
                                   decoded,&error) || decoded.size()!=7)
            throw std::runtime_error("Invalid approved Nexdrum kit: "+error);
        std::array<DrumPad,7> output{};
        std::copy(decoded.begin(),decoded.end(),output.begin());
        return output;
    }();
    return kit;
}
} // namespace aiora
