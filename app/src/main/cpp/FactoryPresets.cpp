#include "FactoryPresets.h"
#include "ApprovedFactoryPatches.h"
#include "SpectrelloFactoryV2.h"
#include "ProjectCodec.h"
#include <stdexcept>
#include <string_view>

namespace aiora {
Patch makeDefaultPatch(const char* name){
    Patch p;
    p.name=name?name:"Spectrachord Init";
    for(auto& op:p.ops){
        op=Operator{};
        op.enabled=false;
        op.level=0;
        op.volumeCurve=initialOperatorVolume();
        for(auto& channel:op.modChannels)channel.enabled=false;
        op.modChannelsConfigured=true;
    }
    p.ops[0].enabled=true;
    p.ops[0].level=.9f;
    p.ops[0].wave=Wave::Sine;
    p.filter.evolving=initialEvolvingFilter();
    p.masterAmpCurve=initialMasterAmpCurve();
    p.fx={};
    p.fx.count=0;p.fx.rackConfigured=true;
    p.unison=0;p.octave=0;p.volume=.8f;
    return p;
}
Patch makeFactoryPatch(FactoryPreset preset){
    const char* name="Spectrachord Init";
    switch(preset){
        case FactoryPreset::Subula:name="Subula";break;
        case FactoryPreset::Spectrello:name="Spectrello";break;
        case FactoryPreset::Nebular:name="Nebular";break;
        case FactoryPreset::Nexdrum:name="Nexdrum";break;
        default:break;
    }
    if(preset==FactoryPreset::Subula||
       preset==FactoryPreset::Spectrello||
       preset==FactoryPreset::Nebular){
        const std::string_view source=preset==FactoryPreset::Nebular?
            factory_patch_data::nebularSteelPhysicsV2:
            (preset==FactoryPreset::Subula?
                factory_patch_data::subulaMetalFretlessV4:
                factory_patch_data::spectrelloCarbonFiberV2);
        Patch result;
        std::string error;
        if(!deserializePatchJson(source,result,&error))
            throw std::runtime_error("Invalid approved factory patch: "+error);
        result.name=name;
        return result;
    }
    Patch p=makeDefaultPatch(name);
    if(preset==FactoryPreset::Nexdrum){
        p.nexdrumLow=40;p.nexdrumHigh=60;
    }
    return p;
}
const std::array<Patch,static_cast<size_t>(FactoryPreset::Count)>& factoryBank(){
    static const std::array<Patch,static_cast<size_t>(FactoryPreset::Count)> bank{
        makeFactoryPatch(FactoryPreset::Subula),
        makeFactoryPatch(FactoryPreset::Spectrello),
        makeFactoryPatch(FactoryPreset::Nebular),
        makeFactoryPatch(FactoryPreset::Nexdrum)
    };
    return bank;
}
} // namespace aiora
