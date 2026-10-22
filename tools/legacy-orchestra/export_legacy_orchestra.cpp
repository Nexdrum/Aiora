// Export from the unmodified 9a4853e8 factory revision, not current factory.
#include "FactoryPresets.h"
#include "ProjectCodec.h"
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace aiora;
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    const std::filesystem::path dir(argv[1]);
    std::filesystem::create_directories(dir);
    struct Entry {FactoryPreset preset;const char* filename;};
    const std::array<Entry,19> entries{{
        {FactoryPreset::Violin,"Violin.patch.json"},
        {FactoryPreset::Viola,"Viola.patch.json"},
        {FactoryPreset::Cello,"Cello.patch.json"},
        {FactoryPreset::Contrabass,"Contrabass.patch.json"},
        {FactoryPreset::Flute,"Flute.patch.json"},
        {FactoryPreset::Clarinet,"Clarinet.patch.json"},
        {FactoryPreset::Oboe,"Oboe.patch.json"},
        {FactoryPreset::Bassoon,"Bassoon.patch.json"},
        {FactoryPreset::FrenchHorn,"French_Horn.patch.json"},
        {FactoryPreset::Trumpet,"Trumpet.patch.json"},
        {FactoryPreset::Trombone,"Trombone.patch.json"},
        {FactoryPreset::Tuba,"Tuba.patch.json"},
        {FactoryPreset::Timpani,"Timpani.patch.json"},
        {FactoryPreset::ConcertBassDrum,"Concert_Bass_Drum.patch.json"},
        {FactoryPreset::ConcertSnareDrum,"Concert_Snare_Drum.patch.json"},
        {FactoryPreset::OrchestralCymbals,"Orchestral_Cymbals.patch.json"},
        {FactoryPreset::TamTamGong,"Tam_Tam_Gong.patch.json"},
        {FactoryPreset::Triangle,"Triangle.patch.json"},
        {FactoryPreset::Marimba,"Marimba.patch.json"}
    }};
    for(const auto& e:entries){
        const std::string json=serializePatchJson(makeFactoryPatch(e.preset));
        Patch check;std::string error;
        if(!deserializePatchJson(json,check,&error))
            throw std::runtime_error(std::string(e.filename)+": "+error);
        std::ofstream file(dir/e.filename,std::ios::binary);
        file<<json;
        if(!file)throw std::runtime_error(std::string("Failed: ")+e.filename);
        std::cout << e.filename << " " << json.size() << " bytes\n";
    }
}
