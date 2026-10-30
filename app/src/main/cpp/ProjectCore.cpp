#include "ProjectCore.h"
#include "FactoryPresets.h"
#include "NexdrumKit.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace aiora {
namespace {
constexpr int kGridLow = 38;
constexpr int kGridHigh = 86;
constexpr int kRollLow = 2;
constexpr int kRollHigh = 122;

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
float clampCurveValue(int kind,float v){return kind==0?v:std::clamp(v,0.0f,1.0f);}

// A point's coordinate zero is the center of its first time cell.
// -0.5 is the physical start edge; length-0.5 is the end edge.
constexpr float kCurveMinStep=-0.5f;
constexpr float kCurveCollisionEpsilon=0.0001f;
float curveMaxStep(const Note& n) noexcept {
    return std::max(1.0f,n.lengthSteps)-0.5f;
}
bool occupiedStep(const std::vector<CurvePoint>& points,float step,int except=-1) noexcept {
    for(size_t i=0;i<points.size();++i)
        if(static_cast<int>(i)!=except &&
           std::fabs(points[i].step-step)<kCurveCollisionEpsilon)return true;
    return false;
}
// Imported/older projects could contain stacked control points. Keep the
// final stored value at a duplicate time, without modifying unrelated notes.
void normalizeNoteCurves(Note& n) {
    for(int kind=0;kind<3;++kind){
        auto* curve=curveFor(n,kind);
        for(auto& point:*curve){
            point.step=std::clamp(point.step,kCurveMinStep,curveMaxStep(n));
            point.value=clampCurveValue(kind,point.value);
        }
        std::stable_sort(curve->begin(),curve->end(),
            [](const CurvePoint&a,const CurvePoint&b){return a.step<b.step;});
        std::vector<CurvePoint> unique;
        unique.reserve(curve->size());
        for(const auto& point:*curve){
            if(!unique.empty() &&
               std::fabs(unique.back().step-point.step)<kCurveCollisionEpsilon)
                unique.back()=point;
            else unique.push_back(point);
        }
        *curve=std::move(unique);
    }
}

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
        case OperatorParam::Ratio:return std::clamp(v,0.125f,16.0f);
        case OperatorParam::Semitone:return std::clamp(v,-12.0f,12.0f);
        case OperatorParam::Detune:return std::clamp(v,-50.0f,50.0f);
        case OperatorParam::Level:return std::clamp(v,0.0f,1.0f);
    }
    return v;
}

float clampPatchParam(PatchParam param,float v){
    switch(param){
        case PatchParam::PeakRatio:return std::clamp(v,0.5f,8.0f);
        case PatchParam::PeakFrequency:return std::clamp(v,40.0f,8000.0f);
        case PatchParam::PeakQ:return std::clamp(v,0.3f,12.0f);
        case PatchParam::PeakGain:return std::clamp(v,-18.0f,18.0f);
        case PatchParam::LfoAmount:
        case PatchParam::LfoVelocity:
        case PatchParam::Unison:
        case PatchParam::Volume:
            return std::clamp(v,0.0f,1.0f);
        case PatchParam::LfoRate:return std::clamp(v,0.1f,20.0f);
        case PatchParam::LfoAttack:return std::clamp(v,0.0f,2.0f);
    }
    return v;
}

HarmonicBank defaultHarmonics(){
    return presetWaveHarmonics(Wave::Saw);
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
std::string ProjectCore::songName() const {std::scoped_lock lock(mutex_);return project_.name;}
void ProjectCore::setSongName(const std::string& name){
    std::scoped_lock lock(mutex_);
    const auto start=name.find_first_not_of(" \t\r\n");
    if(start==std::string::npos)return;
    const auto end=name.find_last_not_of(" \t\r\n");
    project_.name=name.substr(start,std::min<size_t>(end-start+1,80));
}
bool ProjectCore::replaceProject(Project project,int selectedTrack){
    std::scoped_lock lock(mutex_);
    
    for(auto& track:project.tracks)
        for(auto& note:track.notes)normalizeNoteCurves(note);
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

bool ProjectCore::setSelectedOperatorCurve(int opIndex,int kind,const OperatorCurve& curve){
    if(opIndex<0||opIndex>=6||kind<0||kind>3||curve.count>kMaxOperatorPoints)return false;
    std::scoped_lock lock(mutex_);
    auto* p=selectedPatchUnsafe();if(!p)return false;
    auto& op=p->ops[static_cast<size_t>(opIndex)];
    if(kind==0)op.volumeCurve=curve;
    else if(kind==1)op.pitchCurve=curve;
    else if(kind==2)op.volumeMorphCurve=curve;
    else op.pitchMorphCurve=curve;
    return true;
}
bool ProjectCore::setSelectedOperatorModChannel(int opIndex,int slot,const OperatorModChannel& channel){
    if(opIndex<0||opIndex>=6||slot<0||slot>=6||
       channel.response.count>kMaxOperatorPoints||
       static_cast<int>(channel.destination)>4||channel.response.source>2)
        return false;
    std::scoped_lock lock(mutex_);
    auto* p=selectedPatchUnsafe();if(!p)return false;
    auto& op=p->ops[static_cast<size_t>(opIndex)];
    if(!op.modChannelsConfigured){
        for(auto& c:op.modChannels)c=initialOperatorModChannel();
    }
    op.modChannels[static_cast<size_t>(slot)]=channel;
    op.modChannelsConfigured=true;
    return true;
}
bool ProjectCore::setSelectedKeyMapRange(int lowMidi,int highMidi){
    if(lowMidi<0||highMidi>127||lowMidi>=highMidi)return false;
    std::scoped_lock lock(mutex_);
    auto* p=selectedPatchUnsafe();if(!p)return false;
    p->keyMapLow=lowMidi;p->keyMapHigh=highMidi;
    return true;
}
int ProjectCore::addTrack(bool drums) {
    std::scoped_lock lock(mutex_);
    
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
void ProjectCore::setTrackName(int index,const std::string& name) {std::scoped_lock lock(mutex_);if(!validTrack(index))return;std::string n=name;project_.tracks[index].name=n.empty()?autoName(index):n;}
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

bool ProjectCore::applyDrumKit(int trackIndex,std::vector<DrumPad> pads,
                               bool append,std::string* error){
    const auto fail=[&](const char* why){if(error)*error=why;return false;};
    std::scoped_lock lock(mutex_);
    if(!validTrack(trackIndex)||!project_.tracks[trackIndex].drums)
        return fail("Select a drum track before importing a kit");
    auto& track=project_.tracks[trackIndex];
    if(pads.empty()||pads.size()>49||track.pads.size()+(append?pads.size():0)>49)
        return fail("A drum kit must have 1 to 49 pads");
    std::array<bool,kGridHigh-kGridLow+1> used{};
    std::vector<std::string> identifiers;
    if(append){
        for(const auto& existing:track.pads){
            identifiers.push_back(existing.id);
            for(int m=std::max(kGridLow,existing.lowMidi);
                m<=std::min(kGridHigh,existing.highMidi);++m)
                used[static_cast<size_t>(m-kGridLow)]=true;
        }
    }
    for(auto& pad:pads){
        if(pad.lowMidi<kGridLow||pad.highMidi>kGridHigh||
           pad.lowMidi>pad.highMidi||
           pad.centerMidi<pad.lowMidi||pad.centerMidi>pad.highMidi)
            return fail("Imported pad pitch range is invalid");
        for(int m=pad.lowMidi;m<=pad.highMidi;++m){
            const auto slot=static_cast<size_t>(m-kGridLow);
            if(used[slot])
                return fail("Pad ranges overlap; use Replace Kit or choose an unused range");
            used[slot]=true;
        }
        pad.volume=std::clamp(pad.volume,0.0f,1.0f);
        pad.pan=std::clamp(pad.pan,-1.0f,1.0f);
        pad.patch.fundamentalMidi=pad.centerMidi;
        std::string id=pad.id;
        if(id.empty())id="pad";
        if(std::find(identifiers.begin(),identifiers.end(),id)!=identifiers.end()){
            // Append can encounter identical identifiers in otherwise
            // disjoint kits. Preserve every pad, assigning unique IDs.
            const std::string base=id;
            int suffix=2;
            do{id=base+"-"+std::to_string(suffix++);}
            while(std::find(identifiers.begin(),identifiers.end(),id)!=identifiers.end());
        }
        pad.id=std::move(id);
        identifiers.push_back(pad.id);
    }
    if(append){
        const int first=static_cast<int>(track.pads.size());
        track.pads.insert(track.pads.end(),
            std::make_move_iterator(pads.begin()),
            std::make_move_iterator(pads.end()));
        track.selectedPad=first;
    }else{
        track.pads=std::move(pads);
        track.selectedPad=0;
    }
    // Track notes, track-level mixing, other tracks, and song metadata
    // deliberately remain unchanged.
    return true;
}

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
    if(!t.drums||t.pads.size()>=49)return -1;

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
void ProjectCore::setPadName(int t,int p,const std::string& name){std::scoped_lock lock(mutex_);if(!validPad(t,p))return;project_.tracks[t].pads[p].name=name;}
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
        normalizeNoteCurves(copy);
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

        // A group point is measured from the selected notes' first
        // PHYSICAL edge. Individual note curve coordinates instead use the
        // first CELL CENTER as zero (-0.5 = first edge, L-0.5 = last).
        // Reproject through absolute edge time so a group envelope meets
        // every selected note at its real start and end, even for 1-cell
        // notes and for overlapping notes with different lengths.
        const float length=std::max(1.0f,note.lengthSteps);
        const float groupA=note.startStep-0.5f-groupStart;
        const float groupB=groupA+length;

        std::vector<CurvePoint> projected;
        projected.reserve(points.size()+2);
        projected.push_back({
            -0.5f,
            std::clamp(groupCurveValue(points,groupA,fallback),0.0f,1.0f),
            true});

        for(const auto& p:points){
            if(p.step<=groupA+1.0e-5f||p.step>=groupB-1.0e-5f)continue;
            
            projected.push_back({
                p.step-groupA-0.5f,
                std::clamp(p.value,0.0f,1.0f),true});
        }
        projected.push_back({
            length-0.5f,
            std::clamp(groupCurveValue(points,groupB,fallback),0.0f,1.0f),
            true});

        *curve=std::move(projected);
        normalizeNoteCurves(note);
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
    std::scoped_lock lock(mutex_);
    if(!validNote(t,n))return -1;
    auto& note=project_.tracks[t].notes[n];
    auto* c=curveFor(note,kind);
    if(!c)return -1;
    step=std::clamp(step,kCurveMinStep,curveMaxStep(note));
    value=clampCurveValue(kind,value);
    // Exactly one point is allowed at any time position within a curve.
    // On creation an occupied position selects the existing point.
    for(int i=0;i<static_cast<int>(c->size());++i)
        if(std::fabs((*c)[i].step-step)<kCurveCollisionEpsilon)return i;
    
    c->push_back({step,value,free});
    std::sort(c->begin(),c->end(),
        [](const CurvePoint&a,const CurvePoint&b){return a.step<b.step;});
    for(int i=0;i<static_cast<int>(c->size());++i)
        if(std::fabs((*c)[i].step-step)<kCurveCollisionEpsilon)return i;
    return -1;
}
bool ProjectCore::updateCurvePoint(int t,int n,int kind,int p,float step,float value,bool free){
    std::scoped_lock lock(mutex_);
    if(!validNote(t,n))return false;
    auto& note=project_.tracks[t].notes[n];
    auto* c=curveFor(note,kind);
    if(!c||p<0||p>=static_cast<int>(c->size()))return false;
    step=std::clamp(step,kCurveMinStep,curveMaxStep(note));
    if(occupiedStep(*c,step,p))return false; // Keep prior valid location.
    auto previous=(*c)[p];
    previous.step=step;
    previous.value=clampCurveValue(kind,value);
    previous.free=free;
    (*c)[p]=previous;
    std::sort(c->begin(),c->end(),
        [](const CurvePoint&a,const CurvePoint&b){return a.step<b.step;});
    return true;
}
bool ProjectCore::updateCurveMidpoint(
    int track,int note,int kind,int point,float midpoint){
    std::scoped_lock lock(mutex_);
    if(!validNote(track,note))return false;
    auto* curve=curveFor(project_.tracks[track].notes[note],kind);
    if(!curve||point<0||point>=static_cast<int>(curve->size())||
       (kind!=0&&point==0))return false;
    (*curve)[static_cast<size_t>(point)].midpoint=
        std::clamp(midpoint,0.05f,0.95f);
    return true;
}
bool ProjectCore::updateCurveHandle(
    int track,int note,int kind,int point,float midpoint,float value){
    std::scoped_lock lock(mutex_);
    if(!validNote(track,note))return false;
    auto* c=curveFor(project_.tracks[track].notes[note],kind);
    if(!c||point<0||point>=static_cast<int>(c->size())||
       (kind!=0&&point==0))return false;
    auto& p=(*c)[static_cast<size_t>(point)];
    const float before=point>0?
        (*c)[static_cast<size_t>(point-1)].value:0.0f;
    const float after=p.value;
    if(std::fabs(after-before)<=1.0e-5f){
        // A level section has no meaningful apex. Lock the X in its
        // neutral position rather than saving inert handle movement.
        p.midpoint=0.5f;
        p.handleValue=before;
        p.handleActive=false;
        return false;
    }
    p.midpoint=std::clamp(midpoint,0.05f,0.95f);
    p.handleValue=std::clamp(
        clampCurveValue(kind,value),
        std::min(before,after),std::max(before,after));
    p.handleActive=true;
    return true;
}
bool ProjectCore::replaceNoteCurves(int track,int note,
    const std::vector<CurvePoint>& bend,
    const std::vector<CurvePoint>& velocity,
    const std::vector<CurvePoint>& mod){
    std::scoped_lock lock(mutex_);
    if(!validNote(track,note))return false;
    
    auto& n=project_.tracks[track].notes[note];
    n.bend=bend;n.velocity=velocity;n.mod=mod;
    return true;
}
bool ProjectCore::deleteCurvePoint(int t,int n,int kind,int p){std::scoped_lock lock(mutex_);if(!validNote(t,n))return false;auto*c=curveFor(project_.tracks[t].notes[n],kind);if(!c||p<0||p>=static_cast<int>(c->size()))return false;c->erase(c->begin()+p);return true;}
float ProjectCore::lastStep() const {std::scoped_lock lock(mutex_);float mx=-1;for(const auto&t:project_.tracks)for(const auto&n:t.notes)mx=std::max(mx,n.startStep+std::max(1.0f,n.lengthSteps)-1.0f);return mx;}
int ProjectCore::playLengthSteps() const {std::scoped_lock lock(mutex_);const int bar=std::max(1,project_.beats)*std::max(1,project_.divisions);float end=0.0f;bool any=false;for(const auto&t:project_.tracks)for(const auto&n:t.notes){end=std::max(end,n.startStep+std::max(1.0f,n.lengthSteps));any=true;}if(!any)return bar;return std::max(bar,static_cast<int>(std::ceil(end/static_cast<float>(bar)))*bar);}

float ProjectCore::bpm() const {std::scoped_lock lock(mutex_);return project_.bpm;}
int ProjectCore::beats() const {std::scoped_lock lock(mutex_);return project_.beats;}
int ProjectCore::divisions() const {std::scoped_lock lock(mutex_);return project_.divisions;}
bool ProjectCore::dozenal() const {std::scoped_lock lock(mutex_);return project_.dozenal;}
float ProjectCore::masterVolume() const {std::scoped_lock lock(mutex_);return project_.masterVolume;}
void ProjectCore::setBpm(float v){std::scoped_lock lock(mutex_);project_.bpm=std::clamp(v,12.0f,288.0f);}
void ProjectCore::setSignature(int b,int d){std::scoped_lock lock(mutex_);project_.beats=std::clamp(b,1,12);project_.divisions=std::clamp(d,1,12);}
void ProjectCore::setDozenal(bool e){std::scoped_lock lock(mutex_);project_.dozenal=e;}
void ProjectCore::setMasterVolume(float v){std::scoped_lock lock(mutex_);project_.masterVolume=std::clamp(v,0.0f,1.0f);}

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

Fx ProjectCore::fxRack(bool master) const {
    std::scoped_lock lock(mutex_);
    if(master)return project_.masterFx;
    const auto* patch=selectedPatchUnsafe();
    return patch?patch->fx:Fx{};
}
bool ProjectCore::addFxStage(bool master,FxType type){
    std::scoped_lock lock(mutex_);
    Fx* fx=master?&project_.masterFx:nullptr;
    if(!master){auto* p=selectedPatchUnsafe();if(!p)return false;fx=&p->fx;}
    if(fx->count>=kMaxFxStages||!isAvailableFxType(type))return false;
    fx->chain[fx->count++]=makeFxModule(type);return true;
}
bool ProjectCore::removeFxStage(bool master,int index){
    std::scoped_lock lock(mutex_);
    Fx* fx=master?&project_.masterFx:nullptr;
    if(!master){auto* p=selectedPatchUnsafe();if(!p)return false;fx=&p->fx;}
    if(index<0||index>=fx->count)return false;
    for(int i=index;i+1<fx->count;++i)fx->chain[i]=fx->chain[i+1];
    --fx->count;return true;
}
bool ProjectCore::moveFxStage(bool master,int index,int direction){
    std::scoped_lock lock(mutex_);
    Fx* fx=master?&project_.masterFx:nullptr;
    if(!master){auto* p=selectedPatchUnsafe();if(!p)return false;fx=&p->fx;}
    const int other=index+direction;
    if(index<0||other<0||index>=fx->count||other>=fx->count)return false;
    std::swap(fx->chain[index],fx->chain[other]);return true;
}
bool ProjectCore::setFxStageEnabled(bool master,int index,bool enabled){
    std::scoped_lock lock(mutex_);
    Fx* fx=master?&project_.masterFx:nullptr;
    if(!master){auto* p=selectedPatchUnsafe();if(!p)return false;fx=&p->fx;}
    if(index<0||index>=fx->count)return false;
    fx->chain[index].enabled=enabled;return true;
}
bool ProjectCore::setFxStageParameter(bool master,int index,int parameter,float value){
    std::scoped_lock lock(mutex_);
    Fx* fx=master?&project_.masterFx:nullptr;
    if(!master){auto* p=selectedPatchUnsafe();if(!p)return false;fx=&p->fx;}
    if(index<0||index>=fx->count)return false;
    auto& mod=fx->chain[index];
    if(parameter<0||static_cast<size_t>(parameter)>=fxParamCount(mod.type))return false;
    const auto desc=fxParamDesc(mod.type,static_cast<size_t>(parameter));
    mod.params[static_cast<size_t>(parameter)]=std::clamp(value,desc.lo,desc.hi);
    return true;
}
DspPatch ProjectCore::padDspPatch(int t,int p) const {std::scoped_lock lock(mutex_);return validPad(t,p)?toDspPatch(project_.tracks[t].pads[p].patch):toDspPatch(makeFactoryPatch(FactoryPreset::SpectrachordInit));}
Fx ProjectCore::padFx(int t,int p) const {std::scoped_lock lock(mutex_);return validPad(t,p)?project_.tracks[t].pads[p].patch.fx:Fx{};}

bool ProjectCore::setSelectedOperatorEnabled(int opIndex,bool enabled){
    std::scoped_lock lock(mutex_);auto* p=selectedPatchUnsafe();if(!p||opIndex<0||opIndex>=6)return false;p->ops[opIndex].enabled=enabled;return true;
}
bool ProjectCore::setSelectedOperatorRandomPhase(int opIndex,bool enabled){
    std::scoped_lock lock(mutex_);
    auto* patch=selectedPatchUnsafe();
    if(!patch||opIndex<0||opIndex>=6)return false;
    patch->ops[static_cast<size_t>(opIndex)].randomPhase=enabled;
    return true;
}
bool ProjectCore::setSelectedOperatorRandomPhaseDegrees(int opIndex,float degrees){
    std::scoped_lock lock(mutex_);
    auto* patch=selectedPatchUnsafe();
    if(!patch||opIndex<0||opIndex>=6||!std::isfinite(degrees))return false;
    patch->ops[static_cast<size_t>(opIndex)].randomPhaseDegrees=
        std::clamp(degrees,0.0f,180.0f);
    return true;
}
bool ProjectCore::setSelectedOperatorWave(int opIndex,Wave wave,bool morph){
    std::scoped_lock lock(mutex_);
    auto* p=selectedPatchUnsafe();
    if(!p||opIndex<0||opIndex>=6)return false;
    auto& op=p->ops[static_cast<size_t>(opIndex)];
    if(wave==Wave::Hiss)wave=Wave::Noise;
    if(morph){
        const Wave old=effectiveWaveMorph(op);
        if(wave==Wave::Custom&&(old!=Wave::Custom||!op.hasHarmMute)){
            const Wave source=isTextureWave(old)?Wave::Custom:old;
            op.harmMute=presetWaveHarmonics(source);
            op.harmMutePhase=presetWavePhases(source);
            op.hasHarmMute=true;
        }
        if(!op.waveMorphConfigured){
            op.textureMorphPitched=op.texturePitched;
            op.textureMorph=op.texture;
        }
        op.waveMorph=wave;
        op.waveMorphConfigured=true;
    }else{
        if(wave==Wave::Custom&&(op.wave!=Wave::Custom||!op.hasHarm)){
            const Wave source=isTextureWave(op.wave)?Wave::Custom:op.wave;
            op.harm=presetWaveHarmonics(source);
            op.harmPhase=presetWavePhases(source);
            op.hasHarm=true;
        }
        op.wave=wave;
    }
    return true;
}
bool ProjectCore::setSelectedTexturePitched(int opIndex,bool pitched,bool morph){
    std::scoped_lock lock(mutex_);
    auto* p=selectedPatchUnsafe();
    if(!p||opIndex<0||opIndex>=6)return false;
    auto& o=p->ops[static_cast<size_t>(opIndex)];
    if(!isTextureWave(morph?effectiveWaveMorph(o):o.wave))return false;
    if(morph){
        if(!o.waveMorphConfigured){
            o.waveMorph=o.wave;o.waveMorphConfigured=true;
            o.textureMorph=o.texture;
        }
        o.textureMorphPitched=pitched;
    }else o.texturePitched=pitched;
    return true;
}
bool ProjectCore::setSelectedTextureParam(int opIndex,int parameter,float value,bool morph){
    std::scoped_lock lock(mutex_);
    auto* p=selectedPatchUnsafe();
    if(!p||opIndex<0||opIndex>=6||parameter<0||parameter>=5||
       !std::isfinite(value))return false;
    auto& o=p->ops[static_cast<size_t>(opIndex)];
    if((morph?effectiveWaveMorph(o):o.wave)!=Wave::TextureCustom)return false;
    auto& t=morph?o.textureMorph:o.texture;
    float* values[5]{&t.density,&t.size,&t.color,&t.resonance,&t.chaos};
    *values[parameter]=std::clamp(value,0.0f,1.0f);
    return true;
}
bool ProjectCore::setSelectedTextureSource(int opIndex,int source,bool morph){
    std::scoped_lock lock(mutex_);
    auto* p=selectedPatchUnsafe();
    if(!p||opIndex<0||opIndex>=6||source<0||source>3)return false;
    auto& o=p->ops[static_cast<size_t>(opIndex)];
    if((morph?effectiveWaveMorph(o):o.wave)!=Wave::TextureCustom)return false;
    (morph?o.textureMorph:o.texture).source=static_cast<uint8_t>(source);
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
    }
    return true;
}
bool ProjectCore::setSelectedHarmonic(int opIndex,int partialIndex,float value,bool muted){
    std::scoped_lock lock(mutex_);auto* p=selectedPatchUnsafe();
    if(!p||opIndex<0||opIndex>=6||partialIndex<0||partialIndex>=static_cast<int>(kEditableHarmonics))return false;
    auto& op=p->ops[opIndex];const float v=std::clamp(value,0.0f,1.0f);
    if(muted){
        if(isTextureWave(effectiveWaveMorph(op)))return false;
        if(effectiveWaveMorph(op)!=Wave::Custom||!op.hasHarmMute){
            op.harmMute=presetWaveHarmonics(effectiveWaveMorph(op));
            op.harmMutePhase=presetWavePhases(effectiveWaveMorph(op));
        }
        op.harmMute[partialIndex]=v;op.hasHarmMute=true;
        op.waveMorph=Wave::Custom;op.waveMorphConfigured=true;
    }else{
        if(isTextureWave(op.wave))return false;
        if(op.wave!=Wave::Custom||!op.hasHarm){
            op.harm=presetWaveHarmonics(op.wave);
            op.harmPhase=presetWavePhases(op.wave);
        }
        op.harm[partialIndex]=v;op.hasHarm=true;
        op.wave=Wave::Custom;
    }
    return true;
}
bool ProjectCore::setSelectedHarmonicPhase(int opIndex,int partialIndex,float value,bool muted){
    std::scoped_lock lock(mutex_);auto* p=selectedPatchUnsafe();
    if(!p||opIndex<0||opIndex>=6||partialIndex<0||
       partialIndex>=static_cast<int>(kEditableHarmonics)||!std::isfinite(value))return false;
    auto& op=p->ops[opIndex];
    // Endpoints 0 and 1 are identical phases; retain 1 in the UI when set.
    const float v=std::clamp(value,0.0f,1.0f);
    if(muted){
        const Wave bank=effectiveWaveMorph(op);
        if(isTextureWave(bank))return false;
        if(bank!=Wave::Custom||!op.hasHarmMute){
            op.harmMute=presetWaveHarmonics(bank);
            op.harmMutePhase=presetWavePhases(bank);
        }
        op.harmMutePhase[partialIndex]=v;
        op.hasHarmMute=true;op.waveMorph=Wave::Custom;op.waveMorphConfigured=true;
    }else{
        if(isTextureWave(op.wave))return false;
        if(op.wave!=Wave::Custom||!op.hasHarm){
            op.harm=presetWaveHarmonics(op.wave);
            op.harmPhase=presetWavePhases(op.wave);
        }
        op.harmPhase[partialIndex]=v;
        op.hasHarm=true;op.wave=Wave::Custom;
    }
    return true;
}
bool ProjectCore::setSelectedMatrixAmount(int modulator,int carrier,float value){
    std::scoped_lock lock(mutex_);auto* p=selectedPatchUnsafe();
    if(!p||modulator<0||modulator>=6||carrier<0||carrier>=6)return false;
    p->matrix[modulator][carrier]=std::clamp(value,0.0f,1.0f);return true;
}
bool ProjectCore::setSelectedEvolvingFilter(const EvolvingFilter& model){
    std::scoped_lock lock(mutex_);auto* p=selectedPatchUnsafe();if(!p)return false;
    auto clean=model;
    for(int lane=0;lane<6;++lane){
        auto& c=clean.lanes[lane].curve;
        if(c.count<2||c.count>kMaxOperatorPoints)return false;
        c.releaseIndex=static_cast<uint8_t>(std::clamp<int>(c.releaseIndex,0,c.count-2));
        c.decayIndex=static_cast<uint8_t>(std::clamp<int>(c.decayIndex,0,c.count-2));
        for(int i=0;i<c.count;++i){
            auto& pt=c.points[i];
            if(!std::isfinite(pt.time)||!std::isfinite(pt.value))return false;
            pt.value=std::clamp(pt.value,0.0f,1.0f);
            pt.time=std::clamp(pt.time,0.0f,120.0f);
            if(i>0&&pt.time<=c.points[i-1].time)return false;
            clean.lanes[lane].transition[i]=std::clamp(clean.lanes[lane].transition[i],0.0f,.95f);
        }
    }
    for(auto& slope:clean.slopes){
        auto& q=slope.profile;
        if(q.count<2||q.count>kMaxOperatorPoints)return false;
        for(int i=0;i<q.count;++i){
            auto& pt=q.points[i];
            if(!std::isfinite(pt.time)||!std::isfinite(pt.value))return false;
            pt.value=std::clamp(pt.value,0.0f,1.0f);
            pt.time=std::clamp(pt.time,0.0f,1.0f);
            if(i&&pt.time<=q.points[i-1].time)return false;
        }
    }
    for(auto& m:clean.mods){
        if(m.destination>=6||m.response.source>2||m.response.count<2||
           m.response.count>kMaxOperatorPoints)return false;
        for(int i=0;i<m.response.count;++i){
            auto& pt=m.response.points[i];
            if(!std::isfinite(pt.time)||!std::isfinite(pt.value))return false;
            pt.time=std::clamp(pt.time,0.0f,1.0f);
            pt.value=std::clamp(pt.value,0.0f,1.0f);
            if(i>0&&pt.time<=m.response.points[i-1].time)return false;
            m.transition[i]=std::clamp(m.transition[i],0.0f,.95f);
        }
    }
    p->filter.evolving=clean;
    return true;
}
bool ProjectCore::setSelectedMasterAmpCurve(const OperatorCurve& curve){
    std::scoped_lock lock(mutex_);auto* patch=selectedPatchUnsafe();
    if(!patch||curve.count<2||curve.count>kMaxOperatorPoints)return false;
    auto clean=curve;
    clean.releaseIndex=static_cast<uint8_t>(std::clamp<int>(clean.releaseIndex,0,clean.count-2));
    clean.decayIndex=static_cast<uint8_t>(std::clamp<int>(clean.decayIndex,0,clean.count-2));
    for(int i=0;i<clean.count;++i){
        auto& p=clean.points[i];
        if(!std::isfinite(p.time)||!std::isfinite(p.value))return false;
        p.time=std::clamp(p.time,0.0f,120.0f);
        p.value=std::clamp(p.value,0.0f,1.0f);
        if(i>0&&p.time<=clean.points[i-1].time)return false;
    }
    patch->masterAmpCurve=clean;return true;
}
bool ProjectCore::setSelectedAmpMod(int slot,const OperatorResponse& response){
    std::scoped_lock lock(mutex_);
    auto* patch=selectedPatchUnsafe();
    if(!patch||slot<0||slot>=3||response.source>2||
       response.count<2||response.count>kMaxOperatorPoints)return false;
    auto clean=response;
    for(int i=0;i<clean.count;++i){
        auto& pt=clean.points[i];
        if(!std::isfinite(pt.time)||!std::isfinite(pt.value))return false;
        pt.time=std::clamp(pt.time,0.0f,1.0f);
        pt.value=std::clamp(pt.value,0.0f,1.0f);
        if(i>0&&pt.time<=clean.points[i-1].time)return false;
    }
    patch->ampMods[static_cast<size_t>(slot)]=clean;
    return true;
}
bool ProjectCore::setSelectedUnisonCurve(const OperatorCurve& curve){
    std::scoped_lock lock(mutex_);auto* patch=selectedPatchUnsafe();
    if(!patch||curve.count<2||curve.count>kMaxOperatorPoints)return false;
    auto clean=curve;
    clean.enabled=true;
    clean.releaseIndex=static_cast<uint8_t>(std::clamp<int>(clean.releaseIndex,0,clean.count-2));
    clean.decayIndex=static_cast<uint8_t>(std::clamp<int>(clean.decayIndex,0,clean.count-2));
    for(int i=0;i<clean.count;++i){
        auto& p=clean.points[i];
        if(!std::isfinite(p.time)||!std::isfinite(p.value))return false;
        p.time=std::clamp(p.time,0.0f,120.0f);
        p.value=std::clamp(p.value,0.0f,1.0f);
        if(i>0&&p.time<=clean.points[i-1].time)return false;
    }
    patch->unisonCurve=clean;
    return true;
}
bool ProjectCore::setSelectedPeakEnabled(bool enabled){
    std::scoped_lock lock(mutex_);auto* p=selectedPatchUnsafe();if(!p)return false;p->peak.enabled=enabled;return true;
}
bool ProjectCore::setSelectedPeakFollowNote(bool followNote){
    std::scoped_lock lock(mutex_);auto* p=selectedPatchUnsafe();if(!p)return false;p->peak.followNote=followNote;return true;
}
bool ProjectCore::setSelectedLfoTarget(LfoTarget target){
    std::scoped_lock lock(mutex_);auto* p=selectedPatchUnsafe();if(!p)return false;p->lfo.target=target;return true;
}
bool ProjectCore::setSelectedPatchParam(PatchParam param,float value){
    std::scoped_lock lock(mutex_);auto* p=selectedPatchUnsafe();if(!p)return false;const float v=clampPatchParam(param,value);
    switch(param){
        case PatchParam::PeakRatio:p->peak.ratio=v;break;
        case PatchParam::PeakFrequency:p->peak.frequency=v;break;
        case PatchParam::PeakQ:p->peak.q=v;break;
        case PatchParam::PeakGain:p->peak.gainDb=v;break;
        case PatchParam::LfoRate:p->lfo.rate=v;break;
        case PatchParam::LfoAmount:p->lfo.amount=v;break;
        case PatchParam::LfoAttack:p->lfo.attack=v;break;
        case PatchParam::LfoVelocity:p->lfo.velocitySensitivity=v;break;
        case PatchParam::Unison:p->unison=v;break;
        case PatchParam::Volume:p->volume=v;break;
    }
    return true;
}
} // namespace aiora
