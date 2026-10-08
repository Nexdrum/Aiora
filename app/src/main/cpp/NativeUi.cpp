#include "NativeUi.h"
#include "NativeEditor.h"
#include "NativeOverlay.h"

#include <GLES3/gl3.h>

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <utility>
#include <cmath>
#include <cstdint>

#include "ProjectCore.h"
#include "AudioEngine.h"

namespace aiora {
namespace {

constexpr NativeUi::Rgb kBg{0.0627f, 0.0706f, 0.0863f};
constexpr NativeUi::Rgb kTop{0.0784f, 0.0902f, 0.1137f};
constexpr NativeUi::Rgb kPanel{0.0863f, 0.1020f, 0.1294f};
constexpr NativeUi::Rgb kRollBg{0.0471f, 0.0549f, 0.0706f};
constexpr NativeUi::Rgb kCell{0.1020f, 0.1216f, 0.1608f};
constexpr NativeUi::Rgb kBeat{0.1255f, 0.1490f, 0.2039f};
constexpr NativeUi::Rgb kBar{0.1412f, 0.1725f, 0.2275f};
constexpr NativeUi::Rgb kButton{0.1373f, 0.1569f, 0.2000f};
constexpr NativeUi::Rgb kCyan{0.0f, 0.80f, 0.80f};
constexpr NativeUi::Rgb kBlue{0.0f, 0.5608f, 1.0f};
constexpr NativeUi::Rgb kOrange{1.0f, 0.6667f, 0.0f};
constexpr NativeUi::Rgb kGold{1.0f, 0.76f, 0.08f};
constexpr NativeUi::Rgb kGreen{0.18f, 0.82f, 0.42f};
constexpr NativeUi::Rgb kRed{0.90f, 0.18f, 0.20f};
constexpr NativeUi::Rgb kWhite{0.91f, 0.925f, 0.945f};
constexpr NativeUi::Rgb kPurple{0.788f, 0.557f, 1.0f};
constexpr NativeUi::Rgb kMuted{0.35f, 0.39f, 0.47f};

constexpr std::array<NativeUi::Rgb, 12> kPitchColors{{
    {0.2275f, 1.0000f, 0.0000f},
    {0.0000f, 1.0000f, 0.9255f},
    {0.0000f, 0.5608f, 1.0000f},
    {0.0588f, 0.0000f, 0.9843f},
    {0.3882f, 0.0000f, 0.7451f},
    {0.4314f, 0.0000f, 0.5020f},
    {0.5961f, 0.0000f, 0.0000f},
    {0.7843f, 0.0000f, 0.0000f},
    {0.9529f, 0.0000f, 0.0000f},
    {1.0000f, 0.4706f, 0.0000f},
    {1.0000f, 0.9373f, 0.0000f},
    {0.6667f, 1.0000f, 0.0000f},
}};

constexpr std::array<NativePage,6> kNavPages{{
    NativePage::Tracks,
    NativePage::Drums,
    NativePage::Synth,
    NativePage::Fx,
    NativePage::Roll,
    NativePage::Play
}};

constexpr std::array<int,6> kNavIconIndices{{0,1,3,4,2,5}};


NativeUi::Rgb mix(NativeUi::Rgb a, NativeUi::Rgb b, float amount) noexcept {
    amount = std::clamp(amount, 0.0f, 1.0f);
    return {
        a.r + (b.r - a.r) * amount,
        a.g + (b.g - a.g) * amount,
        a.b + (b.b - a.b) * amount
    };
}

NativeUi::Rgb rollSpectrumColor(float pitch) noexcept {
    const int lower=static_cast<int>(std::floor(pitch));
    const float amount=std::clamp(
        pitch-static_cast<float>(lower),0.0f,1.0f);
    const int pc0=((lower%12)+12)%12;
    const int pc1=(pc0+1)%12;
    return mix(
        kPitchColors[static_cast<size_t>(pc0)],
        kPitchColors[static_cast<size_t>(pc1)],
        amount);
}

NativeOverlay::Color overlayColor(NativeUi::Rgb c,float alpha=1.0f) noexcept {
    return {c.r,c.g,c.b,alpha};
}

bool pageHasPadQuick(NativePage page) noexcept {
    return page == NativePage::Drums;
}

int drumIconIndex(std::string_view id) noexcept {
    static constexpr std::array<std::string_view,12> ids{
        "kick","snare","tom","floortom","hihat","crash",
        "ride","bongo","conga","clap","shaker","cowbell"};
    for(int i=0;i<static_cast<int>(ids.size());++i)
        if(ids[static_cast<size_t>(i)]==id)return i;
    return 0;
}

std::string drumIconName(std::string_view id){
    static constexpr std::array<const char*,12> names{
        "Kick","Snare","Tom","Floor tom","Hi-hat","Cymbal",
        "Ride","Bongo","Conga","Clap","Shaker","Cowbell"};
    return names[static_cast<size_t>(drumIconIndex(id))];
}

std::string pitchCoord(int midi){
    static constexpr char digits[]="0123456789XE";
    const int t=midi-62;
    const bool neg=t<0;
    const int a=std::abs(t);
    const int ip=a/12;
    const int fp=a%12;
    std::string out;
    if(neg)out.push_back('-');
    if(ip<12)out.push_back(digits[ip]);
    else out+=std::to_string(ip);
    if(fp){
        out.push_back('.');
        out.push_back(digits[fp]);
    }
    return out;
}


float rollCurveLength(const Note& note) noexcept {
    return std::max(0.0f,std::max(1.0f,note.lengthSteps)-1.0f);
}

const DrumPad* rollPadForMidi(const Track& track,int midi) noexcept {
    if(!track.drums)return nullptr;
    for(const auto& pad:track.pads){
        const int lo=std::min(pad.lowMidi,pad.highMidi);
        const int hi=std::max(pad.lowMidi,pad.highMidi);
        if(midi>=lo&&midi<=hi)return &pad;
    }
    return nullptr;
}

float rollClampBend(const Track& track,const Note& note,float value) noexcept {
    float out=std::clamp(value,-12.0f,12.0f);
    if(const auto* pad=rollPadForMidi(track,note.midi)){
        const float lo=
            static_cast<float>(std::min(pad->lowMidi,pad->highMidi))-
            0.5f-static_cast<float>(note.midi);
        const float hi=
            static_cast<float>(std::max(pad->lowMidi,pad->highMidi))+
            0.5f-static_cast<float>(note.midi);
        out=std::clamp(out,lo,hi);
    }
    return out;
}

float rollLerp(
    float x,float x0,float y0,float x1,float y1) noexcept {
    if(std::fabs(x1-x0)<1.0e-5f)return y1;
    const float t=std::clamp((x-x0)/(x1-x0),0.0f,1.0f);
    return y0+(y1-y0)*t;
}

float rollBendAt(
    const Track& track,const Note& note,float step) noexcept {
    const float end=rollCurveLength(note);
    const float x=std::clamp(step,0.0f,end);
    if(note.bend.empty())return 0.0f;

    float prevStep=0.0f;
    float prevValue=0.0f;
    for(const auto& point:note.bend){
        const float ps=std::clamp(point.step,0.0f,end);
        const float pv=rollClampBend(track,note,point.value);
        if(ps<=prevStep+1.0e-5f){
            prevStep=ps;
            prevValue=pv;
            if(x<=ps+1.0e-5f)return pv;
            continue;
        }
        if(x<=ps)return rollLerp(x,prevStep,prevValue,ps,pv);
        prevStep=ps;
        prevValue=pv;
    }
    // Hold the last explicit bend through the note tail. Extending a
    // note must preserve its current sounding pitch instead of inventing an
    // implicit glide back to the written MIDI note.
    return prevValue;
}

float rollLevelAt(
    const std::vector<CurvePoint>& points,
    const Note& note,float step,float fallback) noexcept {
    if(points.empty())return fallback;
    const float end=rollCurveLength(note);
    const float x=std::clamp(step,0.0f,end);
    float prevStep=0.0f;
    float prevValue=std::clamp(points.front().value,0.0f,1.0f);
    for(const auto& point:points){
        const float ps=std::clamp(point.step,0.0f,end);
        const float pv=std::clamp(point.value,0.0f,1.0f);
        if(ps<=prevStep+1.0e-5f){
            prevStep=ps;
            prevValue=pv;
            if(x<=ps+1.0e-5f)return pv;
            continue;
        }
        if(x<=ps)return rollLerp(x,prevStep,prevValue,ps,pv);
        prevStep=ps;
        prevValue=pv;
    }
    return prevValue;
}

float rollVisualCurveStep(const Note& note,float fraction) noexcept {
    const float end=rollCurveLength(note);
    if(end<=0.0f)return 0.0f;
    const float visualSteps=std::max(1.0f,note.lengthSteps);
    return std::clamp(
        fraction*visualSteps-0.5f,
        0.0f,end);
}

float rollJaggedWave(float stepPosition) noexcept {
    // Two even triangular teeth per sequencer step. M scales only the
    // amplitude, so the border morphs from smooth to an orderly zigzag rather
    // than turning into independent random noise on each edge.
    float phase=std::fmod(std::max(0.0f,stepPosition)*2.0f,1.0f);
    if(phase<0.0f)phase+=1.0f;
    return 1.0f-4.0f*std::fabs(phase-0.5f);
}

bool rollPointInPolygon(
    const std::vector<RollLassoPoint>& polygon,float x,float y) noexcept {
    if(polygon.size()<3)return false;
    bool inside=false;
    size_t j=polygon.size()-1;
    for(size_t i=0;i<polygon.size();++i){
        const auto& a=polygon[i];
        const auto& b=polygon[j];
        const bool crosses=
            ((a.y>y)!=(b.y>y)) &&
            (x<(b.x-a.x)*(y-a.y)/(b.y-a.y)+a.x);
        if(crosses)inside=!inside;
        j=i;
    }
    return inside;
}

float rollPitchX(
    const std::vector<int>& columns,float pitch,int pitchOffset,
    float gridLeft,float colW) noexcept {
    if(columns.empty())return gridLeft;

    // Pitch geometry is allowed to continue beyond the defined piano-roll
    // columns. Only the final ribbon quad is clipped to the viewport. This is
    // essential for bends and harmonic shadows whose base pitch is offscreen
    // but whose bent path crosses into view.
    float visualIndex=0.0f;
    if(columns.size()==1){
        visualIndex=pitch-static_cast<float>(columns.front());
    }else if(pitch<=static_cast<float>(columns.front())){
        const float a=static_cast<float>(columns[0]);
        const float b=static_cast<float>(columns[1]);
        const float span=std::max(1.0f,b-a);
        visualIndex=(pitch-a)/span;
    }else if(pitch>=static_cast<float>(columns.back())){
        const size_t last=columns.size()-1;
        const float a=static_cast<float>(columns[last-1]);
        const float b=static_cast<float>(columns[last]);
        const float span=std::max(1.0f,b-a);
        visualIndex=
            static_cast<float>(last)+(pitch-b)/span;
    }else{
        const auto hi=std::lower_bound(
            columns.begin(),columns.end(),pitch,
            [](int a,float b){return static_cast<float>(a)<b;});
        const int hiIndex=static_cast<int>(std::distance(columns.begin(),hi));
        if(hi!=columns.end()&&std::fabs(static_cast<float>(*hi)-pitch)<1.0e-5f){
            visualIndex=static_cast<float>(hiIndex);
        }else{
            const int loIndex=std::max(0,hiIndex-1);
            const float a=static_cast<float>(columns[static_cast<size_t>(loIndex)]);
            const float b=static_cast<float>(columns[static_cast<size_t>(hiIndex)]);
            const float t=std::fabs(b-a)<1.0e-5f?0.0f:
                std::clamp((pitch-a)/(b-a),0.0f,1.0f);
            visualIndex=static_cast<float>(loIndex)+t;
        }
    }
    return gridLeft+
        (visualIndex-static_cast<float>(pitchOffset)+0.5f)*colW;
}

float rollPitchFromX(
    const std::vector<int>& columns,float x,int pitchOffset,
    float gridLeft,float colW) noexcept {
    if(columns.empty())return 62.0f;
    float visual=
        (x-gridLeft)/std::max(1.0f,colW)+
        static_cast<float>(pitchOffset)-0.5f;
    visual=std::clamp(
        visual,0.0f,static_cast<float>(columns.size()-1));
    const int lo=std::clamp(
        static_cast<int>(std::floor(visual)),
        0,static_cast<int>(columns.size())-1);
    const int hi=std::min(lo+1,static_cast<int>(columns.size())-1);
    const float t=visual-static_cast<float>(lo);
    return static_cast<float>(columns[static_cast<size_t>(lo)])+
        (static_cast<float>(columns[static_cast<size_t>(hi)])-
         static_cast<float>(columns[static_cast<size_t>(lo)]))*t;
}

} // namespace

void NativeUi::resize(int width, int height) noexcept {
    width_ = std::max(0, width);
    height_ = std::max(0, height);
    NativeEditor::instance().resize(width_, height_);
    NativeEditor::instance().setSafeInsets(safeLeft_,safeTop_,safeRight_,safeBottom_);
}

void NativeUi::setSafeInsets(int left,int top,int right,int bottom) noexcept {
    safeLeft_=std::clamp(left,0,std::max(0,width_/2));
    safeTop_=std::clamp(top,0,std::max(0,height_/2));
    safeRight_=std::clamp(right,0,std::max(0,width_/2));
    safeBottom_=std::clamp(bottom,0,std::max(0,height_/2));
    NativeEditor::instance().setSafeInsets(safeLeft_,safeTop_,safeRight_,safeBottom_);
}

void NativeUi::setPitchActive(int midi, bool active) noexcept {
    if (midi < kGridLow || midi > kGridHigh) return;
    active_[static_cast<size_t>(midi - kGridLow)] = active;
}

void NativeUi::clearPitchActivity() noexcept {
    active_.fill(false);
}

void NativeUi::openDropdown(
    DropdownKind kind,int context,Rect anchor,int selected,
    std::vector<std::string> labels){
    dropdownKind_=kind;
    dropdownContext_=context;
    dropdownAnchor_=anchor;
    dropdownSelected_=selected;
    dropdownLabels_=std::move(labels);
    if(dropdownLabels_.empty())closeDropdown();
}

void NativeUi::closeDropdown() noexcept {
    dropdownKind_=DropdownKind::None;
    dropdownContext_=-1;
    dropdownSelected_=-1;
    dropdownAnchor_={};
    dropdownLabels_.clear();
}

int NativeUi::dropdownColumns() const noexcept {
    const int n=static_cast<int>(dropdownLabels_.size());
    if(n>20)return 3;
    if(n>10)return 2;
    return 1;
}

NativeUi::Rect NativeUi::dropdownPanelRect() const noexcept {
    const int n=std::max(1,static_cast<int>(dropdownLabels_.size()));
    const int cols=dropdownColumns();
    const int rows=(n+cols-1)/cols;
    const float margin=std::clamp(
        static_cast<float>(std::max(1,width_-safeLeft_-safeRight_))*0.018f,
        10.0f,18.0f);
    const float left=static_cast<float>(safeLeft_)+margin;
    const float right=static_cast<float>(width_-safeRight_)-margin;
    const float top=static_cast<float>(safeTop_)+margin;
    const float bottom=static_cast<float>(height_-safeBottom_)-margin;
    const float availableW=std::max(80.0f,right-left);
    const float availableH=std::max(80.0f,bottom-top);
    const float wantedCol=std::max(164.0f,dropdownAnchor_.w/std::max(1,cols));
    const float wantedW=std::max(dropdownAnchor_.w,wantedCol*cols+8.0f);
    const float w=std::min(availableW,wantedW);
    float itemH=std::clamp(dropdownAnchor_.h,46.0f,62.0f);
    itemH=std::min(itemH,std::max(32.0f,(availableH-8.0f)/rows));
    const float h=std::min(availableH,itemH*rows+8.0f);
    float x=std::clamp(dropdownAnchor_.x,left,std::max(left,right-w));
    if(x+w>right)x=right-w;
    const float below=dropdownAnchor_.y+dropdownAnchor_.h+5.0f;
    const float above=dropdownAnchor_.y-h-5.0f;
    float y=(below+h<=bottom)?below:above;
    y=std::clamp(y,top,std::max(top,bottom-h));
    return {x,y,w,h};
}

NativeUi::Rect NativeUi::dropdownItemRect(int index) const noexcept {
    const auto panel=dropdownPanelRect();
    const int n=std::max(1,static_cast<int>(dropdownLabels_.size()));
    const int cols=dropdownColumns();
    const int rows=(n+cols-1)/cols;
    const float pad=4.0f;
    const float colW=(panel.w-pad*2.0f)/cols;
    const float itemH=(panel.h-pad*2.0f)/rows;
    const int col=index%cols;
    const int row=index/cols;
    return {
        panel.x+pad+col*colW,
        panel.y+pad+row*itemH,
        colW,
        itemH
    };
}

bool NativeUi::openUiDropdownAt(float x,float y){
    auto& project=ProjectCore::instance();

    if(page_!=NativePage::Tracks&&trackSwitchRect(1).contains(x,y)){
        std::vector<std::string> labels;
        const int count=project.trackCount();
        labels.reserve(static_cast<size_t>(std::max(0,count)));
        for(int i=0;i<count;++i){
            std::string label=std::to_string(i+1)+" "+project.trackName(i);
            if(label.size()>26)label.resize(26);
            labels.push_back(std::move(label));
        }
        openDropdown(
            DropdownKind::Track,-1,trackSwitchRect(1),project.selectedTrack(),
            std::move(labels));
        return dropdownOpen();
    }

    if((page_==NativePage::Synth||page_==NativePage::Fx)){
        const int track=project.selectedTrack();
        const auto field=trackSwitchRect(6);
        if(track>=0&&project.trackIsDrums(track)&&field.contains(x,y)){
            std::vector<std::string> labels;
            const int count=project.padCount(track);
            labels.reserve(static_cast<size_t>(std::max(0,count)));
            for(int i=0;i<count;++i){
                std::string label=std::to_string(i+1)+" "+
                    drumIconName(project.padIcon(track,i))+" "+
                    pitchCoord(project.padCenter(track,i));
                if(label.size()>24)label.resize(24);
                labels.push_back(std::move(label));
            }
            openDropdown(
                DropdownKind::Pad,track,field,project.selectedPad(track),
                std::move(labels));
            return dropdownOpen();
        }
    }

    if(page_==NativePage::Tracks&&patchPresetRect().contains(x,y)){
        std::vector<std::string> labels{
            "Spectrachord Init","Subula","Spectrello","Nebular","Nexdrum"};
        const std::string current=project.selectedTrack()>=0
            ?project.trackPatchName(project.selectedTrack()):std::string{};
        int selected=0;
        for(int i=0;i<static_cast<int>(labels.size());++i)
            if(current==labels[static_cast<size_t>(i)]){selected=i;break;}
        if(project.selectedTrack()>=0&&project.trackIsDrums(project.selectedTrack()))
            selected=4;
        openDropdown(
            DropdownKind::Patch,project.selectedTrack(),patchPresetRect(),selected,
            std::move(labels));
        return true;
    }

    return false;
}

std::optional<DropdownChoice> NativeUi::hitDropdown(float x,float y){
    if(!dropdownOpen())return std::nullopt;
    for(int i=0;i<static_cast<int>(dropdownLabels_.size());++i){
        if(!dropdownItemRect(i).contains(x,y))continue;
        const DropdownChoice choice{dropdownKind_,dropdownContext_,i};
        closeDropdown();
        return choice;
    }
    return std::nullopt;
}

NativeUi::Rect NativeUi::headerPerformanceRect() const noexcept {
    const float usableW=std::max(0,width_-safeLeft_-safeRight_);
    const float margin=std::clamp(usableW*0.018f,10.0f,18.0f);
    const float headerH=std::clamp(usableW*0.142f,94.0f,116.0f);
    const float scopeH=std::clamp(headerH*0.56f,54.0f,66.0f);
    return {
        static_cast<float>(safeLeft_)+usableW-margin-scopeH,
        static_cast<float>(safeTop_)+(headerH-scopeH)*0.5f,
        scopeH,scopeH
    };
}

NativeUi::Rect NativeUi::headerScopeRect() const noexcept {
    const float usableW=std::max(0,width_-safeLeft_-safeRight_);
    const float margin=std::clamp(usableW*0.018f,10.0f,18.0f);
    const float gap=std::clamp(usableW*0.010f,7.0f,11.0f);
    const float buttonGap=std::clamp(usableW*0.010f,8.0f,12.0f);
    const float headerH=std::clamp(usableW*0.142f,94.0f,116.0f);
    const float brand=std::clamp(usableW*0.255f,190.0f,224.0f);
    const float play=std::clamp(headerH*0.76f,66.0f,82.0f);
    const float side=std::clamp(play*0.78f,52.0f,64.0f);
    const float transportW=side+buttonGap+play+buttonGap+side;
    const float left=
        static_cast<float>(safeLeft_)+margin+brand+gap+transportW+gap;
    const auto performance=headerPerformanceRect();
    const float right=performance.x-gap;
    const float scopeH=std::clamp(headerH*0.56f,54.0f,66.0f);
    return {
        left,
        static_cast<float>(safeTop_)+(headerH-scopeH)*0.5f,
        std::max(56.0f,right-left),
        scopeH
    };
}

NativeUi::Rect NativeUi::headerControlRect(int index) const noexcept {
    if(index<0||index>2)return {};
    const float usableW=std::max(0,width_-safeLeft_-safeRight_);
    const float margin=std::clamp(usableW*0.018f,10.0f,18.0f);
    const float gap=std::clamp(usableW*0.010f,7.0f,11.0f);
    const float buttonGap=std::clamp(usableW*0.010f,8.0f,12.0f);
    const float headerH=std::clamp(usableW*0.142f,94.0f,116.0f);
    const float brand=std::clamp(usableW*0.255f,190.0f,224.0f);
    const float play=std::clamp(headerH*0.76f,66.0f,82.0f);
    const float side=std::clamp(play*0.78f,52.0f,64.0f);
    const float startX=static_cast<float>(safeLeft_)+margin+brand+gap;
    if(index==0){
        return {
            startX,
            static_cast<float>(safeTop_)+(headerH-side)*0.5f,
            side,side
        };
    }
    if(index==1){
        return {
            startX+side+buttonGap,
            static_cast<float>(safeTop_)+(headerH-play)*0.5f,
            play,play
        };
    }
    return {
        startX+side+buttonGap+play+buttonGap,
        static_cast<float>(safeTop_)+(headerH-side)*0.5f,
        side,side
    };
}

NativeUi::Rect NativeUi::navRect(int index) const noexcept {
    const float usableW=std::max(0,width_-safeLeft_-safeRight_);
    const float margin=std::clamp(usableW*0.018f,10.0f,18.0f);
    const float headerH=std::clamp(usableW*0.142f,94.0f,116.0f);
    const float navH=std::clamp(usableW*0.106f,76.0f,92.0f);
    const float gap=std::clamp(usableW*0.010f,7.0f,11.0f);
    const float available=std::max(
        0.0f,usableW-margin*2.0f-gap*5.0f);
    const float buttonW=available/6.0f;
    return {
        static_cast<float>(safeLeft_)+margin+index*(buttonW+gap),
        static_cast<float>(safeTop_)+headerH+gap,
        buttonW,navH
    };
}

NativeUi::Rect NativeUi::contentRect() const noexcept {
    const float usableW=std::max(0,width_-safeLeft_-safeRight_);
    const float margin=std::clamp(usableW*0.018f,10.0f,18.0f);
    const auto nav=navRect(0);
    const float gap=std::clamp(usableW*0.016f,10.0f,16.0f);
    const float top=nav.y+nav.h+gap;
    const float bottom=static_cast<float>(height_-safeBottom_);
    return {
        static_cast<float>(safeLeft_)+margin,
        top,
        std::max(0.0f,usableW-margin*2.0f),
        std::max(0.0f,bottom-top-margin)
    };
}

NativeUi::Rect NativeUi::bodyContentRect() const noexcept {
    auto r=contentRect();
    if(page_==NativePage::Tracks||page_==NativePage::Performance)return r;
    const float gap=std::max(5.0f,std::min(width_,height_)*0.007f);
    const float h=trackSwitchRect(0).h;
    r.y+=h+gap;
    r.h=std::max(0.0f,r.h-h-gap);
    return r;
}

NativeUi::Rect NativeUi::pageScrollViewportRect() const noexcept {
    return (page_==NativePage::Tracks||page_==NativePage::Performance)
        ?contentRect():bodyContentRect();
}

NativeUi::Rect NativeUi::pageScrollGutterRect() const noexcept {
    const auto view=pageScrollViewportRect();
    const float gutter=std::min(
        rollGutterPixels(),
        std::max(0.0f,view.w-120.0f));
    return {view.x,view.y,gutter,view.h};
}

NativeUi::Rect NativeUi::pageScrollContentRect() const noexcept {
    const auto view=pageScrollViewportRect();
    const float gutter=std::min(
        rollGutterPixels(),
        std::max(0.0f,view.w-120.0f));
    return {
        view.x+gutter,
        view.y,
        std::max(0.0f,view.w-gutter),
        view.h
    };
}

NativeUi::Rect NativeUi::trackSwitchRect(int part) const noexcept {
    const auto content=contentRect();
    const float gap=std::max(5.0f,std::min(width_,height_)*0.007f);
    const float h=std::clamp(content.w*0.090f,68.0f,82.0f);
    const float prevW=h,nextW=h;

    if(page_==NativePage::Roll){
        const float modeW=h*0.82f;
        const float mainW=std::max(
            90.0f,
            content.w-prevW-nextW-modeW*4.0f-gap*6.0f);
        float x=content.x;
        if(part==0)return {x,content.y,prevW,h};
        x+=prevW+gap;
        if(part==1)return {x,content.y,mainW,h};
        x+=mainW+gap;
        if(part==2)return {x,content.y,nextW,h};
        x+=nextW+gap;
        if(part>=3&&part<=6)
            return {x+(part-3)*(modeW+gap),content.y,modeW,h};
        return {};
    }

    auto& project=ProjectCore::instance();
    const int track=project.selectedTrack();
    const bool padField=
        (page_==NativePage::Synth||page_==NativePage::Fx) &&
        track>=0 && project.trackIsDrums(track);
    float x=content.x;
    if(part==0)return {x,content.y,prevW,h};
    x+=prevW+gap;
    const float middle=std::max(90.0f,content.w-prevW-nextW-gap*3.0f);
    if(padField){
        const float trackW=middle*0.54f-gap*0.5f;
        const float padW=middle-trackW-gap;
        if(part==1)return {x,content.y,trackW,h};
        if(part==6)return {x+trackW+gap,content.y,padW,h};
        if(part==2)return {content.x+content.w-nextW,content.y,nextW,h};
    }else{
        if(part==1)return {x,content.y,middle,h};
        if(part==2)return {content.x+content.w-nextW,content.y,nextW,h};
    }
    return {};
}

NativeUi::Rect NativeUi::gridAreaRect() const noexcept {
    if(page_==NativePage::Drums){
        const auto pad=drumPadCardRect();
        const float outer=std::clamp(pad.w*0.022f,14.0f,20.0f);
        const float width=pad.w-outer*2.0f;
        return {
            pad.x+outer,
            pad.y+106.0f,
            width,width
        };
    }
    if(page_==NativePage::Play){
        const auto body=bodyContentRect();
        const float outer=std::clamp(body.w*0.022f,14.0f,20.0f);
        const float width=body.w-outer*2.0f;
        return {
            body.x+outer,
            body.y+64.0f,
            width,width
        };
    }
    return bodyContentRect();
}

NativeUi::Rect NativeUi::gridRect(int visualRow, int column) const noexcept {
    const auto area = gridAreaRect();
    const float gap = std::max(2.0f, std::min(width_, height_) * 0.006f);
    const float usableW = std::max(0.0f, area.w - gap * 6.0f);
    const float usableH = std::max(0.0f, area.h - gap * 6.0f);
    const float cell = std::max(1.0f, std::min(usableW / 7.0f, usableH / 7.0f));
    const float gridW = cell * 7.0f + gap * 6.0f;
    const float gridH = cell * 7.0f + gap * 6.0f;
    const float originX = area.x + (area.w - gridW) * 0.5f;
    const float originY = area.y + (area.h - gridH) * 0.5f;

    return {
        originX + column * (cell + gap),
        originY + visualRow * (cell + gap),
        cell,
        cell
    };
}

NativeUi::Rect NativeUi::trackCardRect() const noexcept {
    const auto content=pageScrollContentRect();
    const int count=std::max(0,ProjectCore::instance().trackCount());
    const float gap=std::clamp(content.w*0.014f,9.0f,13.0f);
    const float titleH=50.0f;
    const float rowH=std::clamp(content.w*0.175f,132.0f,152.0f);
    const float addH=74.0f;
    const float h=
        gap+titleH+gap+
        count*rowH+std::max(0,count-1)*gap+
        gap+addH+gap;
    return {content.x,content.y-trackScrollY_,content.w,h};
}

NativeUi::Rect NativeUi::patchCardRect() const noexcept {
    const auto prev=trackCardRect();
    const float gap=std::clamp(prev.w*0.014f,9.0f,13.0f);
    return {prev.x,prev.y+prev.h+gap,prev.w,174.0f};
}

NativeUi::Rect NativeUi::patchPresetRect() const noexcept {
    const auto card=patchCardRect();
    return {card.x+20.0f,card.y+96.0f,card.w-40.0f,56.0f};
}

NativeUi::Rect NativeUi::aiCardRect() const noexcept {
    const auto prev=patchCardRect();
    const float gap=std::clamp(prev.w*0.014f,9.0f,13.0f);
    return {prev.x,prev.y+prev.h+gap,prev.w,196.0f};
}

NativeUi::Rect NativeUi::songCardRect() const noexcept {
    const auto prev=aiCardRect();
    const float gap=std::clamp(prev.w*0.014f,9.0f,13.0f);
    return {prev.x,prev.y+prev.h+gap,prev.w,486.0f};
}

NativeUi::Rect NativeUi::masterCardRect() const noexcept {
    const auto prev=songCardRect();
    const float gap=std::clamp(prev.w*0.014f,9.0f,13.0f);
    return {prev.x,prev.y+prev.h+gap,prev.w,226.0f};
}

NativeUi::Rect NativeUi::trackSongSliderRect(int index) const noexcept {
    const auto card=songCardRect();
    const float pad=std::clamp(card.w*0.022f,14.0f,20.0f);
    if(index==0){
        return {card.x+pad,card.y+224.0f,card.w-pad*2.0f,58.0f};
    }
    const float gap=12.0f;
    const float w=(card.w-pad*2.0f-gap)*0.5f;
    return {
        card.x+pad+(index-1)*(w+gap),
        card.y+296.0f,
        w,58.0f
    };
}

NativeUi::Rect NativeUi::trackUtilityRect(int index) const noexcept {
    if(index==0){
        const auto card=songCardRect();
        const float pad=std::clamp(card.w*0.022f,14.0f,20.0f);
        return {card.x+pad,card.y+404.0f,card.w-pad*2.0f,56.0f};
    }
    if(index==1){
        const auto card=aiCardRect();
        const float pad=std::clamp(card.w*0.022f,14.0f,20.0f);
        return {
            card.x+card.w*0.64f,
            card.y+72.0f,
            card.w*0.32f-pad*0.2f,
            72.0f
        };
    }

    const auto card=songCardRect();
    const float pad=std::clamp(card.w*0.022f,14.0f,20.0f);
    const float gap=10.0f;
    const float w=(card.w-pad*2.0f-gap*2.0f)/3.0f;
    const float row1=card.y+64.0f;
    const float row2=card.y+138.0f;

    switch(index){
        case 2: return {card.x+pad+(w+gap),row1,w,64.0f}; // Clear
        case 3: return {card.x+pad,row1,w,64.0f};         // New
        case 4: return {card.x+pad+2.0f*(w+gap),row1,w,64.0f}; // Save
        case 5: return {card.x+pad,row2,w,64.0f};         // Load
        default:return {};
    }
}

NativeUi::Rect NativeUi::projectTransferRect(int index) const noexcept {
    const auto card=songCardRect();
    const float pad=std::clamp(card.w*0.022f,14.0f,20.0f);
    const float gap=10.0f;
    const float w=(card.w-pad*2.0f-gap*2.0f)/3.0f;
    return {
        card.x+pad+(index+1)*(w+gap),
        card.y+138.0f,w,64.0f
    };
}

NativeUi::Rect NativeUi::addTrackRect(TrackAddKind kind) const noexcept {
    if(kind==TrackAddKind::Drums)return {};
    const auto card=trackCardRect();
    const float gap=std::clamp(card.w*0.014f,9.0f,13.0f);
    return {
        card.x+gap,
        card.y+card.h-gap-74.0f,
        card.w-gap*2.0f,
        74.0f
    };
}

NativeUi::Rect NativeUi::masterSliderRect(int index) const noexcept {
    const auto card=masterCardRect();
    const float pad=std::clamp(card.w*0.022f,14.0f,20.0f);
    return {
        card.x+pad,
        card.y+64.0f+index*72.0f,
        card.w-pad*2.0f,
        58.0f
    };
}

NativeUi::Rect NativeUi::trackRect(int index,int count) const noexcept {
    const auto card=trackCardRect();
    const float gap=std::clamp(card.w*0.014f,9.0f,13.0f);
    const float titleH=50.0f;
    const float top=card.y+gap+titleH+gap;
    const float rowH=std::clamp(card.w*0.175f,132.0f,152.0f);
    return {
        card.x+gap,
        top+index*(rowH+gap),
        card.w-gap*2.0f,
        rowH
    };
}

NativeUi::Rect NativeUi::trackPartRect(int index,int count,int part) const noexcept {
    const auto r=trackRect(index,count);
    const float gap=std::clamp(r.w*0.010f,6.0f,10.0f);
    const float bottomY=r.y+r.h*0.58f;
    const float button=std::clamp(r.h*0.28f,36.0f,44.0f);

    if(part==2)return {r.x+12.0f,bottomY,button,button};
    if(part==3)return {r.x+12.0f+button+gap,bottomY,button,button};
    if(part==4)return {
        r.x+r.w-button-12.0f,
        r.y+10.0f,
        button,button
    };

    const float controlsX=r.x+12.0f+button*2.0f+gap*2.0f+10.0f;
    const float controlsRight=r.x+r.w-12.0f;
    const float sliderGap=12.0f;
    const float sliderW=(controlsRight-controlsX-sliderGap)*0.5f;
    return {
        controlsX+(part==1?(sliderW+sliderGap):0.0f),
        bottomY,
        sliderW,button
    };
}

NativeUi::Rect NativeUi::trackReorderRect(int index,int count) const noexcept {
    const auto r=trackRect(index,count);
    const float button=std::clamp(r.h*0.28f,36.0f,44.0f);
    const float gap=std::clamp(r.w*0.010f,6.0f,10.0f);
    const auto del=trackPartRect(index,count,4);
    return {
        del.x-button-gap,
        del.y,
        button,button
    };
}

NativeUi::Rect NativeUi::drumKitCardRect() const noexcept {
    const auto view=pageScrollContentRect();
    auto& project=ProjectCore::instance();
    const int track=project.selectedTrack();
    const int count=(track>=0&&project.trackIsDrums(track))
        ?project.padCount(track):0;
    const float gap=std::clamp(view.w*0.014f,9.0f,13.0f);
    const float rowH=std::clamp(view.w*0.185f,138.0f,160.0f);
    const float headerH=132.0f;
    const float addH=74.0f;
    const float h=
        gap+headerH+
        count*rowH+std::max(0,count-1)*gap+
        gap+addH+gap;
    return {view.x,view.y-drumScrollY_,view.w,h};
}

NativeUi::Rect NativeUi::drumPadCardRect() const noexcept {
    const auto kit=drumKitCardRect();
    const float gap=std::clamp(kit.w*0.014f,9.0f,13.0f);
    const float pad=std::clamp(kit.w*0.022f,14.0f,20.0f);
    const float gridW=kit.w-pad*2.0f;
    const float gridGap=std::clamp(kit.w*0.009f,6.0f,9.0f);
    const float gridCell=(gridW-gridGap*6.0f)/7.0f;
    const float gridH=gridCell*7.0f+gridGap*6.0f;
    const float iconsH=2.0f*94.0f+12.0f;
    const float h=106.0f+gridH+22.0f+iconsH+22.0f+72.0f+24.0f;
    return {kit.x,kit.y+kit.h+gap,kit.w,h};
}

NativeUi::Rect NativeUi::padQuickRect(int index, int count) const noexcept {
    const auto card=drumKitCardRect();
    const float gap=std::clamp(card.w*0.014f,9.0f,13.0f);
    const float rowH=std::clamp(card.w*0.185f,138.0f,160.0f);
    const float top=card.y+gap+132.0f;
    return {
        card.x+gap,
        top+index*(rowH+gap),
        card.w-gap*2.0f,
        rowH
    };
}

NativeUi::Rect NativeUi::padReorderRect(int index,int count) const noexcept {
    const auto row=padQuickRect(index,count);
    return {
        row.x+row.w-118.0f,
        row.y+18.0f,
        44.0f,44.0f
    };
}

NativeUi::Rect NativeUi::drumEditorRect() const noexcept {
    return drumPadCardRect();
}

NativeUi::Rect NativeUi::drumActionRect(int index) const noexcept {
    const auto kit=drumKitCardRect();
    const auto pad=drumPadCardRect();
    const float gap=std::clamp(pad.w*0.014f,9.0f,13.0f);
    if(index==0){
        return {pad.x+110.0f,pad.y+20.0f,156.0f,58.0f};
    }
    if(index==1){
        return {
            kit.x+gap,
            kit.y+kit.h-gap-74.0f,
            kit.w-gap*2.0f,
            74.0f
        };
    }
    const float bottomY=pad.y+pad.h-gap-72.0f;
    const float w=(pad.w-gap*3.0f)*0.5f;
    if(index==2)return {pad.x+gap,bottomY,w,72.0f};
    return {pad.x+gap*2.0f+w,bottomY,w,72.0f};
}

NativeUi::Rect NativeUi::drumSliderRect(int index) const noexcept {
    auto& project=ProjectCore::instance();
    const int track=project.selectedTrack();
    const int padIndex=(track>=0&&project.trackIsDrums(track))
        ?project.selectedPad(track):-1;
    const int count=(track>=0&&project.trackIsDrums(track))
        ?project.padCount(track):0;
    if(padIndex<0||padIndex>=count)return {};
    const auto row=padQuickRect(padIndex,count);
    const float sliderLeft=row.x+18.0f;
    const float available=row.w-36.0f;
    const float pairGap=24.0f;
    const float pairW=(available-pairGap)*0.5f;
    return {
        sliderLeft+(index==1?(pairW+pairGap):0.0f),
        row.y+row.h*0.61f,
        pairW,
        row.h*0.34f
    };
}

NativeUi::Rect NativeUi::drumIconRect(int index) const noexcept {
    const auto pad=drumPadCardRect();
    const float gap=8.0f;
    const float outer=std::clamp(pad.w*0.022f,14.0f,20.0f);
    const int row=index/6,col=index%6;
    const float w=(pad.w-outer*2.0f-gap*5.0f)/6.0f;

    const float gridGap=std::clamp(pad.w*0.009f,6.0f,9.0f);
    const float gridW=pad.w-outer*2.0f;
    const float gridCell=(gridW-gridGap*6.0f)/7.0f;
    const float gridH=gridCell*7.0f+gridGap*6.0f;
    const float top=pad.y+106.0f+gridH+22.0f;
    return {
        pad.x+outer+col*(w+gap),
        top+row*(94.0f+12.0f),
        w,94.0f
    };
}

NativeUi::Rect NativeUi::rollSelectionToolRect() const noexcept {
    return trackSwitchRect(3);
}

NativeUi::Rect NativeUi::rollModeRect(int index) const noexcept {
    if(index<0||index>2)return {};
    return trackSwitchRect(index+4);
}

NativeUi::Rect NativeUi::rollGroupAutomationRect() const noexcept {
    if(rollGroupAutomationKind_<1||rollGroupAutomationKind_>2||
       !rollNoteSelectionActive())return {};
    const auto viewport=rollViewportRect();
    const float rowH=rollCellPixels();
    const float header=rollHeaderPixels();
    const float width=std::clamp(rollGutterPixels()*0.70f,52.0f,78.0f);
    const float gridTop=viewport.y+header;
    const float gridBottom=viewport.y+viewport.h;
    float top=gridTop+
        (rollGroupAutomationStart_+0.5f-
         static_cast<float>(rollStepOffset_))*rowH;
    float bottom=gridTop+
        (rollGroupAutomationEnd_+0.5f-
         static_cast<float>(rollStepOffset_))*rowH;
    if(bottom<top)std::swap(top,bottom);
    if(std::fabs(bottom-top)<rowH*0.75f)bottom=top+rowH*0.75f;
    top=std::max(gridTop,top);
    bottom=std::min(gridBottom,bottom);
    if(bottom<=top)return {};
    return {viewport.x+viewport.w-width,top,width,bottom-top};
}

NativeUi::Rect NativeUi::rollViewportRect() const noexcept {
    return bodyContentRect();
}

float NativeUi::rollCellPixels() const noexcept {
    const float shortSide=static_cast<float>(std::min(
        std::max(1,width_-safeLeft_-safeRight_),
        std::max(1,height_-safeTop_-safeBottom_)));
    return std::clamp(shortSide*0.076f,54.0f,68.0f);
}

float NativeUi::rollColumnPixels() const noexcept {
    const auto viewport=rollViewportRect();
    const float available=std::max(1.0f,viewport.w-rollGutterPixels());
    const float preferred=rollCellPixels();
    const int visibleCols=std::max(
        1,static_cast<int>(std::floor(available/preferred)));
    return available/static_cast<float>(visibleCols);
}

float NativeUi::rollGutterPixels() const noexcept {
    return std::clamp(rollCellPixels()*1.55f,78.0f,108.0f);
}

float NativeUi::rollHeaderPixels() const noexcept {
    auto& project=ProjectCore::instance();
    const int track=project.selectedTrack();
    if(track>=0&&project.trackIsDrums(track)){
        return std::clamp(rollCellPixels()*1.95f,104.0f,132.0f);
    }
    return std::clamp(rollCellPixels()*1.32f,72.0f,96.0f);
}

std::vector<int> NativeUi::rollColumns() const {
    auto& project = ProjectCore::instance();
    const int track = project.selectedTrack();
    std::vector<int> columns;

    if (track >= 0 && project.trackIsDrums(track)) {
        const int pads = project.padCount(track);
        for (int p = 0; p < pads; ++p) {
            int lo = project.padLow(track, p);
            int hi = project.padHigh(track, p);
            if (lo > hi) std::swap(lo, hi);
            if (lo < 0 || hi < 0) continue;
            for (int midi = lo; midi <= hi; ++midi) columns.push_back(midi);
        }
        std::sort(columns.begin(), columns.end());
        columns.erase(std::unique(columns.begin(), columns.end()), columns.end());
    }

    if (columns.empty()) {
        columns.reserve(97);
        for (int midi = 14; midi <= 110; ++midi) columns.push_back(midi);
    }
    return columns;
}

int NativeUi::rollTotalRows() const noexcept {
    return std::max(64, ProjectCore::instance().playLengthSteps() + 32);
}

float NativeUi::performanceHeaderPixels() const noexcept {
    return std::clamp(
        std::min(width_,height_)*0.052f,
        38.0f,52.0f);
}

float NativeUi::performanceRowPixels() const noexcept {
    const auto view=contentRect();
    const float available=std::max(
        1.0f,view.h-performanceHeaderPixels());
    return std::clamp(available/64.0f,14.0f,22.0f);
}

float NativeUi::performanceStartStep() const noexcept {
    const auto view=contentRect();
    const float header=performanceHeaderPixels();
    const float rowH=performanceRowPixels();
    const int visibleRows=std::max(
        1,static_cast<int>(std::floor(
            (view.h-header)/std::max(1.0f,rowH))));
    auto& project=ProjectCore::instance();
    const int total=std::max(
        64,project.playLengthSteps()+
           std::max(1,project.beats()*project.divisions()));
    const float maxStart=
        static_cast<float>(std::max(0,total-visibleRows));
    if(performanceFollow_){
        const float anchor=static_cast<float>(visibleRows)*0.28f;
        return std::clamp(
            AudioEngine::instance().playheadPosition()-anchor,
            0.0f,maxStart);
    }
    return std::clamp(performanceScrollStep_,0.0f,maxStart);
}

void NativeUi::togglePerformancePage() noexcept {
    closeDropdown();
    if(page_==NativePage::Performance){
        page_=performanceReturnPage_;
        return;
    }
    performanceReturnPage_=page_;
    performanceFollow_=true;
    page_=NativePage::Performance;
}

void NativeUi::scrollRoll(int pitchDelta, int stepDelta) noexcept {
    const auto columns=rollColumns();
    const auto viewport=rollViewportRect();
    const float rowH=rollCellPixels();
    const float colW=rollColumnPixels();
    const int visibleCols=std::max(
        1,static_cast<int>(std::lround(
            (viewport.w-rollGutterPixels())/colW)));
    const int visibleRows=std::max(
        1,static_cast<int>(std::floor(
            (viewport.h-rollHeaderPixels())/rowH)));

    const int maxPitch=std::max(
        0,static_cast<int>(columns.size())-visibleCols);
    const int maxStep=std::max(0,rollTotalRows()-visibleRows);
    rollPitchOffset_=std::clamp(rollPitchOffset_+pitchDelta,0,maxPitch);
    rollStepOffset_=std::clamp(rollStepOffset_+stepDelta,0,maxStep);
}

void NativeUi::setRollStartStep(int step) noexcept {
    rollStartStep_=std::max(0,step);
}

void NativeUi::setRollSelection(bool active,int anchorStep,int endStep) noexcept {
    if(active){
        if(!rollSelectionActive_)rollRangeAllTracks_=false;
        clearRollNoteSelection();
    }else{
        rollRangeAllTracks_=false;
    }
    rollSelectionActive_=active;
    rollSelectionAnchorStep_=std::max(0,anchorStep);
    rollSelectionEndStep_=std::max(0,endStep);
}

void NativeUi::setRollNoteSelection(int track,std::vector<int> indices){
    indices.erase(
        std::remove_if(
            indices.begin(),indices.end(),
            [](int i){return i<0;}),
        indices.end());
    std::sort(indices.begin(),indices.end());
    indices.erase(std::unique(indices.begin(),indices.end()),indices.end());
    if(indices.empty()){
        clearRollNoteSelection();
        return;
    }
    rollSelectionActive_=false;
    rollRangeAllTracks_=false;
    rollSelectedTrack_=track;
    rollSelectedNotes_=std::move(indices);
    clearRollGroupAutomation();
}

void NativeUi::clearRollNoteSelection() noexcept {
    rollSelectedTrack_=-1;
    rollSelectedNotes_.clear();
    clearRollGroupAutomation();
}

bool NativeUi::rollNoteSelected(int noteIndex) const noexcept {
    return rollSelectedTrack_>=0&&
        std::binary_search(
            rollSelectedNotes_.begin(),rollSelectedNotes_.end(),noteIndex);
}

void NativeUi::beginRollLasso(float x,float y){
    rollLassoPath_.clear();
    rollLassoPath_.push_back({x,y});
}

void NativeUi::appendRollLasso(float x,float y){
    if(rollLassoPath_.empty()){
        rollLassoPath_.push_back({x,y});
        return;
    }
    const auto& last=rollLassoPath_.back();
    const float dx=x-last.x,dy=y-last.y;
    if(dx*dx+dy*dy>=16.0f)
        rollLassoPath_.push_back({x,y});
}

std::vector<int> NativeUi::finishRollLasso(){
    std::vector<int> hits;
    const auto polygon=rollLassoPath_;
    rollLassoPath_.clear();
    if(polygon.size()<3)return hits;

    auto& project=ProjectCore::instance();
    const int track=project.selectedTrack();
    if(track<0)return hits;
    const auto snapshot=project.projectCopy();
    if(track>=static_cast<int>(snapshot.tracks.size()))return hits;
    const auto& sourceTrack=snapshot.tracks[static_cast<size_t>(track)];

    const auto viewport=rollViewportRect();
    const float colW=rollColumnPixels();
    const float rowH=rollCellPixels();
    const float gridLeft=viewport.x+rollGutterPixels();
    const float gridTop=viewport.y+rollHeaderPixels();
    const auto columns=rollColumns();
    if(columns.empty())return hits;

    for(int n=0;n<static_cast<int>(sourceTrack.notes.size());++n){
        const auto& note=sourceTrack.notes[static_cast<size_t>(n)];
        const float length=std::max(1.0f,note.lengthSteps);
        bool selected=false;
        constexpr int samples=10;
        for(int s=0;s<=samples&&!selected;++s){
            const float fraction=
                static_cast<float>(s)/static_cast<float>(samples);
            const float py=gridTop+
                (note.startStep+fraction*length-
                 static_cast<float>(rollStepOffset_))*rowH;
            if(py<gridTop-rowH||py>viewport.y+viewport.h+rowH)continue;
            const float rel=rollVisualCurveStep(note,fraction);
            const float px=rollPitchX(
                columns,
                static_cast<float>(note.midi)+
                    rollBendAt(sourceTrack,note,rel),
                rollPitchOffset_,gridLeft,colW);
            selected=rollPointInPolygon(polygon,px,py);
        }
        if(selected)hits.push_back(n);
    }
    return hits;
}

void NativeUi::setRollGroupAutomation(
    int kind,float start,float end,std::vector<RollGroupPoint> points){
    rollGroupAutomationKind_=kind;
    rollGroupAutomationStart_=std::max(0.0f,start);
    rollGroupAutomationEnd_=std::max(rollGroupAutomationStart_,end);
    for(auto& p:points){
        p.step=std::clamp(
            p.step,0.0f,
            std::max(0.0f,rollGroupAutomationEnd_-rollGroupAutomationStart_));
        p.value=std::clamp(p.value,0.0f,1.0f);
    }
    std::sort(
        points.begin(),points.end(),
        [](const RollGroupPoint&a,const RollGroupPoint&b){
            return a.step<b.step;
        });
    rollGroupAutomationPoints_=std::move(points);
}

void NativeUi::clearRollGroupAutomation() noexcept {
    rollGroupAutomationKind_=-1;
    rollGroupAutomationStart_=0.0f;
    rollGroupAutomationEnd_=0.0f;
    rollGroupAutomationPoints_.clear();
}

bool NativeUi::hitRollGroupAutomationGutter(float x,float y) const noexcept {
    const auto r=rollGroupAutomationRect();
    return r.w>0.0f&&r.h>0.0f&&r.contains(x,y);
}

std::optional<int> NativeUi::hitRollGroupAutomationPoint(
    float x,float y) const noexcept {
    if(!hitRollGroupAutomationGutter(x,y))return std::nullopt;
    const auto r=rollGroupAutomationRect();
    const auto viewport=rollViewportRect();
    const float rowH=rollCellPixels();
    const float gridTop=viewport.y+rollHeaderPixels();
    const float radius=std::max(16.0f,std::min(r.w,rowH)*0.34f);
    const float radius2=radius*radius;
    int best=-1;
    float bestDist=radius2;
    for(int i=0;i<static_cast<int>(rollGroupAutomationPoints_.size());++i){
        const auto& p=rollGroupAutomationPoints_[static_cast<size_t>(i)];
        const float px=r.x+p.value*r.w;
        const float py=gridTop+
            (rollGroupAutomationStart_+p.step+0.5f-
             static_cast<float>(rollStepOffset_))*rowH;
        const float dx=px-x,dy=py-y;
        const float d=dx*dx+dy*dy;
        if(d<=bestDist){bestDist=d;best=i;}
    }
    if(best<0)return std::nullopt;
    return best;
}

bool NativeUi::rollGroupAutomationPosition(
    float x,float y,float& step,float& value) const noexcept {
    const auto r=rollGroupAutomationRect();
    if(r.w<=0.0f||r.h<=0.0f)return false;
    const auto viewport=rollViewportRect();
    const float rowH=rollCellPixels();
    const float gridTop=viewport.y+rollHeaderPixels();
    const float absolute=
        static_cast<float>(rollStepOffset_)+
        (y-gridTop)/std::max(1.0f,rowH)-0.5f;
    const float duration=
        std::max(0.0f,rollGroupAutomationEnd_-rollGroupAutomationStart_);
    step=std::clamp(
        absolute-rollGroupAutomationStart_,0.0f,duration);
    value=std::clamp((x-r.x)/std::max(1.0f,r.w),0.0f,1.0f);
    return true;
}


void NativeUi::scrollPage(float deltaPixels) noexcept {
    if(page_==NativePage::Tracks){
        const auto view=contentRect();
        const auto bottom=masterCardRect();
        const float total=(bottom.y+trackScrollY_+bottom.h)-view.y;
        const float maxScroll=std::max(0.0f,total-view.h);
        trackScrollY_=std::clamp(
            trackScrollY_+deltaPixels,0.0f,maxScroll);
    }else if(page_==NativePage::Drums){
        const auto view=bodyContentRect();
        const auto bottom=drumPadCardRect();
        const float total=(bottom.y+drumScrollY_+bottom.h)-view.y;
        const float maxScroll=std::max(0.0f,total-view.h);
        drumScrollY_=std::clamp(
            drumScrollY_+deltaPixels,0.0f,maxScroll);
    }else if(page_==NativePage::Performance){
        const auto view=contentRect();
        const float header=performanceHeaderPixels();
        const float rowH=performanceRowPixels();
        const int visibleRows=std::max(
            1,static_cast<int>(std::floor(
                (view.h-header)/std::max(1.0f,rowH))));
        const int total=std::max(
            64,ProjectCore::instance().playLengthSteps()+
               std::max(1,ProjectCore::instance().beats()*
                          ProjectCore::instance().divisions()));
        const float maxStart=
            static_cast<float>(std::max(0,total-visibleRows));
        if(performanceFollow_){
            performanceScrollStep_=performanceStartStep();
            performanceFollow_=false;
        }
        performanceScrollStep_=std::clamp(
            performanceScrollStep_+
                deltaPixels/std::max(1.0f,rowH),
            0.0f,maxStart);
    }
}

bool NativeUi::hitScrollableBody(float x,float y) const noexcept {
    if(page_==NativePage::Performance)
        return contentRect().contains(x,y);
    if(page_!=NativePage::Tracks&&page_!=NativePage::Drums)return false;
    return pageScrollGutterRect().contains(x,y);
}

NativeUi::Rgb NativeUi::pitchColor(int midi) const noexcept {
    const int pc = ((midi % 12) + 12) % 12;
    return kPitchColors[static_cast<size_t>(pc)];
}

void NativeUi::drawPageScrollGutter() const noexcept {
    if(page_!=NativePage::Tracks&&page_!=NativePage::Drums)return;
    const auto gutter=pageScrollGutterRect();
    if(gutter.w<=0.0f||gutter.h<=0.0f)return;

    auto& ov=NativeOverlay::instance();
    constexpr int bands=72;
    for(int i=0;i<bands;++i){
        const float t0=static_cast<float>(i)/static_cast<float>(bands);
        const float t1=static_cast<float>(i+1)/static_cast<float>(bands);
        const float p=t0*11.0f;
        const int idx=std::clamp(static_cast<int>(std::floor(p)),0,10);
        const float local=p-static_cast<float>(idx);
        const auto spectral=mix(
            kPitchColors[static_cast<size_t>(idx)],
            kPitchColors[static_cast<size_t>(idx+1)],
            local);
        const auto color=mix(kBg,spectral,0.18f);
        const float y0=gutter.y+gutter.h*t0;
        const float y1=gutter.y+gutter.h*t1;
        ov.addRect(
            {gutter.x,y0,gutter.w,std::max(1.0f,y1-y0+0.5f)},
            overlayColor(color));
    }

    // Subtle dark edge keeps the spectrum lane visually separate from controls.
    ov.addRect(
        {gutter.x+gutter.w-2.0f,gutter.y,2.0f,gutter.h},
        overlayColor(mix(kBg,kPanel,0.65f)));
}

void NativeUi::fillRect(const Rect& rect, Rgb color) const noexcept {
    if (rect.w <= 0.0f || rect.h <= 0.0f || width_ <= 0 || height_ <= 0) return;
    NativeOverlay::Rect clipped{rect.x,rect.y,rect.w,rect.h};
    if(!NativeOverlay::instance().clipRect(clipped))return;

    const int x = std::max(0, static_cast<int>(clipped.x));
    const int top = std::max(0, static_cast<int>(clipped.y));
    const int w = std::max(0, std::min(width_ - x, static_cast<int>(clipped.w)));
    const int h = std::max(0, std::min(height_ - top, static_cast<int>(clipped.h)));
    if (w <= 0 || h <= 0) return;

    const int glY = height_ - top - h;
    glScissor(x, glY, w, h);
    glClearColor(color.r, color.g, color.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void NativeUi::drawDropdown() const noexcept {
    if(!dropdownOpen()||dropdownLabels_.empty())return;
    auto& ov=NativeOverlay::instance();
    const auto panel=dropdownPanelRect();
    fillRect(panel,kCyan);
    const float border=2.0f;
    fillRect({
        panel.x+border,panel.y+border,
        std::max(0.0f,panel.w-border*2.0f),
        std::max(0.0f,panel.h-border*2.0f)},kTop);

    for(int i=0;i<static_cast<int>(dropdownLabels_.size());++i){
        auto rr=dropdownItemRect(i);
        const float gap=2.0f;
        rr={rr.x+gap,rr.y+gap,
            std::max(0.0f,rr.w-gap*2.0f),
            std::max(0.0f,rr.h-gap*2.0f)};
        const bool selected=i==dropdownSelected_;
        fillRect(rr,selected?mix(kButton,kCyan,0.62f):kButton);
        std::string label=dropdownLabels_[static_cast<size_t>(i)];
        const size_t maxChars=dropdownColumns()==1?28:18;
        if(label.size()>maxChars)label.resize(maxChars);
        ov.addTextCentered(
            label,{rr.x,rr.y,rr.w,rr.h},
            std::clamp(rr.h/50.0f,0.78f,1.08f),
            overlayColor(selected?kBg:kWhite));
    }
}

void NativeUi::drawScope() const noexcept {
    const auto r=headerScopeRect();
    if(r.w<56.0f||r.h<20.0f)return;

    std::array<float,AudioEngine::kScopeReadSamples> samples{};
    AudioEngine::instance().copyScope(samples);
    auto& ov=NativeOverlay::instance();

    fillRect(r,mix(kTop,kBg,0.70f));

    // Quiet center reference only; the waveform is the primary visual.
    fillRect({
        r.x,
        r.y+r.h*0.5f-0.5f,
        r.w,1.0f},
        mix(kMuted,kTop,0.70f));

    float peak=0.0f;
    for(float s:samples)peak=std::max(peak,std::fabs(s));
    // Keep quiet signals readable without pumping loud material beyond the box.
    const float gain=peak>0.0005f
        ?std::clamp(0.82f/peak,1.0f,10.0f)
        :1.0f;

    // Trigger on a rising zero crossing in the first half of the buffer.
    size_t trigger=0;
    float bestSlope=0.0f;
    const size_t searchEnd=samples.size()/2;
    for(size_t i=1;i<searchEnd;++i){
        if(samples[i-1]<=0.0f&&samples[i]>0.0f){
            const float slope=samples[i]-samples[i-1];
            if(slope>bestSlope){
                bestSlope=slope;
                trigger=i;
            }
        }
    }

    const size_t available=samples.size()-trigger;
    if(available<2)return;

    const int points=std::clamp(
        static_cast<int>(r.w/2.0f),
        64,
        static_cast<int>(available));

    auto sampleAt=[&](float t){
        const float pos=
            static_cast<float>(trigger)+
            t*static_cast<float>(available-1);
        const size_t i0=std::min(
            samples.size()-1,
            static_cast<size_t>(std::floor(pos)));
        const size_t i1=std::min(samples.size()-1,i0+1);
        const float f=pos-static_cast<float>(i0);
        return samples[i0]+(samples[i1]-samples[i0])*f;
    };

    float prevX=r.x;
    float prevY=r.y+r.h*0.5f-
        std::clamp(sampleAt(0.0f)*gain,-1.0f,1.0f)*(r.h*0.43f);

    for(int i=1;i<points;++i){
        const float t=static_cast<float>(i)/static_cast<float>(points-1);
        const float x=r.x+t*r.w;
        const float s=std::clamp(sampleAt(t)*gain,-1.0f,1.0f);
        const float y=r.y+r.h*0.5f-s*(r.h*0.43f);
        const auto color=pitchColor((i*12)/std::max(1,points-1));

        // Soft glow plus crisp center trace.
        ov.addLine(
            prevX,prevY,x,y,
            4.5f,
            overlayColor(mix(kTop,color,0.58f),0.62f));
        ov.addLine(
            prevX,prevY,x,y,
            1.8f,
            overlayColor(mix(color,kWhite,0.12f),0.98f));

        prevX=x;
        prevY=y;
    }
}

void NativeUi::drawTrackSwitchBar() const noexcept {
    if(page_==NativePage::Tracks)return;
    auto& project=ProjectCore::instance();
    const int track=project.selectedTrack();
    if(track<0||track>=project.trackCount())return;

    auto& ov=NativeOverlay::instance();
    const auto prev=trackSwitchRect(0);
    const auto field=trackSwitchRect(1);
    const auto next=trackSwitchRect(2);

    fillRect(prev,kButton);
    fillRect(next,kButton);
    ov.addTextCentered(
        "◀",{prev.x,prev.y,prev.w,prev.h},
        1.15f,overlayColor(kWhite));
    ov.addTextCentered(
        "▶",{next.x,next.y,next.w,next.h},
        1.15f,overlayColor(kWhite));

    fillRect(field,kButton);
    const float inset=std::max(2.0f,field.h*0.055f);
    fillRect({field.x+inset,field.y+inset,field.w-inset*2.0f,field.h-inset*2.0f},kRollBg);
    std::string label=std::to_string(track+1)+" "+project.trackName(track);
    if(label.size()>24)label.resize(24);
    ov.addText(
        label,
        field.x+field.h*0.28f,
        field.y+field.h*0.35f,
        std::max(0.72f,field.h/40.0f),
        overlayColor(kWhite));
    ov.addDownChevron(
        {field.x+field.w-field.h*0.72f,field.y,field.h*0.72f,field.h},
        overlayColor(kWhite));

    if((page_==NativePage::Synth||page_==NativePage::Fx)&&project.trackIsDrums(track)){
        const int pad=project.selectedPad(track);
        const auto padR=trackSwitchRect(6);
        fillRect(padR,kButton);
        fillRect({padR.x+inset,padR.y+inset,padR.w-inset*2.0f,padR.h-inset*2.0f},kRollBg);
        std::string pLabel="Pad";
        if(pad>=0&&pad<project.padCount(track)){
            pLabel=std::to_string(pad+1)+" "+project.padIcon(track,pad);
        }
        ov.addText(
            pLabel,
            padR.x+padR.h*0.20f,
            padR.y+padR.h*0.35f,
            std::max(0.65f,padR.h/42.0f),
            overlayColor(kWhite));
        ov.addDownChevron(
            {padR.x+padR.w-padR.h*0.72f,padR.y,padR.h*0.72f,padR.h},
            overlayColor(kWhite));
    }

    if(page_==NativePage::Roll){
        const bool rangeSelected=
            rollSelectionActive_&&
            rollSelectionAnchorStep_!=rollSelectionEndStep_;
        const bool noteSelected=
            rollNoteSelectionActive()&&rollSelectedTrack_==track;
        const auto tool=rollSelectionToolRect();

        if(rangeSelected){
            fillRect(tool,rollRangeAllTracks_?kCyan:kButton);
            ov.addTextCentered(
                rollRangeAllTracks_?"ALL":"1T",
                {tool.x,tool.y,tool.w,tool.h},
                std::max(0.70f,tool.h/42.0f),
                overlayColor(rollRangeAllTracks_?kBg:kWhite));
        }else{
            const bool lasso=rollSelectionTool_==RollSelectionTool::Lasso;
            fillRect(tool,lasso?kCyan:kButton);
            const auto col=overlayColor(lasso?kBg:kWhite);
            const float thick=std::max(2.0f,tool.h*0.042f);

            const auto drawLasso=[&](float cx,float cy,float rx,float ry){
                float px=cx+rx,py=cy;
                for(int s=1;s<=18;++s){
                    const float a=
                        static_cast<float>(s)*6.28318530718f/18.0f;
                    const float x=cx+std::cos(a)*rx;
                    const float y=cy+std::sin(a)*ry;
                    ov.addLine(px,py,x,y,thick,col);
                    px=x;py=y;
                }
                ov.addLine(
                    cx+rx*0.72f,cy+ry*0.70f,
                    cx+rx*1.08f,cy+ry*1.18f,
                    thick,col);
            };
            const auto drawPencil=[&](float cx,float cy,float scale){
                const float dx=scale*0.42f,dy=scale*0.34f;
                ov.addLine(cx-dx,cy+dy,cx+dx,cy-dy,thick*1.5f,col);
                ov.addLine(
                    cx+dx,cy-dy,
                    cx+dx+scale*0.16f,cy-dy-scale*0.16f,
                    thick,col);
                ov.addLine(
                    cx-dx,cy+dy,
                    cx-dx-scale*0.12f,cy+dy+scale*0.12f,
                    thick,col);
            };

            if(lasso){
                drawLasso(
                    tool.x+tool.w*0.50f,tool.y+tool.h*0.40f,
                    tool.w*0.24f,tool.h*0.20f);
                drawPencil(
                    tool.x+tool.w*0.50f,tool.y+tool.h*0.76f,
                    tool.h*0.19f);
                if(rollMultiLasso_){
                    ov.addText(
                        "+",
                        tool.x+tool.w*0.72f,
                        tool.y+tool.h*0.10f,
                        0.72f,col);
                }
            }else{
                drawPencil(
                    tool.x+tool.w*0.50f,tool.y+tool.h*0.37f,
                    tool.h*0.34f);
                drawLasso(
                    tool.x+tool.w*0.50f,tool.y+tool.h*0.76f,
                    tool.w*0.13f,tool.h*0.10f);
            }
        }

        const RollMode modes[3]{RollMode::Bend,RollMode::Velocity,RollMode::Mod};
        const bool lassoMode=
            !rangeSelected&&
            rollSelectionTool_==RollSelectionTool::Lasso;
        for(int i=0;i<3;++i){
            const auto rr=rollModeRect(i);
            const bool deleteButton=i==0&&(rangeSelected||noteSelected);
            const bool disabled=lassoMode&&(i==1||i==2);
            const bool active=
                !deleteButton&&!disabled&&rollMode_==modes[i];
            fillRect(
                rr,
                deleteButton?kRed:
                disabled?mix(kButton,kRollBg,0.58f):
                active?kCyan:kButton);
            const auto col=overlayColor(
                disabled?kMuted:(active?kBg:kWhite));
            if(i==0){
                if(deleteButton){
                    const float w=rr.w*0.30f,h=rr.h*0.35f;
                    const float x=rr.x+(rr.w-w)*0.5f;
                    const float y=rr.y+rr.h*0.37f;
                    ov.addRect({x,y,w,h},overlayColor(kWhite));
                    ov.addLine(
                        x-w*0.10f,y-h*0.16f,
                        x+w*1.10f,y-h*0.16f,
                        std::max(2.0f,rr.h*0.045f),
                        overlayColor(kWhite));
                    ov.addLine(
                        x+w*0.30f,y-h*0.28f,
                        x+w*0.70f,y-h*0.28f,
                        std::max(2.0f,rr.h*0.045f),
                        overlayColor(kWhite));
                }else{
                    const float cx=rr.x+rr.w*0.5f,cy=rr.y+rr.h*0.5f;
                    float px=cx-rr.w*0.22f,py=cy;
                    for(int s=1;s<=8;++s){
                        const float u=static_cast<float>(s)/8.0f;
                        const float x=cx-rr.w*0.22f+u*rr.w*0.44f;
                        const float y=cy+std::sin(u*6.28318530718f)*rr.h*0.13f;
                        ov.addLine(px,py,x,y,std::max(2.0f,rr.h*0.045f),col);
                        px=x;py=y;
                    }
                }
            }else{
                ov.addTextCentered(i==1?"V":"M",{rr.x,rr.y,rr.w,rr.h},
                    std::max(0.85f,rr.h/35.0f),col);
                if(disabled){
                    const float pad=rr.w*0.22f;
                    ov.addLine(
                        rr.x+pad,rr.y+rr.h-pad,
                        rr.x+rr.w-pad,rr.y+pad,
                        std::max(2.0f,rr.h*0.045f),
                        overlayColor(kMuted,0.95f));
                }
            }
        }
    }
}

void NativeUi::drawGrid() const noexcept {
    int selectedLow=-1,selectedHigh=-1,selectedPad=-1,selectedCenter=-1,track=-1;
    bool rangeMode=false;
    auto& project=ProjectCore::instance();

    if(page_==NativePage::Drums){
        track=project.selectedTrack();
        if(track>=0&&project.trackIsDrums(track)){
            selectedPad=project.selectedPad(track);
            if(selectedPad>=0&&selectedPad<project.padCount(track)){
                selectedLow=project.padLow(track,selectedPad);
                selectedHigh=project.padHigh(track,selectedPad);
                selectedCenter=project.padCenter(track,selectedPad);
                if(selectedLow>selectedHigh)std::swap(selectedLow,selectedHigh);
                rangeMode=drumRangeMode();
            }
        }
    }

    for(int visualRow=0;visualRow<7;++visualRow){
        const int block=6-visualRow;
        for(int column=0;column<7;++column){
            const int midi=kGridLow+block*7+column;
            const bool active=active_[static_cast<size_t>(midi-kGridLow)];
            const bool selected=selectedLow>=0&&midi>=selectedLow&&midi<=selectedHigh;
            bool unavailable=false;

            if(track>=0&&selectedPad>=0){
                int testLo=midi,testHi=midi;
                if(rangeMode&&selectedCenter>=0){testLo=std::min(selectedCenter,midi);testHi=std::max(selectedCenter,midi);}
                for(int p=0;p<project.padCount(track);++p){
                    if(p==selectedPad)continue;
                    const int lo=std::min(project.padLow(track,p),project.padHigh(track,p));
                    const int hi=std::max(project.padLow(track,p),project.padHigh(track,p));
                    if(!(testHi<lo||testLo>hi)){unavailable=true;break;}
                }
            }

            const auto color=pitchColor(midi);
            const auto rect=gridRect(visualRow,column);
            const auto borderColor=unavailable?kMuted:(selected?mix(color,kOrange,0.58f):color);
            fillRect(rect,mix(kBg,borderColor,active?0.98f:(selected?0.86f:0.62f)));

            const float border=std::max(2.0f,rect.w*(selected?0.075f:0.055f));
            Rect inner{
                rect.x+border,rect.y+border,
                std::max(0.0f,rect.w-border*2.0f),
                std::max(0.0f,rect.h-border*2.0f)
            };
            fillRect(inner,unavailable?mix(kPanel,kMuted,0.28f):mix(kPanel,color,active?0.52f:(selected?0.30f:0.16f)));

            auto& overlay=NativeOverlay::instance();
            const float glyphInset=rect.w*0.19f;
            const Rect glyphRect{
                rect.x+glyphInset,rect.y+glyphInset,
                rect.w-glyphInset*2.0f,rect.h-glyphInset*2.0f};
            if(!unavailable){
                const float glow=std::max(1.5f,rect.w*0.018f);
                overlay.addPitchGlyph(
                    midi%12,
                    {glyphRect.x-glow,glyphRect.y-glow,
                     glyphRect.w+glow*2.0f,glyphRect.h+glow*2.0f},
                    overlayColor(color,active?0.52f:0.32f));
            }
            overlay.addPitchGlyph(
                midi%12,
                {glyphRect.x,glyphRect.y,glyphRect.w,glyphRect.h},
                overlayColor(unavailable?kMuted:kWhite));
            if(((midi%12)+12)%12==2){
                const int octave=(midi-62)/12;
                const std::string label=octave>0?("+"+std::to_string(octave)):std::to_string(octave);
                overlay.addText(
                    label,
                    rect.x+rect.w*0.62f,
                    rect.y+rect.h*0.71f,
                    0.66f,
                    overlayColor(kMuted));
            }
        }
    }
}

void NativeUi::drawTracks() const noexcept {
    drawPageScrollGutter();
    auto& project=ProjectCore::instance();
    auto& overlay=NativeOverlay::instance();

    const auto drawCard=[&](Rect r,const char* title){
        fillRect(r,kButton);
        const float border=2.0f;
        fillRect({
            r.x+border,r.y+border,
            std::max(0.0f,r.w-border*2.0f),
            std::max(0.0f,r.h-border*2.0f)},kPanel);
        overlay.addText(
            title,r.x+18.0f,r.y+16.0f,
            1.05f,overlayColor(kWhite));
    };

    const auto drawSlider=[&](
        Rect r,float value,Rgb accent,
        std::string_view label,std::string_view valueText){
        const float labelW=std::clamp(r.w*0.17f,54.0f,88.0f);
        const float valueW=std::clamp(r.w*0.12f,40.0f,64.0f);
        const float trackX=r.x+labelW;
        const float trackW=std::max(20.0f,r.w-labelW-valueW-10.0f);
        const float cy=r.y+r.h*0.56f;
        overlay.addText(
            label,r.x,r.y+r.h*0.28f,
            0.88f,overlayColor(kWhite));
        fillRect({
            trackX,cy-3.0f,trackW,6.0f},kMuted);
        fillRect({
            trackX,cy-3.0f,
            trackW*std::clamp(value,0.0f,1.0f),6.0f},accent);
        const float knob=18.0f;
        fillRect({
            trackX+trackW*std::clamp(value,0.0f,1.0f)-knob*0.5f,
            cy-knob*0.5f,
            knob,knob},mix(kWhite,accent,0.38f));
        overlay.addText(
            valueText,
            r.x+r.w-valueW+6.0f,
            r.y+r.h*0.28f,
            0.82f,overlayColor(kMuted));
    };

    // TRACKS
    const auto trackCard=trackCardRect();
    drawCard(trackCard,"TRACKS");
    const int count=project.trackCount();
    const int selected=project.selectedTrack();

    for(int i=0;i<count;++i){
        const auto rect=trackRect(i,count);
        const bool drum=project.trackIsDrums(i);
        const bool isSelected=i==selected;
        const Rgb edge=isSelected?kCyan:kButton;
        fillRect(rect,edge);
        const float inset=2.0f;
        fillRect({
            rect.x+inset,rect.y+inset,
            rect.w-inset*2.0f,rect.h-inset*2.0f},
            mix(kPanel,isSelected?kCyan:kPanel,isSelected?0.08f:0.0f));

        const float numW=34.0f;
        overlay.addText(
            std::to_string(i+1),
            rect.x+12.0f,
            rect.y+20.0f,
            1.08f,overlayColor(kWhite));

        std::string name=project.trackName(i);
        if(name.size()>22)name.resize(22);
        overlay.addText(
            name,
            rect.x+numW+12.0f,
            rect.y+18.0f,
            1.16f,overlayColor(kWhite));

        std::string patch=project.trackPatchName(i);
        if(patch.empty())patch=drum?"Nexdrum":"Spectrachord";
        if(patch.size()>18)patch.resize(18);
        overlay.addText(
            patch,
            rect.x+rect.w*0.43f,
            rect.y+21.0f,
            0.92f,overlayColor(kMuted));

        const float values[2]={
            project.trackVolume(i),
            (project.trackPan(i)+1.0f)*0.5f};
        const Rgb accents[2]={kCyan,kCyan};
        const char* labels[2]={"Vol","Pan"};
        for(int p=0;p<2;++p){
            const auto sr=trackPartRect(i,count,p);
            const float labelW=34.0f;
            const float x0=sr.x+labelW;
            const float w=std::max(10.0f,sr.w-labelW-4.0f);
            const float cy=sr.y+sr.h*0.5f;
            overlay.addText(
                labels[p],sr.x,sr.y+sr.h*0.20f,
                0.70f,overlayColor(kMuted));
            fillRect({x0,cy-3.0f,w,6.0f},kMuted);
            fillRect({
                x0,cy-3.0f,w*std::clamp(values[p],0.0f,1.0f),6.0f},
                accents[p]);
            const float knob=16.0f;
            fillRect({
                x0+w*values[p]-knob*0.5f,
                cy-knob*0.5f,knob,knob},kWhite);
        }

        for(int p=2;p<5;++p){
            const auto br=trackPartRect(i,count,p);
            const bool active=
                p==2?project.trackMute(i):
                p==3?project.trackSolo(i):false;
            fillRect(
                br,
                active
                    ?mix(kButton,p==2?kMuted:kOrange,0.65f)
                    :kButton);
            const char* label=p==2?"M":p==3?"S":"×";
            overlay.addTextCentered(
                label,{br.x,br.y,br.w,br.h},
                1.00f,overlayColor(kWhite));
        }

        const auto grip=trackReorderRect(i,count);
        fillRect(grip,kButton);
        for(int g=0;g<3;++g){
            const float gy=grip.y+grip.h*(0.32f+g*0.18f);
            overlay.addLine(
                grip.x+grip.w*0.24f,gy,
                grip.x+grip.w*0.76f,gy,
                2.2f,overlayColor(kMuted));
        }
    }

    const auto add=addTrackRect(TrackAddKind::Melodic);
    fillRect(add,kButton);
    overlay.addTextCentered(
        "+ Add track",{add.x,add.y,add.w,add.h},
        1.0f,overlayColor(kWhite));

    // PATCH
    const auto patchCard=patchCardRect();
    drawCard(patchCard,"PATCH");
    std::string patchName=
        selected>=0?project.trackPatchName(selected):std::string("Spectrachord Init");
    if(patchName.empty())patchName="Spectrachord Init";
    overlay.addText(
        patchName,
        patchCard.x+20.0f,patchCard.y+58.0f,
        1.12f,overlayColor(kCyan));
    const Rect patchField=patchPresetRect();
    fillRect(patchField,kRollBg);
    overlay.addText(
        patchName,
        patchField.x+14.0f,patchField.y+16.0f,
        1.00f,overlayColor(kWhite));
    overlay.addDownChevron({
        patchField.x+patchField.w-36.0f,
        patchField.y,36.0f,patchField.h},
        overlayColor(kWhite));

    // AI SOUND DESIGNER
    const auto ai=aiCardRect();
    drawCard(ai,"AI SOUND DESIGNER");
    overlay.addText(
        "Describe a sound in the clipboard",
        ai.x+20.0f,ai.y+70.0f,
        0.94f,overlayColor(kMuted));
    overlay.addText(
        "AI writes the Spectrachord patch + FX.",
        ai.x+20.0f,ai.y+108.0f,
        1.00f,overlayColor(kWhite));
    const auto aiBtn=trackUtilityRect(1);
    fillRect(aiBtn,kButton);
    overlay.addTextCentered(
        "Tune synth",{aiBtn.x,aiBtn.y,aiBtn.w,aiBtn.h},
        0.86f,overlayColor(kWhite));

    // SONG
    const auto song=songCardRect();
    drawCard(song,"SONG");
    const auto fresh=trackUtilityRect(3);
    const auto clear=trackUtilityRect(2);
    const auto save=trackUtilityRect(4);
    const auto load=trackUtilityRect(5);
    const auto exp=projectTransferRect(0);
    const auto imp=projectTransferRect(1);
    for(const auto& rr:{fresh,clear,save,load,exp,imp})fillRect(rr,kButton);
    overlay.addTextCentered("New",{fresh.x,fresh.y,fresh.w,fresh.h},0.92f,overlayColor(kWhite));
    overlay.addTextCentered("Clear trk",{clear.x,clear.y,clear.w,clear.h},0.88f,overlayColor(kWhite));
    overlay.addTextCentered("Save",{save.x,save.y,save.w,save.h},0.92f,overlayColor(kWhite));
    overlay.addTextCentered("Load",{load.x,load.y,load.w,load.h},0.92f,overlayColor(kWhite));
    overlay.addTextCentered("Export WAV",{exp.x,exp.y,exp.w,exp.h},0.72f,overlayColor(kWhite));
    overlay.addTextCentered("Export MIDI",{imp.x,imp.y,imp.w,imp.h},0.70f,overlayColor(kWhite));

    const auto tempoR=trackSongSliderRect(0);
    const float tempoN=std::clamp((project.bpm()-12.0f)/276.0f,0.0f,1.0f);
    drawSlider(
        tempoR,tempoN,kCyan,
        "Tempo",
        std::to_string(static_cast<int>(std::lround(project.bpm()))));

    const auto beatsR=trackSongSliderRect(1);
    const auto divR=trackSongSliderRect(2);
    drawSlider(
        beatsR,
        static_cast<float>(project.beats()-1)/11.0f,
        kCyan,
        "Beats",
        std::to_string(project.beats()));
    drawSlider(
        divR,
        static_cast<float>(project.divisions()-1)/11.0f,
        kCyan,
        "Notes",
        std::to_string(project.divisions()));

    overlay.addText(
        "Beats per measure / notes per beat",
        song.x+20.0f,song.y+372.0f,
        0.80f,overlayColor(kMuted));

    const auto dz=trackUtilityRect(0);
    fillRect(dz,kPanel);
    const float box=26.0f;
    fillRect({
        dz.x,dz.y+(dz.h-box)*0.5f,
        box,box},
        project.dozenal()?kCyan:kMuted);
    if(project.dozenal()){
        fillRect({
            dz.x+6.0f,dz.y+(dz.h-box)*0.5f+6.0f,
            box-12.0f,box-12.0f},kPanel);
    }
    overlay.addText(
        "Dozenal numbers",
        dz.x+box+12.0f,dz.y+dz.h*0.30f,
        0.88f,overlayColor(kWhite));

    // MASTER
    const auto master=masterCardRect();
    drawCard(master,"MASTER");
    for(int i=0;i<2;++i){
        const auto mr=masterSliderRect(i);
        const float value=i==0?project.masterVolume():project.masterReverb();
        drawSlider(
            mr,value,i==0?kCyan:kPurple,
            i==0?"Volume":"Reverb",
            std::to_string(static_cast<int>(std::lround(value*100.0f))));
    }
}

bool NativeUi::trackPointerDown(float x,float y){
    if(page_!=NativePage::Tracks)return false;
    auto& project=ProjectCore::instance();
    trackControlChanged_=false;
    trackActiveSlider_=-1;
    trackActiveIndex_=-1;
    trackReorderIndex_=-1;

    for(int i=0;i<2;++i){
        const auto r=masterSliderRect(i);if(!r.contains(x,y))continue;
        const float labelW=std::clamp(r.w*0.17f,54.0f,88.0f);
        const float valueW=std::clamp(r.w*0.12f,40.0f,64.0f);
        const float start=r.x+labelW;
        const float width=std::max(2.0f,r.w-labelW-valueW-10.0f);
        const float n=std::clamp((x-start)/width,0.0f,1.0f);
        if(i==0)project.setMasterVolume(n);else project.setMasterReverb(n);
        trackActiveSlider_=i+2;trackControlChanged_=true;return true;
    }

    for(int i=0;i<3;++i){
        const auto r=trackSongSliderRect(i);if(!r.contains(x,y))continue;
        const float labelW=std::clamp(r.w*0.17f,54.0f,88.0f);
        const float valueW=std::clamp(r.w*0.12f,40.0f,64.0f);
        const float start=r.x+labelW;
        const float width=std::max(2.0f,r.w-labelW-valueW-10.0f);
        const float n=std::clamp((x-start)/width,0.0f,1.0f);
        if(i==0)project.setBpm(12.0f+n*276.0f);
        else if(i==1)project.setSignature(1+static_cast<int>(std::lround(n*11.0f)),project.divisions());
        else project.setSignature(project.beats(),1+static_cast<int>(std::lround(n*11.0f)));
        trackActiveSlider_=i+4;trackControlChanged_=true;return true;
    }

    const int count=project.trackCount();
    for(int t=0;t<count;++t){
        if(trackReorderRect(t,count).contains(x,y)){
            project.selectTrack(t);
            trackReorderIndex_=t;
            return true;
        }
    }
    for(int t=0;t<count;++t){
        for(int p=0;p<5;++p){
            const auto r=trackPartRect(t,count,p);if(!r.contains(x,y))continue;
            if(p<2){
                const float labelW=34.0f;
                const float start=r.x+labelW;
                const float width=std::max(2.0f,r.w-labelW-4.0f);
                const float n=std::clamp((x-start)/width,0.0f,1.0f);
                if(p==0)project.setTrackVolume(t,n);else project.setTrackPan(t,n*2.0f-1.0f);
                trackActiveSlider_=p;trackActiveIndex_=t;trackControlChanged_=true;
            }else if(p==2){project.setTrackMute(t,!project.trackMute(t));trackControlChanged_=true;}
            else if(p==3){project.setTrackSolo(t,!project.trackSolo(t));trackControlChanged_=true;}
            else {trackControlChanged_=project.deleteTrack(t);}
            return true;
        }
    }
    return false;
}

bool NativeUi::trackPointerMove(float x,float y){
    auto& project=ProjectCore::instance();
    if(page_!=NativePage::Tracks)return false;

    if(trackReorderIndex_>=0){
        const int count=project.trackCount();
        if(count<=1)return true;
        int target=trackReorderIndex_;
        float best=1.0e9f;
        for(int i=0;i<count;++i){
            const auto row=trackRect(i,count);
            const float d=std::fabs(y-(row.y+row.h*0.5f));
            if(d<best){best=d;target=i;}
        }
        if(target!=trackReorderIndex_&&
           project.reorderTrack(trackReorderIndex_,target)){
            trackReorderIndex_=target;
            trackControlChanged_=true;
        }
        return true;
    }

    if(trackActiveSlider_<0)return false;

    if(trackActiveSlider_==2||trackActiveSlider_==3){
        const int i=trackActiveSlider_-2;
        const auto r=masterSliderRect(i);
        const float labelW=std::clamp(r.w*0.17f,54.0f,88.0f);
        const float valueW=std::clamp(r.w*0.12f,40.0f,64.0f);
        const float start=r.x+labelW;
        const float width=std::max(2.0f,r.w-labelW-valueW-10.0f);
        const float n=std::clamp((x-start)/width,0.0f,1.0f);
        if(i==0)project.setMasterVolume(n);else project.setMasterReverb(n);
    }else if(trackActiveSlider_>=4&&trackActiveSlider_<=6){
        const int i=trackActiveSlider_-4;
        const auto r=trackSongSliderRect(i);
        const float labelW=std::clamp(r.w*0.17f,54.0f,88.0f);
        const float valueW=std::clamp(r.w*0.12f,40.0f,64.0f);
        const float start=r.x+labelW;
        const float width=std::max(2.0f,r.w-labelW-valueW-10.0f);
        const float n=std::clamp((x-start)/width,0.0f,1.0f);
        if(i==0)project.setBpm(12.0f+n*276.0f);
        else if(i==1)project.setSignature(1+static_cast<int>(std::lround(n*11.0f)),project.divisions());
        else project.setSignature(project.beats(),1+static_cast<int>(std::lround(n*11.0f)));
    }else{
        const int count=project.trackCount();
        if(trackActiveIndex_<0||trackActiveIndex_>=count)return false;
        const auto r=trackPartRect(trackActiveIndex_,count,trackActiveSlider_);
        const float labelW=34.0f;
        const float start=r.x+labelW;
        const float width=std::max(2.0f,r.w-labelW-4.0f);
        const float n=std::clamp((x-start)/width,0.0f,1.0f);
        if(trackActiveSlider_==0)project.setTrackVolume(trackActiveIndex_,n);
        else project.setTrackPan(trackActiveIndex_,n*2.0f-1.0f);
    }
    trackControlChanged_=true;return true;
}

bool NativeUi::trackPointerUp(){
    const bool changed=trackControlChanged_;
    trackControlChanged_=false;
    trackActiveSlider_=-1;
    trackActiveIndex_=-1;
    trackReorderIndex_=-1;
    return changed;
}

void NativeUi::drawPadQuick() const noexcept {
    auto& project=ProjectCore::instance();
    const int track=project.selectedTrack();
    if(track<0||!project.trackIsDrums(track))return;

    auto& ov=NativeOverlay::instance();
    const int count=project.padCount(track);
    const int selected=project.selectedPad(track);

    for(int i=0;i<count;++i){
        const auto row=padQuickRect(i,count);
        const bool isSelected=i==selected;
        const int center=project.padCenter(track,i);
        const int lo=std::min(project.padLow(track,i),project.padHigh(track,i));
        const int hi=std::max(project.padLow(track,i),project.padHigh(track,i));
        const auto pc=pitchColor(center);

        fillRect(row,isSelected?kOrange:kButton);
        const float border=isSelected?2.5f:1.5f;
        fillRect({
            row.x+border,row.y+border,
            row.w-border*2.0f,row.h-border*2.0f},
            mix(kPanel,isSelected?kOrange:pc,isSelected?0.07f:0.025f));

        const std::string iconId=project.padIcon(track,i);
        const int iconIndex=drumIconIndex(iconId);
        const Rect iconR{
            row.x+18.0f,row.y+18.0f,
            62.0f,62.0f};
        ov.addDrumIcon(
            iconIndex,
            {iconR.x,iconR.y,iconR.w,iconR.h},
            overlayColor(pc));

        // Reserve a fixed pitch-glyph lane so ranged endpoints never
        // collide with the pad name. Single notes can use the full glyph size;
        // ranged pads use a slightly smaller pair centered in the same lane.
        const float glyphLaneX=row.x+92.0f;
        const float glyphLaneW=142.0f;
        const float glyphY=row.y+18.0f;
        if(lo!=hi){
            const float glyphSize=42.0f;
            const float dashW=20.0f;
            const float total=glyphSize*2.0f+dashW;
            const float gx=glyphLaneX+(glyphLaneW-total)*0.5f;
            ov.addPitchGlyph(
                lo%12,
                {gx,glyphY,glyphSize,glyphSize},
                overlayColor(kWhite));
            ov.addTextCentered(
                "–",
                {gx+glyphSize,glyphY,dashW,glyphSize},
                0.92f,overlayColor(kOrange));
            ov.addPitchGlyph(
                hi%12,
                {gx+glyphSize+dashW,glyphY,glyphSize,glyphSize},
                overlayColor(kWhite));
        }else{
            const float glyphSize=52.0f;
            const float gx=glyphLaneX+(glyphLaneW-glyphSize)*0.5f;
            ov.addPitchGlyph(
                lo%12,
                {gx,glyphY,glyphSize,glyphSize},
                overlayColor(kWhite));
        }

        const float infoX=row.x+250.0f;
        std::string name=project.padName(track,i);
        if(name.empty())name=drumIconName(iconId);
        if(name.size()>24)name.resize(24);
        std::string range=pitchCoord(lo);
        if(lo!=hi)range+="–"+pitchCoord(hi);
        std::string patch=project.padPatchName(track,i);
        if(patch.size()>25)patch.resize(25);

        ov.addText(
            name,
            infoX,row.y+20.0f,
            1.06f,overlayColor(kWhite));
        ov.addText(
            range+(patch.empty()?std::string{}:" · "+patch),
            infoX,row.y+54.0f,
            0.78f,overlayColor(kMuted));

        const auto grip=padReorderRect(i,count);
        fillRect(grip,kButton);
        for(int g=0;g<3;++g){
            const float gy=grip.y+grip.h*(0.32f+g*0.18f);
            ov.addLine(
                grip.x+grip.w*0.24f,gy,
                grip.x+grip.w*0.76f,gy,
                2.2f,overlayColor(kMuted));
        }

        const Rect del{
            row.x+row.w-62.0f,
            row.y+18.0f,
            44.0f,44.0f};
        fillRect(del,kButton);
        ov.addTextCentered(
            "×",{del.x,del.y,del.w,del.h},
            1.06f,overlayColor(kWhite));

        // Mirror the HTML per-pad Vol/Pan rows. The selected pad is live-editable;
        // the other rows remain visible so the mixer state is always readable.
        const float vals[2]={
            project.padVolume(track,i),
            (project.padPan(track,i)+1.0f)*0.5f};
        const char* labels[2]={"Vol","Pan"};
        const float sliderTop=row.y+row.h*0.68f;
        const float sliderLeft=row.x+18.0f;
        const float available=row.w-36.0f;
        const float pairGap=24.0f;
        const float pairW=(available-pairGap)*0.5f;
        for(int s=0;s<2;++s){
            const float sx=sliderLeft+s*(pairW+pairGap);
            ov.addText(
                labels[s],sx,sliderTop,
                0.74f,overlayColor(kMuted));
            const float barX=sx+42.0f;
            const float barW=pairW-42.0f;
            const float cy=sliderTop+15.0f;
            fillRect({barX,cy-4.0f,barW,8.0f},kMuted);
            fillRect({
                barX,cy-3.0f,
                barW*std::clamp(vals[s],0.0f,1.0f),8.0f},
                kCyan);
            const float knob=18.0f;
            fillRect({
                barX+barW*vals[s]-knob*0.5f,
                cy-knob*0.5f,knob,knob},
                isSelected?kWhite:mix(kWhite,kMuted,0.28f));
        }
    }
}

bool NativeUi::drumRangeMode() const noexcept {
    if(drumRangeArmed_)return true;
    auto& project=ProjectCore::instance();const int track=project.selectedTrack();
    if(track<0||!project.trackIsDrums(track))return false;
    const int pad=project.selectedPad(track);
    if(pad<0||pad>=project.padCount(track))return false;
    return project.padLow(track,pad)!=project.padHigh(track,pad);
}

void NativeUi::drawDrumEditor() const noexcept {
    auto& project=ProjectCore::instance();
    const int track=project.selectedTrack();
    const auto card=drumPadCardRect();
    auto& ov=NativeOverlay::instance();

    fillRect(card,kButton);
    fillRect({
        card.x+2.0f,card.y+2.0f,
        card.w-4.0f,card.h-4.0f},kPanel);

    ov.addText(
        "PAD",card.x+20.0f,card.y+20.0f,
        1.18f,overlayColor(kWhite));
    ov.addText(
        "Note",card.x+20.0f,card.y+62.0f,
        0.96f,overlayColor(kWhite));

    if(track<0||track>=project.trackCount()){
        ov.addText(
            "Create a Spectrachord track on the Tracks page",
            card.x+20.0f,card.y+108.0f,
            0.82f,overlayColor(kMuted));
        return;
    }
    if(!project.trackIsDrums(track)){
        ov.addText(
            "Convert this track to a drum kit above",
            card.x+20.0f,card.y+108.0f,
            0.84f,overlayColor(kMuted));
        return;
    }
    const int pad=project.selectedPad(track);
    const int count=project.padCount(track);

    const auto range=drumActionRect(0);
    fillRect(range,drumRangeMode()?mix(kButton,kCyan,0.70f):kButton);
    ov.addTextCentered(
        drumRangeMode()?"Range: on":"Range",
        {range.x,range.y,range.w,range.h},
        0.92f,overlayColor(kWhite));

    if(pad<0||pad>=count)return;

    static constexpr const char* names[12]={
        "Kick","Snare","Tom","Floor tom","Hi-hat","Cymbal",
        "Ride","Bongo","Conga","Clap","Shaker","Cowbell"};
    static constexpr const char* ids[12]={
        "kick","snare","tom","floortom","hihat","crash",
        "ride","bongo","conga","clap","shaker","cowbell"};

    const std::string current=project.padIcon(track,pad);
    for(int i=0;i<12;++i){
        const auto rr=drumIconRect(i);
        const bool sel=current==ids[i];
        fillRect(rr,sel?mix(kButton,kOrange,0.72f):kButton);
        const float icon=std::min(rr.w,rr.h)*0.52f;
        ov.addDrumIcon(
            i,
            {rr.x+(rr.w-icon)*0.5f,rr.y+8.0f,icon,icon},
            overlayColor(sel?kWhite:kMuted));
        ov.addTextCentered(
            names[i],
            {rr.x+2.0f,rr.y+rr.h-31.0f,rr.w-4.0f,26.0f},
            0.72f,overlayColor(sel?kWhite:kMuted));
    }

    const auto tap=drumActionRect(2);
    const auto del=drumActionRect(3);
    fillRect(tap,kButton);
    fillRect(del,kButton);
    ov.addTextCentered(
        "Tap pad",{tap.x,tap.y,tap.w,tap.h},
        0.90f,overlayColor(kWhite));
    ov.addTextCentered(
        "Delete pad",{del.x,del.y,del.w,del.h},
        0.90f,overlayColor(kWhite));
}

void NativeUi::drawDrums() const noexcept {
    const auto view=bodyContentRect();
    fillRect(view,kBg);
    drawPageScrollGutter();

    auto& project=ProjectCore::instance();
    auto& ov=NativeOverlay::instance();
    const int track=project.selectedTrack();

    const auto kit=drumKitCardRect();
    fillRect(kit,kButton);
    fillRect({
        kit.x+2.0f,kit.y+2.0f,
        kit.w-4.0f,kit.h-4.0f},kPanel);
    ov.addText(
        "DRUM KIT",kit.x+20.0f,kit.y+20.0f,
        1.18f,overlayColor(kWhite));
    ov.addText(
        "Kit",kit.x+20.0f,kit.y+72.0f,
        1.00f,overlayColor(kWhite));

    const Rect kitField{
        kit.x+92.0f,kit.y+58.0f,
        kit.w-114.0f,58.0f};
    fillRect(kitField,kRollBg);
    const bool hasTrack=track>=0&&track<project.trackCount();
    const bool hasDrumTrack=hasTrack&&project.trackIsDrums(track);
    ov.addText(
        hasDrumTrack?"Drum kit":
        hasTrack?"Instrument track":"No track selected",
        kitField.x+14.0f,kitField.y+17.0f,
        1.02f,
        overlayColor(hasTrack?kWhite:kMuted));

    drawPadQuick();

    const auto add=drumActionRect(1);
    fillRect(add,hasTrack?kButton:mix(kButton,kRollBg,0.58f));
    ov.addTextCentered(
        hasDrumTrack?"+ Add Pad":
        hasTrack?"Convert to drum kit":"Create a track on Tracks page",
        {add.x,add.y,add.w,add.h},
        hasDrumTrack?0.96f:0.78f,
        overlayColor(hasTrack?kWhite:kMuted));

    drawDrumEditor();
    drawGrid();
}

bool NativeUi::drumPointerDown(float x,float y){
    if(page_!=NativePage::Drums)return false;
    auto& project=ProjectCore::instance();
    const int track=project.selectedTrack();
    drumControlChanged_=false;
    drumActiveSlider_=-1;
    drumReorderIndex_=-1;

    if(track<0||track>=project.trackCount())return false;

    if(!project.trackIsDrums(track)){
        if(drumActionRect(1).contains(x,y)){
            drumControlChanged_=project.convertTrackToDrumKit(track);
            drumRangeArmed_=false;
            return true;
        }
        return false;
    }

    const int initialCount=project.padCount(track);
    for(int p=0;p<initialCount;++p){
        if(padReorderRect(p,initialCount).contains(x,y)){
            project.selectPad(track,p);
            drumReorderIndex_=p;
            drumRangeArmed_=false;
            return true;
        }
    }
    for(int p=0;p<initialCount;++p){
        const auto row=padQuickRect(p,initialCount);
        const Rect del{
            row.x+row.w-62.0f,
            row.y+18.0f,
            44.0f,44.0f};
        if(del.contains(x,y)){
            drumControlChanged_=project.deleteDrumPad(track,p);
            drumRangeArmed_=false;
            return true;
        }
    }

    for(int i=0;i<4;++i){
        if(!drumActionRect(i).contains(x,y))continue;
        if(i==0){
            const int pad=project.selectedPad(track);
            if(pad<0||pad>=project.padCount(track))return true;
            if(drumRangeMode()){
                const int center=project.padCenter(track,pad);
                drumControlChanged_=project.setPadRange(track,pad,center,center);
                drumRangeArmed_=false;
            }else drumRangeArmed_=true;
        }else if(i==1){
            drumControlChanged_=project.addDrumPad(track)>=0;
            drumRangeArmed_=false;
        }else if(i==2){
            const int pad=project.selectedPad(track);
            if(pad>=0&&pad<project.padCount(track)){
                const int voice=AudioEngine::instance().noteOnPad(
                    pad,project.padCenter(track,pad),0.85f);
                AudioEngine::instance().noteOff(voice);
            }
        }else{
            const int pad=project.selectedPad(track);
            drumControlChanged_=pad>=0&&project.deleteDrumPad(track,pad);
            drumRangeArmed_=false;
        }
        return true;
    }

    const int pad=project.selectedPad(track);
    if(pad<0||pad>=project.padCount(track))return false;
    for(int i=0;i<2;++i){
        const auto r=drumSliderRect(i);if(!r.contains(x,y))continue;
        drumActiveSlider_=i;
        const float start=r.x+42.0f;
        const float width=std::max(1.0f,r.w-42.0f);
        const float n=std::clamp((x-start)/width,0.0f,1.0f);
        if(i==0)project.setPadVolume(track,pad,n);else project.setPadPan(track,pad,n*2.0f-1.0f);
        drumControlChanged_=true;return true;
    }

    static constexpr const char* ids[12]={"kick","snare","tom","floortom","hihat","crash","ride","bongo","conga","clap","shaker","cowbell"};
    for(int i=0;i<12;++i)if(drumIconRect(i).contains(x,y)){
        drumControlChanged_=project.setPadIcon(track,pad,ids[i]);return true;
    }
    return false;
}

bool NativeUi::drumPointerMove(float x,float y){
    if(page_!=NativePage::Drums)return false;
    auto& project=ProjectCore::instance();
    const int track=project.selectedTrack();
    if(track<0||!project.trackIsDrums(track))return false;

    if(drumReorderIndex_>=0){
        const int count=project.padCount(track);
        if(count<=1)return true;
        int target=drumReorderIndex_;
        float best=1.0e9f;
        for(int i=0;i<count;++i){
            const auto row=padQuickRect(i,count);
            const float d=std::fabs(y-(row.y+row.h*0.5f));
            if(d<best){best=d;target=i;}
        }
        if(target!=drumReorderIndex_&&
           project.reorderDrumPad(track,drumReorderIndex_,target)){
            drumReorderIndex_=target;
            drumControlChanged_=true;
        }
        return true;
    }

    if(drumActiveSlider_<0)return false;
    const int pad=project.selectedPad(track);
    if(pad<0||pad>=project.padCount(track))return false;
    const auto r=drumSliderRect(drumActiveSlider_);
    const float start=r.x+42.0f;
    const float width=std::max(1.0f,r.w-42.0f);
    const float n=std::clamp((x-start)/width,0.0f,1.0f);
    if(drumActiveSlider_==0)project.setPadVolume(track,pad,n);else project.setPadPan(track,pad,n*2.0f-1.0f);
    drumControlChanged_=true;return true;
}

bool NativeUi::drumPointerUp(){
    const bool changed=drumControlChanged_;
    drumControlChanged_=false;
    drumActiveSlider_=-1;
    drumReorderIndex_=-1;
    return changed;
}

bool NativeUi::drumPitchTap(int midi){
    if(page_!=NativePage::Drums)return false;
    auto& project=ProjectCore::instance();const int track=project.selectedTrack();
    if(track<0||!project.trackIsDrums(track))return false;
    const int pad=project.selectedPad(track);if(pad<0||pad>=project.padCount(track))return false;
    midi=std::clamp(midi,kGridLow,kGridHigh);
    if(drumRangeMode()){
        const int center=project.padCenter(track,pad);
        const bool ok=project.setPadRange(track,pad,std::min(center,midi),std::max(center,midi));
        if(ok)drumRangeArmed_=true;
        return ok;
    }
    return project.setPadCenter(track,pad,midi);
}

void NativeUi::drawRoll() const noexcept {
    auto& project=ProjectCore::instance();
    const auto content=bodyContentRect();
    fillRect(content,kPanel);

    const int track=project.selectedTrack();
    if(track<0)return;

    const auto viewport=rollViewportRect();
    fillRect(viewport,kRollBg);

    const auto columns=rollColumns();
    const float rowH=rollCellPixels();
    const float colW=rollColumnPixels();
    const float gutter=rollGutterPixels();
    const float header=rollHeaderPixels();
    const int visibleCols=std::max(
        1,static_cast<int>(std::lround((viewport.w-gutter)/colW)));
    const int visibleRows=std::max(
        1,static_cast<int>(std::floor((viewport.h-header)/rowH)));

    const int pitchOffset=std::clamp(
        rollPitchOffset_,0,
        std::max(0,static_cast<int>(columns.size())-visibleCols));
    const int stepOffset=std::clamp(
        rollStepOffset_,0,std::max(0,rollTotalRows()-visibleRows));

    const bool hasSelectionRange=
        rollSelectionActive_&&
        rollSelectionAnchorStep_!=rollSelectionEndStep_;
    const bool showPaste=!rollSelectionActive_&&rollClipboardAvailable_;
    const auto cornerColor=
        hasSelectionRange?mix(kPanel,kOrange,0.42f):
        showPaste?mix(kPanel,kGreen,0.38f):
        kPanel;
    fillRect({viewport.x,viewport.y,gutter-1.0f,header-1.0f},cornerColor);
    if(hasSelectionRange||showPaste){
        auto& cornerOv=NativeOverlay::instance();
        const std::string label=hasSelectionRange?"C":"P";
        cornerOv.addText(
            label,
            viewport.x+gutter*0.38f,
            viewport.y+header*0.30f,
            1.55f,
            overlayColor(kWhite));
    }

    for(int cc=0;
        cc<visibleCols&&pitchOffset+cc<static_cast<int>(columns.size());
        ++cc){
        const int midi=columns[static_cast<size_t>(pitchOffset+cc)];
        const float rawLeft=viewport.x+gutter+cc*colW;
        const float rawRight=viewport.x+gutter+(cc+1)*colW;
        const float x=std::round(rawLeft);
        const float right=std::round(rawRight);
        const float cellW=std::max(1.0f,right-x);
        const auto pcColor=pitchColor(midi);
        fillRect(
            {x,viewport.y,cellW,header-2.0f},
            mix(kPanel,pcColor,0.10f));
        fillRect(
            {right-1.0f,viewport.y,1.0f,header-2.0f},
            kRollBg);
        fillRect(
            {x,viewport.y+header-4.0f,std::max(1.0f,cellW-1.0f),3.0f},
            pcColor);

        auto& rollOv=NativeOverlay::instance();
        if(project.trackIsDrums(track)){
            int padIndex=-1;
            for(int p=0;p<project.padCount(track);++p){
                const int lo=std::min(project.padLow(track,p),project.padHigh(track,p));
                const int hi=std::max(project.padLow(track,p),project.padHigh(track,p));
                if(midi>=lo&&midi<=hi){padIndex=p;break;}
            }
            if(padIndex>=0){
                const int iconIndex=drumIconIndex(project.padIcon(track,padIndex));
                const float iconS=std::min(cellW*0.50f,header*0.40f);
                rollOv.addDrumIcon(
                    iconIndex,
                    {x+(cellW-iconS)*0.5f,viewport.y+4.0f,iconS,iconS},
                    overlayColor(pcColor));
            }
            const float glyphS=std::min(cellW*0.62f,header*0.42f);
            const Rect gr{x+(cellW-glyphS)*0.5f,viewport.y+header*0.48f,glyphS,glyphS};
            rollOv.addPitchGlyph(
                midi%12,
                {gr.x-1.5f,gr.y-1.5f,gr.w+3.0f,gr.h+3.0f},
                overlayColor(pcColor,0.32f));
            rollOv.addPitchGlyph(
                midi%12,{gr.x,gr.y,gr.w,gr.h},overlayColor(kWhite));
        }else{
            const float glyphS=std::min(cellW*0.62f,header*0.68f);
            const Rect gr{x+(cellW-glyphS)*0.5f,viewport.y+3.0f,glyphS,glyphS};
            rollOv.addPitchGlyph(
                midi%12,
                {gr.x-1.5f,gr.y-1.5f,gr.w+3.0f,gr.h+3.0f},
                overlayColor(pcColor,0.32f));
            rollOv.addPitchGlyph(
                midi%12,{gr.x,gr.y,gr.w,gr.h},overlayColor(kWhite));
        }

        if(((midi%12)+12)%12==2){
            const int octave=(midi-62)/12;
            const std::string label=
                octave>0?("+"+std::to_string(octave)):std::to_string(octave);
            rollOv.addText(
                label,
                x+cellW*0.61f,
                viewport.y+header*(project.trackIsDrums(track)?0.82f:0.66f),
                0.68f,
                overlayColor(kMuted));
        }
    }

    const int beatLen=std::max(1,project.divisions());
    const int barLen=std::max(1,project.beats())*beatLen;

    const auto projectSnapshot=project.projectCopy();

    for(int rr=0;rr<visibleRows;++rr){
        const int step=stepOffset+rr;
        const float y=viewport.y+header+rr*rowH;
        const Rgb rowColor=kCell;

        const float gutterT=std::clamp(
            (y-viewport.y-header)/
                std::max(1.0f,viewport.h-header),
            0.0f,1.0f);
        const float spectrumPos=gutterT*11.0f;
        const int spectrumIndex=std::clamp(
            static_cast<int>(std::floor(spectrumPos)),0,10);
        const float spectrumLocal=
            spectrumPos-static_cast<float>(spectrumIndex);
        const auto gutterSpectrum=mix(
            kPitchColors[static_cast<size_t>(spectrumIndex)],
            kPitchColors[static_cast<size_t>(spectrumIndex+1)],
            spectrumLocal);
        const auto gutterColor=mix(rowColor,gutterSpectrum,0.16f);
        fillRect(
            {viewport.x,y,gutter-2.0f,rowH-1.0f},
            gutterColor);
        if(step%barLen==0){
            const int bar=step/barLen+1;
            NativeOverlay::instance().addText(
                "b"+std::to_string(bar),
                viewport.x+4.0f,
                y+rowH*0.31f,
                0.78f,
                overlayColor(kWhite));
        }

        for(int cc=0;
            cc<visibleCols&&pitchOffset+cc<static_cast<int>(columns.size());
            ++cc){
            const int midi=columns[static_cast<size_t>(pitchOffset+cc)];
            const float rawLeft=viewport.x+gutter+cc*colW;
            const float rawRight=viewport.x+gutter+(cc+1)*colW;
            const float cellX=std::round(rawLeft);
            const float cellRight=std::round(rawRight);
            const float cellW=std::max(1.0f,cellRight-cellX);
            const int pitchClass=((midi%12)+12)%12;
            auto tinted=mix(rowColor,pitchColor(midi),0.075f);
            fillRect({cellX,y,cellW,rowH-1.0f},tinted);
            fillRect(
                {cellRight-1.0f,y,1.0f,rowH-1.0f},
                kRollBg);
        }

        // Keep rhythmic structure in line weight instead of cell brightness.
        // This leaves harmonic shadows as the only source of grid darkening.
        if(step%beatLen==0){
            const bool barBoundary=step%barLen==0;
            const float lineWeight=barBoundary?3.0f:2.0f;
            const auto lineColor=barBoundary?kBar:kBeat;
            const float gridX=viewport.x+gutter;
            const float gridW=std::max(0.0f,viewport.w-gutter);
            fillRect(
                {gridX,y,gridW,lineWeight},
                lineColor);
            fillRect(
                {gridX,y+rowH-lineWeight,gridW,lineWeight},
                lineColor);
        }
    }

    auto& rollOverlay=NativeOverlay::instance();
    const float gridLeft=viewport.x+gutter;
    const float gridRight=viewport.x+viewport.w;
    const float gridTop=viewport.y+header;
    const float gridBottom=viewport.y+viewport.h;

    const auto ribbonHalfWidth=[&](const Note& note,float step){
        const float minHalf=std::max(1.5f,colW*0.055f);
        const float maxHalf=std::max(minHalf,colW*0.5f-2.0f);
        const float velocity=rollLevelAt(
            note.velocity,note,step,1.0f);
        return minHalf+(maxHalf-minHalf)*velocity;
    };

    const auto addRibbon=[&](
        const Track& sourceTrack,const Note& note,
        int sourceTrackIndex,int noteIndex,int targetMidi,
        NativeOverlay::Color color,bool spectrum){
        const float length=std::max(1.0f,note.lengthSteps);
        const float top=
            viewport.y+header+
            (note.startStep-static_cast<float>(stepOffset))*rowH+1.0f;
        const float height=std::max(rowH,length*rowH)-2.0f;
        const int segments=std::clamp(
            static_cast<int>(std::ceil(length*12.0f)),6,480);
        (void)sourceTrackIndex;
        (void)noteIndex;

        for(int i=0;i<segments;++i){
            const float f0=
                static_cast<float>(i)/static_cast<float>(segments);
            const float f1=
                static_cast<float>(i+1)/static_cast<float>(segments);
            float y0=top+f0*height;
            float y1=top+f1*height;
            if(y1<=gridTop||y0>=gridBottom)continue;

            const float s0=rollVisualCurveStep(note,f0);
            const float s1=rollVisualCurveStep(note,f1);
            const float pitch0=
                static_cast<float>(targetMidi)+
                rollBendAt(sourceTrack,note,s0);
            const float pitch1=
                static_cast<float>(targetMidi)+
                rollBendAt(sourceTrack,note,s1);
            const float c0=rollPitchX(
                columns,pitch0,pitchOffset,gridLeft,colW);
            const float c1=rollPitchX(
                columns,pitch1,pitchOffset,gridLeft,colW);

            const float half0=ribbonHalfWidth(note,s0);
            const float half1=ribbonHalfWidth(note,s1);
            const float rough0=
                rollLevelAt(note.mod,note,s0,0.0f)*colW*0.11f;
            const float rough1=
                rollLevelAt(note.mod,note,s1,0.0f)*colW*0.11f;

            const float jag0=rollJaggedWave(f0*length);
            const float jag1=rollJaggedWave(f1*length);
            const float edge0=std::max(1.2f,half0+jag0*rough0);
            const float edge1=std::max(1.2f,half1+jag1*rough1);
            const float lh0=edge0;
            const float rh0=edge0;
            const float lh1=edge1;
            const float rh1=edge1;

            const float rawL0=c0-lh0;
            const float rawR0=c0+rh0;
            const float rawL1=c1-lh1;
            const float rawR1=c1+rh1;
            if(std::max(rawR0,rawR1)<=gridLeft||
               std::min(rawL0,rawL1)>=gridRight)
                continue;

            const float x0=std::clamp(rawL0,gridLeft+1.0f,gridRight-1.0f);
            const float x1=std::clamp(rawR0,gridLeft+1.0f,gridRight-1.0f);
            const float x2=std::clamp(rawR1,gridLeft+1.0f,gridRight-1.0f);
            const float x3=std::clamp(rawL1,gridLeft+1.0f,gridRight-1.0f);
            y0=std::clamp(y0,gridTop,gridBottom);
            y1=std::clamp(y1,gridTop,gridBottom);
            if(y1<=y0||x1<=x0||x2<=x3)continue;

            const auto topColor=
                spectrum?overlayColor(rollSpectrumColor(pitch0)):color;
            const auto bottomColor=
                spectrum?overlayColor(rollSpectrumColor(pitch1)):color;
            rollOverlay.addGradientQuad(
                x0,y0,x1,y0,x2,y1,x3,y1,
                topColor,bottomColor);
        }
    };

    // Project-wide harmonic shadows inherit the complete source performance:
    // bend moves their center path, velocity changes their width, and M roughens
    // their edges. The shadow is opaque at one fixed dark tone, so overlapping
    // tracks never accumulate additional darkness.
    const auto shadowColor=
        overlayColor(mix(kCell,kRollBg,0.34f),1.0f);
    for(int sourceTrackIndex=0;
        sourceTrackIndex<static_cast<int>(projectSnapshot.tracks.size());
        ++sourceTrackIndex){
        const auto& sourceTrack=
            projectSnapshot.tracks[static_cast<size_t>(sourceTrackIndex)];
        for(int noteIndex=0;
            noteIndex<static_cast<int>(sourceTrack.notes.size());
            ++noteIndex){
            const auto& note=
                sourceTrack.notes[static_cast<size_t>(noteIndex)];
            const float length=std::max(1.0f,note.lengthSteps);
            if(note.startStep+length<=static_cast<float>(stepOffset)||
               note.startStep>=static_cast<float>(stepOffset+visibleRows))
                continue;
            const int pc=((note.midi%12)+12)%12;
            int shadowLow=columns.empty()?note.midi:columns.front();
            int shadowHigh=columns.empty()?note.midi:columns.back();
            if(!project.trackIsDrums(track)){
                // Include one extra octave beyond each absolute roll edge.
                // Bend is limited to +/-12 semitones, so this is enough for a
                // shadow that begins outside the piano roll to bend into view.
                shadowLow-=12;
                shadowHigh+=12;
            }
            for(int targetMidi=shadowLow;
                targetMidi<=shadowHigh;
                ++targetMidi){
                if(((targetMidi%12)+12)%12!=pc)continue;
                addRibbon(
                    sourceTrack,note,
                    sourceTrackIndex,noteIndex,targetMidi,
                    shadowColor,false);
            }
        }
    }

    // Reassert beat/bar boundaries over the shaped shadows.
    for(int rr=0;rr<visibleRows;++rr){
        const int step=stepOffset+rr;
        if(step%beatLen!=0)continue;
        const float y=viewport.y+header+rr*rowH;
        const bool barBoundary=step%barLen==0;
        const float lineWeight=barBoundary?3.0f:2.0f;
        const auto lineColor=barBoundary?kBar:kBeat;
        rollOverlay.addRect(
            {gridLeft,y,std::max(0.0f,gridRight-gridLeft),lineWeight},
            overlayColor(lineColor));
        rollOverlay.addRect(
            {gridLeft,y+rowH-lineWeight,
             std::max(0.0f,gridRight-gridLeft),lineWeight},
            overlayColor(lineColor));
    }

    if(track<static_cast<int>(projectSnapshot.tracks.size())){
        const auto& selectedTrack=
            projectSnapshot.tracks[static_cast<size_t>(track)];
        const bool groupEditing=
            rollSelectedTrack_==track&&rollNoteSelectionActive()&&
            rollSelectionTool_==RollSelectionTool::Pencil&&
            (rollMode_==RollMode::Velocity||rollMode_==RollMode::Mod);
        for(int n=0;n<static_cast<int>(selectedTrack.notes.size());++n){
            const auto& note=selectedTrack.notes[static_cast<size_t>(n)];
            const int midi=note.midi;

            const float start=note.startStep;
            const float length=std::max(1.0f,note.lengthSteps);
            if(start+length<=stepOffset||
               start>=stepOffset+visibleRows)continue;

            addRibbon(
                selectedTrack,note,track,n,midi,
                overlayColor(pitchColor(midi)),true);

            const bool noteSelectedNow=
                rollSelectedTrack_==track&&rollNoteSelected(n);
            if(noteSelectedNow){
                const int highlightSegments=std::clamp(
                    static_cast<int>(std::ceil(length*10.0f)),8,320);
                float prevLX=0.0f,prevRX=0.0f,prevY=0.0f;
                float firstLX=0.0f,firstRX=0.0f,firstY=0.0f;
                bool have=false;
                for(int i=0;i<=highlightSegments;++i){
                    const float fraction=
                        static_cast<float>(i)/
                        static_cast<float>(highlightSegments);
                    const float rel=rollVisualCurveStep(note,fraction);
                    const float center=rollPitchX(
                        columns,
                        static_cast<float>(midi)+
                            rollBendAt(selectedTrack,note,rel),
                        pitchOffset,gridLeft,colW);
                    const float half=ribbonHalfWidth(note,rel);
                    const float rough=
                        rollLevelAt(note.mod,note,rel,0.0f)*colW*0.11f;
                    const float edge=std::max(
                        1.2f,
                        half+
                        rollJaggedWave(fraction*length)*rough);
                    const float lx=std::clamp(
                        center-edge,gridLeft+1.0f,gridRight-1.0f);
                    const float rx=std::clamp(
                        center+edge,gridLeft+1.0f,gridRight-1.0f);
                    const float hy=
                        viewport.y+header+
                        (start+fraction*length-
                         static_cast<float>(stepOffset))*rowH;
                    if(!have){
                        firstLX=lx;firstRX=rx;firstY=hy;
                    }else{
                        rollOverlay.addLine(
                            prevLX,prevY,lx,hy,
                            std::max(2.4f,colW*0.060f),
                            overlayColor(kGold,0.98f));
                        rollOverlay.addLine(
                            prevRX,prevY,rx,hy,
                            std::max(2.4f,colW*0.060f),
                            overlayColor(kGold,0.98f));
                    }
                    prevLX=lx;prevRX=rx;prevY=hy;have=true;
                }
                if(have){
                    if(firstY>=gridTop&&firstY<=gridBottom){
                        rollOverlay.addLine(
                            firstLX,firstY,firstRX,firstY,
                            std::max(2.4f,colW*0.060f),
                            overlayColor(kGold,0.98f));
                    }
                    if(prevY>=gridTop&&prevY<=gridBottom){
                        rollOverlay.addLine(
                            prevLX,prevY,prevRX,prevY,
                            std::max(2.4f,colW*0.060f),
                            overlayColor(kGold,0.98f));
                    }
                }
            }

            const float startY=
                viewport.y+header+
                (start-static_cast<float>(stepOffset))*rowH+1.0f;
            if(startY>=gridTop&&startY<=gridBottom){
                const float step=rollVisualCurveStep(note,0.0f);
                const float center=rollPitchX(
                    columns,
                    static_cast<float>(midi)+
                        rollBendAt(selectedTrack,note,step),
                    pitchOffset,gridLeft,colW);
                const float half=ribbonHalfWidth(note,step);
                if(center+half>gridLeft&&center-half<gridRight){
                    rollOverlay.addLine(
                        std::clamp(center-half,gridLeft+1.0f,gridRight-1.0f),
                        startY,
                        std::clamp(center+half,gridLeft+1.0f,gridRight-1.0f),
                        startY,
                        3.0f,
                        overlayColor(noteSelectedNow?kGold:kTop));
                }
            }

            const float tailY=
                viewport.y+header+
                (start+length-static_cast<float>(stepOffset))*rowH-2.0f;
            if(tailY>=gridTop&&tailY<=gridBottom){
                const float step=rollVisualCurveStep(note,1.0f);
                const float center=rollPitchX(
                    columns,
                    static_cast<float>(midi)+rollBendAt(selectedTrack,note,step),
                    pitchOffset,gridLeft,colW);
                const float half=ribbonHalfWidth(note,step);
                if(center+half>gridLeft&&center-half<gridRight){
                    rollOverlay.addLine(
                        std::clamp(center-half,gridLeft+1.0f,gridRight-1.0f),
                        tailY,
                        std::clamp(center+half,gridLeft+1.0f,gridRight-1.0f),
                        tailY,
                        3.0f,
                        overlayColor(noteSelectedNow?kGold:kTop));
                }
            }

            if(rollMode_==RollMode::Notes||groupEditing)continue;
            const int kind=static_cast<int>(rollMode_)-1;
            const Rgb pointColor=
                rollMode_==RollMode::Bend?kCyan:
                rollMode_==RollMode::Velocity?kWhite:kPurple;
            const auto color=overlayColor(pointColor);
            const float basis=std::min(colW,rowH);
            const float thick=std::max(2.0f,basis*0.045f);
            const int segments=std::clamp(
                static_cast<int>(std::ceil(length*12.0f)),6,480);

            float prevLX=0.0f,prevRX=0.0f,prevY=0.0f;
            bool havePrev=false;
            for(int i=0;i<=segments;++i){
                const float fraction=
                    static_cast<float>(i)/static_cast<float>(segments);
                const float rel=rollVisualCurveStep(note,fraction);
                const float py=
                    viewport.y+header+
                    (start+fraction*length-
                     static_cast<float>(stepOffset))*rowH;
                if(py<gridTop||py>gridBottom)continue;
                const float center=rollPitchX(
                    columns,
                    static_cast<float>(midi)+
                        rollBendAt(selectedTrack,note,rel),
                    pitchOffset,gridLeft,colW);
                float lx=center,rx=center;
                if(rollMode_==RollMode::Velocity){
                    const float half=
                        rollLevelAt(note.velocity,note,rel,1.0f)*
                        (colW*0.48f);
                    lx=center-half;rx=center+half;
                }else if(rollMode_==RollMode::Mod){
                    const float half=
                        rollLevelAt(note.mod,note,rel,0.0f)*
                        (colW*0.48f);
                    lx=center-half;rx=center+half;
                }
                if(havePrev){
                    rollOverlay.addLine(
                        prevLX,prevY,lx,py,thick,color);
                    if(rollMode_!=RollMode::Bend)
                        rollOverlay.addLine(
                            prevRX,prevY,rx,py,thick,color);
                }
                prevLX=lx;prevRX=rx;prevY=py;havePrev=true;
            }

            const auto& points=
                kind==0?note.bend:
                kind==1?note.velocity:note.mod;
            for(const auto& point:points){
                const float rel=
                    std::clamp(point.step,0.0f,rollCurveLength(note));
                const float py=
                    viewport.y+header+
                    (start+rel+0.5f-
                     static_cast<float>(stepOffset))*rowH;
                if(py<gridTop-rowH||py>gridBottom+rowH)continue;

                const bool free=point.free;
                const float radius=
                    free?std::max(7.0f,basis*0.13f):
                         std::max(6.0f,basis*0.11f);
                const float center=rollPitchX(
                    columns,
                    static_cast<float>(midi)+
                        rollBendAt(selectedTrack,note,rel),
                    pitchOffset,gridLeft,colW);

                if(kind==0){
                    const float px=rollPitchX(
                        columns,
                        static_cast<float>(midi)+
                            rollClampBend(selectedTrack,note,point.value),
                        pitchOffset,gridLeft,colW);
                    rollOverlay.addCircle(
                        px,py,radius,thick,color);
                    if(free){
                        rollOverlay.addLine(
                            px-radius,py,px,py-radius,thick,color);
                        rollOverlay.addLine(
                            px,py-radius,px+radius,py,thick,color);
                        rollOverlay.addLine(
                            px+radius,py,px,py+radius,thick,color);
                        rollOverlay.addLine(
                            px,py+radius,px-radius,py,thick,color);
                    }
                }else{
                    const float half=
                        std::clamp(point.value,0.0f,1.0f)*
                        (colW*0.48f);
                    rollOverlay.addCircle(
                        center-half,py,radius,thick,color);
                    rollOverlay.addCircle(
                        center+half,py,radius,thick,color);
                }
            }
        }
    }

    const bool groupEditingNow=
        rollSelectedTrack_==track&&rollNoteSelectionActive()&&
        rollSelectionTool_==RollSelectionTool::Pencil&&
        ((rollMode_==RollMode::Velocity&&rollGroupAutomationKind_==1)||
         (rollMode_==RollMode::Mod&&rollGroupAutomationKind_==2));
    if(groupEditingNow&&!rollGroupAutomationPoints_.empty()){
        const auto gr=rollGroupAutomationRect();
        if(gr.w>0.0f&&gr.h>0.0f){
            rollOverlay.addRect(
                {gr.x,gr.y,gr.w,gr.h},
                overlayColor(mix(kPanel,kRollBg,0.40f),0.92f));
            rollOverlay.addLine(
                gr.x,gr.y,gr.x,gr.y+gr.h,
                2.0f,overlayColor(kCyan,0.75f));
            const float duration=
                std::max(
                    1.0e-5f,
                    rollGroupAutomationEnd_-rollGroupAutomationStart_);
            float prevX=0.0f,prevY=0.0f;
            bool have=false;
            for(const auto& p:rollGroupAutomationPoints_){
                const float px=gr.x+p.value*gr.w;
                const float py=gridTop+
                    (rollGroupAutomationStart_+p.step+0.5f-
                     static_cast<float>(stepOffset))*rowH;
                if(have){
                    rollOverlay.addLine(
                        prevX,prevY,px,py,
                        std::max(2.5f,rowH*0.065f),
                        overlayColor(
                            rollGroupAutomationKind_==1?kWhite:kPurple,
                            0.98f));
                }
                if(py>=gridTop-16.0f&&py<=gridBottom+16.0f){
                    rollOverlay.addCircle(
                        px,py,
                        std::max(6.0f,rowH*0.12f),
                        std::max(2.0f,rowH*0.045f),
                        overlayColor(
                            rollGroupAutomationKind_==1?kWhite:kPurple,
                            0.98f));
                }
                prevX=px;prevY=py;have=true;
            }
            rollOverlay.addText(
                rollGroupAutomationKind_==1?"V":"M",
                gr.x+5.0f,gr.y+5.0f,
                0.62f,
                overlayColor(kWhite,0.78f));
            (void)duration;
        }
    }

    if(rollLassoPath_.size()>=2){
        for(size_t i=1;i<rollLassoPath_.size();++i){
            const auto& a=rollLassoPath_[i-1];
            const auto& b=rollLassoPath_[i];
            rollOverlay.addLine(
                a.x,a.y,b.x,b.y,
                3.0f,
                overlayColor(kCyan,0.96f));
        }
        if(rollLassoPath_.size()>=3){
            const auto& a=rollLassoPath_.back();
            const auto& b=rollLassoPath_.front();
            rollOverlay.addLine(
                a.x,a.y,b.x,b.y,
                2.0f,
                overlayColor(kCyan,0.42f));
        }
    }

    // Blue is the independent playback/paste start marker.
    if(rollStartStep_>=stepOffset&&
       rollStartStep_<=stepOffset+visibleRows){
        const float sy=
            viewport.y+header+(rollStartStep_-stepOffset)*rowH;
        rollOverlay.addRect(
            {viewport.x,sy,viewport.w,3.0f},
            overlayColor(kBlue,0.98f));
    }

    // Orange belongs exclusively to selection mode. The anchor remains
    // visible even before the second boundary is chosen.
    if(rollSelectionActive_){
        const int anchor=rollSelectionAnchorStep_;
        if(anchor>=stepOffset&&anchor<=stepOffset+visibleRows){
            const float ay=
                viewport.y+header+(anchor-stepOffset)*rowH;
            rollOverlay.addRect(
                {viewport.x,ay,viewport.w,3.0f},
                overlayColor(kOrange,0.98f));
        }

        if(rollSelectionAnchorStep_!=rollSelectionEndStep_){
            const int lo=std::min(
                rollSelectionAnchorStep_,rollSelectionEndStep_);
            const int hi=std::max(
                rollSelectionAnchorStep_,rollSelectionEndStep_);
            const float top=
                viewport.y+header+(lo-stepOffset)*rowH;
            const float bottom=
                viewport.y+header+(hi-stepOffset)*rowH;
            const float clippedTop=std::max(viewport.y+header,top);
            const float clippedBottom=std::min(
                viewport.y+viewport.h,bottom);
            if(clippedBottom>clippedTop){
                rollOverlay.addRect(
                    {viewport.x,clippedTop,viewport.w,
                     clippedBottom-clippedTop},
                    overlayColor(kOrange,0.12f));
            }

            const int end=rollSelectionEndStep_;
            if(end>=stepOffset&&end<=stepOffset+visibleRows){
                const float ey=
                    viewport.y+header+(end-stepOffset)*rowH;
                rollOverlay.addRect(
                    {viewport.x,ey,viewport.w,2.0f},
                    overlayColor(kOrange,0.86f));
            }
        }
    }

    if(AudioEngine::instance().transportPlaying()){
        const int ph=AudioEngine::instance().playheadStep();
        if(ph>=stepOffset&&ph<stepOffset+visibleRows){
            const float py=viewport.y+header+(ph-stepOffset)*rowH;
            fillRect({
                viewport.x+gutter,py,
                std::max(0.0f,viewport.w-gutter),2.0f},kCyan);
        }
    }
}

void NativeUi::drawPerformance() const noexcept {
    auto& project=ProjectCore::instance();
    const auto snapshot=project.projectCopy();
    const auto view=contentRect();
    const float header=performanceHeaderPixels();
    const float gridTop=view.y+header;
    const float gridBottom=view.y+view.h;
    const float gutter=std::clamp(view.w*0.054f,40.0f,54.0f);
    const float gridLeft=view.x+gutter;
    const float gridRight=view.x+view.w;
    const float gridW=std::max(1.0f,gridRight-gridLeft);
    const float rowH=performanceRowPixels();
    const int visibleRows=std::max(
        1,static_cast<int>(std::floor(
            (gridBottom-gridTop)/std::max(1.0f,rowH))));
    const float startStep=performanceStartStep();

    std::vector<int> columns;
    columns.reserve(97);
    for(int midi=14;midi<=110;++midi)columns.push_back(midi);
    const float colW=gridW/static_cast<float>(columns.size());

    fillRect(view,kRollBg);
    auto& ov=NativeOverlay::instance();

    // Compact full-range pitch overview: spectrum strip plus D-centered octave
    // markers. All 97 pitches remain visible at once.
    fillRect({view.x,view.y,gutter-1.0f,header},mix(kTop,kBg,0.40f));
    for(size_t i=0;i<columns.size();++i){
        const float x=gridLeft+static_cast<float>(i)*colW;
        const auto base=pitchColor(columns[i]);
        fillRect(
            {x,view.y,std::max(1.0f,colW),header},
            mix(base,kRollBg,0.78f));
        fillRect(
            {x,view.y+header-4.0f,std::max(1.0f,colW),4.0f},
            base);
    }
    ov.addText(
        "PERFORMANCE",
        view.x+7.0f,view.y+header*0.28f,
        0.66f,overlayColor(kWhite));

    for(int midi=14;midi<=110;midi+=12){
        const float x=rollPitchX(
            columns,static_cast<float>(midi),0,gridLeft,colW);
        ov.addLine(
            x,view.y,x,gridBottom,
            1.0f,overlayColor(mix(kBar,kRollBg,0.36f),0.88f));
        std::string label=pitchCoord(midi);
        ov.addTextCentered(
            label,
            {x-colW*3.0f,view.y+3.0f,colW*6.0f,header-8.0f},
            0.52f,overlayColor(kWhite,0.78f));
    }

    const int beatLen=std::max(1,project.divisions());
    const int barLen=std::max(1,project.beats())*beatLen;
    const int firstStep=std::max(
        0,static_cast<int>(std::floor(startStep)));
    const int lastStep=
        static_cast<int>(std::ceil(startStep+
            static_cast<float>(visibleRows)))+1;
    for(int step=firstStep;step<=lastStep;++step){
        const float y=
            gridTop+(static_cast<float>(step)-startStep)*rowH;
        if(y<gridTop||y>gridBottom)continue;
        if(step%beatLen!=0)continue;
        const bool bar=step%barLen==0;
        ov.addLine(
            gridLeft,y,gridRight,y,
            bar?2.0f:1.0f,
            overlayColor(bar?kBar:kBeat,bar?0.78f:0.48f));
        if(bar){
            const int barNumber=step/barLen+1;
            ov.addText(
                "b"+std::to_string(barNumber),
                view.x+5.0f,y+4.0f,
                0.58f,overlayColor(kWhite,0.72f));
        }
    }

    const auto addPerformanceRibbon=[&](
        const Track& sourceTrack,const Note& note,
        int sourceTrackIndex,int noteIndex){
        const float length=std::max(1.0f,note.lengthSteps);
        if(note.startStep+length<=startStep||
           note.startStep>=startStep+static_cast<float>(visibleRows)+1.0f)
            return;

        const int segments=std::clamp(
            static_cast<int>(std::ceil(length*8.0f)),6,360);
        (void)sourceTrackIndex;
        (void)noteIndex;

        for(int i=0;i<segments;++i){
            const float f0=
                static_cast<float>(i)/static_cast<float>(segments);
            const float f1=
                static_cast<float>(i+1)/static_cast<float>(segments);
            float y0=
                gridTop+(note.startStep+f0*length-startStep)*rowH;
            float y1=
                gridTop+(note.startStep+f1*length-startStep)*rowH;
            if(y1<=gridTop||y0>=gridBottom)continue;

            const float s0=rollVisualCurveStep(note,f0);
            const float s1=rollVisualCurveStep(note,f1);
            const float pitch0=
                static_cast<float>(note.midi)+
                rollBendAt(sourceTrack,note,s0);
            const float pitch1=
                static_cast<float>(note.midi)+
                rollBendAt(sourceTrack,note,s1);
            const float c0=rollPitchX(
                columns,pitch0,0,gridLeft,colW);
            const float c1=rollPitchX(
                columns,pitch1,0,gridLeft,colW);

            const float minHalf=0.85f;
            const float maxHalf=std::max(1.25f,colW*0.48f);
            const float v0=rollLevelAt(note.velocity,note,s0,1.0f);
            const float v1=rollLevelAt(note.velocity,note,s1,1.0f);
            const float half0=minHalf+(maxHalf-minHalf)*v0;
            const float half1=minHalf+(maxHalf-minHalf)*v1;
            const float rough0=
                rollLevelAt(note.mod,note,s0,0.0f)*colW*0.15f;
            const float rough1=
                rollLevelAt(note.mod,note,s1,0.0f)*colW*0.15f;
            const float edge0=std::max(
                0.7f,half0+rollJaggedWave(f0*length)*rough0);
            const float edge1=std::max(
                0.7f,half1+rollJaggedWave(f1*length)*rough1);

            const float rawL0=c0-edge0;
            const float rawR0=c0+edge0;
            const float rawL1=c1-edge1;
            const float rawR1=c1+edge1;
            if(std::max(rawR0,rawR1)<=gridLeft||
               std::min(rawL0,rawL1)>=gridRight)
                continue;

            const float x0=std::clamp(rawL0,gridLeft,gridRight);
            const float x1=std::clamp(rawR0,gridLeft,gridRight);
            const float x2=std::clamp(rawR1,gridLeft,gridRight);
            const float x3=std::clamp(rawL1,gridLeft,gridRight);
            y0=std::clamp(y0,gridTop,gridBottom);
            y1=std::clamp(y1,gridTop,gridBottom);
            if(y1<=y0||x1<=x0||x2<=x3)continue;

            const auto topColor=
                overlayColor(rollSpectrumColor(pitch0),0.90f);
            const auto bottomColor=
                overlayColor(rollSpectrumColor(pitch1),0.90f);
            ov.addGradientQuad(
                x0,y0,x1,y0,x2,y1,x3,y1,
                topColor,bottomColor);
        }
    };

    // Actual notes only: no harmonic shadows in Performance Display.
    for(int t=0;t<static_cast<int>(snapshot.tracks.size());++t){
        const auto& sourceTrack=snapshot.tracks[static_cast<size_t>(t)];
        for(int n=0;n<static_cast<int>(sourceTrack.notes.size());++n){
            addPerformanceRibbon(
                sourceTrack,
                sourceTrack.notes[static_cast<size_t>(n)],
                t,n);
        }
    }

    // Keep the current transport position fixed near the upper third while
    // follow mode scrolls the score beneath it.
    const float playhead=AudioEngine::instance().playheadPosition();
    const float py=
        gridTop+(playhead-startStep)*rowH;
    if(py>=gridTop&&py<=gridBottom){
        ov.addLine(
            view.x,py,gridRight,py,
            5.0f,overlayColor(kCyan,0.16f));
        ov.addLine(
            view.x,py,gridRight,py,
            1.8f,overlayColor(kCyan,0.98f));
    }
}

void NativeUi::drawPlaceholder() const noexcept {
    const auto content = contentRect();
    fillRect(content, kPanel);

    const float gap = std::max(6.0f, content.w * 0.012f);
    const float topOffset = pageHasPadQuick(page_) ? std::clamp(content.h * 0.14f, 34.0f, 62.0f) + gap : 0.0f;
    const float rowH = std::max(28.0f, (content.h - topOffset) * 0.12f);
    for (int i = 0; i < 4; ++i) {
        Rect row{
            content.x + gap,
            content.y + topOffset + gap + i * (rowH + gap),
            std::max(0.0f, content.w - gap * 2.0f),
            rowH
        };
        fillRect(row, i == 0 ? mix(kButton, kCyan, 0.22f) : kButton);
    }

    if (pageHasPadQuick(page_)) drawPadQuick();
}

void NativeUi::render() const noexcept {
    if (width_ <= 0 || height_ <= 0) return;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    NativeOverlay::instance().begin(width_,height_);

    glScissor(0, 0, width_, height_);
    glClearColor(kBg.r, kBg.g, kBg.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    const float usableW=std::max(0,width_-safeLeft_-safeRight_);
    const float headerH=std::clamp(usableW*0.142f,94.0f,116.0f);
    fillRect({
        static_cast<float>(safeLeft_),
        static_cast<float>(safeTop_),
        usableW,headerH},kTop);
    auto& overlay=NativeOverlay::instance();
    const float margin=std::clamp(usableW*0.018f,10.0f,18.0f);
    const float logoSize=std::clamp(headerH*0.66f,54.0f,70.0f);
    const NativeOverlay::Rect logoR{
        static_cast<float>(safeLeft_)+margin,
        static_cast<float>(safeTop_)+(headerH-logoSize)*0.5f,
        logoSize,logoSize};
    overlay.addSpectrumLogo(logoR);
    overlay.addText(
        "A I O R A",
        static_cast<float>(safeLeft_)+margin+logoSize+14.0f,
        static_cast<float>(safeTop_)+headerH*0.36f,
        1.02f,overlayColor(kWhite));

    auto& project=ProjectCore::instance();
    auto& audio=AudioEngine::instance();
    const auto startR=headerControlRect(0);
    const auto playR=headerControlRect(1);
    const auto endR=headerControlRect(2);
    fillRect(startR,kButton);
    fillRect(playR,audio.transportPlaying()?kOrange:kGreen);
    fillRect(endR,kButton);
    overlay.addTextCentered(
        "|<",{startR.x,startR.y,startR.w,startR.h},
        1.32f,overlayColor(kWhite));
    if(audio.transportPlaying()){
        const float s=playR.h*0.28f;
        overlay.addRect({
            playR.x+(playR.w-s)*0.5f,
            playR.y+(playR.h-s)*0.5f,
            s,s},overlayColor(kWhite));
    }else{
        overlay.addTextCentered(
            "▶",{playR.x,playR.y,playR.w,playR.h},
            1.65f,overlayColor(kWhite));
    }
    overlay.addTextCentered(
        ">|",{endR.x,endR.y,endR.w,endR.h},
        1.32f,overlayColor(kWhite));
    drawScope();

    const auto performanceR=headerPerformanceRect();
    const bool performanceActive=page_==NativePage::Performance;
    fillRect(performanceR,performanceActive?kCyan:kButton);
    const float performanceInset=std::max(5.0f,performanceR.w*0.16f);
    overlay.addNavIcon(
        2,
        {performanceR.x+performanceInset,
         performanceR.y+performanceInset,
         performanceR.w-performanceInset*2.0f,
         performanceR.h-performanceInset*2.0f},
        overlayColor(performanceActive?kBg:kWhite));
    const float phY=performanceR.y+performanceR.h*0.22f;
    overlay.addLine(
        performanceR.x+performanceR.w*0.18f,phY,
        performanceR.x+performanceR.w*0.82f,phY,
        2.0f,
        overlayColor(performanceActive?kBg:kCyan,0.92f));

    for (int i = 0; i < 6; ++i) {
        const auto page = kNavPages[static_cast<size_t>(i)];
        const auto nr=navRect(i);
        fillRect(nr,page==page_?kCyan:kButton);
        const float inset=std::max(3.0f,nr.h*0.08f);
        overlay.addNavIcon(
            kNavIconIndices[static_cast<size_t>(i)],
            {nr.x+inset,nr.y+inset,nr.w-inset*2.0f,nr.h-inset*2.0f},
            overlayColor(page==page_?kBg:kWhite));
    }

    if(page_!=NativePage::Tracks&&page_!=NativePage::Performance)
        drawTrackSwitchBar();

    const auto pageClip=
        (page_==NativePage::Tracks||page_==NativePage::Performance)
        ?contentRect():bodyContentRect();
    overlay.setClip({pageClip.x,pageClip.y,pageClip.w,pageClip.h});

    switch (page_) {
        case NativePage::Tracks:
            drawTracks();
            break;
        case NativePage::Drums:
            drawDrums();
            break;
        case NativePage::Roll:
            drawRoll();
            break;
        case NativePage::Synth:
            fillRect(bodyContentRect(), kPanel);
            NativeEditor::instance().renderSynth();
            break;
        case NativePage::Fx:
            fillRect(bodyContentRect(), kPanel);
            NativeEditor::instance().renderFx();
            break;
        case NativePage::Performance:
            drawPerformance();
            break;
        case NativePage::Play:{
            const auto body=bodyContentRect();
            const float gridW=std::max(0.0f,body.w);
            const float cardH=std::min(
                body.h,
                64.0f+gridW+18.0f);
            const Rect card{body.x,body.y,body.w,cardH};
            fillRect(card,kButton);
            fillRect({
                card.x+2.0f,card.y+2.0f,
                std::max(0.0f,card.w-4.0f),
                std::max(0.0f,card.h-4.0f)},kPanel);
            overlay.addText(
                "PLAY · space = play/pause",
                card.x+18.0f,card.y+18.0f,
                0.98f,overlayColor(kWhite));
            drawGrid();
            break;
        }
        default:
            drawPlaceholder();
            break;
    }

    overlay.clearClip();
    if(dropdownOpen()){
        // The page contains deferred overlay geometry (glyphs/text). Flush it
        // before painting the popup so nothing from underneath can be drawn
        // over the dropdown afterward.
        overlay.flush();
        overlay.begin(width_,height_);
        glEnable(GL_SCISSOR_TEST);
        drawDropdown();
        glDisable(GL_SCISSOR_TEST);
        overlay.flush();
    }else{
        glDisable(GL_SCISSOR_TEST);
        overlay.flush();
    }
}

std::optional<HeaderAction> NativeUi::hitHeader(float x,float y) const noexcept {
    if(headerControlRect(0).contains(x,y))return HeaderAction::TransportStart;
    if(headerControlRect(1).contains(x,y))return HeaderAction::TransportToggle;
    if(headerControlRect(2).contains(x,y))return HeaderAction::TransportEnd;
    if(headerPerformanceRect().contains(x,y))return HeaderAction::PerformanceView;
    return std::nullopt;
}

std::optional<NativePage> NativeUi::hitNav(float x, float y) const noexcept {
    for (int i = 0; i < 6; ++i) {
        if (navRect(i).contains(x, y)) {
            return kNavPages[static_cast<size_t>(i)];
        }
    }
    return std::nullopt;
}

std::optional<TrackSwitchAction> NativeUi::hitTrackSwitch(float x,float y) const noexcept {
    if(page_==NativePage::Tracks||page_==NativePage::Performance)
        return std::nullopt;
    if(trackSwitchRect(0).contains(x,y))return TrackSwitchAction::PreviousTrack;
    if(trackSwitchRect(2).contains(x,y))return TrackSwitchAction::NextTrack;

    if(page_==NativePage::Synth||page_==NativePage::Fx){
        auto& project=ProjectCore::instance();
        const int track=project.selectedTrack();
        const auto pr=trackSwitchRect(6);
        if(track>=0&&project.trackIsDrums(track)&&pr.contains(x,y)){
            return x<pr.x+pr.w*0.5f
                ?TrackSwitchAction::PreviousPad
                :TrackSwitchAction::NextPad;
        }
    }
    return std::nullopt;
}

std::optional<int> NativeUi::hitPitch(float x, float y) const noexcept {
    if (page_ != NativePage::Play && page_ != NativePage::Drums) {
        return std::nullopt;
    }

    for (int visualRow = 0; visualRow < 7; ++visualRow) {
        for (int column = 0; column < 7; ++column) {
            if (!gridRect(visualRow, column).contains(x, y)) continue;
            const int block = 6 - visualRow;
            return kGridLow + block * 7 + column;
        }
    }
    return std::nullopt;
}

std::optional<int> NativeUi::hitTrack(float x, float y) const noexcept {
    if (page_ != NativePage::Tracks) return std::nullopt;
    const int count = ProjectCore::instance().trackCount();
    for (int i = 0; i < count; ++i) {
        if (trackRect(i, count).contains(x, y)) return i;
    }
    return std::nullopt;
}

std::optional<int> NativeUi::hitTrackName(float x,float y) const noexcept {
    if(page_!=NativePage::Tracks)return std::nullopt;
    const int count=ProjectCore::instance().trackCount();
    for(int i=0;i<count;++i){
        const auto row=trackRect(i,count);
        const Rect nameRect{
            row.x+44.0f,
            row.y+6.0f,
            std::max(0.0f,row.w*0.38f-44.0f),
            54.0f
        };
        if(nameRect.contains(x,y))return i;
    }
    return std::nullopt;
}

std::optional<TrackUtilityAction> NativeUi::hitTrackUtility(float x,float y) const noexcept {
    if(page_!=NativePage::Tracks)return std::nullopt;
    if(trackUtilityRect(0).contains(x,y))return TrackUtilityAction::DozenalToggle;
    if(trackUtilityRect(1).contains(x,y))return TrackUtilityAction::AiFromClipboard;
    if(trackUtilityRect(2).contains(x,y))return TrackUtilityAction::ClearTrack;
    if(trackUtilityRect(3).contains(x,y))return TrackUtilityAction::NewProject;
    if(trackUtilityRect(4).contains(x,y))return TrackUtilityAction::SaveProject;
    if(trackUtilityRect(5).contains(x,y))return TrackUtilityAction::LoadProject;
    return std::nullopt;
}

std::optional<ProjectTransferAction> NativeUi::hitProjectTransfer(float x,float y) const noexcept {
    if(page_!=NativePage::Tracks)return std::nullopt;
    if(projectTransferRect(0).contains(x,y))return ProjectTransferAction::ExportWav;
    if(projectTransferRect(1).contains(x,y))return ProjectTransferAction::ExportMidi;
    return std::nullopt;
}

std::optional<TrackAddKind> NativeUi::hitAddTrack(float x, float y) const noexcept {
    if (page_ != NativePage::Tracks) return std::nullopt;
    if (addTrackRect(TrackAddKind::Melodic).contains(x, y)) return TrackAddKind::Melodic;
    return std::nullopt;
}

std::optional<int> NativeUi::hitPadQuick(float x, float y) const noexcept {
    if (!pageHasPadQuick(page_)) return std::nullopt;

    auto& project = ProjectCore::instance();
    const int track = project.selectedTrack();
    if (track < 0 || !project.trackIsDrums(track)) return std::nullopt;

    const int count = project.padCount(track);
    for (int i = 0; i < count; ++i) {
        if (padQuickRect(i, count).contains(x, y)) return i;
    }
    return std::nullopt;
}

std::optional<int> NativeUi::hitPadName(float x,float y) const noexcept {
    if(page_!=NativePage::Drums)return std::nullopt;
    auto& project=ProjectCore::instance();
    const int track=project.selectedTrack();
    if(track<0||!project.trackIsDrums(track))return std::nullopt;
    const int count=project.padCount(track);
    for(int i=0;i<count;++i){
        const auto row=padQuickRect(i,count);
        const Rect nameRect{
            row.x+242.0f,
            row.y+8.0f,
            std::max(0.0f,row.w-322.0f),
            52.0f
        };
        if(nameRect.contains(x,y))return i;
    }
    return std::nullopt;
}

std::optional<RollMode> NativeUi::hitRollMode(float x, float y) const noexcept {
    if(page_!=NativePage::Roll)return std::nullopt;
    const RollMode modes[3]{RollMode::Bend,RollMode::Velocity,RollMode::Mod};
    const bool lassoMode=
        rollSelectionTool_==RollSelectionTool::Lasso&&
        !(rollSelectionActive_&&
          rollSelectionAnchorStep_!=rollSelectionEndStep_);
    for(int i=0;i<3;++i){
        if(i==0&&rollHasAnySelection())continue;
        if(lassoMode&&(i==1||i==2))continue;
        if(rollModeRect(i).contains(x,y))return modes[i];
    }
    return std::nullopt;
}

bool NativeUi::hitRollSelectionTool(float x,float y) const noexcept {
    if(page_!=NativePage::Roll)return false;
    if(rollSelectionActive_&&
       rollSelectionAnchorStep_!=rollSelectionEndStep_)return false;
    return rollSelectionToolRect().contains(x,y);
}

bool NativeUi::hitRollRangeScopeToggle(float x,float y) const noexcept {
    if(page_!=NativePage::Roll||
       !rollSelectionActive_||
       rollSelectionAnchorStep_==rollSelectionEndStep_)return false;
    return rollSelectionToolRect().contains(x,y);
}

bool NativeUi::hitRollDelete(float x,float y) const noexcept {
    return page_==NativePage::Roll&&rollHasAnySelection()&&
        rollModeRect(0).contains(x,y);
}

bool NativeUi::hitRollPitchHeader(float x,float y) const noexcept {
    if(page_!=NativePage::Roll)return false;
    const auto viewport=rollViewportRect();
    const float gutter=rollGutterPixels();
    const float header=rollHeaderPixels();
    return x>=viewport.x+gutter && x<viewport.x+viewport.w &&
           y>=viewport.y && y<viewport.y+header;
}

std::optional<int> NativeUi::hitRollPitchHeaderMidi(float x,float y) const noexcept {
    if(!hitRollPitchHeader(x,y))return std::nullopt;
    const auto viewport=rollViewportRect();
    const float gutter=rollGutterPixels();
    const float colW=rollColumnPixels();
    const int visibleCol=static_cast<int>(
        std::floor((x-viewport.x-gutter)/colW));
    const auto columns=rollColumns();
    const int colIndex=rollPitchOffset_+visibleCol;
    if(visibleCol<0||colIndex<0||colIndex>=static_cast<int>(columns.size()))
        return std::nullopt;
    return columns[static_cast<size_t>(colIndex)];
}

bool NativeUi::hitRollBeatGutter(float x,float y) const noexcept {
    if(page_!=NativePage::Roll)return false;
    const auto viewport=rollViewportRect();
    const float gutter=rollGutterPixels();
    const float header=rollHeaderPixels();
    return x>=viewport.x && x<viewport.x+gutter &&
           y>=viewport.y+header && y<viewport.y+viewport.h;
}

std::optional<int> NativeUi::hitRollBeatStep(float x,float y) const noexcept {
    if(!hitRollBeatGutter(x,y))return std::nullopt;
    const auto viewport=rollViewportRect();
    const float rowH=rollCellPixels();
    const float header=rollHeaderPixels();
    const float local=(y-viewport.y-header)/std::max(1.0f,rowH);
    const int step=rollStepOffset_+
        static_cast<int>(std::floor(local));
    return std::max(0,step);
}

std::optional<RollCornerAction> NativeUi::hitRollCornerAction(
    float x,float y) const noexcept {
    if(page_!=NativePage::Roll)return std::nullopt;
    const auto viewport=rollViewportRect();
    const Rect corner{
        viewport.x,viewport.y,
        rollGutterPixels()-1.0f,
        rollHeaderPixels()-1.0f};
    if(!corner.contains(x,y))return std::nullopt;
    if(rollSelectionActive_&&
       rollSelectionAnchorStep_!=rollSelectionEndStep_)
        return RollCornerAction::Copy;
    if(!rollSelectionActive_&&rollClipboardAvailable_)
        return RollCornerAction::Paste;
    return std::nullopt;
}

bool NativeUi::hitRollNoteArea(float x,float y) const noexcept {
    if(page_!=NativePage::Roll)return false;
    const auto viewport=rollViewportRect();
    const float gutter=rollGutterPixels();
    const float header=rollHeaderPixels();
    return x>=viewport.x+gutter && x<viewport.x+viewport.w &&
           y>=viewport.y+header && y<viewport.y+viewport.h;
}

std::optional<RollNoteHit> NativeUi::hitRollNote(
    float x,float y) const noexcept {

    if(page_!=NativePage::Roll||!hitRollNoteArea(x,y))
        return std::nullopt;

    auto& project=ProjectCore::instance();
    const int track=project.selectedTrack();
    if(track<0)return std::nullopt;

    const auto snapshot=project.projectCopy();
    if(track>=static_cast<int>(snapshot.tracks.size()))
        return std::nullopt;
    const auto& sourceTrack=snapshot.tracks[static_cast<size_t>(track)];

    const auto viewport=rollViewportRect();
    const float colW=rollColumnPixels();
    const float rowH=rollCellPixels();
    const float gutter=rollGutterPixels();
    const float header=rollHeaderPixels();
    const auto columns=rollColumns();
    if(columns.empty())return std::nullopt;

    const float gridLeft=viewport.x+gutter;
    const float gridTop=viewport.y+header;
    const float bodyMargin=std::max(5.0f,std::min(colW,rowH)*0.10f);
    const float tailYRadius=std::max(18.0f,rowH*0.34f);
    const float tailXMargin=std::max(8.0f,colW*0.12f);

    // Iterate in reverse draw order so the visually top-most selected-track
    // ribbon wins when shaped notes overlap.
    for(int n=static_cast<int>(sourceTrack.notes.size())-1;n>=0;--n){
        const auto& note=sourceTrack.notes[static_cast<size_t>(n)];
        const float length=std::max(1.0f,note.lengthSteps);
        const float top=
            gridTop+
            (note.startStep-static_cast<float>(rollStepOffset_))*rowH+1.0f;
        const float height=std::max(rowH,length*rowH)-2.0f;
        const float bottom=top+height;
        if(y<top-bodyMargin||y>bottom+bodyMargin)continue;

        const float fraction=std::clamp(
            (y-top)/std::max(1.0f,height),0.0f,1.0f);
        const float step=rollVisualCurveStep(note,fraction);
        const float center=rollPitchX(
            columns,
            static_cast<float>(note.midi)+
                rollBendAt(sourceTrack,note,step),
            rollPitchOffset_,gridLeft,colW);

        const float minHalf=std::max(1.5f,colW*0.055f);
        const float maxHalf=std::max(minHalf,colW*0.5f-2.0f);
        const float velocity=rollLevelAt(
            note.velocity,note,step,1.0f);
        const float half=
            minHalf+(maxHalf-minHalf)*velocity;
        const float rough=
            rollLevelAt(note.mod,note,step,0.0f)*colW*0.11f;
        if(std::fabs(x-center)>half+rough+bodyMargin)continue;

        const float tailStep=rollVisualCurveStep(note,1.0f);
        const float tailCenter=rollPitchX(
            columns,
            static_cast<float>(note.midi)+
                rollBendAt(sourceTrack,note,tailStep),
            rollPitchOffset_,gridLeft,colW);
        const float tailVelocity=rollLevelAt(
            note.velocity,note,tailStep,1.0f);
        const float tailHalf=
            minHalf+(maxHalf-minHalf)*tailVelocity;
        const float tailRough=
            rollLevelAt(note.mod,note,tailStep,0.0f)*colW*0.11f;
        const bool tail=
            std::fabs(y-bottom)<=tailYRadius&&
            std::fabs(x-tailCenter)<=
                tailHalf+tailRough+tailXMargin;

        return RollNoteHit{n,tail};
    }

    return std::nullopt;
}

std::optional<RollCellHit> NativeUi::hitRollCell(float x,float y) const noexcept {
    if(page_!=NativePage::Roll)return std::nullopt;

    const auto viewport=rollViewportRect();
    const float colW=rollColumnPixels();
    const float rowH=rollCellPixels();
    const float gutter=rollGutterPixels();
    const float header=rollHeaderPixels();
    if(x<viewport.x+gutter||y<viewport.y+header||
       x>=viewport.x+viewport.w||y>=viewport.y+viewport.h){
        return std::nullopt;
    }

    const auto columns=rollColumns();
    const int visibleCol=static_cast<int>(
        std::floor((x-viewport.x-gutter)/colW));
    const int visibleRow=static_cast<int>(
        std::floor((y-viewport.y-header)/rowH));
    const int colIndex=rollPitchOffset_+visibleCol;
    const int step=rollStepOffset_+visibleRow;
    if(visibleCol<0||visibleRow<0||
       colIndex<0||colIndex>=static_cast<int>(columns.size())||
       step<0||step>=rollTotalRows()){
        return std::nullopt;
    }

    const float cellLeft=viewport.x+gutter+visibleCol*colW;
    const float cellTop=viewport.y+header+visibleRow*rowH;
    return RollCellHit{
        columns[static_cast<size_t>(colIndex)],
        step,
        std::clamp((x-cellLeft)/colW,0.0f,1.0f),
        std::clamp((y-cellTop)/rowH,0.0f,1.0f)
    };
}

bool NativeUi::rollAutomationPosition(
    float x,float y,int noteIndex,int curveKind,bool free,
    float& step,float& value) const noexcept {

    auto& project=ProjectCore::instance();
    const int track=project.selectedTrack();
    if(track<0||noteIndex<0||noteIndex>=project.noteCount(track))return false;

    const auto viewport=rollViewportRect();
    const float colW=rollColumnPixels();
    const float rowH=rollCellPixels();
    const float gutter=rollGutterPixels();
    const float header=rollHeaderPixels();
    const auto columns=rollColumns();
    const auto snapshot=project.projectCopy();
    if(track>=static_cast<int>(snapshot.tracks.size()))return false;
    const auto& sourceTrack=snapshot.tracks[static_cast<size_t>(track)];
    if(noteIndex>=static_cast<int>(sourceTrack.notes.size()))return false;
    const auto& note=sourceTrack.notes[static_cast<size_t>(noteIndex)];

    const int midi=note.midi;
    const float gridLeft=viewport.x+gutter;
    const float baseX=rollPitchX(
        columns,static_cast<float>(midi),
        rollPitchOffset_,gridLeft,colW);

    const float start=note.startStep;
    const float length=std::max(1.0f,note.lengthSteps);
    const float maxStep=std::max(0.0f,length-1.0f);

    if(free){
        const float absolute=
            rollStepOffset_+
            (y-viewport.y-header)/rowH-0.5f;
        step=std::clamp(absolute-start,0.0f,maxStep);
    }else{
        const auto hit=hitRollCell(x,y);
        if(!hit)return false;
        step=std::clamp(
            static_cast<float>(hit->step)-start,
            0.0f,maxStep);
    }

    if(curveKind==0){
        if(free){
            const float targetPitch=rollPitchFromX(
                columns,x,rollPitchOffset_,gridLeft,colW);
            value=rollClampBend(
                sourceTrack,note,targetPitch-static_cast<float>(midi));
        }else{
            const auto hit=hitRollCell(x,y);
            if(!hit)return false;
            value=rollClampBend(
                sourceTrack,note,
                static_cast<float>(hit->midi-midi));
        }
    }else{
        const float centerX=rollPitchX(
            columns,
            static_cast<float>(midi)+
                rollBendAt(sourceTrack,note,step),
            rollPitchOffset_,gridLeft,colW);
        value=std::clamp(
            std::fabs(x-centerX)/(colW*0.48f),
            0.0f,1.0f);
    }
    return true;
}

std::optional<RollAutomationHit> NativeUi::hitRollAutomation(
    float x,float y,int curveKind) const noexcept {

    if(page_!=NativePage::Roll||curveKind<0||curveKind>2)return std::nullopt;
    auto& project=ProjectCore::instance();
    const int track=project.selectedTrack();
    if(track<0)return std::nullopt;

    const auto viewport=rollViewportRect();
    const float colW=rollColumnPixels();
    const float rowH=rollCellPixels();
    const float gutter=rollGutterPixels();
    const float header=rollHeaderPixels();
    const auto columns=rollColumns();
    const float gridLeft=viewport.x+gutter;
    const float radius=std::max(20.0f,std::min(colW,rowH)*0.42f);
    const float radius2=radius*radius;
    const auto snapshot=project.projectCopy();
    if(track>=static_cast<int>(snapshot.tracks.size()))return std::nullopt;
    const auto& sourceTrack=snapshot.tracks[static_cast<size_t>(track)];

    int bestNote=-1,bestPoint=-1;
    float bestDist=radius2;
    float bestStep=0.0f,bestValue=0.0f;
    bool bestFree=false;

    const int noteCount=static_cast<int>(sourceTrack.notes.size());
    for(int n=0;n<noteCount;++n){
        const auto& note=sourceTrack.notes[static_cast<size_t>(n)];
        const int midi=note.midi;
        const float start=note.startStep;
        const auto& points=
            curveKind==0?note.bend:
            curveKind==1?note.velocity:note.mod;

        for(int p=0;p<static_cast<int>(points.size());++p){
            const auto& point=points[static_cast<size_t>(p)];
            const float rel=point.step;
            const float v=point.value;
            const float py=
                viewport.y+header+
                (start+rel+0.5f-rollStepOffset_)*rowH;

            if(curveKind==0){
                const float px=rollPitchX(
                    columns,
                    static_cast<float>(midi)+
                        rollClampBend(sourceTrack,note,v),
                    rollPitchOffset_,gridLeft,colW);
                const float dx=px-x,dy=py-y,dd=dx*dx+dy*dy;
                if(dd<=bestDist){
                    bestDist=dd;bestNote=n;bestPoint=p;
                    bestStep=rel;bestValue=v;bestFree=point.free;
                }
            }else{
                const float center=rollPitchX(
                    columns,
                    static_cast<float>(midi)+
                        rollBendAt(sourceTrack,note,rel),
                    rollPitchOffset_,gridLeft,colW);
                const float half=std::clamp(v,0.0f,1.0f)*(colW*0.48f);
                for(float px:{center-half,center+half}){
                    const float dx=px-x,dy=py-y,dd=dx*dx+dy*dy;
                    if(dd<=bestDist){
                        bestDist=dd;bestNote=n;bestPoint=p;
                        bestStep=rel;bestValue=v;bestFree=point.free;
                    }
                }
            }
        }
    }

    if(bestPoint>=0){
        return RollAutomationHit{
            bestNote,bestPoint,bestStep,bestValue,bestFree};
    }

    const auto cellHit=hitRollCell(x,y);
    if(!cellHit)return std::nullopt;

    int noteIndex=-1;
    float noteDistance=1.0e9f;
    for(int n=0;n<noteCount;++n){
        const auto& note=sourceTrack.notes[static_cast<size_t>(n)];
        const float start=note.startStep;
        const float length=std::max(1.0f,note.lengthSteps);
        if(static_cast<float>(cellHit->step)<start||
           static_cast<float>(cellHit->step)>start+length-1.0f)continue;
        const float rel=std::clamp(
            static_cast<float>(cellHit->step)-start,
            0.0f,rollCurveLength(note));
        const float center=rollPitchX(
            columns,
            static_cast<float>(note.midi)+
                rollBendAt(sourceTrack,note,rel),
            rollPitchOffset_,gridLeft,colW);
        const float d=std::fabs(center-x);
        const float limit=
            curveKind==0?colW*3.2f:
            std::max(colW*0.62f,
                (colW*0.5f)*rollLevelAt(
                    curveKind==1?note.velocity:note.mod,
                    note,rel,curveKind==1?1.0f:0.0f)+radius);
        if(d<=limit&&d<noteDistance){
            noteDistance=d;noteIndex=n;
        }
    }
    if(noteIndex<0)return std::nullopt;

    float step=0.0f,value=0.0f;
    if(!rollAutomationPosition(
        x,y,noteIndex,curveKind,false,step,value))return std::nullopt;
    return RollAutomationHit{noteIndex,-1,step,value,false};
}

int NativeUi::padIndexForMidi(int midi) noexcept {
    const int clamped = std::clamp(midi, kGridLow, kGridHigh);
    const int lowToHighBlock = (clamped - kGridLow) / 7;
    return 6 - lowToHighBlock;
}

} // namespace aiora
