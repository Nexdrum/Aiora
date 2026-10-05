#include "ProjectCore.h"
#include "FactoryPresets.h"
#include "NexdrumKit.h"
#include <algorithm>
#include <array>

namespace aiora {
namespace {
constexpr int kMaxTracks = 10;
constexpr int kGridLow = 38;
constexpr int kGridHigh = 86;
}

ProjectCore& ProjectCore::instance() {
    static ProjectCore core;
    return core;
}

std::string ProjectCore::autoName(int index) {
    static const std::array<const char*,5> names{"Lead / Keys","Bass","Pad / Strings","Arp / FX","Drums"};
    if(index >= 0 && index < static_cast<int>(names.size())) return names[static_cast<size_t>(index)];
    return "Track " + std::to_string(index + 1);
}

bool ProjectCore::validTrack(int index) const noexcept {
    return index >= 0 && index < static_cast<int>(project_.tracks.size());
}
bool ProjectCore::validPad(int trackIndex,int padIndex) const noexcept {
    return validTrack(trackIndex) && padIndex >= 0 && padIndex < static_cast<int>(project_.tracks[trackIndex].pads.size());
}

void ProjectCore::reset() {
    std::scoped_lock lock(mutex_);
    project_ = {};
    selectedTrack_ = -1;
}

int ProjectCore::addTrack(bool drums) {
    std::scoped_lock lock(mutex_);
    if(static_cast<int>(project_.tracks.size()) >= kMaxTracks) return -1;
    const int index = static_cast<int>(project_.tracks.size());
    Track t;
    t.name = autoName(index);
    t.drums = drums;
    t.patch = makeFactoryPatch(FactoryPreset::SpectrachordInit);
    if(drums) {
        t.patch = makeFactoryPatch(FactoryPreset::Nexdrum);
        const auto& kit=nexdrumKit();
        t.pads.assign(kit.begin(),kit.end());
        t.selectedPad=6;
    }
    project_.tracks.push_back(std::move(t));
    selectedTrack_ = index;
    return index;
}

bool ProjectCore::deleteTrack(int index) {
    std::scoped_lock lock(mutex_);
    if(!validTrack(index)) return false;
    project_.tracks.erase(project_.tracks.begin()+index);
    if(project_.tracks.empty()) selectedTrack_=-1;
    else selectedTrack_=std::clamp(index,0,static_cast<int>(project_.tracks.size())-1);
    return true;
}

bool ProjectCore::selectTrack(int index) {
    std::scoped_lock lock(mutex_);
    if(!validTrack(index)) return false;
    selectedTrack_=index;return true;
}
int ProjectCore::selectedTrack() const { std::scoped_lock lock(mutex_); return selectedTrack_; }
int ProjectCore::trackCount() const { std::scoped_lock lock(mutex_); return static_cast<int>(project_.tracks.size()); }

std::string ProjectCore::trackName(int index) const {
    std::scoped_lock lock(mutex_);return validTrack(index)?project_.tracks[index].name:std::string{};
}
void ProjectCore::setTrackName(int index,const std::string& name) {
    std::scoped_lock lock(mutex_);if(!validTrack(index))return;
    std::string n=name.substr(0,24);project_.tracks[index].name=n.empty()?autoName(index):n;
}
bool ProjectCore::trackIsDrums(int index) const { std::scoped_lock lock(mutex_);return validTrack(index)&&project_.tracks[index].drums; }
bool ProjectCore::trackMute(int index) const { std::scoped_lock lock(mutex_);return validTrack(index)&&project_.tracks[index].mute; }
bool ProjectCore::trackSolo(int index) const { std::scoped_lock lock(mutex_);return validTrack(index)&&project_.tracks[index].solo; }
float ProjectCore::trackVolume(int index) const { std::scoped_lock lock(mutex_);return validTrack(index)?project_.tracks[index].volume:0.0f; }
float ProjectCore::trackPan(int index) const { std::scoped_lock lock(mutex_);return validTrack(index)?project_.tracks[index].pan:0.0f; }
void ProjectCore::setTrackMute(int index,bool value){std::scoped_lock lock(mutex_);if(validTrack(index))project_.tracks[index].mute=value;}
void ProjectCore::setTrackSolo(int index,bool value){std::scoped_lock lock(mutex_);if(validTrack(index))project_.tracks[index].solo=value;}
void ProjectCore::setTrackVolume(int index,float value){std::scoped_lock lock(mutex_);if(validTrack(index))project_.tracks[index].volume=std::clamp(value,0.0f,1.0f);}
void ProjectCore::setTrackPan(int index,float value){std::scoped_lock lock(mutex_);if(validTrack(index))project_.tracks[index].pan=std::clamp(value,-1.0f,1.0f);}

bool ProjectCore::loadNexdrumKit(int trackIndex) {
    std::scoped_lock lock(mutex_);
    if(!validTrack(trackIndex))return false;
    auto& t=project_.tracks[trackIndex];
    t.drums=true;t.patch=makeFactoryPatch(FactoryPreset::Nexdrum);
    const auto& kit=nexdrumKit();t.pads.assign(kit.begin(),kit.end());t.selectedPad=6;
    return true;
}
int ProjectCore::padCount(int trackIndex) const {std::scoped_lock lock(mutex_);return validTrack(trackIndex)?static_cast<int>(project_.tracks[trackIndex].pads.size()):0;}
int ProjectCore::selectedPad(int trackIndex) const {std::scoped_lock lock(mutex_);return validTrack(trackIndex)?project_.tracks[trackIndex].selectedPad:-1;}
bool ProjectCore::selectPad(int trackIndex,int padIndex){std::scoped_lock lock(mutex_);if(!validPad(trackIndex,padIndex))return false;project_.tracks[trackIndex].selectedPad=padIndex;return true;}
std::string ProjectCore::padIcon(int t,int p) const {std::scoped_lock lock(mutex_);return validPad(t,p)?project_.tracks[t].pads[p].icon:std::string{};}
std::string ProjectCore::padPatchName(int t,int p) const {std::scoped_lock lock(mutex_);return validPad(t,p)?project_.tracks[t].pads[p].patch.name:std::string{};}
int ProjectCore::padCenter(int t,int p) const {std::scoped_lock lock(mutex_);return validPad(t,p)?project_.tracks[t].pads[p].centerMidi:-1;}
int ProjectCore::padLow(int t,int p) const {std::scoped_lock lock(mutex_);return validPad(t,p)?project_.tracks[t].pads[p].lowMidi:-1;}
int ProjectCore::padHigh(int t,int p) const {std::scoped_lock lock(mutex_);return validPad(t,p)?project_.tracks[t].pads[p].highMidi:-1;}
float ProjectCore::padVolume(int t,int p) const {std::scoped_lock lock(mutex_);return validPad(t,p)?project_.tracks[t].pads[p].volume:0.0f;}
float ProjectCore::padPan(int t,int p) const {std::scoped_lock lock(mutex_);return validPad(t,p)?project_.tracks[t].pads[p].pan:0.0f;}
void ProjectCore::setPadVolume(int t,int p,float v){std::scoped_lock lock(mutex_);if(validPad(t,p))project_.tracks[t].pads[p].volume=std::clamp(v,0.0f,1.0f);}
void ProjectCore::setPadPan(int t,int p,float v){std::scoped_lock lock(mutex_);if(validPad(t,p))project_.tracks[t].pads[p].pan=std::clamp(v,-1.0f,1.0f);}

bool ProjectCore::rangeFree(int trackIndex,int padIndex,int low,int high) const noexcept {
    if(!validTrack(trackIndex))return false;
    const auto& pads=project_.tracks[trackIndex].pads;
    for(int i=0;i<static_cast<int>(pads.size());++i){
        if(i==padIndex)continue;
        const int a=std::min(pads[i].lowMidi,pads[i].highMidi),b=std::max(pads[i].lowMidi,pads[i].highMidi);
        if(!(high<a||low>b))return false;
    }
    return true;
}
bool ProjectCore::setPadRange(int t,int p,int low,int high){
    std::scoped_lock lock(mutex_);if(!validPad(t,p))return false;
    low=std::clamp(low,kGridLow,kGridHigh);high=std::clamp(high,kGridLow,kGridHigh);if(low>high)std::swap(low,high);
    if(!rangeFree(t,p,low,high))return false;
    auto& pad=project_.tracks[t].pads[p];pad.lowMidi=low;pad.highMidi=high;pad.centerMidi=std::clamp(pad.centerMidi,low,high);pad.patch.fundamentalMidi=pad.centerMidi;return true;
}

float ProjectCore::bpm() const {std::scoped_lock lock(mutex_);return project_.bpm;}
int ProjectCore::beats() const {std::scoped_lock lock(mutex_);return project_.beats;}
int ProjectCore::divisions() const {std::scoped_lock lock(mutex_);return project_.divisions;}
bool ProjectCore::dozenal() const {std::scoped_lock lock(mutex_);return project_.dozenal;}
float ProjectCore::masterVolume() const {std::scoped_lock lock(mutex_);return project_.masterVolume;}
float ProjectCore::masterReverb() const {std::scoped_lock lock(mutex_);return project_.masterReverb;}
void ProjectCore::setBpm(float v){std::scoped_lock lock(mutex_);project_.bpm=std::clamp(v,12.0f,288.0f);}
void ProjectCore::setSignature(int b,int d){std::scoped_lock lock(mutex_);project_.beats=std::clamp(b,1,12);project_.divisions=std::clamp(d,1,12);}
void ProjectCore::setDozenal(bool e){std::scoped_lock lock(mutex_);project_.dozenal=e;}
void ProjectCore::setMasterVolume(float v){std::scoped_lock lock(mutex_);project_.masterVolume=std::clamp(v,0.0f,1.0f);}
void ProjectCore::setMasterReverb(float v){std::scoped_lock lock(mutex_);project_.masterReverb=std::clamp(v,0.0f,1.0f);}

DspPatch ProjectCore::selectedDspPatch() const {
    std::scoped_lock lock(mutex_);
    if(!validTrack(selectedTrack_))return toDspPatch(makeFactoryPatch(FactoryPreset::SpectrachordInit));
    const auto& t=project_.tracks[selectedTrack_];
    if(t.drums&&validPad(selectedTrack_,t.selectedPad))return toDspPatch(t.pads[t.selectedPad].patch);
    return toDspPatch(t.patch);
}
Fx ProjectCore::selectedFx() const {
    std::scoped_lock lock(mutex_);
    if(!validTrack(selectedTrack_))return makeFactoryPatch(FactoryPreset::SpectrachordInit).fx;
    const auto& t=project_.tracks[selectedTrack_];
    if(t.drums&&validPad(selectedTrack_,t.selectedPad))return t.pads[t.selectedPad].patch.fx;
    return t.patch.fx;
}
DspPatch ProjectCore::padDspPatch(int t,int p) const {std::scoped_lock lock(mutex_);return validPad(t,p)?toDspPatch(project_.tracks[t].pads[p].patch):toDspPatch(makeFactoryPatch(FactoryPreset::SpectrachordInit));}
Fx ProjectCore::padFx(int t,int p) const {std::scoped_lock lock(mutex_);return validPad(t,p)?project_.tracks[t].pads[p].patch.fx:Fx{};}

} // namespace aiora
