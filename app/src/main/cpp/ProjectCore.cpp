#include "ProjectCore.h"
#include "FactoryPresets.h"
#include "NexdrumKit.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace aiora {
namespace {
constexpr int kMaxTracks = 10;
constexpr int kGridLow = 38;
constexpr int kGridHigh = 86;
constexpr int kRollLow = 14;
constexpr int kRollHigh = 110;
constexpr int kMaxCurvePoints = 24;

std::vector<CurvePoint>* curveFor(Note& n,int kind){
    if(kind==0)return &n.bend;
    if(kind==1)return &n.velocity;
    if(kind==2)return &n.mod;
    return nullptr;
}
const std::vector<CurvePoint>* curveFor(const Note& n,int kind){
    if(kind==0)return &n.bend;
    if(kind==1)return &n.velocity;
    if(kind==2)return &n.mod;
    return nullptr;
}
float clampCurveValue(int kind,float v){return kind==0?std::clamp(v,-12.0f,12.0f):std::clamp(v,0.0f,1.0f);}

float groupCurveValue(
    const std::vector<CurvePoint>& points,float step,float fallback) noexcept {
    if(points.empty())return fallback;
    if(points.size()==1||step<=points.front().step)return points.front().value;
    for(size_t i=1;i<points.size();++i){
        if(step<=points[i].step){
            const auto& a=points[i-1];
            const auto& b=points[i];
            const float t=(step-a.step)/std::max(1.0e-6f,b.step-a.step);
            return a.value+(b.value-a.value)*std::clamp(t,0.0f,1.0f);
        }
    }
    return points.back().value;
}

float clampOperatorParam(OperatorParam param,float v){
    switch(param){
        case OperatorParam::Ratio:return std::clamp(v,0.125f,8.0f);
        case OperatorParam::Semitone:return std::clamp(v,-12.0f,12.0f);
        case OperatorParam::Detune:return std::clamp(v,-50.0f,50.0f);
        case OperatorParam::Level:return std::clamp(v,0.0f,1.0f);
        case OperatorParam::Attack:return std::clamp(v,0.001f,1.0f);
        case OperatorParam::Decay:return std::clamp(v,0.005f,1.5f);
        case OperatorParam::Sustain:return std::clamp(v,0.0f,1.0f);
        case OperatorParam::Release:return std::clamp(v,0.02f,2.0f);
    }
    return v;
}

float clampPatchParam(PatchParam param,float v){
    switch(param){
        case PatchParam::FilterCutoff:return std::clamp(v,40.0f,18000.0f);
        case PatchParam::FilterResonance:return std::clamp(v,0.1f,18.0f);
        case PatchParam::FilterEnv:return std::clamp(v,0.0f,1.0f);
        case PatchParam::FilterAttack:return std::clamp(v,0.005f,1.0f);
        case PatchParam::FilterDecay:return std::clamp(v,0.01f,1.5f);
        case PatchParam::FilterSustain:return std::clamp(v,0.0f,1.0f);
        case PatchParam::FilterRelease:return std::clamp(v,0.02f,2.0f);
        case PatchParam::AmpAttack:return std::clamp(v,0.001f,1.0f);
        case PatchParam::AmpDecay:return std::clamp(v,0.01f,1.5f);
        case PatchParam::AmpSustain:return std::clamp(v,0.0f,1.0f);
        case PatchParam::AmpRelease:return std::clamp(v,0.02f,2.0f);
        case PatchParam::VelocityAmp:
        case PatchParam::VelocityFilter:
        case PatchParam::LfoAmount:
        case PatchParam::LfoVelocity:
        case PatchParam::Unison:
        case PatchParam::Glide:
        case PatchParam::Volume:
        case PatchParam::Distortion:
        case PatchParam::Delay:
        case PatchParam::Reverb:
            return std::clamp(v,0.0f,1.0f);
        case PatchParam::LfoRate:return std::clamp(v,0.1f,20.0f);
        case PatchParam::LfoAttack:return std::clamp(v,0.0f,2.0f);
        case PatchParam::DelayTime:return std::clamp(v,0.03f,1.0f);
        case PatchParam::DelayFeedback:return std::clamp(v,0.0f,0.85f);
    }
    return v;
}

float clampModValue(ModTarget target,float v){
    switch(target){
        case ModTarget::Cutoff:return std::clamp(v,40.0f,18000.0f);
        case ModTarget::Resonance:return std::clamp(v,0.1f,18.0f);
        case ModTarget::FilterEnv:return std::clamp(v,0.0f,1.0f);
        case ModTarget::AmpAttack:return std::clamp(v,0.001f,1.0f);
        case ModTarget::AmpDecay:return std::clamp(v,0.01f,1.5f);
        case ModTarget::AmpSustain:return std::clamp(v,0.0f,1.0f);
        case ModTarget::AmpRelease:return std::clamp(v,0.02f,2.0f);
        case ModTarget::FilterAttack:return std::clamp(v,0.005f,1.0f);
        case ModTarget::FilterDecay:return std::clamp(v,0.01f,1.5f);
        case ModTarget::FilterSustain:return std::clamp(v,0.0f,1.0f);
        case ModTarget::FilterRelease:return std::clamp(v,0.02f,2.0f);
        case ModTarget::Op1: case ModTarget::Op2: case ModTarget::Op3:
        case ModTarget::Op4: case ModTarget::Op5: case ModTarget::Op6:
        case ModTarget::Morph1: case ModTarget::Morph2: case ModTarget::Morph3:
        case ModTarget::Morph4: case ModTarget::Morph5: case ModTarget::Morph6:
        case ModTarget::Unison:
            return std::clamp(v,0.0f,1.0f);
        case ModTarget::Fm:return std::clamp(v,0.0f,1.5f);
        case ModTarget::LfoAmount:return std::clamp(v,0.0f,2.0f);
        case ModTarget::LfoRate:return std::clamp(v,0.1f,20.0f);
        case ModTarget::Volume:return std::clamp(v,0.0f,2.0f);
        case ModTarget::None:return 0.0f;
    }
    return v;
}

std::array<float,16> defaultHarmonics(){
    return {1.0f,0.5f,0.33f,0.25f,0.2f,0.16f,0.14f,0.12f,0.1f,0.09f,0.08f,0.07f,0.06f,0.05f,0.04f,0.03f};
}
}

ProjectCore& ProjectCore::instance() {
    static ProjectCore core;
    return core;
}

std::string ProjectCore::autoName(int) {
    return "Spectrachord";
}

bool ProjectCore::validTrack(int index) const noexcept {return index >= 0 && index < static_cast<int>(project_.tracks.size());}
bool ProjectCore::validPad(int trackIndex,int padIndex) const noexcept {return validTrack(trackIndex) && padIndex >= 0 && padIndex < static_cast<int>(project_.tracks[trackIndex].pads.size());}
bool ProjectCore::validNote(int trackIndex,int noteIndex) const noexcept {return validTrack(trackIndex)&&noteIndex>=0&&noteIndex<static_cast<int>(project_.tracks[trackIndex].notes.size());}
bool ProjectCore::pitchAllowed(int trackIndex,int midi) const noexcept {
    if(!validTrack(trackIndex)||midi<kRollLow||midi>kRollHigh)return false;
    const auto& t=project_.tracks[trackIndex];
    if(!t.drums||t.pads.empty())return true;
    for(const auto& p:t.pads){const int lo=std::min(p.lowMidi,p.highMidi),hi=std::max(p.lowMidi,p.highMidi);if(midi>=lo&&midi<=hi)return true;}
    return false;
}

Patch* ProjectCore::selectedPatchUnsafe() noexcept {
    if(!validTrack(selectedTrack_))return nullptr;
    auto& t=project_.tracks[selectedTrack_];
    if(t.drums&&validPad(selectedTrack_,t.selectedPad))return &t.pads[t.selectedPad].patch;
    return &t.patch;
}
const Patch* ProjectCore::selectedPatchUnsafe() const noexcept {
    if(!validTrack(selectedTrack_))return nullptr;
    const auto& t=project_.tracks[selectedTrack_];
    if(t.drums&&validPad(selectedTrack_,t.selectedPad))return &t.pads[t.selectedPad].patch;
    return &t.patch;
}

void ProjectCore::reset() {std::scoped_lock lock(mutex_);project_ = {};selectedTrack_ = -1;}
Project ProjectCore::projectCopy() const {std::scoped_lock lock(mutex_);return project_;}
bool ProjectCore::replaceProject(Project project,int selectedTrack){
    std::scoped_lock lock(mutex_);
    if(project.tracks.size()>static_cast<size_t>(kMaxTracks))project.tracks.resize(kMaxTracks);
    project_=std::move(project);
    if(project_.tracks.empty())selectedTrack_=-1;
    else selectedTrack_=std::clamp(selectedTrack,0,static_cast<int>(project_.tracks.size())-1);
    return true;
}
bool ProjectCore::replaceSelectedPatch(Patch patch){
    std::scoped_lock lock(mutex_);
    auto* target=selectedPatchUnsafe();if(!target)return false;
    if(validTrack(selectedTrack_)&&project_.tracks[selectedTrack_].drums&&validPad(selectedTrack_,project_.tracks[selectedTrack_].selectedPad)){
        patch.fundamentalMidi=project_.tracks[selectedTrack_].pads[project_.tracks[selectedTrack_].selectedPad].centerMidi;
    }
    *target=std::move(patch);return true;
}

int ProjectCore::addTrack(bool drums) {
    std::scoped_lock lock(mutex_);
    if(static_cast<int>(project_.tracks.size()) >= kMaxTracks) return -1;
    const int index = static_cast<int>(project_.tracks.size());
    Track t;
    t.name="Spectrachord";
    t.drums=drums;
    t.patch=makeFactoryPatch(FactoryPreset::SpectrachordInit);
    if(drums){
        t.name="Nexdrum";
        t.patch=makeFactoryPatch(FactoryPreset::Nexdrum);
        const auto& kit=nexdrumKit();
        t.pads.assign(kit.begin(),kit.end());
        t.selectedPad=std::clamp(6,0,std::max(0,static_cast<int>(t.pads.size())-1));
    }
    project_.tracks.push_back(std::move(t));
    selectedTrack_=index;
    return index;
}

bool ProjectCore::deleteTrack(int index) {
    std::scoped_lock lock(mutex_);if(!validTrack(index)) return false;
    project_.tracks.erase(project_.tracks.begin()+index);
    if(project_.tracks.empty()) selectedTrack_=-1;else selectedTrack_=std::clamp(index,0,static_cast<int>(project_.tracks.size())-1);
    return true;
}
bool ProjectCore::reorderTrack(int fromIndex,int toIndex){
    std::scoped_lock lock(mutex_);
    if(!validTrack(fromIndex)||!validTrack(toIndex))return false;
    if(fromIndex==toIndex)return true;
    auto moving=std::move(project_.tracks[static_cast<size_t>(fromIndex)]);
    project_.tracks.erase(project_.tracks.begin()+fromIndex);
    project_.tracks.insert(project_.tracks.begin()+toIndex,std::move(moving));
    if(selectedTrack_==fromIndex)selectedTrack_=toIndex;
    else if(fromIndex<selectedTrack_&&selectedTrack_<=toIndex)--selectedTrack_;
    else if(toIndex<=selectedTrack_&&selectedTrack_<fromIndex)++selectedTrack_;
    return true;
}
bool ProjectCore::selectTrack(int index) {std::scoped_lock lock(mutex_);if(!validTrack(index)) return false;selectedTrack_=index;return true;}
int ProjectCore::selectedTrack() const { std::scoped_lock lock(mutex_); return selectedTrack_; }
int ProjectCore::trackCount() const { std::scoped_lock lock(mutex_); return static_cast<int>(project_.tracks.size()); }

std::string ProjectCore::trackName(int index) const {std::scoped_lock lock(mutex_);return validTrack(index)?project_.tracks[index].name:std::string{};}
std::string ProjectCore::trackPatchName(int index) const {
    std::scoped_lock lock(mutex_);
    if(!validTrack(index))return {};
    const auto& t=project_.tracks[index];
    if(t.drums)return t.patch.name.empty()?std::string("Nexdrum"):t.patch.name;
    return t.patch.name;
}
void ProjectCore::setTrackName(int index,const std::string& name) {std::scoped_lock lock(mutex_);if(!validTrack(index))return;std::string n=name.substr(0,24);project_.tracks[index].name=n.empty()?autoName(index):n;}
void ProjectCore::clearTrackNotes(int index){std::scoped_lock lock(mutex_);if(validTrack(index))project_.tracks[index].notes.clear();}
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
    std::scoped_lock lock(mutex_);if(!validTrack(trackIndex))return false;
    auto& t=project_.tracks[trackIndex];t.drums=true;t.patch=makeFactoryPatch(FactoryPreset::Nexdrum);
    const auto& kit=nexdrumKit();t.pads.assign(kit.begin(),kit.end());
    for(size_t i=0;i<t.pads.size();++i)if(t.pads[i].id.empty())t.pads[i].id="pad"+std::to_string(i+1);
    t.selectedPad=std::clamp(6,0,std::max(0,static_cast<int>(t.pads.size())-1));return true;
}
bool ProjectCore::convertTrackToDrumKit(int trackIndex){
    std::scoped_lock lock(mutex_);
    if(!validTrack(trackIndex))return false;
    auto& t=project_.tracks[trackIndex];
    if(t.drums)return true;
    constexpr int centerMidi=62; // D4, AIORA's pitch center.
    DrumPad pad;
    pad.centerMidi=centerMidi;
    pad.lowMidi=centerMidi;
    pad.highMidi=centerMidi;
    pad.icon="kick";
    pad.name="New Pad";
    pad.id="pad1";
    pad.patch=t.patch;
    pad.patch.fundamentalMidi=centerMidi;
    t.pads.clear();
    t.pads.push_back(std::move(pad));
    t.selectedPad=0;
    t.drums=true;
    return true;
}
int ProjectCore::addDrumPad(int trackIndex){
    std::scoped_lock lock(mutex_);
    if(!validTrack(trackIndex))return -1;
    auto& t=project_.tracks[trackIndex];
    if(!t.drums||t.pads.size()>=24)return -1;

    std::array<bool,kGridHigh-kGridLow+1> taken{};
    for(const auto& p:t.pads){
        const int lo=std::max(kGridLow,std::min(p.lowMidi,p.highMidi));
        const int hi=std::min(kGridHigh,std::max(p.lowMidi,p.highMidi));
        for(int m=lo;m<=hi;++m)
            taken[static_cast<size_t>(m-kGridLow)]=true;
    }

    constexpr int centerMidi=62; // D4.
    int midi=-1;
    for(int m=centerMidi;m<=kGridHigh;++m){
        if(!taken[static_cast<size_t>(m-kGridLow)]){midi=m;break;}
    }
    if(midi<0){
        for(int m=centerMidi-1;m>=kGridLow;--m){
            if(!taken[static_cast<size_t>(m-kGridLow)]){midi=m;break;}
        }
    }
    if(midi<0)return -1;

    DrumPad p;
    p.centerMidi=midi;
    p.lowMidi=midi;
    p.highMidi=midi;
    p.icon="kick";
    p.name="New Pad";
    p.patch=makeFactoryPatch(FactoryPreset::SpectrachordInit);
    p.patch.fundamentalMidi=midi;

    int idNumber=1;
    for(;;++idNumber){
        const std::string candidate="pad"+std::to_string(idNumber);
        const bool used=std::any_of(
            t.pads.begin(),t.pads.end(),
            [&](const DrumPad& existing){return existing.id==candidate;});
        if(!used){p.id=candidate;break;}
    }

    t.pads.push_back(std::move(p));
    t.selectedPad=static_cast<int>(t.pads.size())-1;
    return t.selectedPad;
}
bool ProjectCore::deleteDrumPad(int trackIndex,int padIndex){
    std::scoped_lock lock(mutex_);
    if(!validPad(trackIndex,padIndex))return false;
    auto& t=project_.tracks[trackIndex];

    if(t.pads.size()==1){
        // Deleting the final pad unwraps its complete sound back into a normal
        // Spectrachord track instead of destroying the sound.
        const auto finalPad=t.pads.front();
        t.patch=finalPad.patch;
        t.volume=std::clamp(t.volume*finalPad.volume,0.0f,1.0f);
        t.pan=std::clamp(t.pan+finalPad.pan,-1.0f,1.0f);
        t.pads.clear();
        t.selectedPad=0;
        t.drums=false;
        return true;
    }

    const auto& p=t.pads[static_cast<size_t>(padIndex)];
    const int lo=std::min(p.lowMidi,p.highMidi);
    const int hi=std::max(p.lowMidi,p.highMidi);
    t.notes.erase(
        std::remove_if(
            t.notes.begin(),t.notes.end(),
            [&](const Note& n){return n.midi>=lo&&n.midi<=hi;}),
        t.notes.end());
    t.pads.erase(t.pads.begin()+padIndex);
    t.selectedPad=std::clamp(
        padIndex,0,static_cast<int>(t.pads.size())-1);
    return true;
}
bool ProjectCore::reorderDrumPad(int trackIndex,int fromIndex,int toIndex){
    std::scoped_lock lock(mutex_);
    if(!validPad(trackIndex,fromIndex)||!validPad(trackIndex,toIndex))
        return false;
    if(fromIndex==toIndex)return true;
    auto& t=project_.tracks[trackIndex];
    auto moving=std::move(t.pads[static_cast<size_t>(fromIndex)]);
    t.pads.erase(t.pads.begin()+fromIndex);
    t.pads.insert(t.pads.begin()+toIndex,std::move(moving));
    if(t.selectedPad==fromIndex)t.selectedPad=toIndex;
    else if(fromIndex<t.selectedPad&&t.selectedPad<=toIndex)--t.selectedPad;
    else if(toIndex<=t.selectedPad&&t.selectedPad<fromIndex)++t.selectedPad;
    return true;
}
bool ProjectCore::setPadCenter(int trackIndex,int padIndex,int midi){
    std::scoped_lock lock(mutex_);if(!validPad(trackIndex,padIndex))return false;
    midi=std::clamp(midi,kGridLow,kGridHigh);if(!rangeFree(trackIndex,padIndex,midi,midi))return false;
    auto& pad=project_.tracks[trackIndex].pads[padIndex];const int old=pad.centerMidi;
    pad.centerMidi=midi;pad.lowMidi=midi;pad.highMidi=midi;pad.patch.fundamentalMidi=midi;
    for(auto& n:project_.tracks[trackIndex].notes)if(n.midi==old)n.midi=midi;
    return true;
}
bool ProjectCore::setPadIcon(int trackIndex,int padIndex,const std::string& icon){
    std::scoped_lock lock(mutex_);if(!validPad(trackIndex,padIndex)||icon.empty())return false;
    project_.tracks[trackIndex].pads[padIndex].icon=icon.substr(0,24);return true;
}
int ProjectCore::padCount(int trackIndex) const {std::scoped_lock lock(mutex_);return validTrack(trackIndex)?static_cast<int>(project_.tracks[trackIndex].pads.size()):0;}
int ProjectCore::selectedPad(int trackIndex) const {std::scoped_lock lock(mutex_);return validTrack(trackIndex)?project_.tracks[trackIndex].selectedPad:-1;}
bool ProjectCore::selectPad(int trackIndex,int padIndex){std::scoped_lock lock(mutex_);if(!validPad(trackIndex,padIndex))return false;project_.tracks[trackIndex].selectedPad=padIndex;return true;}
std::string ProjectCore::padIcon(int t,int p) const {std::scoped_lock lock(mutex_);return validPad(t,p)?project_.tracks[t].pads[p].icon:std::string{};}
std::string ProjectCore::padName(int t,int p) const {std::scoped_lock lock(mutex_);return validPad(t,p)?project_.tracks[t].pads[p].name:std::string{};}
void ProjectCore::setPadName(int t,int p,const std::string& name){std::scoped_lock lock(mutex_);if(!validPad(t,p))return;project_.tracks[t].pads[p].name=name.substr(0,24);}
std::string ProjectCore::padPatchName(int t,int p) const {std::scoped_lock lock(mutex_);return validPad(t,p)?project_.tracks[t].pads[p].patch.name:std::string{};}
int ProjectCore::padCenter(int t,int p) const {std::scoped_lock lock(mutex_);return validPad(t,p)?project_.tracks[t].pads[p].centerMidi:-1;}
int ProjectCore::padLow(int t,int p) const {std::scoped_lock lock(mutex_);return validPad(t,p)?project_.tracks[t].pads[p].lowMidi:-1;}
int ProjectCore::padHigh(int t,int p) const {std::scoped_lock lock(mutex_);return validPad(t,p)?project_.tracks[t].pads[p].highMidi:-1;}
float ProjectCore::padVolume(int t,int p) const {std::scoped_lock lock(mutex_);return validPad(t,p)?project_.tracks[t].pads[p].volume:0.0f;}
float ProjectCore::padPan(int t,int p) const {std::scoped_lock lock(mutex_);return validPad(t,p)?project_.tracks[t].pads[p].pan:0.0f;}
void ProjectCore::setPadVolume(int t,int p,float v){std::scoped_lock lock(mutex_);if(validPad(t,p))project_.tracks[t].pads[p].volume=std::clamp(v,0.0f,1.0f);}
void ProjectCore::setPadPan(int t,int p,float v){std::scoped_lock lock(mutex_);if(validPad(t,p))project_.tracks[t].pads[p].pan=std::clamp(v,-1.0f,1.0f);}

bool ProjectCore::rangeFree(int trackIndex,int padIndex,int low,int high) const noexcept {
    if(!validTrack(trackIndex))return false;const auto& pads=project_.tracks[trackIndex].pads;
    for(int i=0;i<static_cast<int>(pads.size());++i){if(i==padIndex)continue;const int a=std::min(pads[i].lowMidi,pads[i].highMidi),b=std::max(pads[i].lowMidi,pads[i].highMidi);if(!(high<a||low>b))return false;}return true;
}
bool ProjectCore::setPadRange(int t,int p,int low,int high){
    std::scoped_lock lock(mutex_);if(!validPad(t,p))return false;
    low=std::clamp(low,kGridLow,kGridHigh);high=std::clamp(high,kGridLow,kGridHigh);if(low>high)std::swap(low,high);if(!rangeFree(t,p,low,high))return false;
    auto& pad=project_.tracks[t].pads[p];pad.lowMidi=low;pad.highMidi=high;pad.centerMidi=std::clamp(pad.centerMidi,low,high);pad.patch.fundamentalMidi=pad.centerMidi;return true;
}

int ProjectCore::noteCount(int t) const {std::scoped_lock lock(mutex_);return validTrack(t)?static_cast<int>(project_.tracks[t].notes.size()):0;}
int ProjectCore::addNote(int t,int midi,float start,float length){
    std::scoped_lock lock(mutex_);if(!validTrack(t)||!pitchAllowed(t,midi))return -1;
    start=std::max(0.0f,start);length=std::max(1.0f,length);
    auto& notes=project_.tracks[t].notes;
    // Notes are intentionally polyphonic even on the same pitch and time.
    notes.push_back(Note{midi,start,length,{},{},{}});
    return static_cast<int>(notes.size())-1;
}
int ProjectCore::pasteNotes(int t,const std::vector<Note>& source,float start){
    std::scoped_lock lock(mutex_);
    if(!validTrack(t)||source.empty())return 0;
    start=std::max(0.0f,start);
    auto& notes=project_.tracks[t].notes;
    int added=0;
    for(const auto& src:source){
        if(!pitchAllowed(t,src.midi))continue;
        Note copy=src;
        copy.startStep=std::max(0.0f,start+src.startStep);
        copy.lengthSteps=std::max(1.0f,copy.lengthSteps);
        notes.push_back(std::move(copy));
        ++added;
    }
    return added;
}
std::vector<int> ProjectCore::duplicateNotes(
    int t,const std::vector<int>& noteIndices,float stepOffset){
    std::scoped_lock lock(mutex_);
    std::vector<int> added;
    if(!validTrack(t)||noteIndices.empty())return added;
    const auto& source=project_.tracks[t].notes;
    std::vector<Note> copies;
    copies.reserve(noteIndices.size());
    for(int index:noteIndices){
        if(index<0||index>=static_cast<int>(source.size()))continue;
        Note copy=source[static_cast<size_t>(index)];
        copy.startStep=std::max(0.0f,copy.startStep+stepOffset);
        if(!pitchAllowed(t,copy.midi))continue;
        copies.push_back(std::move(copy));
    }
    auto& notes=project_.tracks[t].notes;
    added.reserve(copies.size());
    for(auto& copy:copies){
        notes.push_back(std::move(copy));
        added.push_back(static_cast<int>(notes.size())-1);
    }
    return added;
}
int ProjectCore::deleteNotes(int t,const std::vector<int>& noteIndices){
    std::scoped_lock lock(mutex_);
    if(!validTrack(t)||noteIndices.empty())return 0;
    auto indices=noteIndices;
    std::sort(indices.begin(),indices.end());
    indices.erase(std::unique(indices.begin(),indices.end()),indices.end());
    std::sort(indices.rbegin(),indices.rend());
    auto& notes=project_.tracks[t].notes;
    int removed=0;
    for(int index:indices){
        if(index<0||index>=static_cast<int>(notes.size()))continue;
        notes.erase(notes.begin()+index);
        ++removed;
    }
    return removed;
}
int ProjectCore::deleteNotesInRange(int t,float startStep,float endStep){
    std::scoped_lock lock(mutex_);
    if(!validTrack(t))return 0;
    if(endStep<startStep)std::swap(startStep,endStep);
    auto& notes=project_.tracks[t].notes;
    const auto old=notes.size();
    notes.erase(
        std::remove_if(
            notes.begin(),notes.end(),
            [&](const Note& n){
                return n.startStep>=startStep&&n.startStep<endStep;
            }),
        notes.end());
    return static_cast<int>(old-notes.size());
}
bool ProjectCore::moveNotes(
    int t,const std::vector<int>& noteIndices,
    int midiDelta,float stepDelta){
    std::scoped_lock lock(mutex_);
    if(!validTrack(t)||noteIndices.empty())return false;
    auto indices=noteIndices;
    std::sort(indices.begin(),indices.end());
    indices.erase(std::unique(indices.begin(),indices.end()),indices.end());
    auto& notes=project_.tracks[t].notes;
    for(int index:indices){
        if(index<0||index>=static_cast<int>(notes.size()))return false;
        const auto& n=notes[static_cast<size_t>(index)];
        if(n.startStep+stepDelta<0.0f||
           !pitchAllowed(t,n.midi+midiDelta))
            return false;
    }
    for(int index:indices){
        auto& n=notes[static_cast<size_t>(index)];
        n.midi+=midiDelta;
        n.startStep+=stepDelta;
    }
    return true;
}
bool ProjectCore::applyGroupAutomation(
    int t,const std::vector<int>& noteIndices,int kind,
    float groupStart,const std::vector<CurvePoint>& groupPoints){
    std::scoped_lock lock(mutex_);
    if(!validTrack(t)||(kind!=1&&kind!=2)||
       noteIndices.empty()||groupPoints.empty())return false;

    auto points=groupPoints;
    for(auto& p:points){
        p.step=std::max(0.0f,p.step);
        p.value=clampCurveValue(kind,p.value);
    }
    std::sort(
        points.begin(),points.end(),
        [](const CurvePoint&a,const CurvePoint&b){return a.step<b.step;});

    auto indices=noteIndices;
    std::sort(indices.begin(),indices.end());
    indices.erase(std::unique(indices.begin(),indices.end()),indices.end());
    auto& notes=project_.tracks[t].notes;
    const float fallback=kind==1?1.0f:0.0f;
    bool changed=false;

    for(int index:indices){
        if(index<0||index>=static_cast<int>(notes.size()))continue;
        auto& note=notes[static_cast<size_t>(index)];
        auto* curve=curveFor(note,kind);
        if(!curve)continue;

        const float localEnd=
            std::max(0.0f,std::max(1.0f,note.lengthSteps)-1.0f);
        const float groupA=note.startStep-groupStart;
        const float groupB=groupA+localEnd;

        std::vector<CurvePoint> projected;
        projected.reserve(std::min<size_t>(points.size()+2,kMaxCurvePoints));
        projected.push_back({
            0.0f,
            std::clamp(groupCurveValue(points,groupA,fallback),0.0f,1.0f),
            true});

        if(localEnd>1.0e-5f){
            for(const auto& p:points){
                if(p.step<=groupA+1.0e-5f||p.step>=groupB-1.0e-5f)continue;
                if(projected.size()>=static_cast<size_t>(kMaxCurvePoints-1))break;
                projected.push_back({
                    p.step-groupA,std::clamp(p.value,0.0f,1.0f),true});
            }
            projected.push_back({
                localEnd,
                std::clamp(groupCurveValue(points,groupB,fallback),0.0f,1.0f),
                true});
        }

        *curve=std::move(projected);
        changed=true;
    }
    return changed;
}
bool ProjectCore::deleteNote(int t,int n){std::scoped_lock lock(mutex_);if(!validNote(t,n))return false;project_.tracks[t].notes.erase(project_.tracks[t].notes.begin()+n);return true;}
bool ProjectCore::updateNote(int t,int n,int midi,float start,float length){
    std::scoped_lock lock(mutex_);if(!validNote(t,n)||!pitchAllowed(t,midi))return false;
    auto& note=project_.tracks[t].notes[n];note.midi=midi;note.startStep=std::max(0.0f,start);note.lengthSteps=std::max(1.0f,length);return true;
}
int ProjectCore::noteMidi(int t,int n) const {std::scoped_lock lock(mutex_);return validNote(t,n)?project_.tracks[t].notes[n].midi:-1;}
float ProjectCore::noteStart(int t,int n) const {std::scoped_lock lock(mutex_);return validNote(t,n)?project_.tracks[t].notes[n].startStep:-1.0f;}
float ProjectCore::noteLength(int t,int n) const {std::scoped_lock lock(mutex_);return validNote(t,n)?project_.tracks[t].notes[n].lengthSteps:0.0f;}
int ProjectCore::curvePointCount(int t,int n,int kind) const {std::scoped_lock lock(mutex_);if(!validNote(t,n))return 0;const auto*c=curveFor(project_.tracks[t].notes[n],kind);return c?static_cast<int>(c->size()):0;}
float ProjectCore::curvePointStep(int t,int n,int kind,int p) const {std::scoped_lock lock(mutex_);if(!validNote(t,n))return 0;const auto*c=curveFor(project_.tracks[t].notes[n],kind);return c&&p>=0&&p<static_cast<int>(c->size())?(*c)[p].step:0;}
float ProjectCore::curvePointValue(int t,int n,int kind,int p) const {std::scoped_lock lock(mutex_);if(!validNote(t,n))return 0;const auto*c=curveFor(project_.tracks[t].notes[n],kind);return c&&p>=0&&p<static_cast<int>(c->size())?(*c)[p].value:0;}
bool ProjectCore::curvePointFree(int t,int n,int kind,int p) const {std::scoped_lock lock(mutex_);if(!validNote(t,n))return false;const auto*c=curveFor(project_.tracks[t].notes[n],kind);return c&&p>=0&&p<static_cast<int>(c->size())&&(*c)[p].free;}
int ProjectCore::addCurvePoint(int t,int n,int kind,float step,float value,bool free){
    std::scoped_lock lock(mutex_);if(!validNote(t,n))return -1;auto*c=curveFor(project_.tracks[t].notes[n],kind);if(!c||static_cast<int>(c->size())>=kMaxCurvePoints)return -1;
    const float maxStep=std::max(0.0f,project_.tracks[t].notes[n].lengthSteps-(kind==0?1.0f:0.5f));c->push_back({std::clamp(step,0.0f,maxStep),clampCurveValue(kind,value),free});
    std::sort(c->begin(),c->end(),[](const CurvePoint&a,const CurvePoint&b){return a.step<b.step;});
    for(int i=0;i<static_cast<int>(c->size());++i)if(std::fabs((*c)[i].step-std::clamp(step,0.0f,maxStep))<0.0001f&&(*c)[i].free==free)return i;return static_cast<int>(c->size())-1;
}
bool ProjectCore::updateCurvePoint(int t,int n,int kind,int p,float step,float value,bool free){
    std::scoped_lock lock(mutex_);if(!validNote(t,n))return false;auto*c=curveFor(project_.tracks[t].notes[n],kind);if(!c||p<0||p>=static_cast<int>(c->size()))return false;
    const float maxStep=std::max(0.0f,project_.tracks[t].notes[n].lengthSteps-(kind==0?1.0f:0.5f));(*c)[p]={std::clamp(step,0.0f,maxStep),clampCurveValue(kind,value),free};std::sort(c->begin(),c->end(),[](const CurvePoint&a,const CurvePoint&b){return a.step<b.step;});return true;
}
bool ProjectCore::deleteCurvePoint(int t,int n,int kind,int p){std::scoped_lock lock(mutex_);if(!validNote(t,n))return false;auto*c=curveFor(project_.tracks[t].notes[n],kind);if(!c||p<0||p>=static_cast<int>(c->size()))return false;c->erase(c->begin()+p);return true;}
float ProjectCore::lastStep() const {std::scoped_lock lock(mutex_);float mx=-1;for(const auto&t:project_.tracks)for(const auto&n:t.notes)mx=std::max(mx,n.startStep+std::max(1.0f,n.lengthSteps)-1.0f);return mx;}
int ProjectCore::playLengthSteps() const {std::scoped_lock lock(mutex_);const int bar=std::max(1,project_.beats)*std::max(1,project_.divisions);float end=0.0f;bool any=false;for(const auto&t:project_.tracks)for(const auto&n:t.notes){end=std::max(end,n.startStep+std::max(1.0f,n.lengthSteps));any=true;}if(!any)return bar;return std::max(bar,static_cast<int>(std::ceil(end/static_cast<float>(bar)))*bar);}

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

Patch ProjectCore::selectedPatch() const {
    std::scoped_lock lock(mutex_);
    const auto* p=selectedPatchUnsafe();
    return p?*p:makeFactoryPatch(FactoryPreset::SpectrachordInit);
}
DspPatch ProjectCore::selectedDspPatch() const {
    std::scoped_lock lock(mutex_);
    const auto* p=selectedPatchUnsafe();
    return p?toDspPatch(*p):toDspPatch(makeFactoryPatch(FactoryPreset::SpectrachordInit));
}
Fx ProjectCore::selectedFx() const {
    std::scoped_lock lock(mutex_);if(!validTrack(selectedTrack_))return makeFactoryPatch(FactoryPreset::SpectrachordInit).fx;
    const auto& t=project_.tracks[selectedTrack_];if(t.drums&&validPad(selectedTrack_,t.selectedPad))return t.pads[t.selectedPad].patch.fx;return t.patch.fx;
}
DspPatch ProjectCore::padDspPatch(int t,int p) const {std::scoped_lock lock(mutex_);return validPad(t,p)?toDspPatch(project_.tracks[t].pads[p].patch):toDspPatch(makeFactoryPatch(FactoryPreset::SpectrachordInit));}
Fx ProjectCore::padFx(int t,int p) const {std::scoped_lock lock(mutex_);return validPad(t,p)?project_.tracks[t].pads[p].patch.fx:Fx{};}

bool ProjectCore::setSelectedOperatorEnabled(int opIndex,bool enabled){
    std::scoped_lock lock(mutex_);auto* p=selectedPatchUnsafe();if(!p||opIndex<0||opIndex>=6)return false;p->ops[opIndex].enabled=enabled;return true;
}
bool ProjectCore::setSelectedOperatorWave(int opIndex,Wave wave){
    std::scoped_lock lock(mutex_);auto* p=selectedPatchUnsafe();if(!p||opIndex<0||opIndex>=6)return false;
    auto& op=p->ops[opIndex];op.wave=wave;
    if(wave==Wave::Custom&&!op.hasHarm){op.harm=defaultHarmonics();op.hasHarm=true;}
    return true;
}
bool ProjectCore::setSelectedOperatorParam(int opIndex,OperatorParam param,float value){
    std::scoped_lock lock(mutex_);auto* p=selectedPatchUnsafe();if(!p||opIndex<0||opIndex>=6)return false;
    auto& op=p->ops[opIndex];const float v=clampOperatorParam(param,value);
    switch(param){
        case OperatorParam::Ratio:op.ratio=v;break;
        case OperatorParam::Semitone:
            op.semitoneOffset=std::round(v);
            break;
        case OperatorParam::Detune:
            op.detuneCents=std::round(v);
            break;
        case OperatorParam::Level:op.level=v;break;
        case OperatorParam::Attack:op.env.attack=v;break;
        case OperatorParam::Decay:op.env.decay=v;break;
        case OperatorParam::Sustain:op.env.sustain=v;break;
        case OperatorParam::Release:op.env.release=v;break;
    }
    return true;
}
bool ProjectCore::setSelectedHarmonic(int opIndex,int partialIndex,float value,bool muted){
    std::scoped_lock lock(mutex_);auto* p=selectedPatchUnsafe();
    if(!p||opIndex<0||opIndex>=6||partialIndex<0||partialIndex>=16)return false;
    auto& op=p->ops[opIndex];const float v=std::clamp(value,0.0f,1.0f);
    if(muted){op.harmMute[partialIndex]=v;op.hasHarmMute=true;}
    else{op.harm[partialIndex]=v;op.hasHarm=true;}
    return true;
}
bool ProjectCore::setSelectedMatrixAmount(int modulator,int carrier,float value){
    std::scoped_lock lock(mutex_);auto* p=selectedPatchUnsafe();
    if(!p||modulator<0||modulator>=6||carrier<0||carrier>=6)return false;
    p->matrix[modulator][carrier]=std::clamp(value,0.0f,1.0f);return true;
}
bool ProjectCore::setSelectedFilterType(FilterType type){
    std::scoped_lock lock(mutex_);auto* p=selectedPatchUnsafe();if(!p)return false;p->filter.type=type;return true;
}
bool ProjectCore::setSelectedLfoTarget(LfoTarget target){
    std::scoped_lock lock(mutex_);auto* p=selectedPatchUnsafe();if(!p)return false;p->lfo.target=target;return true;
}
bool ProjectCore::setSelectedPatchParam(PatchParam param,float value){
    std::scoped_lock lock(mutex_);auto* p=selectedPatchUnsafe();if(!p)return false;const float v=clampPatchParam(param,value);
    switch(param){
        case PatchParam::FilterCutoff:p->filter.cutoff=v;break;
        case PatchParam::FilterResonance:p->filter.resonance=v;break;
        case PatchParam::FilterEnv:p->filter.envAmount=v;break;
        case PatchParam::FilterAttack:p->filter.env.attack=v;break;
        case PatchParam::FilterDecay:p->filter.env.decay=v;break;
        case PatchParam::FilterSustain:p->filter.env.sustain=v;break;
        case PatchParam::FilterRelease:p->filter.env.release=v;break;
        case PatchParam::AmpAttack:p->amp.attack=v;break;
        case PatchParam::AmpDecay:p->amp.decay=v;break;
        case PatchParam::AmpSustain:p->amp.sustain=v;break;
        case PatchParam::AmpRelease:p->amp.release=v;break;
        case PatchParam::VelocityAmp:p->velocityAmp=v;break;
        case PatchParam::VelocityFilter:p->velocityFilter=v;break;
        case PatchParam::LfoRate:p->lfo.rate=v;break;
        case PatchParam::LfoAmount:p->lfo.amount=v;break;
        case PatchParam::LfoAttack:p->lfo.attack=v;break;
        case PatchParam::LfoVelocity:p->lfo.velocitySensitivity=v;break;
        case PatchParam::Unison:p->unison=v;break;
        case PatchParam::Glide:p->glide=v;break;
        case PatchParam::Volume:p->volume=v;break;
        case PatchParam::Distortion:p->fx.distortion=v;break;
        case PatchParam::Delay:p->fx.delay=v;break;
        case PatchParam::DelayTime:p->fx.delayTime=v;break;
        case PatchParam::DelayFeedback:p->fx.delayFeedback=v;break;
        case PatchParam::Reverb:p->fx.reverb=v;break;
    }
    return true;
}

int ProjectCore::selectedModSlotCount() const {
    std::scoped_lock lock(mutex_);const auto* p=selectedPatchUnsafe();return p?static_cast<int>(p->modSlotCount):0;
}
ModSlot ProjectCore::selectedModSlot(int slotIndex) const {
    std::scoped_lock lock(mutex_);const auto* p=selectedPatchUnsafe();
    if(!p||slotIndex<0||
       slotIndex>=static_cast<int>(p->modSlotCount)||
       slotIndex>=static_cast<int>(p->modSlots.size()))return {};
    return p->modSlots[slotIndex];
}
bool ProjectCore::addSelectedModSlot(ModTarget target){
    std::scoped_lock lock(mutex_);
    auto* p=selectedPatchUnsafe();
    if(!p||p->modSlotCount>=p->modSlots.size())return false;
    const int index=p->modSlotCount++;
    float v=0.0f;
    switch(target){
        case ModTarget::Cutoff:v=p->filter.cutoff;break;
        case ModTarget::Resonance:v=p->filter.resonance;break;
        case ModTarget::FilterEnv:v=p->filter.envAmount;break;
        case ModTarget::AmpAttack:v=p->amp.attack;break;
        case ModTarget::AmpDecay:v=p->amp.decay;break;
        case ModTarget::AmpSustain:v=p->amp.sustain;break;
        case ModTarget::AmpRelease:v=p->amp.release;break;
        case ModTarget::FilterAttack:v=p->filter.env.attack;break;
        case ModTarget::FilterDecay:v=p->filter.env.decay;break;
        case ModTarget::FilterSustain:v=p->filter.env.sustain;break;
        case ModTarget::FilterRelease:v=p->filter.env.release;break;
        case ModTarget::Op1:v=p->ops[0].level;break;
        case ModTarget::Op2:v=p->ops[1].level;break;
        case ModTarget::Op3:v=p->ops[2].level;break;
        case ModTarget::Op4:v=p->ops[3].level;break;
        case ModTarget::Op5:v=p->ops[4].level;break;
        case ModTarget::Op6:v=p->ops[5].level;break;
        case ModTarget::LfoAmount:v=p->lfo.amount;break;
        case ModTarget::LfoRate:v=p->lfo.rate;break;
        case ModTarget::Unison:v=p->unison;break;
        case ModTarget::Volume:v=p->volume;break;
        default:v=0.0f;break;
    }
    p->modSlots[index]={target,v,v};return true;
}
bool ProjectCore::removeSelectedModSlot(int slotIndex){
    std::scoped_lock lock(mutex_);auto* p=selectedPatchUnsafe();
    if(!p||slotIndex<0||slotIndex>=static_cast<int>(p->modSlotCount))return false;
    for(int i=slotIndex;i+1<static_cast<int>(p->modSlotCount);++i)p->modSlots[i]=p->modSlots[i+1];
    if(p->modSlotCount>0)--p->modSlotCount;
    p->modSlots[p->modSlotCount]={};return true;
}
bool ProjectCore::setSelectedModSlot(int slotIndex,ModTarget target,float minValue,float maxValue){
    std::scoped_lock lock(mutex_);auto* p=selectedPatchUnsafe();
    if(!p||slotIndex<0||
       slotIndex>=static_cast<int>(p->modSlotCount)||
       slotIndex>=static_cast<int>(p->modSlots.size()))return false;
    p->modSlots[slotIndex]={target,clampModValue(target,minValue),clampModValue(target,maxValue)};return true;
}

} // namespace aiora
