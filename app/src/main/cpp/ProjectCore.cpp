#include "ProjectCore.h"
#include "FactoryPresets.h"
#include "NexdrumKit.h"
#include <algorithm>
#include <array>
#include <cmath>

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

float clampOperatorParam(OperatorParam param,float v){
    switch(param){
        case OperatorParam::Ratio:return std::clamp(v,0.125f,16.0f);
        case OperatorParam::Detune:return std::clamp(v,-100.0f,100.0f);
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

std::string ProjectCore::autoName(int index) {
    static const std::array<const char*,5> names{"Lead / Keys","Bass","Pad / Strings","Arp / FX","Drums"};
    if(index >= 0 && index < static_cast<int>(names.size())) return names[static_cast<size_t>(index)];
    return "Track " + std::to_string(index + 1);
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
    Track t;t.name = autoName(index);t.drums = drums;t.patch = makeFactoryPatch(FactoryPreset::SpectrachordInit);
    if(drums) {t.patch = makeFactoryPatch(FactoryPreset::Nexdrum);const auto& kit=nexdrumKit();t.pads.assign(kit.begin(),kit.end());t.selectedPad=6;}
    project_.tracks.push_back(std::move(t));selectedTrack_ = index;return index;
}

bool ProjectCore::deleteTrack(int index) {
    std::scoped_lock lock(mutex_);if(!validTrack(index)) return false;
    project_.tracks.erase(project_.tracks.begin()+index);
    if(project_.tracks.empty()) selectedTrack_=-1;else selectedTrack_=std::clamp(index,0,static_cast<int>(project_.tracks.size())-1);
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
    t.selectedPad=6;return true;
}
int ProjectCore::addDrumPad(int trackIndex){
    std::scoped_lock lock(mutex_);if(!validTrack(trackIndex))return -1;
    auto& t=project_.tracks[trackIndex];if(!t.drums||t.pads.size()>=24)return -1;
    std::array<bool,kGridHigh-kGridLow+1> taken{};
    for(const auto& p:t.pads){
        const int lo=std::max(kGridLow,std::min(p.lowMidi,p.highMidi));
        const int hi=std::min(kGridHigh,std::max(p.lowMidi,p.highMidi));
        for(int m=lo;m<=hi;++m)taken[static_cast<size_t>(m-kGridLow)]=true;
    }
    static constexpr std::array<int,12> suggested{38,42,45,43,49,51,47,39,52,59,56,66};
    int midi=-1;
    for(int m:suggested)if(m>=kGridLow&&m<=kGridHigh&&!taken[static_cast<size_t>(m-kGridLow)]){midi=m;break;}
    if(midi<0)for(int m=kGridLow;m<=kGridHigh;++m)if(!taken[static_cast<size_t>(m-kGridLow)]){midi=m;break;}
    if(midi<0)return -1;
    static constexpr std::array<const char*,12> icons{"kick","snare","tom","floortom","hihat","crash","ride","bongo","conga","clap","shaker","cowbell"};
    DrumPad p;p.centerMidi=midi;p.lowMidi=midi;p.highMidi=midi;p.icon=icons[t.pads.size()%icons.size()];
    p.patch=makeFactoryPatch(FactoryPreset::SpectrachordInit);p.patch.fundamentalMidi=midi;p.patch.name=std::string(p.icon)+" pad";
    p.id="pad"+std::to_string(t.pads.size()+1);
    t.pads.push_back(std::move(p));t.selectedPad=static_cast<int>(t.pads.size())-1;return t.selectedPad;
}
bool ProjectCore::deleteDrumPad(int trackIndex,int padIndex){
    std::scoped_lock lock(mutex_);if(!validPad(trackIndex,padIndex))return false;
    auto& t=project_.tracks[trackIndex];const auto& p=t.pads[padIndex];
    const int lo=std::min(p.lowMidi,p.highMidi),hi=std::max(p.lowMidi,p.highMidi);
    t.notes.erase(std::remove_if(t.notes.begin(),t.notes.end(),[&](const Note& n){return n.midi>=lo&&n.midi<=hi;}),t.notes.end());
    t.pads.erase(t.pads.begin()+padIndex);
    t.selectedPad=t.pads.empty()?0:std::clamp(padIndex,0,static_cast<int>(t.pads.size())-1);
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
    for(int i=0;i<static_cast<int>(notes.size());++i)if(notes[i].midi==midi&&std::fabs(notes[i].startStep-start)<0.001f)return i;
    notes.push_back(Note{midi,start,length,{},{},{}});return static_cast<int>(notes.size())-1;
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
    const float maxStep=std::max(0.0f,project_.tracks[t].notes[n].lengthSteps-1.0f);c->push_back({std::clamp(step,0.0f,maxStep),clampCurveValue(kind,value),free});
    std::sort(c->begin(),c->end(),[](const CurvePoint&a,const CurvePoint&b){return a.step<b.step;});
    for(int i=0;i<static_cast<int>(c->size());++i)if(std::fabs((*c)[i].step-std::clamp(step,0.0f,maxStep))<0.0001f&&(*c)[i].free==free)return i;return static_cast<int>(c->size())-1;
}
bool ProjectCore::updateCurvePoint(int t,int n,int kind,int p,float step,float value,bool free){
    std::scoped_lock lock(mutex_);if(!validNote(t,n))return false;auto*c=curveFor(project_.tracks[t].notes[n],kind);if(!c||p<0||p>=static_cast<int>(c->size()))return false;
    const float maxStep=std::max(0.0f,project_.tracks[t].notes[n].lengthSteps-1.0f);(*c)[p]={std::clamp(step,0.0f,maxStep),clampCurveValue(kind,value),free};std::sort(c->begin(),c->end(),[](const CurvePoint&a,const CurvePoint&b){return a.step<b.step;});return true;
}
bool ProjectCore::deleteCurvePoint(int t,int n,int kind,int p){std::scoped_lock lock(mutex_);if(!validNote(t,n))return false;auto*c=curveFor(project_.tracks[t].notes[n],kind);if(!c||p<0||p>=static_cast<int>(c->size()))return false;c->erase(c->begin()+p);return true;}
float ProjectCore::lastStep() const {std::scoped_lock lock(mutex_);float mx=-1;for(const auto&t:project_.tracks)for(const auto&n:t.notes)mx=std::max(mx,n.startStep);return mx;}
int ProjectCore::playLengthSteps() const {std::scoped_lock lock(mutex_);const int bar=std::max(1,project_.beats)*std::max(1,project_.divisions);float mx=-1;for(const auto&t:project_.tracks)for(const auto&n:t.notes)mx=std::max(mx,n.startStep);if(mx<0)return bar;return static_cast<int>(std::ceil((mx+1.0f)/static_cast<float>(bar)))*bar;}

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
        case OperatorParam::Detune:op.detuneCents=v;break;
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
    if(!p||slotIndex<0||slotIndex>=static_cast<int>(p->modSlotCount)||slotIndex>=4)return {};
    return p->modSlots[slotIndex];
}
bool ProjectCore::addSelectedModSlot(ModTarget target){
    std::scoped_lock lock(mutex_);auto* p=selectedPatchUnsafe();if(!p||p->modSlotCount>=4)return false;
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
    if(!p||slotIndex<0||slotIndex>=static_cast<int>(p->modSlotCount)||slotIndex>=4)return false;
    p->modSlots[slotIndex]={target,clampModValue(target,minValue),clampModValue(target,maxValue)};return true;
}

} // namespace aiora
