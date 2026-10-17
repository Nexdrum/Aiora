#include "NativeEditor.h"
#include "NativeOverlay.h"

#include <GLES3/gl3.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cmath>
#include <string>

namespace aiora {
namespace {

constexpr NativeEditor::Rgb kBg{0.0627f,0.0706f,0.0863f};
constexpr NativeEditor::Rgb kPanel{0.0863f,0.1020f,0.1294f};
constexpr NativeEditor::Rgb kButton{0.1373f,0.1569f,0.2000f};
constexpr NativeEditor::Rgb kRollBg{0.0471f,0.0549f,0.0706f};
constexpr NativeEditor::Rgb kTrack{0.18f,0.21f,0.27f};
constexpr NativeEditor::Rgb kMuted{0.35f,0.39f,0.47f};
constexpr NativeEditor::Rgb kCyan{0.0f,0.80f,0.80f};
constexpr NativeEditor::Rgb kOrange{1.0f,0.6667f,0.0f};
constexpr NativeEditor::Rgb kPurple{0.788f,0.557f,1.0f};
constexpr NativeEditor::Rgb kGreen{0.23f,0.92f,0.45f};
constexpr NativeEditor::Rgb kRed{0.95f,0.18f,0.18f};
constexpr NativeEditor::Rgb kWhite{0.91f,0.925f,0.945f};

constexpr std::array<NativeEditor::Rgb,12> kSpectrumColors{{
    {0.2275f,1.0000f,0.0000f},
    {0.0000f,1.0000f,0.9255f},
    {0.0000f,0.5608f,1.0000f},
    {0.0588f,0.0000f,0.9843f},
    {0.3882f,0.0000f,0.7451f},
    {0.4314f,0.0000f,0.5020f},
    {0.5961f,0.0000f,0.0000f},
    {0.7843f,0.0000f,0.0000f},
    {0.9529f,0.0000f,0.0000f},
    {1.0000f,0.4706f,0.0000f},
    {1.0000f,0.9373f,0.0000f},
    {0.6667f,1.0000f,0.0000f},
}};

constexpr std::array<PatchParam,7> kFilterParams{
    PatchParam::FilterCutoff,PatchParam::FilterResonance,PatchParam::FilterEnv,
    PatchParam::FilterAttack,PatchParam::FilterDecay,PatchParam::FilterSustain,PatchParam::FilterRelease
};
constexpr std::array<PatchParam,9> kAmpParams{
    PatchParam::AmpAttack,PatchParam::AmpDecay,PatchParam::AmpSustain,PatchParam::AmpRelease,
    PatchParam::Volume,PatchParam::VelocityAmp,PatchParam::VelocityFilter,PatchParam::Unison,PatchParam::Glide
};
constexpr std::array<PatchParam,4> kLfoParams{PatchParam::LfoRate,PatchParam::LfoAmount,PatchParam::LfoAttack,PatchParam::LfoVelocity};
constexpr std::array<OperatorParam,8> kOperatorParams{
    OperatorParam::Ratio,OperatorParam::Semitone,
    OperatorParam::Detune,OperatorParam::Level,
    OperatorParam::Attack,OperatorParam::Decay,
    OperatorParam::Sustain,OperatorParam::Release
};

NativeEditor::Rgb mix(NativeEditor::Rgb a,NativeEditor::Rgb b,float t) noexcept {
    t=std::clamp(t,0.0f,1.0f);
    return {a.r+(b.r-a.r)*t,a.g+(b.g-a.g)*t,a.b+(b.b-a.b)*t};
}

NativeOverlay::Color overlayColor(NativeEditor::Rgb c,float alpha=1.0f) noexcept {
    return {c.r,c.g,c.b,alpha};
}

const char* modTargetName(ModTarget t) noexcept {
    switch(t){
        case ModTarget::None:return "NONE";
        case ModTarget::Cutoff:return "CUTOFF";
        case ModTarget::Resonance:return "RESO";
        case ModTarget::FilterEnv:return "FENV";
        case ModTarget::AmpAttack:return "A ATK";
        case ModTarget::AmpDecay:return "A DEC";
        case ModTarget::AmpSustain:return "A SUS";
        case ModTarget::AmpRelease:return "A REL";
        case ModTarget::FilterAttack:return "F ATK";
        case ModTarget::FilterDecay:return "F DEC";
        case ModTarget::FilterSustain:return "F SUS";
        case ModTarget::FilterRelease:return "F REL";
        case ModTarget::Op1:return "OP1"; case ModTarget::Op2:return "OP2";
        case ModTarget::Op3:return "OP3"; case ModTarget::Op4:return "OP4";
        case ModTarget::Op5:return "OP5"; case ModTarget::Op6:return "OP6";
        case ModTarget::Morph1:return "MORPH1"; case ModTarget::Morph2:return "MORPH2";
        case ModTarget::Morph3:return "MORPH3"; case ModTarget::Morph4:return "MORPH4";
        case ModTarget::Morph5:return "MORPH5"; case ModTarget::Morph6:return "MORPH6";
        case ModTarget::Fm:return "FM";
        case ModTarget::LfoAmount:return "LFO A";
        case ModTarget::LfoRate:return "LFO R";
        case ModTarget::Unison:return "UNI";
        case ModTarget::Volume:return "VOL";
        case ModTarget::Op1Decay:return "OP1 DEC";case ModTarget::Op2Decay:return "OP2 DEC";
        case ModTarget::Op3Decay:return "OP3 DEC";case ModTarget::Op4Decay:return "OP4 DEC";
        case ModTarget::Op5Decay:return "OP5 DEC";case ModTarget::Op6Decay:return "OP6 DEC";
        case ModTarget::PeakRatio:return "PEAK x";
        case ModTarget::PeakFrequency:return "PEAK Hz";
        case ModTarget::PeakQ:return "PEAK Q";
        case ModTarget::PeakGain:return "PEAK dB";
    }
    return "?";
}

bool selectedTrackIsDrums(){
    auto& p=ProjectCore::instance();
    const int t=p.selectedTrack();
    return t>=0&&p.trackIsDrums(t);
}

} // namespace

NativeEditor& NativeEditor::instance(){static NativeEditor e;return e;}

void NativeEditor::resize(int width,int height) noexcept {
    width_=std::max(0,width);
    height_=std::max(0,height);
}

void NativeEditor::setSafeInsets(int left,int top,int right,int bottom) noexcept {
    safeLeft_=std::clamp(left,0,std::max(0,width_/2));
    safeTop_=std::clamp(top,0,std::max(0,height_/2));
    safeRight_=std::clamp(right,0,std::max(0,width_/2));
    safeBottom_=std::clamp(bottom,0,std::max(0,height_/2));
}

NativeEditor::Rect NativeEditor::viewportRect() const noexcept {
    // Mirror NativeUi's exact header/nav/track-switch geometry. Keeping a
    // second, older set of dimensions here caused the Synth tabs to start
    // inside the track selector row.
    const float usableW=std::max(0,width_-safeLeft_-safeRight_);
    const float margin=std::clamp(usableW*0.018f,10.0f,18.0f);
    const float headerH=std::clamp(usableW*0.142f,94.0f,116.0f);
    const float navGap=std::clamp(usableW*0.010f,7.0f,11.0f);
    const float navH=std::clamp(usableW*0.106f,76.0f,92.0f);
    const float contentGap=std::clamp(usableW*0.016f,10.0f,16.0f);
    const float contentW=std::max(0.0f,usableW-margin*2.0f);
    const float switchGap=std::max(
        5.0f,
        static_cast<float>(std::min(width_,height_))*0.007f);
    const float switchH=std::clamp(contentW*0.090f,68.0f,82.0f);
    const float editorGap=std::clamp(contentW*0.016f,10.0f,16.0f);
    const float top=
        static_cast<float>(safeTop_)+
        headerH+navGap+navH+contentGap+switchH+switchGap+editorGap;
    const float bottom=static_cast<float>(height_-safeBottom_);
    return {
        static_cast<float>(safeLeft_)+margin,
        top,
        contentW,
        std::max(0.0f,bottom-top-margin)
    };
}

float NativeEditor::scrollGutterPixels() const noexcept {
    const float shortSide=static_cast<float>(std::min(
        std::max(1,width_-safeLeft_-safeRight_),
        std::max(1,height_-safeTop_-safeBottom_)));
    const float cell=std::clamp(shortSide*0.076f,54.0f,68.0f);
    return std::clamp(cell*1.55f,78.0f,108.0f);
}

NativeEditor::Rect NativeEditor::scrollGutterRect() const noexcept {
    const auto view=viewportRect();
    const float gutter=std::min(
        scrollGutterPixels(),
        std::max(0.0f,view.w-120.0f));
    return {view.x,view.y,gutter,view.h};
}

NativeEditor::Rect NativeEditor::contentRect() const noexcept {
    const auto view=viewportRect();
    const float gutter=std::min(
        scrollGutterPixels(),
        std::max(0.0f,view.w-120.0f));
    return {
        view.x+gutter,
        view.y,
        std::max(0.0f,view.w-gutter),
        view.h
    };
}

NativeEditor::Rect NativeEditor::editorRect() const noexcept {
    return contentRect();
}

NativeEditor::Rect NativeEditor::patchTransferRect(EditorPage page,int index) const noexcept {
    if(index==0)return {};
    const auto e=editorRect();
    const float gap=std::clamp(e.w*0.014f,9.0f,13.0f);
    const float h=page==EditorPage::Synth?52.0f:70.0f;
    const float w=(e.w-gap*3.0f)*0.5f;

    float top=e.y+gap;
    if(page==EditorPage::Synth){
        top=e.y+e.h-63.0f;
    }else{
        const auto last=fxGroupRect(3);
        top=last.y+last.h+gap+46.0f;
    }

    return {
        e.x+gap+(index-1)*(w+gap),
        top,
        w,h
    };
}

NativeEditor::Rect NativeEditor::bodyRect() const noexcept {
    return editorRect();
}

NativeEditor::Rect NativeEditor::factoryPresetRect(int) const noexcept {
    return {};
}

NativeEditor::Rect NativeEditor::synthTabRect(int index) const noexcept {
    const auto b=bodyRect();
    const float gap=std::clamp(b.w*0.014f,9.0f,13.0f);
    const float h=66.0f;
    const float w=(b.w-gap*3.0f)*0.5f;
    return {
        b.x+gap+index*(w+gap),
        b.y+gap,
        w,h
    };
}

NativeEditor::Rect NativeEditor::operatorCardRect(int index) const noexcept {
    const auto b=bodyRect();
    const auto tab=synthTabRect(0);
    const float gap=std::clamp(b.w*0.014f,9.0f,13.0f);
    const float w=(b.w-gap*3.0f)*0.5f;
    const float h=1022.0f;
    const int row=index/2,col=index%2;
    return {
        b.x+gap+col*(w+gap),
        tab.y+tab.h+gap+row*(h+gap)-synthScrollY_,
        w,h
    };
}

NativeEditor::Rect NativeEditor::operatorWaveFieldRect(int op) const noexcept {
    const auto c=operatorCardRect(op);
    return {c.x+16.0f,c.y+62.0f,c.w-32.0f,66.0f};
}

NativeEditor::Rect NativeEditor::operatorCardParamRect(int op,int param) const noexcept {
    const auto c=operatorCardRect(op);
    return {
        c.x+16.0f,
        c.y+144.0f+param*82.0f,
        c.w-32.0f,
        72.0f
    };
}

NativeEditor::Rect NativeEditor::operatorCardToggleRect(int op) const noexcept {
    const auto c=operatorCardRect(op);
    return {c.x+16.0f,c.y+818.0f,c.w-32.0f,70.0f};
}

NativeEditor::Rect NativeEditor::operatorCardHarmonicRect(int op,int partial) const noexcept {
    const auto c=operatorCardRect(op);
    const float gap=7.0f;
    const int row=partial/8,col=partial%8;
    const float w=(c.w-32.0f-gap*7.0f)/8.0f;
    return {
        c.x+16.0f+col*(w+gap),
        c.y+908.0f+row*56.0f,
        w,48.0f
    };
}

// Legacy geometry helpers retained as wrappers for compatibility.
NativeEditor::Rect NativeEditor::operatorSelectRect(int index) const noexcept {
    return operatorCardRect(index);
}
NativeEditor::Rect NativeEditor::waveRect(int index) const noexcept {
    return operatorWaveFieldRect(std::clamp(index,0,5));
}
NativeEditor::Rect NativeEditor::operatorToggleRect() const noexcept {
    return operatorCardToggleRect(std::clamp(selectedOperator_,0,5));
}
NativeEditor::Rect NativeEditor::operatorSliderRect(int index,bool) const noexcept {
    return operatorCardParamRect(std::clamp(selectedOperator_,0,5),index);
}
NativeEditor::Rect NativeEditor::harmonicRect(int index) const noexcept {
    return operatorCardHarmonicRect(std::clamp(selectedOperator_,0,5),index);
}

NativeEditor::Rect NativeEditor::matrixRect(int modulator,int carrier) const noexcept {
    const auto b=bodyRect();
    const auto tabs=synthTabRect(0);
    const float gap=std::clamp(b.w*0.014f,9.0f,13.0f);
    const float labelW=44.0f;
    const float headerH=34.0f;
    const float hintH=34.0f;
    const float top=tabs.y+tabs.h+gap+hintH+headerH;
    const float availableW=
        std::max(1.0f,b.w-gap*2.0f-labelW);
    const float cellW=
        std::max(54.0f,(availableW-gap*5.0f)/6.0f);
    const float availableH=
        std::max(1.0f,b.y+b.h-top-gap);
    const float rowH=std::clamp(
        (availableH-gap*5.0f)/6.0f,
        64.0f,92.0f);
    const float gridW=cellW*6.0f+gap*5.0f;
    const float ox=b.x+(b.w-(gridW+labelW))*0.5f+labelW;
    const float oy=top;
    return {
        ox+carrier*(cellW+gap),
        oy+modulator*(rowH+gap),
        cellW,rowH
    };
}

NativeEditor::Rect NativeEditor::fxWorkspaceTabRect(int index) const noexcept {
    const auto b=bodyRect();
    const float gap=7.0f;
    const float x=b.x+16;
    const float w=(b.w-32-3*gap)/4.0f;
    return {x+index*(w+gap),b.y+12,w,61};
}
NativeEditor::Rect NativeEditor::lfoWorkspaceRow(int row) const noexcept {
    const auto b=bodyRect();
    return {b.x+22,b.y+186.0f+row*91.0f,b.w-44,77};
}
NativeEditor::Rect NativeEditor::lfoWorkspaceTargetRect() const noexcept {
    const auto b=bodyRect();
    return {b.x+b.w*.53f,b.y+105,b.w*.41f,66};
}
NativeEditor::Rect NativeEditor::fxRackRect() const noexcept {
    const auto b=bodyRect();const float gap=std::clamp(b.w*.014f,9.0f,13.0f);
    const Fx rack=ProjectCore::instance().fxRack(false);
    float height=175;
    for(int i=0;i<rack.count;++i){height+=82;if(i==fxRackExpanded_)height+=64.0f*fxParamCount(rack.chain[i].type);}
    return {b.x+gap,b.y+104.0f-fxScrollY_,b.w-2*gap,height};
}
NativeEditor::Rect NativeEditor::fxRackStageRect(int index) const noexcept {
    const auto c=fxRackRect();const Fx rack=ProjectCore::instance().fxRack(false);
    float y=c.y+72;
    for(int i=0;i<index&&i<rack.count;++i){y+=82;if(i==fxRackExpanded_)y+=64.0f*fxParamCount(rack.chain[i].type);}
    return {c.x+18,y,c.w-36,75};
}
NativeEditor::Rect NativeEditor::fxRackParamRect(int index,int param) const noexcept {
    const auto r=fxRackStageRect(index);
    return {r.x+10,r.y+r.h+param*64.0f,r.w-20,58};
}
NativeEditor::Rect NativeEditor::fxRackAddRect(bool add) const noexcept {
    const auto c=fxRackRect();const float w=(c.w-50)*.5f;
    return {c.x+20+(add?w+10:0),c.y+c.h-83,w,61};
}
NativeEditor::Rect NativeEditor::fxGroupRect(int index) const noexcept {
    const auto b=bodyRect();
    const float gap=std::clamp(b.w*0.014f,9.0f,13.0f);
    static constexpr float heights[4]={1130.0f,844.0f,470.0f,650.0f};
    const int clamped=std::clamp(index,0,3);
    float y=fxRackRect().y+fxRackRect().h+gap;
    for(int i=0;i<clamped;++i)y+=heights[i]+gap;

    float height=heights[clamped];
    if(clamped==3){
        const int count=ProjectCore::instance().selectedModSlotCount(linkSource_);
        height=std::max(
            650.0f,
            262.0f+112.0f*static_cast<float>(std::max(0,count)));
    }

    return {
        b.x+gap,y,
        b.w-gap*2.0f,
        height
    };
}

NativeEditor::Rect NativeEditor::fxSectionParamRect(int section,int row) const noexcept {
    const auto card=fxGroupRect(section);
    float y=card.y+72.0f;
    if(section==0)y=card.y+164.0f+row*78.0f;
    else if(section==1)y=card.y+76.0f+row*78.0f;
    else if(section==2){
        const int visualRow=row+(row>=2?1:0);
        y=card.y+76.0f+visualRow*78.0f;
    }
    return {card.x+20.0f,y,card.w-40.0f,68.0f};
}

NativeEditor::Rect NativeEditor::filterTypeRect(int) const noexcept {
    const auto card=fxGroupRect(0);
    return {
        card.x+card.w*0.58f,
        card.y+84.0f,
        card.w*0.36f,
        62.0f
    };
}

NativeEditor::Rect NativeEditor::peakToggleRect(int index) const noexcept {
    const auto card=fxGroupRect(0);
    const float gap=12.0f,buttonW=(card.w-40.0f-gap)*.5f;
    return {card.x+20.0f+(index?buttonW+gap:0.0f),card.y+782.0f,buttonW,62.0f};
}
NativeEditor::Rect NativeEditor::peakParamRect(int row) const noexcept {
    const auto card=fxGroupRect(0);
    return {card.x+20.0f,card.y+858.0f+row*78.0f,card.w-40.0f,68.0f};
}
NativeEditor::Rect NativeEditor::lfoTargetRect(int) const noexcept {
    const auto card=fxGroupRect(2);
    return {
        card.x+card.w*0.58f,
        card.y+84.0f+2.0f*78.0f,
        card.w*0.36f,
        62.0f
    };
}

NativeEditor::Rect NativeEditor::fxSliderRect(int row,int,bool) const noexcept {
    return fxSectionParamRect(std::clamp(fxGroup_,0,2),row);
}

NativeEditor::Rect NativeEditor::linkSourceRect(LinkSource source) const noexcept {
    const auto card=fxGroupRect(3);
    const float w=(card.w-50.0f)*0.5f;
    return {card.x+20.0f+(source==LinkSource::Mod?w+10.0f:0.0f),
        card.y+70.0f,w,56.0f};
}
NativeEditor::Rect NativeEditor::modRowRect(int slot) const noexcept {
    const auto card=fxGroupRect(3);
    return {
        card.x+20.0f,
        card.y+146.0f+slot*112.0f,
        card.w-40.0f,
        96.0f
    };
}

NativeEditor::Rect NativeEditor::modPartRect(int slot,int part) const noexcept {
    const auto r=modRowRect(slot);
    const float gap=8.0f;
    // Keep six logical parts so existing interaction types remain usable.
    // 1=target, 3=min, 4=max, 5=delete are the visible HTML-style controls.
    static constexpr std::array<float,6> weights{
        0.08f,0.34f,0.01f,0.23f,0.23f,0.10f};
    float total=0.0f;
    for(float w:weights)total+=w;
    const float usable=r.w-gap*5.0f;
    float x=r.x;
    for(int i=0;i<part;++i)
        x+=usable*(weights[static_cast<size_t>(i)]/total)+gap;
    const float w=
        usable*(weights[static_cast<size_t>(part)]/total);
    return {x,r.y,w,r.h};
}

NativeEditor::Rect NativeEditor::modAddRect() const noexcept {
    const auto card=fxGroupRect(3);
    return {
        card.x+20.0f,
        card.y+card.h-88.0f,
        std::min(360.0f,card.w-40.0f),
        66.0f
    };
}

NativeEditor::Range NativeEditor::operatorRange(OperatorParam p) noexcept {
    switch(p){
        case OperatorParam::Ratio:return {0.125f,16.0f};
        case OperatorParam::Semitone:return {-12.0f,12.0f};
        case OperatorParam::Detune:return {-50.0f,50.0f};
        case OperatorParam::Level:return {0.0f,1.0f};
        case OperatorParam::Attack:return {0.001f,1.0f};
        case OperatorParam::Decay:return {0.005f,1.5f};
        case OperatorParam::Sustain:return {0.0f,1.0f};
        case OperatorParam::Release:return {0.02f,2.0f};
    }
    return {0.0f,1.0f};
}
NativeEditor::Range NativeEditor::patchRange(PatchParam p) noexcept {
    switch(p){
        case PatchParam::FilterCutoff:return {40.0f,18000.0f};
        case PatchParam::FilterResonance:return {0.1f,18.0f};
        case PatchParam::FilterEnv:return {0.0f,1.0f};
        case PatchParam::FilterAttack:return {0.005f,1.0f};
        case PatchParam::FilterDecay:return {0.01f,1.5f};
        case PatchParam::FilterSustain:return {0.0f,1.0f};
        case PatchParam::FilterRelease:return {0.02f,2.0f};
        case PatchParam::PeakRatio:return {0.5f,8.0f};
        case PatchParam::PeakFrequency:return {40.0f,8000.0f};
        case PatchParam::PeakQ:return {0.3f,12.0f};
        case PatchParam::PeakGain:return {-18.0f,18.0f};
        case PatchParam::AmpAttack:return {0.001f,1.0f};
        case PatchParam::AmpDecay:return {0.01f,1.5f};
        case PatchParam::AmpSustain:return {0.0f,1.0f};
        case PatchParam::AmpRelease:return {0.02f,8.0f};
        case PatchParam::VelocityAmp:
        case PatchParam::VelocityFilter:
        case PatchParam::LfoAmount:
        case PatchParam::LfoVelocity:
        case PatchParam::Unison:
        case PatchParam::Glide:
        case PatchParam::Volume:
        case PatchParam::Delay:
        case PatchParam::Reverb:return {0.0f,1.0f};
        case PatchParam::Distortion:return {0.0f,10.0f};
        case PatchParam::DistIn:
        case PatchParam::DistOut:return {0.0f,4.0f};
        case PatchParam::ReverbDecay:return {0.2f,12.0f};
        case PatchParam::ReverbRoom:return {0.5f,2.5f};
        case PatchParam::LfoRate:return {0.1f,20.0f};
        case PatchParam::LfoAttack:return {0.0f,2.0f};
        case PatchParam::DelayTime:return {0.03f,1.0f};
        case PatchParam::DelayFeedback:return {0.0f,0.85f};
    }
    return {0.0f,1.0f};
}
NativeEditor::Range NativeEditor::modRange(ModTarget t) noexcept {
    switch(t){
        case ModTarget::Cutoff:return {40.0f,18000.0f};
        case ModTarget::Resonance:return {0.1f,18.0f};
        case ModTarget::FilterEnv:return {0.0f,1.0f};
        case ModTarget::AmpAttack:return {0.001f,1.0f};
        case ModTarget::AmpDecay:return {0.01f,1.5f};
        case ModTarget::AmpSustain:return {0.0f,1.0f};
        case ModTarget::AmpRelease:return {0.02f,8.0f};
        case ModTarget::FilterAttack:return {0.005f,1.0f};
        case ModTarget::FilterDecay:return {0.01f,1.5f};
        case ModTarget::FilterSustain:return {0.0f,1.0f};
        case ModTarget::FilterRelease:return {0.02f,2.0f};
        case ModTarget::Op1:case ModTarget::Op2:case ModTarget::Op3:
        case ModTarget::Op4:case ModTarget::Op5:case ModTarget::Op6:
        case ModTarget::Morph1:case ModTarget::Morph2:case ModTarget::Morph3:
        case ModTarget::Morph4:case ModTarget::Morph5:case ModTarget::Morph6:
        case ModTarget::Unison:return {0.0f,1.0f};
        case ModTarget::Fm:return {0.0f,1.5f};
        case ModTarget::LfoAmount:return {0.0f,2.0f};
        case ModTarget::LfoRate:return {0.1f,20.0f};
        case ModTarget::Volume:return {0.0f,2.0f};
        case ModTarget::PeakRatio:return {0.5f,8.0f};
        case ModTarget::PeakFrequency:return {40.0f,8000.0f};
        case ModTarget::PeakQ:return {0.3f,12.0f};
        case ModTarget::PeakGain:return {-18.0f,18.0f};
        case ModTarget::None:return {0.0f,1.0f};
    }
    return {0.0f,1.0f};
}
float NativeEditor::operatorValue(const Patch& p,int op,OperatorParam param) noexcept {
    if(op<0||op>=6)return 0.0f;const auto& o=p.ops[static_cast<size_t>(op)];
    switch(param){
        case OperatorParam::Ratio:return o.ratio;
        case OperatorParam::Semitone:return o.semitoneOffset;
        case OperatorParam::Detune:return o.detuneCents;
        case OperatorParam::Level:return o.level;
        case OperatorParam::Attack:return o.env.attack;
        case OperatorParam::Decay:return o.env.decay;
        case OperatorParam::Sustain:return o.env.sustain;
        case OperatorParam::Release:return o.env.release;
    }
    return 0.0f;
}
float NativeEditor::patchValue(const Patch& p,PatchParam param) noexcept {
    switch(param){
        case PatchParam::FilterCutoff:return p.filter.cutoff;
        case PatchParam::FilterResonance:return p.filter.resonance;
        case PatchParam::FilterEnv:return p.filter.envAmount;
        case PatchParam::FilterAttack:return p.filter.env.attack;
        case PatchParam::FilterDecay:return p.filter.env.decay;
        case PatchParam::FilterSustain:return p.filter.env.sustain;
        case PatchParam::FilterRelease:return p.filter.env.release;
        case PatchParam::PeakRatio:return p.peak.ratio;
        case PatchParam::PeakFrequency:return p.peak.frequency;
        case PatchParam::PeakQ:return p.peak.q;
        case PatchParam::PeakGain:return p.peak.gainDb;
        case PatchParam::AmpAttack:return p.amp.attack;
        case PatchParam::AmpDecay:return p.amp.decay;
        case PatchParam::AmpSustain:return p.amp.sustain;
        case PatchParam::AmpRelease:return p.amp.release;
        case PatchParam::VelocityAmp:return p.velocityAmp;
        case PatchParam::VelocityFilter:return p.velocityFilter;
        case PatchParam::LfoRate:return p.lfo.rate;
        case PatchParam::LfoAmount:return p.lfo.amount;
        case PatchParam::LfoAttack:return p.lfo.attack;
        case PatchParam::LfoVelocity:return p.lfo.velocitySensitivity;
        case PatchParam::Unison:return p.unison;
        case PatchParam::Glide:return p.glide;
        case PatchParam::Volume:return p.volume;
        case PatchParam::Distortion:return p.fx.distortion;
        case PatchParam::DistIn:return p.fx.distIn;
        case PatchParam::DistOut:return p.fx.distOut;
        case PatchParam::ReverbDecay:return p.fx.reverbDecay;
        case PatchParam::ReverbRoom:return p.fx.reverbRoom;
        case PatchParam::Delay:return p.fx.delay;
        case PatchParam::DelayTime:return p.fx.delayTime;
        case PatchParam::DelayFeedback:return p.fx.delayFeedback;
        case PatchParam::Reverb:return p.fx.reverb;
    }
    return 0.0f;
}
float NativeEditor::normalized(float value,Range r) noexcept {
    if(r.hi<=r.lo)return 0.0f;return std::clamp((value-r.lo)/(r.hi-r.lo),0.0f,1.0f);
}
float NativeEditor::denormalized(float value,Range r) noexcept {
    return r.lo+std::clamp(value,0.0f,1.0f)*(r.hi-r.lo);
}
ModTarget NativeEditor::cycleModTarget(ModTarget current,int direction) noexcept {
    constexpr int first=static_cast<int>(ModTarget::Cutoff);
    constexpr int last=static_cast<int>(ModTarget::PeakGain);
    int n=static_cast<int>(current);
    if(n<first||n>last)n=first;
    n+=direction;
    if(n>last)n=first;
    if(n<first)n=last;
    return static_cast<ModTarget>(n);
}

float NativeEditor::editorScrollMax(EditorPage page) const noexcept {
    const auto b=bodyRect();
    if(page==EditorPage::Synth)return 0.0f;

    if(page==EditorPage::Fx&&fxWorkspace_==3){
        const auto rack=fxRackRect();
        return std::max(0.0f,rack.h+135.0f-b.h);
    }
    const auto footer=patchTransferRect(page,1);
    const float scroll=
        page==EditorPage::Synth?synthScrollY_:fxScrollY_;
    const float bottom=footer.y+scroll+footer.h+24.0f;
    return std::max(0.0f,bottom-(b.y+b.h));
}

void NativeEditor::scrollEditor(EditorPage page,float delta) noexcept {
    if(page==EditorPage::Synth){
        synthScrollY_=std::clamp(
            synthScrollY_+delta,
            0.0f,
            editorScrollMax(EditorPage::Synth));
    }else{
        fxScrollY_=std::clamp(
            fxScrollY_+delta,
            0.0f,
            editorScrollMax(EditorPage::Fx));
    }
}

void NativeEditor::fillRect(Rect r,Rgb c) const noexcept {
    if(width_<=0||height_<=0||r.w<=0||r.h<=0)return;
    NativeOverlay::Rect clipped{r.x,r.y,r.w,r.h};
    if(!NativeOverlay::instance().clipRect(clipped))return;
    const int x=std::max(0,static_cast<int>(clipped.x));
    const int top=std::max(0,static_cast<int>(clipped.y));
    const int w=std::max(0,std::min(width_-x,static_cast<int>(clipped.w)));
    const int h=std::max(0,std::min(height_-top,static_cast<int>(clipped.h)));
    if(w<=0||h<=0)return;
    glScissor(x,height_-top-h,w,h);glClearColor(c.r,c.g,c.b,1.0f);glClear(GL_COLOR_BUFFER_BIT);
}
void NativeEditor::drawScrollGutter() const noexcept {
    const auto gutter=scrollGutterRect();
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
            kSpectrumColors[static_cast<size_t>(idx)],
            kSpectrumColors[static_cast<size_t>(idx+1)],
            local);
        const auto color=mix(kBg,spectral,0.18f);
        const float y0=gutter.y+gutter.h*t0;
        const float y1=gutter.y+gutter.h*t1;
        ov.addRect(
            {gutter.x,y0,gutter.w,std::max(1.0f,y1-y0+0.5f)},
            overlayColor(color));
    }
    ov.addRect(
        {gutter.x+gutter.w-2.0f,gutter.y,2.0f,gutter.h},
        overlayColor(mix(kBg,kPanel,0.65f)));
}

void NativeEditor::drawSlider(Rect r,float norm,Rgb accent) const noexcept {
    fillRect(r,kButton);
    const float inset=std::max(2.0f,r.h*0.18f);
    Rect track{r.x+inset,r.y+r.h*0.38f,std::max(0.0f,r.w-inset*2.0f),std::max(3.0f,r.h*0.24f)};
    fillRect(track,kTrack);
    const float n=std::clamp(norm,0.0f,1.0f);
    fillRect({track.x,track.y,track.w*n,track.h},accent);
    const float knob=std::max(5.0f,r.h*0.42f);
    fillRect({track.x+track.w*n-knob*0.5f,r.y+(r.h-knob)*0.5f,knob,knob},mix(kWhite,accent,0.38f));
}
void NativeEditor::drawButton(Rect r,bool active,Rgb accent) const noexcept {
    fillRect(r,active?accent:kButton);
    const float inset=std::max(2.0f,r.h*0.08f);
    fillRect({r.x+inset,r.y+inset,std::max(0.0f,r.w-inset*2.0f),std::max(0.0f,r.h-inset*2.0f)},active?mix(kPanel,accent,0.18f):kPanel);
}
void NativeEditor::drawPadReservedBackground() const noexcept {
    fillRect(viewportRect(),kPanel);
    drawScrollGutter();
}

void NativeEditor::drawPatchTransfer(EditorPage page) const noexcept {
    const auto e=editorRect();
    const auto copy=patchTransferRect(page,1);
    const auto paste=patchTransferRect(page,2);
    auto& ov=NativeOverlay::instance();

    ov.addText(
        "PATCH FILE",
        e.x+12.0f,
        copy.y-34.0f,
        1.02f,
        overlayColor(kWhite));

    drawButton(copy,false,kCyan);
    drawButton(paste,false,kPurple);
    ov.addTextCentered(
        "Export patch",
        {copy.x,copy.y,copy.w,copy.h},
        0.96f,
        overlayColor(kWhite));
    ov.addTextCentered(
        "Import patch",
        {paste.x,paste.y,paste.w,paste.h},
        0.96f,
        overlayColor(kWhite));
}


// Dedicated point-envelope surface. The existing FM matrix and FX editor
// remain independent, so neither is affected by operator editing gestures.
namespace {
struct OpSurface {
    using Rect=NativeEditor::Rect;
    int op{0},page{0},selected{-1},drag{-1},bend{-1},harmonicView{1},modSlot{0};
    bool custom{false};
    unsigned shadows{0};
    float downX{0},downY{0},lastY{0};
    bool moved{false},panning{false},changed{false};
    std::array<std::array<float,4>,6> scroll{};
    std::array<std::array<float,2>,6> harmonicScroll{};
    // Independent volume and pitch timelines for each operator.
    // The scale factors are relative to the original 320 px per second.
    // Default to 8x for fine transient editing.
    std::array<std::array<uint8_t,4>,6> zoom{{
        {{4,4,4,4}},{{4,4,4,4}},{{4,4,4,4}},
        {{4,4,4,4}},{{4,4,4,4}},{{4,4,4,4}}
    }};
    static constexpr std::array<float,6> kTimeZoomFactors{
        0.5f,1.0f,2.0f,4.0f,8.0f,16.0f
    };
    float zoomFactor()const{
        return kTimeZoomFactors[zoom[op][page]];
    }
    float pixelsPerSecond()const{
        return 320.0f*zoomFactor();
    }
    std::string zoomLabel()const{
        const float factor=zoomFactor();
        return factor<1.0f?"0.5x":std::to_string(static_cast<int>(factor))+"x";
    }
    // Zoom in or out one level, keeping the visible top time unchanged.
    // Boundaries do not wrap around to the opposite magnification.
    void adjustTimeZoom(int direction){
        if(page>=4||direction==0)return;
        auto& index=zoom[op][page];
        const int next=std::clamp(static_cast<int>(index)+direction,0,
            static_cast<int>(kTimeZoomFactors.size())-1);
        if(next==index)return;
        const float topSeconds=scroll[op][page]/pixelsPerSecond();
        index=static_cast<uint8_t>(next);
        const float maxScroll=std::max(0.0f,120.0f*pixelsPerSecond()-rect(11).h);
        scroll[op][page]=std::clamp(topSeconds*pixelsPerSecond(),0.0f,maxScroll);
    }
    std::chrono::steady_clock::time_point started{};
    Rect view{};
    Rect rect(int row,int index=0) const {
        const float x=view.x+10,w=view.w-20;
        const float y=view.y+88;
        switch(row){
            case 0: {const float ww=(w-25)/6;return {x+index*(ww+5),y,ww,54};}
            case 1: return {x,y+64,w*.38f,60}; // waveform
            case 2: return {x+w*.38f+6,y+64,w*.23f-6,60}; // ratio
            case 3: return {x+w*.61f+6,y+64,w*.19f-6,60}; // semitone
            case 4: return {x+w*.80f+6,y+64,w*.20f-6,60}; // operator on/off
            case 5: return {x,y+132,(w-8)*.5f,45}; // level
            case 6: return {x+(w-8)*.5f+8,y+132,(w-8)*.5f,45}; // fine cents
            case 7: {const int n=custom?6:5;const float ww=(w-(n-1)*6)/n;return {x+index*(ww+6),y+184,ww,52};}
            case 8: {const float ww=(w-30)/6;return {x+index*(ww+6),y+244,ww,48};}
            case 9: {
                // Bound the graph from its ACTUAL top, not the preceding
                // control row. The old height overran the viewport by 301px
                // and drew Mod endpoints into Android's navigation region.
                const float graphTop=y+301.0f;
                const float footerStart=view.y+view.h-63.0f;
                // Keep the patch-file label and buttons clear of the graph,
                // while preserving as much vertical editing space as fits.
                const float graphBottom=footerStart-72.0f;
                return {x,graphTop,w,std::max(0.0f,graphBottom-graphTop)};
            } // graph ends inside editor, above its fixed patch footer
            case 10:{auto r=rect(9);const float ww=(r.w-22)/4;return {r.x+4+index*(ww+6),r.y+3,ww,43};}
            case 11:{
                auto r=rect(9);
                return {r.x+42,r.y+57,std::max(0.0f,r.w-67),
                    std::max(0.0f,r.h-78)};
            }
        }
        return {};
    }
    float harmonicRowHeight()const{return std::max(245.0f,rect(11).h*.62f);}
    void scrollBy(float dy){
        if(page<4){
            auto& amount=scroll[op][page];
            amount=std::clamp(amount+dy,0.0f,std::max(0.0f,120.0f*pixelsPerSecond()-rect(11).h));
        }else if(page==5){
            auto& amount=harmonicScroll[op][harmonicView==2?1:0];
            amount=std::clamp(amount+dy,0.0f,
                std::max(0.0f,2.0f*harmonicRowHeight()-rect(11).h+10.0f));
        }
    }
    static bool pitchPage(int which){return which==1||which==3;}
    OperatorCurve env(const Operator&o) const {
        if(page==1)return o.pitchCurve.enabled?o.pitchCurve:initialOperatorPitch();
        if(page==2)return o.volumeMorphCurve.enabled?o.volumeMorphCurve:
            (o.volumeCurve.enabled?o.volumeCurve:legacyOperatorVolume(o.env));
        if(page==3)return o.pitchMorphCurve.enabled?o.pitchMorphCurve:
            (o.pitchCurve.enabled?o.pitchCurve:initialOperatorPitch());
        return o.volumeCurve.enabled?o.volumeCurve:legacyOperatorVolume(o.env);
    }
    OperatorModChannel channel(const Operator&o) const {
        if(o.modChannelsConfigured)return o.modChannels[modSlot];
        auto c=initialOperatorModChannel();
        if(modSlot==0&&o.expressionCurve.enabled&&o.expressionCurve.count>=2){
            c.response=o.expressionCurve;
            c.enabled=true;
        }
        return c;
    }
    OperatorResponse response(const Operator&o) const {
        return channel(o).response;
    }
    bool setChannel(OperatorModChannel c){
        return ProjectCore::instance().setSelectedOperatorModChannel(op,modSlot,c);
    }
    std::pair<float,float> position(const OperatorPoint&pt)const{
        auto r=rect(11);
        if(page==4)return {r.x+std::clamp(pt.time,0.0f,1.0f)*r.w,
            r.y+(1.0f-std::clamp(pt.value,0.0f,1.0f))*r.h};
        const float v=pitchPage(page)?(pt.value+12)/24:pt.value;
        return {r.x+std::clamp(v,0.0f,1.0f)*r.w,
            r.y+pt.time*pixelsPerSecond()-scroll[op][page]};
    }
    bool hitRatio(float x,float y) const {return rect(2).contains(x,y);}
    bool hitSemi(float x,float y) const {return rect(3).contains(x,y);}
    bool hitWave(float x,float y) const {return rect(1).contains(x,y);}
    void draw() const {
        const Patch p=ProjectCore::instance().selectedPatch();
        const auto&o=p.ops[op];
        auto&ov=NativeOverlay::instance();
        auto text=[&](std::string_view label,Rect r,float scale=.87f,NativeOverlay::Color color=overlayColor(kWhite)){
            ov.addTextCentered(label,{r.x,r.y,r.w,r.h},scale,color);
        };
        auto button=[&](Rect r,bool active,bool purple=false){
            ov.addRect({r.x,r.y,r.w,r.h},overlayColor(active?(purple?kPurple:kCyan):kButton));
        };
        for(int i=0;i<6;++i){
            auto r=rect(0,i);button(r,i==op);
            text("OP"+std::to_string(i+1),r,.84f,overlayColor(i==op?kBg:kWhite));
        }
        auto field=[&](Rect r,std::string_view label,std::string value){
            ov.addRect({r.x,r.y,r.w,r.h},overlayColor(kButton));
            ov.addText(label,r.x+7,r.y+5,.7f,overlayColor(kMuted));
            ov.addText(value,r.x+7,r.y+26,.91f,overlayColor(kWhite));
        };
        char buff[32];
        field(rect(1),"Wave",kWaveformLabels[std::clamp(static_cast<int>(o.wave),0,6)]);
        std::snprintf(buff,sizeof(buff),"%.4g",static_cast<double>(o.ratio));field(rect(2),"Ratio",buff);
        std::snprintf(buff,sizeof(buff),"%+.0f",static_cast<double>(o.semitoneOffset));field(rect(3),"Base ST",buff);
        button(rect(4),o.enabled);text(o.enabled?"ON":"OFF",rect(4),.85f);
        for(int i=0;i<2;++i){
            auto r=rect(5+i);button(r,false);
            const float v=i?((o.detuneCents+50)/100):o.level;
            ov.addText(i?"Fine":"Level",r.x+7,r.y+9,.8f,overlayColor(kWhite));
            ov.addRect({r.x+r.w*.34f,r.y+24,r.w*.60f,5},overlayColor(kMuted));
            ov.addRect({r.x+r.w*.34f,r.y+24,r.w*.60f*std::clamp(v,0.0f,1.0f),5},overlayColor(kCyan));
        }
        for(int i=0;i<(custom?6:5);++i){
            auto r=rect(7,i);button(r,i==page,i>=2);
            const char* tab=i==0?"Vol":i==1?"Pitch":i==2?"Morph":
                i==3?"P Morph":i==4?"Mod":"Harm";
            text(tab,r,i==3?.77f:.9f,overlayColor(i==page?kBg:kWhite));
        }
        for(int i=0;i<6;++i){
            auto r=rect(8,i);
            if(page==5&&i>1)continue;
            const auto c=channel(o);
            const bool active=page==5?(harmonicView==i+1):
                page==4?(i<4?modSlot==i:(i==4?c.response.source==1:c.enabled)):
                (i!=op&&(shadows&(1u<<i)));
            button(r,active,true);
            const std::string name=page==5?(i?"Muted":"Harmonics"):
                page==4?(i<4?"Mod "+std::to_string(i+1):
                         i==4?(c.response.source?"Src M":"Src V"):
                              (c.enabled?"Enabled":"Disabled")):
                ("OP"+std::to_string(i+1));
            text(name,r,.74f,overlayColor(active?kBg:kWhite));
        }
        auto graph=rect(9),plot=rect(11);
        ov.addRect({graph.x,graph.y,graph.w,graph.h},overlayColor(kRollBg));
        if(page<4){
            const auto c=env(o);
            for(int i=0;i<3;++i){
                const auto r=rect(10,i);
                button(r,i==2&&c.sustain);
                text(i==0?"Decay":i==1?"Release":
                    (c.sustain?"Sustain":"Dynamic"),r,.76f);
            }
            // The old Delete slot is a three-part time zoom control:
            // [−] [current magnification] [+]. Both directions are
            // independently tappable and disabled at their limits.
            const auto r=rect(10,3);
            const float side=r.w*.29f;
            const Rect minus{r.x,r.y,side,r.h};
            const Rect plus{r.x+r.w-side,r.y,side,r.h};
            const Rect value{r.x+side+2.0f,r.y,
                std::max(0.0f,r.w-2.0f*side-4.0f),r.h};
            const int zoomIndex=zoom[op][page];
            button(minus,zoomIndex>0);
            button(plus,zoomIndex+1<static_cast<int>(kTimeZoomFactors.size()));
            button(value,false);
            text("-",minus,1.2f);
            text(zoomLabel(),value,.85f);
            text("+",plus,1.2f);
            ov.addText(pitchPage(page)?"-12          0           +12 ST":"0              Volume            100%",
                plot.x,plot.y-18,.8f,overlayColor(kMuted));
        }else if(page==4){
            for(int i=0;i<4;++i){
                auto r=rect(10,i);
                const bool active=static_cast<int>(channel(o).destination)==i;
                button(r,active,true);
                text(i==0?"Vol":i==1?"Pitch":i==2?"Morph":"P Morph",
                    r,.8f,overlayColor(active?kBg:kWhite));
            }
            ov.addText("Output 100%  /  Input 0 -> 100%",
                plot.x,plot.y-18,.8f,overlayColor(kMuted));
        }else{
            ov.addText(harmonicView==2?"Muted partials - drag bars":"Custom harmonics - drag bars",
                plot.x,plot.y-18,.8f,overlayColor(kMuted));
        }
        ov.setClip({plot.x-11,plot.y-9,plot.w+22,plot.h+18});
        for(int i=0;i<=4;++i){
            float x=plot.x+i*plot.w*.25f;
            ov.addLine(x,plot.y,x,plot.y+plot.h,1,overlayColor(kMuted,.5f));
            if(page==4){
                float y=plot.y+i*plot.h*.25f;
                ov.addLine(plot.x,y,plot.x+plot.w,y,1,overlayColor(kMuted,.5f));
            }
        }
        if(page==5&&o.wave==Wave::Custom){
            const auto& harm=harmonicView==2?o.harmMute:o.harm;
            for(int i=0;i<16;++i){
                const int col=i%8,row=i/8;
                const float w=plot.w/8.0f,h=harmonicRowHeight();
                const float x=plot.x+col*w,
                    y=plot.y+row*h-harmonicScroll[op][harmonicView==2?1:0];
                const float v=std::clamp(harm[static_cast<size_t>(i)],0.0f,1.0f);
                ov.addRect({x+3,y+4,w-6,h-8},overlayColor(kButton));
                ov.addRect({x+w*.26f,y+h-8-(h-38)*v,w*.48f,(h-38)*v},
                    overlayColor(harmonicView==2?kPurple:kCyan));
                ov.addTextCentered(std::to_string(i+1),{x,y+h-34,w,27},.7f,overlayColor(kWhite));
            }
            ov.clearClip();return;
        }
        if(page<4){
            // Adaptive timeline ticks. Zooming reveals millisecond detail,
            // and grid labels always represent exact seconds (no snapping).
            const float scale=pixelsPerSecond();
            constexpr std::array<float,14> steps{
                .001f,.002f,.005f,.01f,.02f,.05f,.1f,.2f,
                .5f,1.0f,2.0f,5.0f,10.0f,20.0f
            };
            float tick=steps.back();
            for(float candidate:steps){
                if(candidate*scale>=78.0f){tick=candidate;break;}
            }
            const float scrollStart=scroll[op][page];
            const int first=std::max(0,static_cast<int>(
                std::floor(scrollStart/(scale*tick))));
            const int last=std::min(static_cast<int>(120.0f/tick),
                static_cast<int>(std::ceil((scrollStart+plot.h)/(scale*tick))));
            for(int i=first;i<=last;++i){
                const float seconds=i*tick;
                const float y=plot.y+seconds*scale-scrollStart;
                if(y<plot.y||y>plot.y+plot.h)continue;
                ov.addLine(plot.x,y,plot.x+plot.w,y,1,overlayColor(kMuted,.3f));
                if(seconds<1.0f){
                    std::snprintf(buff,sizeof(buff),"%.0fms",seconds*1000.0f);
                }else{
                    std::snprintf(buff,sizeof(buff),"%.2gs",static_cast<double>(seconds));
                }
                ov.addText(buff,plot.x+3,y+4,.65f,overlayColor(kMuted));
            }
        }
        auto paint=[&](const OperatorPoint*pts,int count,bool ghost){
            if(count<2)return;
            for(int i=1;i<count;++i)for(int k=1;k<=24;++k){
                float t0=pts[i-1].time+(pts[i].time-pts[i-1].time)*(k-1)/24.0f;
                float t1=pts[i-1].time+(pts[i].time-pts[i-1].time)*k/24.0f;
                OperatorPoint a{t0,operatorCurveValue(pts+i-1,2,t0)};
                OperatorPoint b{t1,operatorCurveValue(pts+i-1,2,t1)};
                auto x=position(a),y=position(b);
                ov.addLine(x.first,x.second,y.first,y.second,ghost?2:3.5f,
                    overlayColor(ghost?kPurple:(page==4?kPurple:pitchPage(page)?kCyan:kGreen),ghost?.32f:1.0f));
            }
        };
        auto paintHandles=[&](const OperatorPoint* pts,int count){
            for(int i=1;i<count;++i){
                const auto&prev=pts[i-1],&cur=pts[i];
                const float middle=prev.time+(cur.time-prev.time)*
                    std::clamp(cur.midpoint,.05f,.95f);
                const OperatorPoint handle{
                    middle,operatorCurveValue(pts+i-1,2,middle,prev.value)};
                const auto xy=position(handle);
                constexpr float d=6.5f;
                ov.addLine(xy.first-d,xy.second-d,xy.first+d,xy.second+d,2.3f,overlayColor(kOrange));
                ov.addLine(xy.first-d,xy.second+d,xy.first+d,xy.second-d,2.3f,overlayColor(kOrange));
            }
        };
        if(page<4){
            for(int i=0;i<6;++i)if(i!=op&&(shadows&(1u<<i))){
                const auto&so=p.ops[i];
                const auto c=env(so);
                paint(c.points.data(),c.count,true);
            }
            auto c=env(o);paint(c.points.data(),c.count,false);
            paintHandles(c.points.data(),c.count);
            for(int i=0;i<c.count;++i){
                auto xy=position(c.points[i]);
                ov.addFilledCircle(xy.first,xy.second,i==selected?9:7,overlayColor(i==selected?kOrange:kWhite));
                // Marker text must remain visible at 0% and 100% volume
                // (and at +/-12 ST). Flip it left of right-edge points.
                // Distinct offsets also distinguish D and S/R if both
                // happen to share one point in an older saved patch.
                const float labelX=std::clamp(
                    xy.first>(plot.x+plot.w-35.0f)?xy.first-23.0f:xy.first+12.0f,
                    plot.x+4.0f,plot.x+plot.w-18.0f);
                const auto labelY=[&](float offset){
                    return std::clamp(xy.second+offset,
                        plot.y+5.0f,plot.y+plot.h-18.0f);
                };
                if(i==c.decayIndex)
                    ov.addText("D",labelX,labelY(8.0f),.82f,overlayColor(kCyan));
                if(i==c.releaseIndex)
                    ov.addText(c.sustain?"S":"R",labelX,labelY(-22.0f),
                        .82f,overlayColor(kOrange));
                if(pitchPage(page)&&c.points[i].free)ov.addCircle(xy.first,xy.second,12,1.5f,overlayColor(kPurple));
            }
        }else if(page==4){
            auto c=response(o);paint(c.points.data(),c.count,false);
            paintHandles(c.points.data(),c.count);
            for(int i=0;i<c.count;++i){
                auto xy=position(c.points[i]);
                ov.addFilledCircle(xy.first,xy.second,i==selected?9:7,overlayColor(i==selected?kOrange:kWhite));
            }
        }
        ov.clearClip();
    }
    bool updatePoint(int idx,float x,float y,bool handle){
        auto& core=ProjectCore::instance();
        const auto o=core.selectedPatch().ops[op];
        auto r=rect(11);
        float xval=std::clamp((x-r.x)/std::max(1.0f,r.w),0.0f,1.0f);
        if(page==4){
            auto channelCopy=channel(o);
            auto& c=channelCopy.response;
            if(idx<0||idx>=c.count)return false;
            auto&pt=c.points[idx];
            if(handle){
                if(idx<1)return false;
                const auto&before=c.points[idx-1];
                pt.midpoint=std::clamp((x-r.x-r.w*before.time)/
                    std::max(.00001f,r.w*(pt.time-before.time)),.05f,.95f);
                pt.handleActive=true;
                pt.handleValue=std::clamp(1-(y-r.y)/r.h,
                    std::min(before.value,pt.value),std::max(before.value,pt.value));
            }else{
                pt.time=std::clamp(xval,idx?c.points[idx-1].time+.0001f:0.0f,
                    idx+1<c.count?c.points[idx+1].time-.0001f:1.0f);
                pt.value=std::clamp(1-(y-r.y)/r.h,0.0f,1.0f);
            }
            c.enabled=true;
            channelCopy.enabled=true;
            return setChannel(channelCopy);
        }
        auto c=env(o);if(idx<0||idx>=c.count)return false;
        auto&pt=c.points[idx];
        float val=pitchPage(page)?-12+24*xval:xval;
        if(handle){
            if(idx<1)return false;
            const auto&before=c.points[idx-1];
            const float time=(y-r.y+scroll[op][page])/pixelsPerSecond();
            pt.midpoint=std::clamp(
                (time-before.time)/std::max(.00001f,pt.time-before.time),
                .05f,.95f);
            pt.handleActive=true;
            pt.handleValue=std::clamp(val,
                std::min(before.value,pt.value),std::max(before.value,pt.value));
        }else{
            float time=std::max(0.0f,(y-r.y+scroll[op][page])/pixelsPerSecond());
            pt.time=idx==0?0:std::clamp(time,c.points[idx-1].time+.0001f,
                idx+1<c.count?c.points[idx+1].time-.0001f:120.0f);
            pt.value=pitchPage(page)&&!pt.free?std::round(val):val;
        }
        return core.setSelectedOperatorCurve(op,page,c);
    }
    bool addPoint(float x,float y){
        auto&core=ProjectCore::instance();
        const auto o=core.selectedPatch().ops[op];
        auto r=rect(11);
        float xval=std::clamp((x-r.x)/std::max(1.0f,r.w),0.0f,1.0f);
        OperatorPoint pt{};
        if(page==4){
            auto channelCopy=channel(o);
            auto& c=channelCopy.response;
            if(c.count>=kMaxOperatorPoints)return false;
            pt.time=std::clamp(xval,.0001f,.9999f);
            pt.value=std::clamp(1-(y-r.y)/r.h,0.0f,1.0f);
            int n=0;while(n<c.count&&c.points[n].time<pt.time)++n;
            for(int k=c.count;k>n;--k)c.points[k]=c.points[k-1];
            c.points[n]=pt;++c.count;selected=n;c.enabled=true;
            channelCopy.enabled=true;
            return setChannel(channelCopy);
        }
        auto c=env(o);if(c.count>=kMaxOperatorPoints)return false;
        pt.time=std::clamp((y-r.y+scroll[op][page])/pixelsPerSecond(),.0001f,119.999f);
        pt.value=pitchPage(page)?std::round(-12+24*xval):xval;
        int n=0;while(n<c.count&&c.points[n].time<pt.time)++n;
        // Extending the timeline beyond the last point keeps its silent
        // terminal endpoint, moving it past the new point instead of
        // preventing longer percussion or evolving envelopes.
        if(n==c.count&&c.count<kMaxOperatorPoints){
            c.points[c.count]=c.points[c.count-1];
            c.points[c.count].time=std::min(120.0f,pt.time+.25f);
            c.points[c.count-1]=pt;
            selected=c.count-1;++c.count;
            return core.setSelectedOperatorCurve(op,page,c);
        }
        if(n==0||n==c.count||pt.time-c.points[n-1].time<.0001f||c.points[n].time-pt.time<.0001f)return false;
        for(int k=c.count;k>n;--k)c.points[k]=c.points[k-1];
        c.points[n]=pt;++c.count;if(c.decayIndex>=n)++c.decayIndex;
        if(c.releaseIndex>=n)++c.releaseIndex;
        selected=n;return core.setSelectedOperatorCurve(op,page,c);
    }
    bool down(float x,float y){
        downX=x;downY=lastY=y;started=std::chrono::steady_clock::now();
        moved=panning=changed=false;drag=-1;bend=-1;
        for(int i=0;i<6;++i)if(rect(0,i).contains(x,y)){op=i;selected=-1;return true;}
        auto&core=ProjectCore::instance();
        const auto o=core.selectedPatch().ops[op];
        custom=o.wave==Wave::Custom;
        if(!custom&&page==5)page=4;
        if(rect(4).contains(x,y)){changed=core.setSelectedOperatorEnabled(op,!o.enabled);return true;}
        for(int i=0;i<2;++i)if(rect(5+i).contains(x,y)){drag=-10-i;return move(x,y);}
        for(int i=0;i<(custom?6:5);++i)if(rect(7,i).contains(x,y)){
            page=i;selected=-1;
            if(page==5)harmonicView=1;
            return true;
        }
        for(int i=0;i<6;++i)if(rect(8,i).contains(x,y)){
            if(page==5){
                if(i<2)harmonicView=i+1;
            }else if(page==4){
                if(i<4){modSlot=i;selected=-1;}
                else{
                    auto c=channel(o);
                    if(i==4)c.response.source=static_cast<uint8_t>(c.response.source?0:1);
                    else c.enabled=!c.enabled;
                    changed=setChannel(c);
                }
            }else if(i!=op)shadows^=1u<<i;
            return true;
        }
        if(page==4)for(int i=0;i<4;++i)if(rect(10,i).contains(x,y)){
            auto c=channel(o);
            c.destination=static_cast<OperatorModDestination>(i);
            c.enabled=true;
            changed=setChannel(c);
            return true;
        }
        if(page<4)for(int i=0;i<4;++i)if(rect(10,i).contains(x,y)){
            // Two dedicated zoom targets; tapping the central readout
            // leaves the time scale unchanged. Point deletion remains
            // the quick-tap gesture used by the piano-roll curves.
            if(i==3){
                const auto r=rect(10,3);
                const float side=r.w*.29f;
                if(x<r.x+side)adjustTimeZoom(-1);
                else if(x>=r.x+r.w-side)adjustTimeZoom(1);
                return true;
            }
            auto c=env(o);
            if(i==2)c.sustain=!c.sustain;
            else if(selected>=0&&selected<c.count-1){
                if(i==0)c.decayIndex=selected;
                else c.releaseIndex=selected;
            }else return true;
            changed=core.setSelectedOperatorCurve(op,page,c);return true;
        }
        if(page==5&&o.wave==Wave::Custom){
            if(rect(11).contains(x,y)){
                auto plot=rect(11);
                const int col=std::clamp(static_cast<int>((x-plot.x)/plot.w*8),0,7);
                const float relativeY=y-plot.y+harmonicScroll[op][harmonicView==2?1:0];
                const int row=std::clamp(static_cast<int>(relativeY/harmonicRowHeight()),0,1);
                drag=-50-(row*8+col);return move(x,y);
            }
        }
        if(!rect(11).contains(x,y))return false;
        const auto c=env(o);
        const auto e=response(o);
        const auto*pts=page==4?e.points.data():c.points.data();
        const int count=page==4?e.count:c.count;
        // Every visible orange X is independently draggable without
        // selecting an endpoint first. Pick the closest eligible target
        // rather than letting a 29 px node hitbox mask its curve handle.
        float pointDistance=25.0f*25.0f;
        int pointCandidate=-1;
        for(int i=0;i<count;++i){
            const auto at=position(pts[i]);
            const float dx=x-at.first,dy=y-at.second;
            const float d2=dx*dx+dy*dy;
            if(d2<pointDistance){pointDistance=d2;pointCandidate=i;}
        }
        float handleDistance=22.0f*22.0f;
        int handleCandidate=-1;
        for(int i=1;i<count;++i){
            const float t=pts[i-1].time+(pts[i].time-pts[i-1].time)*
                std::clamp(pts[i].midpoint,.05f,.95f);
            const auto at=position({t,operatorCurveValue(pts+i-1,2,t)});
            const float dx=x-at.first,dy=y-at.second;
            const float d2=dx*dx+dy*dy;
            if(d2<handleDistance){handleDistance=d2;handleCandidate=i;}
        }
        if(handleCandidate>=0&&
           (pointCandidate<0||handleDistance<=pointDistance)){
            bend=handleCandidate;
            return true;
        }
        if(pointCandidate>=0){selected=drag=pointCandidate;return true;}
        drag=-2;return true;
    }
    bool move(float x,float y){
        if(drag==-2){
            // Graph gestures edit points and X handles only. Vertical
            // scrolling belongs exclusively to the colored side gutter.
            if(std::fabs(x-downX)+std::fabs(y-downY)>13)moved=true;
            return true;
        }
        if(drag<=-50){
            const int i=-drag-50,row=i/8;auto r=rect(11);
            const float h=harmonicRowHeight();
            const float top=r.y+row*h-harmonicScroll[op][harmonicView==2?1:0];
            const float amount=std::clamp(1-(y-top)/h,0.0f,1.0f);
            changed=ProjectCore::instance().setSelectedHarmonic(op,i,amount,harmonicView==2)||changed;
            return true;
        }
        if(drag<=-10){
            auto r=rect(drag==-10?5:6);
            float v=std::clamp((x-r.x-r.w*.34f)/(r.w*.60f),0.0f,1.0f);
            changed=ProjectCore::instance().setSelectedOperatorParam(op,drag==-10?OperatorParam::Level:OperatorParam::Detune,
                drag==-10?v:std::round(v*100-50))||changed;
            return true;
        }
        if(drag>=0||bend>=0){
            if(std::fabs(x-downX)+std::fabs(y-downY)>9){
                moved=true;changed=updatePoint(bend>=0?bend:drag,x,y,bend>=0)||changed;
            }
        }
        return true;
    }
    bool up(){
        if(drag==-2&&!moved)changed=addPoint(downX,downY)||changed;
        // A tap on an X inserts a point; dragging its X changes the curve.
        if(bend>=1&&!moved)changed=addPoint(downX,downY)||changed;
        if(drag>=0&&!moved){
            auto&core=ProjectCore::instance();
            const auto o=core.selectedPatch().ops[op];
            const bool longPress=std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now()-started).count()>=450;
            if(pitchPage(page)&&longPress){
                auto c=env(o);
                if(drag<c.count){
                    c.points[drag].free=!c.points[drag].free;
                    if(!c.points[drag].free)c.points[drag].value=std::round(c.points[drag].value);
                    changed=core.setSelectedOperatorCurve(op,page,c)||changed;
                }
            }else if(!longPress&&page==4){
                auto channelCopy=channel(o);
                auto& c=channelCopy.response;
                if(drag>0&&drag<c.count-1){
                    for(int i=drag;i<c.count-1;++i)c.points[i]=c.points[i+1];
                    --c.count;selected=-1;
                    changed=setChannel(channelCopy)||changed;
                }
            }else if(!longPress&&page<4){
                auto c=env(o);
                if(drag>0&&drag<c.count-1){
                    for(int i=drag;i<c.count-1;++i)c.points[i]=c.points[i+1];
                    --c.count;
                    if(c.decayIndex>=drag&&c.decayIndex>0)--c.decayIndex;
                    if(c.releaseIndex>=drag&&c.releaseIndex>0)--c.releaseIndex;
                    c.decayIndex=std::min<int>(c.decayIndex,c.count-2);
                    c.releaseIndex=std::min<int>(c.releaseIndex,c.count-2);
                    selected=-1;
                    changed=core.setSelectedOperatorCurve(op,page,c)||changed;
                }
            }
        }
        drag=bend=-1;
        const bool result=changed;changed=false;return result;
    }
};
OpSurface opSurface;

struct FilterSurface {
    using Rect=NativeEditor::Rect;
    Rect view{};
    bool open{false},moved{false},changed{false};
    int page{0},lane{0},slot{0},selected{-1},drag{-1},bend{-1},transition{-1};
    float downX{},downY{},scroll{0};
    int zoom{2};
    std::chrono::steady_clock::time_point started{};
    static constexpr std::array<float,6> zooms{{.5f,1,2,4,8,16}};
    static const char* laneName(int lane){
        return lane%3==0?"Frequency":lane%3==1?"Amount":"Q / Slope";
    }
    static const char* shapeName(FilterShape shape){
        return shape==FilterShape::LowShelf?"L":shape==FilterShape::Bell?"B":"H";
    }
    float rate()const{return 320.0f*zooms[zoom];}
    Rect bar(int row,int i,int n)const{
        const float x=view.x+13,w=view.w-26;
        const float gap=6,cell=(w-gap*(n-1))/n;
        const float y=view.y+(row==0?89:row==1?153:row==2?216:row==3?279:342);
        return {x+i*(cell+gap),y,cell,row==0?54.0f:50.0f};
    }
    Rect graph()const{
        const float y=view.y+405;
        return {view.x+70,y,view.w-88,
            std::max(70.0f,view.y+view.h-82-y)};
    }
    Rect pane(int i)const{
        Rect r=graph();
        const float w=(r.w-12)/3;
        return {r.x+i*(w+6),r.y,w,r.h};
    }
    static bool staticLine(const OperatorCurve& c){
        return c.count==2&&c.releaseIndex==0&&
            c.points[1].time>=119.0f&&
            std::fabs(c.points[0].value-c.points[1].value)<.00001f;
    }
    float outputX(int lane,float value)const{
        const auto r=pane(lane);
        return r.x+std::clamp(value,0.0f,1.0f)*r.w;
    }
    std::pair<float,float> location(const OperatorPoint& p,int which)const{
        if(page==2){
            const auto r=graph();
            return {r.x+std::clamp(p.time,0.0f,1.0f)*r.w,
                r.y+(1-std::clamp(p.value,0.0f,1.0f))*r.h};
        }
        const auto r=pane(which%3);
        return {r.x+std::clamp(p.value,0.0f,1.0f)*r.w,
            r.y+p.time*rate()-scroll};
    }
    bool qCurve()const{return page==2?
        getModel().mods[slot].destination==2:lane==2;}
    EvolvingFilter getModel()const{
        const auto m=ProjectCore::instance().selectedPatch().filter.evolving;
        return m.configured?m:initialEvolvingFilter();
    }
    int laneIndex()const{return lane+(page==1?3:0);}
    OperatorCurve getCurve(const EvolvingFilter& m)const{
        return page==2?modCurve(m.mods[slot]):m.lanes[laneIndex()].curve;
    }
    static OperatorCurve modCurve(const FilterModChannel& mod){
        OperatorCurve c{};
        c.enabled=mod.response.enabled;c.count=mod.response.count;
        c.points=mod.response.points;
        return c;
    }
    FilterShape currentShape(const EvolvingFilter& m,int n)const{
        return page==2?m.mods[slot].shape[n]:m.lanes[laneIndex()].shape[n];
    }
    float currentTransition(const EvolvingFilter& m,int n)const{
        return page==2?m.mods[slot].transition[n]:m.lanes[laneIndex()].transition[n];
    }
    void setShape(EvolvingFilter& m,int n,FilterShape shape)const{
        if(page==2)m.mods[slot].shape[n]=shape;
        else m.lanes[laneIndex()].shape[n]=shape;
    }
    void setTransition(EvolvingFilter& m,int n,float position)const{
        if(page==2)m.mods[slot].transition[n]=position;
        else m.lanes[laneIndex()].transition[n]=position;
    }
    bool save(EvolvingFilter& m,const OperatorCurve& curve){
        if(page==2){
            auto& response=m.mods[slot].response;
            response.enabled=true;
            response.count=curve.count;response.points=curve.points;
        }else{
            m.lanes[laneIndex()].curve=curve;
        }
        m.configured=true;
        return ProjectCore::instance().setSelectedEvolvingFilter(m);
    }
    void scrollBy(float delta){
        if(page==2)return;
        scroll=std::clamp(scroll+delta,0.0f,
            std::max(0.0f,120.0f*rate()-graph().h));
    }
    void draw()const{
        auto& o=NativeOverlay::instance();
        o.addRect({view.x,view.y,view.w,view.h},overlayColor(kBg));
        const auto model=getModel();
        auto cell=[&](Rect r,const std::string& label,bool active,
                      NativeEditor::Rgb color=kCyan,float size=.75f){
            o.addRect({r.x,r.y,r.w,r.h},overlayColor(active?color:kButton));
            o.addTextCentered(label,{r.x,r.y,r.w,r.h},size,
                overlayColor(active?kBg:kWhite));
        };
        cell(bar(0,0,2),"Frequency / Amount / Q",true,kCyan,.78f);
        cell(bar(0,1,2),model.configured?"Filter active":"Enable filter",model.configured,kGreen,.86f);
        for(int i=0;i<3;++i)cell(bar(1,i,3),
            i==0?"Envelope":i==1?"Morph":"Mod",page==i,kPurple,.88f);
        if(page<2){
            o.addText("All three lines editable - tap a column to edit its points",
                view.x+20,bar(2,0,3).y+15,.76f,overlayColor(kMuted));
            const auto c=getCurve(model);
            cell(bar(3,0,6),"D",selected==c.decayIndex,kCyan,1);
            cell(bar(3,1,6),"R",selected==c.releaseIndex,kOrange,1);
            cell(bar(3,2,6),c.sustain?"S hold":"Dynamic",c.sustain,kGreen,.73f);
            cell(bar(3,3,6),"-",false,kCyan,1.3f);
            cell(bar(3,4,6),zooms[zoom]<1?"0.5x":std::to_string(static_cast<int>(zooms[zoom]))+"x",false,kCyan,.77f);
            cell(bar(3,5,6),"+",false,kCyan,1.3f);
        }else{
            for(int i=0;i<4;++i)cell(bar(2,i,6),"Mod "+std::to_string(i+1),slot==i,kPurple,.8f);
            const auto& m=model.mods[slot];
            cell(bar(2,4,6),m.response.source?"Src M":"Src V",m.response.source==1,kCyan,.7f);
            cell(bar(2,5,6),m.enabled?"On":"Off",m.enabled,kGreen,.8f);
            static const char* dests[6]={"Freq","Amount","Q","F Morph","A Morph","Q Morph"};
            for(int i=0;i<6;++i)cell(bar(3,i,6),dests[i],i==m.destination,kPurple,.73f);
        }
        if(page<2){
            const auto r=graph();
            o.addRect({r.x-5,r.y-6,r.w+10,r.h+12},overlayColor(kRollBg));
            for(int i=0;i<3;++i){
                auto paneRect=pane(i);
                const auto col=i==0?kCyan:i==1?kPurple:kOrange;
                const auto& f=model.lanes[i+(page==1?3:0)];
                const auto& curve=f.curve;
                o.addRect({paneRect.x,paneRect.y,paneRect.w,paneRect.h},
                    overlayColor(kButton,.18f));
                o.addText(i==0?"40Hz - 18k":i==1?"-100  /  +100":"Q 0-100",
                    paneRect.x,paneRect.y-20,.69f,overlayColor(col));
                for(int k=0;k<=4;++k){
                    const float x=paneRect.x+k*paneRect.w*.25f;
                    o.addLine(x,paneRect.y,x,paneRect.y+paneRect.h,1,
                        overlayColor(kMuted,.30f));
                }
                o.setClip({paneRect.x,paneRect.y,paneRect.w,paneRect.h});
                if(staticLine(curve)){
                    const float line=outputX(i,curve.points[0].value);
                    o.addLine(line,paneRect.y,line,paneRect.y+paneRect.h,3,
                        overlayColor(col));
                }
                for(int j=1;j<curve.count&&!staticLine(curve);++j){
                    const auto& a=curve.points[j-1];const auto& b=curve.points[j];
                    for(int k=1;k<=22;++k){
                        const float t0=a.time+(b.time-a.time)*(k-1)/22;
                        const float t1=a.time+(b.time-a.time)*k/22;
                        auto x=location({t0,operatorCurveValue(curve.points.data()+j-1,2,t0)},i);
                        auto y=location({t1,operatorCurveValue(curve.points.data()+j-1,2,t1)},i);
                        o.addLine(x.first,x.second,y.first,y.second,3,
                            overlayColor(col));
                    }
                }
                o.clearClip();
                for(int j=1;j<curve.count;++j){
                    if(staticLine(curve))break;
                    const float time=curve.points[j-1].time+
                        (curve.points[j].time-curve.points[j-1].time)*
                        std::clamp(curve.points[j].midpoint,.05f,.95f);
                    auto x=location({time,operatorCurveValue(curve.points.data()+j-1,2,time)},i);
                    if(x.second>=paneRect.y&&x.second<=paneRect.y+paneRect.h){
                        o.addLine(x.first-6,x.second-6,x.first+6,x.second+6,2,
                            overlayColor(kOrange));
                        o.addLine(x.first-6,x.second+6,x.first+6,x.second-6,2,
                            overlayColor(kOrange));
                    }
                    if(i==2&&f.shape[j-1]!=f.shape[j]){
                        const float t=curve.points[j-1].time+
                            (curve.points[j].time-curve.points[j-1].time)*f.transition[j];
                        auto x=location({t,operatorCurveValue(curve.points.data()+j-1,2,t)},i);
                        if(x.second>=paneRect.y&&x.second<=paneRect.y+paneRect.h)
                            o.addText("T",x.first-5,x.second-14,.8f,overlayColor(kPurple));
                    }
                }
                for(int j=0;j<curve.count;++j){
                    if(staticLine(curve))break;
                    auto x=location(curve.points[j],i);
                    if(x.second<paneRect.y||x.second>paneRect.y+paneRect.h)continue;
                    o.addFilledCircle(x.first,x.second,(i==lane&&j==selected)?8:6,
                        overlayColor(i==lane&&j==selected?kWhite:col));
                    if(i==2)o.addText(shapeName(f.shape[j]),x.first+9,x.second-12,
                        .78f,overlayColor(kOrange));
                    if(i==lane&&j==curve.decayIndex)
                        o.addText("D",x.first+8,x.second+8,.8f,overlayColor(kCyan));
                    if(i==lane&&j==curve.releaseIndex)
                        o.addText(curve.sustain?"S":"R",x.first+8,x.second-20,
                            .8f,overlayColor(kOrange));
                }
            }
            // Visible time labels use the same zoom / scrolling as operators.
            static constexpr std::array<float,14> tickSizes{
                .001f,.002f,.005f,.01f,.02f,.05f,.1f,.2f,
                .5f,1,2,5,10,20};
            float step=tickSizes.back();
            for(float t:tickSizes)if(t*rate()>=60.0f){step=t;break;}
            const int first=std::max(0,static_cast<int>(std::floor(
                scroll/(rate()*step))));
            const int last=std::min(static_cast<int>(120.0f/step),
                static_cast<int>(std::ceil((scroll+r.h)/(rate()*step))));
            for(int k=first;k<=last;++k){
                const float t=k*step,y=r.y+t*rate()-scroll;
                if(y<r.y||y>r.y+r.h)continue;
                o.addLine(r.x,y,r.x+r.w,y,1,overlayColor(kMuted,.22f));
                char label[30];std::snprintf(label,sizeof(label),
                    t<1.0f?"%.3fs":t<10.0f?"%.2fs":"%.1fs",
                    static_cast<double>(t));
                o.addText(label,r.x-60,y-9,.76f,overlayColor(kWhite));
            }
        }else{
            auto r=graph();const auto& m=model.mods[slot];
            const auto& c=m.response;
            o.addRect({r.x-5,r.y-6,r.w+10,r.h+12},overlayColor(kRollBg));
            o.addText("V / M input: 0 to 100%",r.x,r.y-20,.77f,overlayColor(kMuted));
            for(int k=0;k<=4;++k){
                const float x=r.x+r.w*k*.25f,y=r.y+r.h*k*.25f;
                o.addLine(x,r.y,x,r.y+r.h,1,overlayColor(kMuted,.35f));
                o.addLine(r.x,y,r.x+r.w,y,1,overlayColor(kMuted,.35f));
            }
            for(int j=1;j<c.count;++j){
                for(int k=1;k<=30;++k){
                    const float t0=c.points[j-1].time+
                        (c.points[j].time-c.points[j-1].time)*(k-1)/30;
                    const float t1=c.points[j-1].time+
                        (c.points[j].time-c.points[j-1].time)*k/30;
                    auto a=location({t0,operatorCurveValue(c.points.data()+j-1,2,t0)},0);
                    auto b=location({t1,operatorCurveValue(c.points.data()+j-1,2,t1)},0);
                    o.addLine(a.first,a.second,b.first,b.second,3,overlayColor(kPurple));
                }
                const float t=c.points[j-1].time+
                    (c.points[j].time-c.points[j-1].time)*c.points[j].midpoint;
                const auto a=location({t,operatorCurveValue(c.points.data()+j-1,2,t)},0);
                o.addLine(a.first-6,a.second-6,a.first+6,a.second+6,2,overlayColor(kOrange));
                o.addLine(a.first-6,a.second+6,a.first+6,a.second-6,2,overlayColor(kOrange));
                if(m.destination==2&&m.shape[j-1]!=m.shape[j]){
                    const float mid=c.points[j-1].time+
                        (c.points[j].time-c.points[j-1].time)*m.transition[j];
                    const auto xy=location({mid,operatorCurveValue(c.points.data()+j-1,2,mid)},0);
                    o.addText("T",xy.first-5,xy.second-15,.8f,overlayColor(kPurple));
                }
            }
            for(int j=0;j<c.count;++j){
                auto xy=location(c.points[j],0);
                o.addFilledCircle(xy.first,xy.second,j==selected?9:6,
                    overlayColor(j==selected?kOrange:kWhite));
                if(m.destination==2)o.addText(shapeName(m.shape[j]),
                    xy.first+10,xy.second-12,.8f,overlayColor(kOrange));
            }
        }
        if(qCurve()){
            const auto modelNow=getModel();
            const auto c=getCurve(modelNow);
            const int point=std::clamp(selected,0,static_cast<int>(c.count)-1);
            for(int i=0;i<3;++i){
                const auto sh=static_cast<FilterShape>(i);
                cell(bar(4,i,3),i==0?"Low shelf":i==1?"Bell band":"High shelf",
                    currentShape(modelNow,point)==sh,kOrange,.83f);
            }
        }else{
            o.addText("Tap to select / add point   Drag orange X to bend   Hold node to delete",
                view.x+20,view.y+view.h-60,.72f,overlayColor(kMuted));
        }
    }
    bool update(float x,float y){
        auto model=getModel();auto curve=getCurve(model);
        const Rect r=page==2?graph():pane(lane);
        if(page<2&&drag==-3&&staticLine(curve)){
            const auto r=pane(lane);
            const float value=std::clamp((x-r.x)/std::max(1.0f,r.w),0.0f,1.0f);
            curve.points[0].value=curve.points[1].value=value;
            return save(model,curve);
        }
        const int idx=transition>=0?transition:(bend>=0?bend:drag);
        if(idx<0||idx>=curve.count)return false;
        if(transition>=1){
            const float axis=page==2?
                std::clamp((x-r.x)/std::max(1.0f,r.w),0.0f,1.0f):
                std::clamp((y-r.y+scroll)/rate(),0.0f,120.0f);
            const float a=curve.points[transition-1].time;
            const float b=curve.points[transition].time;
            setTransition(model,transition,
                std::clamp((axis-a)/std::max(.000001f,b-a),0.0f,.95f));
            return save(model,curve);
        }
        auto& p=curve.points[idx];
        const float coord=page==2?
            std::clamp((x-r.x)/std::max(1.0f,r.w),0.0f,1.0f):
            std::clamp((y-r.y+scroll)/rate(),0.0f,120.0f);
        const float v=page==2?
            std::clamp(1.0f-(y-r.y)/std::max(1.0f,r.h),0.0f,1.0f):
            std::clamp((x-r.x)/std::max(1.0f,r.w),0.0f,1.0f);
        if(bend>=1){
            const auto& a=curve.points[idx-1];
            p.midpoint=std::clamp((coord-a.time)/
                std::max(.000001f,p.time-a.time),.05f,.95f);
            p.handleValue=std::clamp(v,std::min(a.value,p.value),
                std::max(a.value,p.value));
            p.handleActive=true;
        }else if(drag>=0){
            const float min=idx>0?curve.points[idx-1].time+.0001f:0.0f;
            const float max=idx+1<curve.count?
                curve.points[idx+1].time-.0001f:(page==2?1.0f:120.0f);
            p.time=std::clamp(coord,min,std::max(min,max));
            p.value=v;
        }
        return save(model,curve);
    }
    bool changeShape(int which){
        auto model=getModel();const auto curve=getCurve(model);
        if(!qCurve()||curve.count==0)return false;
        const int index=std::clamp(selected,0,static_cast<int>(curve.count)-1);
        setShape(model,index,static_cast<FilterShape>(which));
        if(page!=2&&staticLine(curve)){
            setShape(model,0,static_cast<FilterShape>(which));
            setShape(model,1,static_cast<FilterShape>(which));
        }
        return save(model,curve);
    }
    bool addPoint(float x,float y){
        auto m=getModel();auto c=getCurve(m);
        if(c.count>=kMaxOperatorPoints)return false;
        const auto r=page==2?graph():pane(lane);
        if(page!=2&&staticLine(c)){
            const float t=std::clamp((y-r.y+scroll)/rate(),.005f,119.65f);
            const float value=std::clamp((x-r.x)/std::max(1.0f,r.w),0.0f,1.0f);
            const float original=c.points[0].value;
            // First tap off the static line creates the starting, selected
            // and finishing nodes. Release is now an editable timed stage.
            c.count=3;c.sustain=false;c.decayIndex=1;c.releaseIndex=1;
            c.points[0]={0.0f,original};
            c.points[1]={t,value};
            c.points[2]={std::min(120.0f,t+.35f),original};
            setShape(m,1,currentShape(m,0));
            setShape(m,2,currentShape(m,0));
            selected=1;return save(m,c);
        }
        OperatorPoint p;
        p.time=page==2?
            std::clamp((x-r.x)/std::max(1.0f,r.w),.0001f,.9999f):
            std::clamp((y-r.y+scroll)/rate(),.0001f,119.999f);
        p.value=page==2?
            std::clamp(1-(y-r.y)/std::max(1.0f,r.h),0.0f,1.0f):
            std::clamp((x-r.x)/std::max(1.0f,r.w),0.0f,1.0f);
        int n=0;while(n<c.count&&c.points[n].time<p.time)++n;
        if(n==0||n==c.count||c.points[n].time-p.time<.0001f||
            p.time-c.points[n-1].time<.0001f)return false;
        for(int k=c.count;k>n;--k){
            c.points[k]=c.points[k-1];
            setShape(m,k,currentShape(m,k-1));
            setTransition(m,k,currentTransition(m,k-1));
        }
        c.points[n]=p;setShape(m,n,currentShape(m,n-1));setTransition(m,n,.65f);
        ++c.count;
        if(page!=2){
            if(c.decayIndex>=n)++c.decayIndex;
            if(c.releaseIndex>=n)++c.releaseIndex;
        }
        selected=n;
        return save(m,c);
    }
    bool down(float x,float y){
        moved=changed=false;drag=bend=transition=-1;
        downX=x;downY=y;started=std::chrono::steady_clock::now();
        if(bar(0,0,2).contains(x,y))return true;
        if(bar(0,1,2).contains(x,y)){
            auto m=getModel();m.configured=!ProjectCore::instance().selectedPatch().filter.evolving.configured;
            changed=ProjectCore::instance().setSelectedEvolvingFilter(m);
            return true;
        }
        for(int i=0;i<3;++i)if(bar(1,i,3).contains(x,y)){
            page=i;selected=-1;return true;
        }
        if(page<2){
            // Each visible lane selects itself by touch; there is no
            // separate frequency / amount / Q focus switch.
            for(int i=0;i<6;++i)if(bar(3,i,6).contains(x,y)){
                auto m=getModel();auto c=getCurve(m);
                if(i==0&&selected>=0&&selected<c.count-1)c.decayIndex=selected;
                if(i==1&&selected>=0&&selected<c.count-1)c.releaseIndex=selected;
                if(i==2)c.sustain=!c.sustain;
                if(i==3||i==5){
                    const float top=scroll/rate();
                    zoom=std::clamp(zoom+(i==3?-1:1),0,5);
                    scroll=std::clamp(top*rate(),0.0f,
                        std::max(0.0f,120.0f*rate()-graph().h));
                }else changed=save(m,c);
                return true;
            }
        }else{
            for(int i=0;i<6;++i)if(bar(2,i,6).contains(x,y)){
                if(i<4){slot=i;selected=-1;}
                else{
                    auto m=getModel();auto& c=m.mods[slot];
                    if(i==4)c.response.source=static_cast<uint8_t>(!c.response.source);
                    else c.enabled=!c.enabled;
                    changed=ProjectCore::instance().setSelectedEvolvingFilter(m);
                }
                return true;
            }
            for(int i=0;i<6;++i)if(bar(3,i,6).contains(x,y)){
                auto m=getModel();
                m.mods[slot].destination=static_cast<uint8_t>(i);
                m.mods[slot].enabled=true;
                selected=-1;
                changed=ProjectCore::instance().setSelectedEvolvingFilter(m);
                return true;
            }
        }
        if(qCurve())for(int i=0;i<3;++i)if(bar(4,i,3).contains(x,y)){
            changed=changeShape(i);return true;
        }
        auto m=getModel();auto c=getCurve(m);
        if(page<2){
            const auto gr=graph();
            for(int i=0;i<3;++i)if(pane(i).contains(x,y)){
                if(lane!=i){lane=i;selected=-1;}
                break;
            }
        }
        c=getCurve(m);
        const Rect r=page==2?graph():pane(lane);
        if(!r.contains(x,y))return true;
        if(page<2&&staticLine(c)){
            const float line=outputX(lane,c.points[0].value);
            if(std::fabs(x-line)<22.0f){
                selected=0;drag=-3;return true;
            }
            drag=-2;return true;
        }
        float pointDistance=24*24;int nearest=-1;
        for(int i=0;i<c.count;++i){
            const auto xy=location(c.points[i],lane);
            const float d=(x-xy.first)*(x-xy.first)+(y-xy.second)*(y-xy.second);
            if(d<pointDistance){pointDistance=d;nearest=i;}
        }
        float bendDistance=20*20;int nearBend=-1;
        float transitionDistance=17*17;int nearTransition=-1;
        for(int i=1;i<c.count;++i){
            const auto& a=c.points[i-1];const auto& b=c.points[i];
            const float mid=a.time+(b.time-a.time)*b.midpoint;
            const auto pt=location({mid,operatorCurveValue(c.points.data()+i-1,2,mid)},lane);
            const float d=(x-pt.first)*(x-pt.first)+(y-pt.second)*(y-pt.second);
            if(d<bendDistance){bendDistance=d;nearBend=i;}
            if(qCurve()&&currentShape(m,i-1)!=currentShape(m,i)){
                const float t=a.time+(b.time-a.time)*currentTransition(m,i);
                const auto q=location({t,operatorCurveValue(c.points.data()+i-1,2,t)},lane);
                const float dt=(x-q.first)*(x-q.first)+(y-q.second)*(y-q.second);
                if(dt<transitionDistance){transitionDistance=dt;nearTransition=i;}
            }
        }
        if(nearTransition>=0&&(nearest<0||transitionDistance<pointDistance)&&
           (nearBend<0||transitionDistance<=bendDistance)){
            transition=nearTransition;return true;
        }
        if(nearBend>=0&&(nearest<0||bendDistance<=pointDistance)){
            bend=nearBend;return true;
        }
        if(nearest>=0){selected=drag=nearest;return true;}
        drag=-2;return true;
    }
    bool move(float x,float y){
        if(drag>=0||drag==-3||bend>=0||transition>=0){
            if(std::fabs(x-downX)+std::fabs(y-downY)>8.0f){
                moved=true;changed=update(x,y)||changed;
            }
        }else if(drag==-2){
            if(std::fabs(x-downX)+std::fabs(y-downY)>12.0f)moved=true;
        }
        return true;
    }
    bool up(){
        if(drag==-2&&!moved)changed=addPoint(downX,downY)||changed;
        if(drag>=1&&!moved){
            const auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now()-started).count();
            if(ms>=500){
                auto m=getModel();auto c=getCurve(m);
                if(c.count>2&&drag<c.count-1){
                    for(int i=drag;i<c.count-1;++i){
                        c.points[i]=c.points[i+1];
                        setShape(m,i,currentShape(m,i+1));
                        setTransition(m,i,currentTransition(m,i+1));
                    }
                    --c.count;
                    if(page<2){
                        if(c.decayIndex>=drag&&c.decayIndex>0)--c.decayIndex;
                        if(c.releaseIndex>=drag&&c.releaseIndex>0)--c.releaseIndex;
                        c.decayIndex=std::min<int>(c.decayIndex,c.count-2);
                        c.releaseIndex=std::min<int>(c.releaseIndex,c.count-2);
                    }
                    selected=-1;changed=save(m,c)||changed;
                }
            }
        }
        drag=bend=transition=-1;
        const bool result=changed;changed=false;
        return result;
    }
};
FilterSurface filterSurface;

struct UnisonSurface {
    using Rect=NativeEditor::Rect;
    Rect view{};
    int selected{-1},drag{-1},bend{-1},zoom{2};
    float scroll{0},downX{0},downY{0};
    bool moved{false},changed{false};
    std::chrono::steady_clock::time_point started{};
    static constexpr std::array<float,6> zooms{{.5f,1,2,4,8,16}};
    float rate()const{return 320.0f*zooms[zoom];}
    OperatorCurve curve()const{
        const auto& p=ProjectCore::instance().selectedPatch();
        return p.unisonCurve.enabled&&p.unisonCurve.count>=2?
            p.unisonCurve:initialUnisonCurve(p.unison);
    }
    static bool flat(const OperatorCurve& c){
        return c.count==2&&c.releaseIndex==0&&c.points[1].time>=119.0f&&
            std::fabs(c.points[0].value-c.points[1].value)<.00001f;
    }
    bool save(const OperatorCurve& c){
        return ProjectCore::instance().setSelectedUnisonCurve(c);
    }
    Rect control(int i)const{
        const float w=view.w-32,gap=6,cw=(w-5*gap)/6;
        return {view.x+16+i*(cw+gap),view.y+174,cw,54};
    }
    Rect graph()const{
        const float y=view.y+280.0f;
        return {view.x+75,y,view.w-98,
            std::max(75.0f,view.y+view.h-83-y)};
    }
    std::pair<float,float> at(const OperatorPoint& p)const{
        const auto r=graph();
        return {r.x+std::clamp(p.value,0.0f,1.0f)*r.w,
                r.y+p.time*rate()-scroll};
    }
    void scrollBy(float dy){
        scroll=std::clamp(scroll+dy,0.0f,
            std::max(0.0f,120.0f*rate()-graph().h));
    }
    void draw()const{
        auto& o=NativeOverlay::instance();
        const auto c=curve();
        o.addRect({view.x,view.y,view.w,view.h},overlayColor(kBg));
        o.addText("Unison - per-note detune strength (0 to 100)",
            view.x+24,view.y+105,.92f,overlayColor(kCyan));
        o.addText(flat(c)?
            "Flat line = constant detune. Drag the line; tap away to add envelope points.":
            "Envelope follows time and releases on note-off.",
            view.x+24,view.y+140,.82f,overlayColor(kMuted));
        const char* label[6]={"D","R",c.sustain?"S Hold":"Dynamic","-",
                             "Zoom","+"};
        for(int i=0;i<6;++i){
            const auto r=control(i);
            const bool on=i==2&&c.sustain;
            o.addRect({r.x,r.y,r.w,r.h},overlayColor(on?kCyan:kButton));
            const std::string v=i==4?(zooms[zoom]<1.0f?"0.5x":
                std::to_string(static_cast<int>(zooms[zoom]))+"x"):label[i];
            o.addTextCentered(v,{r.x,r.y,r.w,r.h},.84f,
                overlayColor(on?kBg:kWhite));
        }
        const auto r=graph();
        o.addRect({r.x-6,r.y-6,r.w+12,r.h+12},overlayColor(kRollBg));
        for(int i=0;i<=4;++i){
            const float x=r.x+i*r.w*.25f;
            o.addLine(x,r.y,x,r.y+r.h,1,overlayColor(kMuted,.45f));
            o.addText(std::to_string(i*25),
                x-9,r.y-23,.76f,overlayColor(kCyan));
        }
        o.setClip({r.x,r.y,r.w,r.h});
        for(int i=1;i<c.count;++i){
            for(int k=1;k<=28;++k){
                const float a=c.points[i-1].time+
                    (c.points[i].time-c.points[i-1].time)*(k-1)/28.0f;
                const float b=c.points[i-1].time+
                    (c.points[i].time-c.points[i-1].time)*k/28.0f;
                const auto va=at({a,operatorCurveValue(c.points.data()+i-1,2,a)});
                const auto vb=at({b,operatorCurveValue(c.points.data()+i-1,2,b)});
                o.addLine(va.first,va.second,vb.first,vb.second,3,
                    overlayColor(kCyan));
            }
        }
        o.clearClip();
        if(!flat(c)){
            for(int i=1;i<c.count;++i){
                const float t=c.points[i-1].time+
                    (c.points[i].time-c.points[i-1].time)*
                    std::clamp(c.points[i].midpoint,.05f,.95f);
                const auto xy=at({t,operatorCurveValue(c.points.data()+i-1,2,t)});
                if(xy.second<r.y||xy.second>r.y+r.h)continue;
                o.addLine(xy.first-6,xy.second-6,xy.first+6,xy.second+6,2,
                    overlayColor(kOrange));
                o.addLine(xy.first-6,xy.second+6,xy.first+6,xy.second-6,2,
                    overlayColor(kOrange));
            }
            for(int i=0;i<c.count;++i){
                const auto xy=at(c.points[i]);
                if(xy.second<r.y||xy.second>r.y+r.h)continue;
                o.addFilledCircle(xy.first,xy.second,selected==i?9:6,
                    overlayColor(selected==i?kWhite:kCyan));
                if(i==c.decayIndex)o.addText("D",xy.first+8,xy.second+10,
                    .78f,overlayColor(kCyan));
                if(i==c.releaseIndex)o.addText(c.sustain?"S":"R",
                    xy.first+8,xy.second-19,.82f,overlayColor(kOrange));
            }
        }
        static constexpr std::array<float,14> ticks{{
            .001f,.002f,.005f,.01f,.02f,.05f,.1f,.2f,
            .5f,1,2,5,10,20}};
        float interval=ticks.back();
        for(float t:ticks)if(t*rate()>=65){interval=t;break;}
        const int first=std::max(0,static_cast<int>(std::floor(
            scroll/(rate()*interval))));
        const int last=std::min(static_cast<int>(120.0f/interval),
            static_cast<int>(std::ceil((scroll+r.h)/(rate()*interval))));
        for(int i=first;i<=last;++i){
            const float t=i*interval,y=r.y+t*rate()-scroll;
            if(y<r.y||y>r.y+r.h)continue;
            o.addLine(r.x,y,r.x+r.w,y,1,overlayColor(kMuted,.25f));
            char label[24];std::snprintf(label,sizeof(label),
                t<1?"%.3fs":t<10?"%.2fs":"%.1fs",static_cast<double>(t));
            o.addText(label,r.x-62,y-8,.75f,overlayColor(kWhite));
        }
    }
    bool setControl(int i){
        auto c=curve();
        if(i==0&&selected>=0&&selected<c.count-1)c.decayIndex=selected;
        else if(i==1&&selected>=0&&selected<c.count-1)c.releaseIndex=selected;
        else if(i==2)c.sustain=!c.sustain;
        else if(i==3||i==5){
            const float top=scroll/rate();
            zoom=std::clamp(zoom+(i==3?-1:1),0,5);
            scroll=std::clamp(top*rate(),0.0f,
                std::max(0.0f,120.0f*rate()-graph().h));
            return false;
        }else return false;
        return save(c);
    }
    bool update(float x,float y){
        auto c=curve();const auto r=graph();
        if(drag==-3&&flat(c)){
            const float value=std::clamp((x-r.x)/r.w,0.0f,1.0f);
            c.points[0].value=c.points[1].value=value;
            return save(c);
        }
        const int index=bend>=0?bend:drag;
        if(index<0||index>=c.count)return false;
        const float time=std::clamp((y-r.y+scroll)/rate(),0.0f,120.0f);
        const float value=std::clamp((x-r.x)/r.w,0.0f,1.0f);
        auto& p=c.points[index];
        if(bend>=1){
            const auto& a=c.points[index-1];
            p.midpoint=std::clamp((time-a.time)/
                std::max(.000001f,p.time-a.time),.05f,.95f);
            p.handleValue=std::clamp(value,std::min(a.value,p.value),
                std::max(a.value,p.value));
            p.handleActive=true;
        }else{
            const float min=index?c.points[index-1].time+.0001f:0;
            const float max=index+1<c.count?
                c.points[index+1].time-.0001f:120;
            p.time=std::clamp(time,min,std::max(min,max));
            p.value=value;
        }
        return save(c);
    }
    bool addPoint(float x,float y){
        auto c=curve();if(c.count>=kMaxOperatorPoints)return false;
        const auto r=graph();
        const float t=std::clamp((y-r.y+scroll)/rate(),.005f,119.65f);
        const float v=std::clamp((x-r.x)/r.w,0.0f,1.0f);
        if(flat(c)){
            const float previous=c.points[0].value;
            c.count=3;c.sustain=false;c.decayIndex=1;c.releaseIndex=1;
            c.points[0]={0,previous};
            c.points[1]={t,v};
            c.points[2]={t+.35f,previous};
            selected=1;return save(c);
        }
        int pos=0;while(pos<c.count&&c.points[pos].time<t)++pos;
        if(pos==0||pos==c.count||t-c.points[pos-1].time<.0002f||
           c.points[pos].time-t<.0002f)return false;
        for(int i=c.count;i>pos;--i)c.points[i]=c.points[i-1];
        c.points[pos]={t,v};++c.count;
        if(c.decayIndex>=pos)++c.decayIndex;
        if(c.releaseIndex>=pos)++c.releaseIndex;
        selected=pos;return save(c);
    }
    bool down(float x,float y){
        moved=changed=false;drag=bend=-1;downX=x;downY=y;
        started=std::chrono::steady_clock::now();
        for(int i=0;i<6;++i)if(control(i).contains(x,y)){
            changed=setControl(i);return true;
        }
        const auto r=graph();
        if(!r.contains(x,y))return true;
        const auto c=curve();
        if(flat(c)){
            const float px=at(c.points[0]).first;
            drag=std::fabs(x-px)<23.0f?-3:-2;
            if(drag==-3)selected=0;
            return true;
        }
        float nearestDistance=24*24;int nearest=-1;
        for(int i=0;i<c.count;++i){
            const auto atPoint=at(c.points[i]);
            const float distance=(x-atPoint.first)*(x-atPoint.first)+
                (y-atPoint.second)*(y-atPoint.second);
            if(distance<nearestDistance){nearestDistance=distance;nearest=i;}
        }
        float nearestBend=20*20;int bendIndex=-1;
        for(int i=1;i<c.count;++i){
            const float t=c.points[i-1].time+
                (c.points[i].time-c.points[i-1].time)*
                c.points[i].midpoint;
            const auto xy=at({t,operatorCurveValue(c.points.data()+i-1,2,t)});
            const float distance=(x-xy.first)*(x-xy.first)+
                (y-xy.second)*(y-xy.second);
            if(distance<nearestBend){nearestBend=distance;bendIndex=i;}
        }
        if(bendIndex>=0&&(nearest<0||nearestBend<=nearestDistance)){
            bend=bendIndex;return true;
        }
        if(nearest>=0){selected=drag=nearest;return true;}
        drag=-2;return true;
    }
    void move(float x,float y){
        if(drag>=0||drag==-3||bend>=0){
            if(std::fabs(x-downX)+std::fabs(y-downY)>6){
                changed=update(x,y)||changed;moved=true;
            }
        }else if(drag==-2&&
                 std::fabs(x-downX)+std::fabs(y-downY)>12)moved=true;
    }
    bool up(){
        if(drag==-2&&!moved)changed=addPoint(downX,downY)||changed;
        if(drag>0&&!moved){
            const auto millis=std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now()-started).count();
            if(millis>=500){
                auto c=curve();
                if(c.count==3&&drag==1){
                    c=initialUnisonCurve(c.points[0].value);
                    selected=-1;changed=save(c)||changed;
                }else if(c.count>3&&drag<c.count-1){
                    for(int i=drag;i<c.count-1;++i)c.points[i]=c.points[i+1];
                    --c.count;
                    if(c.decayIndex>=drag&&c.decayIndex>0)--c.decayIndex;
                    if(c.releaseIndex>=drag&&c.releaseIndex>0)--c.releaseIndex;
                    c.decayIndex=std::min<int>(c.decayIndex,c.count-2);
                    c.releaseIndex=std::min<int>(c.releaseIndex,c.count-2);
                    selected=-1;changed=save(c)||changed;
                }
            }
        }
        drag=bend=-1;
        const bool result=changed;changed=false;return result;
    }
};
UnisonSurface unisonSurface;
} // namespace

void NativeEditor::renderSynth() const noexcept {
    if(!matrixMode_){
        drawPadReservedBackground();
        opSurface.view=bodyRect();
        const auto currentOperator=ProjectCore::instance().selectedPatch().ops[static_cast<size_t>(opSurface.op)];
        opSurface.custom=currentOperator.wave==Wave::Custom;
        if(!opSurface.custom&&opSurface.page==5)opSurface.page=4;
        opSurface.draw();
        drawPatchTransfer(EditorPage::Synth);
        auto& ov=NativeOverlay::instance();auto b=bodyRect();
        auto tab=synthTabRect(0);
        fillRect({b.x,b.y,b.w,tab.y+tab.h-b.y+6},kPanel);
        for(int i=0;i<2;++i){auto r=synthTabRect(i);drawButton(r,i==0,i?kPurple:kCyan);
            ov.addTextCentered(i?"FM Matrix":"Operators",{r.x,r.y,r.w,r.h},1.04f,overlayColor(i?kWhite:kBg));}
        return;
    }
    drawPadReservedBackground();
    const Patch p=ProjectCore::instance().selectedPatch();
    auto& ov=NativeOverlay::instance();
    const auto b=bodyRect();
    const auto firstTab=synthTabRect(0);
    const float gap=std::clamp(b.w*0.014f,9.0f,13.0f);
    const float contentTop=firstTab.y+firstTab.h+gap;

    // Everything below the tab strip scrolls/clips beneath it.
    ov.setClip({
        b.x,
        contentTop,
        b.w,
        std::max(0.0f,b.y+b.h-contentTop)
    });

    if(matrixMode_){
        ov.addText(
            "Row = modulator → column = carrier.",
            b.x+gap,
            contentTop+10.0f,
            0.82f,
            overlayColor(kMuted));

        for(int carrier=0;carrier<6;++carrier){
            const auto rr=matrixRect(0,carrier);
            ov.addTextCentered(
                "C"+std::to_string(carrier+1),
                {rr.x,rr.y-34.0f,rr.w,28.0f},
                0.82f,
                overlayColor(kMuted));
        }

        for(int m=0;m<6;++m){
            const auto first=matrixRect(m,0);
            ov.addTextCentered(
                "M"+std::to_string(m+1),
                {first.x-42.0f,first.y,36.0f,first.h},
                0.82f,
                overlayColor(kMuted));

            for(int carrier=0;carrier<6;++carrier){
                const auto rr=matrixRect(m,carrier);
                const float v=std::clamp(
                    p.matrix[static_cast<size_t>(m)][static_cast<size_t>(carrier)],
                    0.0f,1.0f);

                const float inset=std::max(8.0f,rr.w*0.08f);
                const float x0=rr.x+inset;
                const float w=std::max(12.0f,rr.w-inset*2.0f);
                const float cy=rr.y+rr.h*0.54f;
                fillRect({x0,cy-3.0f,w,6.0f},kTrack);
                fillRect({x0,cy-3.0f,w*v,6.0f},kPurple);

                const float knob=std::clamp(rr.h*0.22f,14.0f,20.0f);
                fillRect({
                    x0+w*v-knob*0.5f,
                    cy-knob*0.5f,
                    knob,knob},
                    mix(kWhite,kPurple,0.18f));
            }
        }
    }else{
        static constexpr const char* kLabels[8]={
            "R","ST","F","L","A","D","S","R"};

        const auto shortValue=[](float v){
            std::string s=std::to_string(v);
            while(s.size()>1&&s.back()=='0')s.pop_back();
            if(!s.empty()&&s.back()=='.')s.pop_back();
            if(s.size()>6)s.resize(6);
            return s;
        };

        for(int op=0;op<6;++op){
            const auto card=operatorCardRect(op);
            const auto& o=p.ops[static_cast<size_t>(op)];
            const bool enabled=o.enabled;

            fillRect(card,enabled?kCyan:kButton);
            fillRect({
                card.x+2.0f,card.y+2.0f,
                card.w-4.0f,card.h-4.0f},
                kPanel);

            ov.addText(
                "OP"+std::to_string(op+1),
                card.x+16.0f,card.y+22.0f,
                1.04f,overlayColor(kWhite));

            const auto wave=operatorWaveFieldRect(op);
            fillRect(wave,mix(kRollBg,kButton,0.25f));
            const int waveIndex=std::clamp(static_cast<int>(o.wave),0,static_cast<int>(kWaveformLabels.size())-1);
            ov.addText(
                kWaveformLabels[static_cast<size_t>(waveIndex)],
                wave.x+12.0f,wave.y+20.0f,
                0.96f,overlayColor(kWhite));
            ov.addDownChevron(
                {wave.x+wave.w-38.0f,wave.y,38.0f,wave.h},
                overlayColor(kWhite));

            for(int paramIndex=0;paramIndex<8;++paramIndex){
                const auto param=kOperatorParams[static_cast<size_t>(paramIndex)];
                const auto rr=operatorCardParamRect(op,paramIndex);
                const float value=operatorValue(p,op,param);
                const float norm=normalized(value,operatorRange(param));

                ov.addText(
                    kLabels[paramIndex],
                    rr.x,rr.y+rr.h*0.26f,
                    paramIndex==1?0.72f:0.82f,
                    overlayColor(kWhite));

                const float labelW=38.0f;
                const float valueW=62.0f;
                const float trackX=rr.x+labelW;
                const float trackW=
                    std::max(20.0f,rr.w-labelW-valueW-6.0f);

                if(param==OperatorParam::Ratio||
                   param==OperatorParam::Semitone){
                    const float editW=
                        std::max(20.0f,rr.w-labelW-4.0f);
                    fillRect(
                        {trackX,rr.y+4.0f,editW,rr.h-8.0f},
                        mix(kRollBg,kButton,0.22f));
                    const float shown=param==OperatorParam::Semitone
                        ?std::round(value):value;
                    ov.addText(
                        shortValue(shown),
                        trackX+10.0f,rr.y+22.0f,
                        1.00f,overlayColor(kWhite));
                }else{
                    const float cy=rr.y+rr.h*0.52f;
                    fillRect({trackX,cy-3.0f,trackW,6.0f},kTrack);
                    fillRect({
                        trackX,cy-3.0f,trackW*norm,6.0f},
                        paramIndex<4?kCyan:kOrange);
                    const float knob=22.0f;
                    fillRect({
                        trackX+trackW*norm-knob*0.5f,
                        cy-knob*0.5f,knob,knob},
                        mix(
                            kWhite,
                            paramIndex<4?kCyan:kOrange,
                            0.25f));

                    const float shown=param==OperatorParam::Detune
                        ?std::round(value):value;
                    ov.addText(
                        shortValue(shown),
                        rr.x+rr.w-valueW+4.0f,
                        rr.y+rr.h*0.26f,
                        0.86f,overlayColor(kMuted));
                }
            }

            const auto toggle=operatorCardToggleRect(op);
            drawButton(toggle,enabled,enabled?kGreen:kRed);
            ov.addTextCentered(
                enabled?"ON":"OFF",
                {toggle.x,toggle.y,toggle.w,toggle.h},
                1.02f,overlayColor(kWhite));

            if(o.wave==Wave::Custom){
                for(int partial=0;partial<16;++partial){
                    const auto hr=operatorCardHarmonicRect(op,partial);
                    const float v=std::clamp(
                        o.harm[static_cast<size_t>(partial)],0.0f,1.0f);
                    fillRect(hr,kButton);
                    fillRect({
                        hr.x+hr.w*0.24f,
                        hr.y+hr.h*(1.0f-v),
                        hr.w*0.52f,
                        std::max(3.0f,hr.h*v)},
                        mix(kCyan,kPurple,0.40f));
                }
            }
        }

        drawPatchTransfer(EditorPage::Synth);
    }

    ov.clearClip();

    // Solid sticky strip so scrolled controls cannot bleed through the tabs.
    fillRect({
        b.x,b.y,b.w,
        std::max(0.0f,contentTop-b.y)},
        kPanel);

    static constexpr const char* kTabs[2]={"Operators","FM Matrix"};
    for(int i=0;i<2;++i){
        const auto rr=synthTabRect(i);
        const bool active=(i==1)==matrixMode_;
        drawButton(rr,active,i==1?kPurple:kCyan);
        ov.addTextCentered(
            kTabs[i],
            {rr.x,rr.y,rr.w,rr.h},
            1.08f,
            overlayColor(active?kBg:kWhite));
    }
}

void NativeEditor::renderFx() const noexcept {
    drawPadReservedBackground();
    auto& ov=NativeOverlay::instance();
    const auto b=bodyRect();
    const auto workspace=std::clamp(fxWorkspace_,0,3);
    if(workspace==0){
        filterSurface.view=b;filterSurface.draw();
    }else if(workspace==1){
        unisonSurface.view=b;unisonSurface.draw();
    }else if(workspace==2){
        const auto p=ProjectCore::instance().selectedPatch();
        ov.addText("LFO - existing modulation controls",
            b.x+24,b.y+109,1.0f,overlayColor(kCyan));
        const auto target=lfoWorkspaceTargetRect();
        drawButton(target,true,kPurple);
        const char* labels[4]={"None","Pitch","Filter","Amp"};
        ov.addTextCentered(
            std::string("Target: ")+labels[std::clamp(static_cast<int>(p.lfo.target),0,3)],
            {target.x,target.y,target.w,target.h},.85f,overlayColor(kBg));
        static constexpr std::array<PatchParam,4> keys{
            PatchParam::LfoRate,PatchParam::LfoAmount,
            PatchParam::LfoAttack,PatchParam::LfoVelocity};
        static constexpr const char* labels4[4]{
            "Rate","Amount","Attack","Velocity"};
        for(int i=0;i<4;++i){
            const auto r=lfoWorkspaceRow(i);
            const auto range=patchRange(keys[i]);
            const float value=patchValue(p,keys[i]);
            const float normalizedValue=normalized(value,range);
            const float lw=std::clamp(r.w*.25f,104.0f,160.0f);
            const float vw=70;
            const float x=r.x+lw;
            const float width=std::max(24.0f,r.w-lw-vw-8.0f);
            const float y=r.y+r.h*.52f;
            ov.addText(labels4[i],r.x,r.y+23,.89f,overlayColor(kWhite));
            fillRect({x,y-3,width,6},kTrack);
            fillRect({x,y-3,width*normalizedValue,6},kPurple);
            fillRect({x+width*normalizedValue-10,y-10,20,20},
                mix(kWhite,kPurple,.22f));
            char shown[28];std::snprintf(shown,sizeof(shown),"%.3g",
                static_cast<double>(value));
            ov.addText(shown,r.x+r.w-vw,r.y+24,.85f,overlayColor(kCyan));
        }
    }else{
        const Fx rack=ProjectCore::instance().fxRack(false);
        const auto rackRect=fxRackRect();
        ov.setClip({b.x,b.y+85,b.w,std::max(0.0f,b.h-85)});
        fillRect(rackRect,kButton);
        fillRect({rackRect.x+2,rackRect.y+2,rackRect.w-4,rackRect.h-4},kPanel);
        ov.addText("Instrument / Pad post-processing FX",
            rackRect.x+20,rackRect.y+18,1.02f,overlayColor(kCyan));
        for(int i=0;i<rack.count;++i){
            const auto& module=rack.chain[i];
            const auto r=fxRackStageRect(i);
            fillRect(r,mix(kPanel,kButton,.45f));
            const float bw=std::clamp(r.w*.10f,39.0f,67.0f);
            const float right=r.x+r.w;
            ov.addText(std::to_string(i+1)+". "+fxTypeName(module.type)+
                (i==fxRackExpanded_?"   -":"   +"),
                r.x+12,r.y+25,.89f,
                overlayColor(module.enabled?kWhite:kMuted));
            const char* labels[4]={"^","v",module.enabled?"On":"Off","X"};
            const Rgb colors[4]={kCyan,kCyan,module.enabled?kGreen:kMuted,kRed};
            for(int j=0;j<4;++j){
                const Rect cell{right-(4-j)*bw,r.y+5,bw-3,r.h-10};
                drawButton(cell,false,colors[j]);
                ov.addTextCentered(labels[j],{cell.x,cell.y,cell.w,cell.h},
                    .85f,overlayColor(kWhite));
            }
            if(i==fxRackExpanded_){
                for(size_t k=0;k<fxParamCount(module.type);++k){
                    const auto pr=fxRackParamRect(i,static_cast<int>(k));
                    const auto d=fxParamDesc(module.type,k);
                    const float n=normalized(module.params[k],{d.lo,d.hi});
                    const float lw=std::clamp(pr.w*.27f,115.0f,182.0f),vw=71.0f;
                    const float x=pr.x+lw,w=std::max(24.0f,pr.w-lw-vw-9.0f);
                    const float y=pr.y+pr.h*.52f;
                    ov.addText(d.name,pr.x,pr.y+pr.h*.26f,
                        .74f,overlayColor(kMuted));
                    fillRect({x,y-3,w,6},kTrack);
                    fillRect({x,y-3,w*n,6},kPurple);
                    fillRect({x+w*n-10,y-10,20,20},mix(kWhite,kPurple,.28f));
                    char shown[30];std::snprintf(shown,sizeof(shown),"%.4g",
                        static_cast<double>(module.params[k]));
                    ov.addText(shown,pr.x+pr.w-vw+3,
                        pr.y+pr.h*.26f,.77f,overlayColor(kWhite));
                }
            }
        }
        const auto select=fxRackAddRect(false),add=fxRackAddRect(true);
        drawButton(select,false,kCyan);drawButton(add,true,kGreen);
        ov.addTextCentered(std::string(fxTypeName(fxRackNewType_)),
            {select.x,select.y,select.w,select.h},.82f,overlayColor(kWhite));
        ov.addDownChevron({select.x+select.w-37,select.y,37,select.h},
            overlayColor(kWhite));
        ov.addTextCentered("+ Add FX",{add.x,add.y,add.w,add.h},
            .90f,overlayColor(kWhite));
        ov.clearClip();
    }
    // A fixed navigation strip; no Glide or legacy expression-link page.
    fillRect({b.x,b.y,b.w,87},kPanel);
    static constexpr const char* names[4]={"Filter","Unison","LFO","FX"};
    for(int i=0;i<4;++i){
        const auto r=fxWorkspaceTabRect(i);
        const bool active=workspace==i;
        drawButton(r,active,i==3?kPurple:kCyan);
        ov.addTextCentered(names[i],{r.x,r.y,r.w,r.h},1.02f,
            overlayColor(active?kBg:kWhite));
    }
    return;

    const Patch p=ProjectCore::instance().selectedPatch();
    auto& ov=NativeOverlay::instance();

    static constexpr const char* kTitles[4]={
        "Filter","Amp envelope","LFO","Expression links"};

    const auto shortValue=[](float v){
        std::string s=std::to_string(v);
        while(s.size()>1&&s.back()=='0')s.pop_back();
        if(!s.empty()&&s.back()=='.')s.pop_back();
        if(s.size()>7)s.resize(7);
        return s;
    };

    const auto drawSection=[&](int section){
        const auto card=fxGroupRect(section);
        fillRect(card,kButton);
        fillRect({
            card.x+2.0f,card.y+2.0f,
            card.w-4.0f,card.h-4.0f},kPanel);
        ov.addText(
            kTitles[section],
            card.x+20.0f,card.y+22.0f,
            1.10f,overlayColor(kCyan));
    };

    const auto drawParam=[&](
        int section,int row,const char* label,
        PatchParam param,Rgb accent,bool peak=false){
        const auto rr=peak?peakParamRect(row):fxSectionParamRect(section,row);
        const float value=patchValue(p,param);
        const float norm=normalized(value,patchRange(param));
        ov.addText(
            label,rr.x,rr.y+rr.h*0.26f,
            0.76f,overlayColor(kMuted));

        const float labelW=std::clamp(rr.w*0.25f,104.0f,160.0f);
        const float valueW=70.0f;
        const float x0=rr.x+labelW;
        const float w=std::max(24.0f,rr.w-labelW-valueW-8.0f);
        const float cy=rr.y+rr.h*0.52f;

        fillRect({x0,cy-3.0f,w,6.0f},kTrack);
        fillRect({x0,cy-3.0f,w*norm,6.0f},accent);
        const float knob=22.0f;
        fillRect({
            x0+w*norm-knob*0.5f,
            cy-knob*0.5f,knob,knob},
            mix(kWhite,accent,0.28f));

        ov.addText(
            shortValue(value),
            rr.x+rr.w-valueW+4.0f,
            rr.y+rr.h*0.26f,
            0.78f,overlayColor(kMuted));
    };

    // Instrument / pad FX only; the master rack lives on Tracks.
    const Fx rack=ProjectCore::instance().fxRack(false);
    const auto rackRect=fxRackRect();
    fillRect(rackRect,kButton);
    fillRect({rackRect.x+2,rackRect.y+2,rackRect.w-4,rackRect.h-4},kPanel);
    ov.addText("Instrument / Pad FX chain",rackRect.x+20,rackRect.y+18,1.08f,overlayColor(kCyan));
    for(int i=0;i<rack.count;++i){
        const auto& module=rack.chain[i];
        const auto r=fxRackStageRect(i);
        fillRect(r,mix(kPanel,kButton,.45f));
        const float bw=std::clamp(r.w*.10f,39.0f,67.0f);
        const float right=r.x+r.w;
        ov.addText(std::to_string(i+1)+". "+fxTypeName(module.type)+
            (i==fxRackExpanded_?"   -":"   +"),
            r.x+12,r.y+25,.89f,overlayColor(module.enabled?kWhite:kMuted));
        const char* labels[4]={"^","v",module.enabled?"On":"Off","X"};
        const Rgb colors[4]={kCyan,kCyan,module.enabled?kGreen:kMuted,kRed};
        for(int j=0;j<4;++j){
            const Rect cell{right-(4-j)*bw,r.y+5,bw-3,r.h-10};
            drawButton(cell,false,colors[j]);
            ov.addTextCentered(labels[j],{cell.x,cell.y,cell.w,cell.h},.85f,overlayColor(kWhite));
        }
        if(i==fxRackExpanded_){
            for(size_t k=0;k<fxParamCount(module.type);++k){
                const auto pr=fxRackParamRect(i,static_cast<int>(k));
                const auto d=fxParamDesc(module.type,k);
                const float n=normalized(module.params[k],{d.lo,d.hi});
                const float lw=std::clamp(pr.w*.27f,115.0f,182.0f),vw=71.0f;
                const float x0=pr.x+lw,w=std::max(24.0f,pr.w-lw-vw-9.0f);
                const float cy=pr.y+pr.h*.52f;
                ov.addText(d.name,pr.x,pr.y+pr.h*.26f,.74f,overlayColor(kMuted));
                fillRect({x0,cy-3,w,6},kTrack);
                fillRect({x0,cy-3,w*n,6},kPurple);
                fillRect({x0+w*n-10,cy-10,20,20},mix(kWhite,kPurple,.28f));
                ov.addText(shortValue(module.params[k]),pr.x+pr.w-vw+3,
                    pr.y+pr.h*.26f,.77f,overlayColor(kWhite));
            }
        }
    }
    const auto pick=fxRackAddRect(false),rackAdd=fxRackAddRect(true);
    drawButton(pick,false,kCyan);drawButton(rackAdd,true,kGreen);
    ov.addTextCentered(std::string(fxTypeName(fxRackNewType_)),
        {pick.x,pick.y,pick.w,pick.h},.82f,overlayColor(kWhite));
    ov.addDownChevron({pick.x+pick.w-37,pick.y,37,pick.h},overlayColor(kWhite));
    ov.addTextCentered("+ Add FX",{rackAdd.x,rackAdd.y,rackAdd.w,rackAdd.h},.90f,overlayColor(kWhite));

    // Filter. Old sliders remain accessible on legacy patches; the new
    // three-lane filter opens in a dedicated full-screen point editor.
    drawSection(0);
    {
        const auto card=fxGroupRect(0);
        const Rect editButton{card.x+card.w-200,card.y+12,180,52};
        drawButton(editButton,true,kCyan);
        ov.addTextCentered("Edit filter",
            {editButton.x,editButton.y,editButton.w,editButton.h},
            .87f,overlayColor(kBg));
        if(p.filter.evolving.configured)
            ov.addText("Evolving filter active - edit F / Amount / Q",
                card.x+20,card.y+70,.82f,overlayColor(kGreen));
    }
    if(!p.filter.evolving.configured){
        const auto card=fxGroupRect(0);
        ov.addText(
            "Type",
            card.x+20.0f,card.y+96.0f,
            0.86f,overlayColor(kMuted));
        const auto field=filterTypeRect(0);
        fillRect(field,mix(kRollBg,kButton,0.24f));
        static constexpr const char* names[3]={
            "lowpass","highpass","bandpass"};
        const int type=std::clamp(static_cast<int>(p.filter.type),0,2);
        ov.addText(
            names[type],
            field.x+12.0f,field.y+19.0f,
            0.90f,overlayColor(kWhite));
        ov.addDownChevron(
            {field.x+field.w-38.0f,field.y,38.0f,field.h},
            overlayColor(kWhite));
    static constexpr const char* filterLabels[7]={
        "Cutoff","Reso","Env→Cut","A","D","S","R"};
    for(int i=0;i<7;++i)
        drawParam(0,i,filterLabels[i],
            kFilterParams[static_cast<size_t>(i)],
            i<3?kCyan:kOrange);

    }else{
        const auto card=fxGroupRect(0);
        ov.addText("The three synchronized curves replace the legacy filter controls.",
            card.x+22,card.y+194,.84f,overlayColor(kMuted));
        ov.addText("Open Edit filter to draw Frequency, Amount and Q.",
            card.x+22,card.y+241,.82f,overlayColor(kCyan));
        ov.addText("Morph between alternate curves or override them with V / M.",
            card.x+22,card.y+288,.82f,overlayColor(kPurple));
        ov.addText("Low shelf / Bell / High shelf on each Q point.",
            card.x+22,card.y+335,.82f,overlayColor(kOrange));
    }

    // A distinct per-note peaking EQ precedes the existing filter.
    // A two-state control keeps Follow Note and Fixed Hz equally accessible.
    {
        const auto card=fxGroupRect(0);
        ov.addText("Parametric Peak",card.x+20.0f,card.y+758.0f,
            1.00f,overlayColor(kPurple));
        const auto enabled=peakToggleRect(0),mode=peakToggleRect(1);
        drawButton(enabled,p.peak.enabled,p.peak.enabled?kGreen:kMuted);
        drawButton(mode,p.peak.followNote,kCyan);
        ov.addTextCentered(p.peak.enabled?"Peak: On":"Peak: Off",
            {enabled.x,enabled.y,enabled.w,enabled.h},.84f,overlayColor(kWhite));
        ov.addTextCentered(p.peak.followNote?"Follow note":"Fixed Hz",
            {mode.x,mode.y,mode.w,mode.h},.84f,overlayColor(kWhite));
        drawParam(0,0,p.peak.followNote?"Center x":"Center Hz",
            p.peak.followNote?PatchParam::PeakRatio:PatchParam::PeakFrequency,kPurple,true);
        drawParam(0,1,"Q",PatchParam::PeakQ,kPurple,true);
        drawParam(0,2,"Gain dB",PatchParam::PeakGain,kPurple,true);
    }

    // Amp envelope
    drawSection(1);
    static constexpr const char* ampLabels[9]={
        "A","D","S","R","Volume",
        "Vel→Amp","Vel→Cut","Unison","Glide"};
    for(int i=0;i<9;++i)
        drawParam(1,i,ampLabels[i],
            kAmpParams[static_cast<size_t>(i)],
            i<5?kCyan:kOrange);

    // LFO remains a synth control; all audio effects are rack modules.
    drawSection(2);
    static constexpr const char* lfoLabels[4]={"Rate","Amount","LFO Atk","LFO Vel"};
    for(int i=0;i<4;++i)
        drawParam(2,i,lfoLabels[i],kLfoParams[static_cast<size_t>(i)],kPurple);

    {
        const auto target=lfoTargetRect(0);
        const auto card=fxGroupRect(2);
        ov.addText(
            "Target",
            card.x+20.0f,target.y+target.h*0.26f,
            0.86f,overlayColor(kMuted));
        fillRect(target,mix(kRollBg,kButton,0.24f));
        static constexpr const char* targetNames[4]={
            "none","pitch","filter","amp"};
        const int targetIndex=std::clamp(static_cast<int>(p.lfo.target),0,3);
        ov.addText(
            targetNames[targetIndex],
            target.x+12.0f,target.y+19.0f,
            0.90f,overlayColor(kWhite));
        ov.addDownChevron(
            {target.x+target.w-38.0f,target.y,38.0f,target.h},
            overlayColor(kWhite));
    }

    // V/M share the target vocabulary; each target has one routing owner.
    drawSection(3);
    auto& project=ProjectCore::instance();
    const auto velocityTab=linkSourceRect(LinkSource::Velocity);
    const auto modTab=linkSourceRect(LinkSource::Mod);
    drawButton(velocityTab,linkSource_==LinkSource::Velocity,kCyan);
    drawButton(modTab,linkSource_==LinkSource::Mod,kPurple);
    ov.addTextCentered(
        "V - Velocity ("+std::to_string(project.selectedModSlotCount(LinkSource::Velocity))+")",
        {velocityTab.x,velocityTab.y,velocityTab.w,velocityTab.h},
        0.85f,overlayColor(kWhite));
    ov.addTextCentered(
        "M - Mod ("+std::to_string(project.selectedModSlotCount(LinkSource::Mod))+")",
        {modTab.x,modTab.y,modTab.w,modTab.h},
        0.85f,overlayColor(kWhite));
    const int count=project.selectedModSlotCount(linkSource_);
    for(int slot=0;slot<count;++slot){
        const auto s=project.selectedModSlot(slot,linkSource_);
        const auto row=modRowRect(slot);
        fillRect(row,mix(kPanel,kButton,0.18f));

        const auto label=modPartRect(slot,0);
        ov.addTextCentered(
            (linkSource_==LinkSource::Velocity?"V":"M")+std::to_string(slot+1),
            {label.x,label.y,label.w,label.h},
            0.82f,overlayColor(kWhite));

        const auto target=modPartRect(slot,1);
        const auto mn=modPartRect(slot,3);
        const auto mx=modPartRect(slot,4);
        const auto del=modPartRect(slot,5);

        fillRect(target,mix(kRollBg,kButton,0.24f));
        ov.addText(
            modTargetName(s.target),
            target.x+10.0f,target.y+31.0f,
            0.82f,overlayColor(kWhite));
        ov.addDownChevron(
            {target.x+target.w-32.0f,target.y,32.0f,target.h},
            overlayColor(kWhite));

        fillRect(mn,mix(kRollBg,kButton,0.20f));
        fillRect(mx,mix(kRollBg,kButton,0.20f));
        ov.addText("MIN",mn.x+10.0f,mn.y+9.0f,
            0.64f,overlayColor(kMuted));
        ov.addText("MAX",mx.x+10.0f,mx.y+9.0f,
            0.64f,overlayColor(kMuted));
        ov.addText(
            shortValue(s.min),
            mn.x+10.0f,mn.y+45.0f,
            0.82f,overlayColor(kWhite));
        ov.addText(
            shortValue(s.max),
            mx.x+10.0f,mx.y+45.0f,
            0.82f,overlayColor(kWhite));

        drawButton(del,false,kRed);
        ov.addTextCentered(
            "×",{del.x,del.y,del.w,del.h},
            0.90f,overlayColor(kWhite));
    }

    const auto add=modAddRect();
    const bool canAdd=true;
    drawButton(add,canAdd,kGreen);
    ov.addTextCentered(
        "+ Add link",
        {add.x,add.y,add.w,add.h},
        0.94f,overlayColor(canAdd?kWhite:kMuted));

    drawPatchTransfer(EditorPage::Fx);
}

std::optional<int> NativeEditor::hitFactoryPreset(float,float) const noexcept {
    // Factory patch selection lives on the Tracks/Patch page in the HTML layout.
    return std::nullopt;
}

bool NativeEditor::openDropdownAt(
    EditorPage page,float x,float y,NativeUi& ui) const {
    if(page==EditorPage::Fx){
        const auto anchor=[](Rect r){return NativeUi::Rect{r.x,r.y,r.w,r.h};};
        const auto& p=ProjectCore::instance().selectedPatch();
        if(fxWorkspace_==3){
            const auto rr=fxRackAddRect(false);
            if(rr.contains(x,y)){
                static const std::vector<std::string> names{
                    "Saturation","Metal Amp","Cabinet","5-Band EQ","Delay","Reverb"};
                ui.openDropdown(DropdownKind::FxType,0,anchor(rr),
                    static_cast<int>(fxRackNewType_),names);
                return true;
            }
        }else if(fxWorkspace_==2){
            const auto rr=lfoWorkspaceTargetRect();
            if(rr.contains(x,y)){
                ui.openDropdown(DropdownKind::LfoTarget,0,anchor(rr),
                    static_cast<int>(p.lfo.target),
                    {"none","pitch","filter","amp"});
                return true;
            }
        }
        return false;
    }
    const Patch p=ProjectCore::instance().selectedPatch();
    const auto anchor=[](Rect r){return NativeUi::Rect{r.x,r.y,r.w,r.h};};

    if(page==EditorPage::Synth){
        // Wave selectors only exist on the Operators tab. Their geometric
        // rectangles may still lie behind the fixed tab strip while the
        // operator body is scrolled, so respect both the active tab and the
        // visible clipped content region before testing them.
        if(matrixMode_)return false;

        const auto b=bodyRect();
        const auto tab=synthTabRect(0);
        const float gap=std::clamp(b.w*0.014f,9.0f,13.0f);
        const float contentTop=tab.y+tab.h+gap;
        const float contentBottom=b.y+b.h;
        if(y<contentTop||y>=contentBottom)return false;

        static const std::vector<std::string> waveNames{
            kWaveformLabels.begin(),kWaveformLabels.end()};
        for(int op=opSurface.op;op<=opSurface.op;++op){
            const auto rr=opSurface.rect(1);
            if(!rr.contains(x,y))continue;
            ui.openDropdown(
                DropdownKind::Wave,op,anchor(rr),
                std::clamp(
                    static_cast<int>(
                        p.ops[static_cast<size_t>(op)].wave),
                    0,static_cast<int>(kWaveformLabels.size())-1),
                waveNames);
            return true;
        }
        return false;
    }

    if(page==EditorPage::Fx){
        const auto rr=fxRackAddRect(false);
        if(rr.contains(x,y)&&bodyRect().contains(x,y)){
            std::vector<std::string> labels;
            for(int i=0;i<=static_cast<int>(FxType::Reverb);++i)
                labels.emplace_back(fxTypeName(static_cast<FxType>(i)));
            ui.openDropdown(DropdownKind::FxType,-1,anchor(rr),
                static_cast<int>(fxRackNewType_),std::move(labels));
            return true;
        }
    }

    {
        const auto rr=filterTypeRect(0);
        if(rr.contains(x,y)){
            ui.openDropdown(
                DropdownKind::FilterType,0,anchor(rr),
                std::clamp(static_cast<int>(p.filter.type),0,2),
                {"lowpass","highpass","bandpass"});
            return true;
        }
    }

    {
        const auto rr=lfoTargetRect(0);
        if(rr.contains(x,y)){
            ui.openDropdown(
                DropdownKind::LfoTarget,0,anchor(rr),
                std::clamp(static_cast<int>(p.lfo.target),0,3),
                {"none","pitch","filter","amp"});
            return true;
        }
    }

    auto& project=ProjectCore::instance();
    const int count=project.selectedModSlotCount();
    // Avoid offering a target already owned by any other V or M route.
    // Current selection remains available for editing older patches.
    const auto availableTargets=[](const Patch& patch,ModTarget current){
        std::vector<ModTarget> available;
        for(int i=static_cast<int>(ModTarget::Cutoff);i<=static_cast<int>(ModTarget::PeakGain);++i){
            const auto target=static_cast<ModTarget>(i);
            bool used=false;
            if(target!=current){
                for(size_t j=0;j<patch.modSlotCount&&j<patch.modSlots.size();++j)
                    used|=patch.modSlots[j].target==target;
                for(size_t j=0;j<patch.velocitySlotCount&&j<patch.velocitySlots.size();++j)
                    used|=patch.velocitySlots[j].target==target;
            }
            if(!used)available.push_back(target);
        }
        return available;
    };
    for(int slot=0;slot<count;++slot){
        const auto rr=modPartRect(slot,1);
        if(!rr.contains(x,y))continue;
        const auto s=project.selectedModSlot(slot,linkSource_);
        const auto available=availableTargets(p,s.target);
        std::vector<std::string> names;
        int selected=0;
        for(size_t i=0;i<available.size();++i){
            names.emplace_back(modTargetName(available[i]));
            if(available[i]==s.target)selected=static_cast<int>(i);
        }
        ui.openDropdown(
            DropdownKind::ModTarget,slot,anchor(rr),selected,names);
        return true;
    }

    return false;
}

bool NativeEditor::applyDropdownChoice(const DropdownChoice& choice){
    auto& project=ProjectCore::instance();
    switch(choice.kind){
        case DropdownKind::FxType:
            if(choice.option<0||choice.option>static_cast<int>(FxType::Reverb))return false;
            fxRackNewType_=static_cast<FxType>(choice.option);
            return false;
        case DropdownKind::Wave:
            if(choice.context<0||choice.context>=6||choice.option<0||
               choice.option>=static_cast<int>(kWaveformLabels.size()))
                return false;
            return project.setSelectedOperatorWave(
                choice.context,static_cast<Wave>(choice.option));

        case DropdownKind::FilterType:
            if(choice.option<0||choice.option>=3)return false;
            return project.setSelectedFilterType(
                static_cast<FilterType>(choice.option));

        case DropdownKind::LfoTarget:
            if(choice.option<0||choice.option>=4)return false;
            return project.setSelectedLfoTarget(
                static_cast<LfoTarget>(choice.option));

        case DropdownKind::ModTarget:{
            if(choice.context<0||choice.context>=project.selectedModSlotCount(linkSource_)||
               choice.option<0)return false;
            const Patch patch=project.selectedPatch();
            const ModTarget current=project.selectedModSlot(choice.context,linkSource_).target;
            std::vector<ModTarget> available;
            for(int i=static_cast<int>(ModTarget::Cutoff);i<=static_cast<int>(ModTarget::PeakGain);++i){
                const auto target=static_cast<ModTarget>(i);
                bool used=false;
                if(target!=current){
                    for(size_t j=0;j<patch.modSlotCount&&j<patch.modSlots.size();++j)
                        used|=patch.modSlots[j].target==target;
                    for(size_t j=0;j<patch.velocitySlotCount&&j<patch.velocitySlots.size();++j)
                        used|=patch.velocitySlots[j].target==target;
                }
                if(!used)available.push_back(target);
            }
            if(static_cast<size_t>(choice.option)>=available.size())return false;
            const auto target=available[static_cast<size_t>(choice.option)];
            const auto rr=modRange(target);
            const float mid=(rr.lo+rr.hi)*0.5f;
            return project.setSelectedModSlot(choice.context,target,mid,mid,linkSource_);
        }

        default:
            return false;
    }
}

std::optional<NativeEditor::ModLinkValueHit> NativeEditor::hitModLinkValue(
    float x,float y) const noexcept {
    // Old numerical link controls are no longer part of this workspace.
    return std::nullopt;
    // Only active, visible M/V numeric fields can open the Android editor.
    if(!viewportRect().contains(x,y)||scrollGutterRect().contains(x,y))
        return std::nullopt;
    const auto hit=hitFx(x,y);
    if(!hit||(hit->kind!=HitKind::ModMin&&hit->kind!=HitKind::ModMax))
        return std::nullopt;
    const auto slot=ProjectCore::instance().selectedModSlot(hit->a,linkSource_);
    if(slot.target==ModTarget::None)return std::nullopt;
    const bool maximum=hit->kind==HitKind::ModMax;
    return ModLinkValueHit{
        hit->a,linkSource_,maximum,slot.target,maximum?slot.max:slot.min};
}

std::optional<int> NativeEditor::hitOperatorRatio(float x,float y) const noexcept {
    if(matrixMode_)return std::nullopt;
    opSurface.view=bodyRect();
    // Do not fall through to retired six-card geometry.
    if(opSurface.hitRatio(x,y))return opSurface.op;
    return std::nullopt;
}

std::optional<int> NativeEditor::hitOperatorSemitone(float x,float y) const noexcept {
    if(matrixMode_)return std::nullopt;
    opSurface.view=bodyRect();
    // Only the visible Base ST field can open the semitone editor.
    // Legacy semitone rows overlap the new Mod and shadow controls.
    if(opSurface.hitSemi(x,y))return opSurface.op;
    return std::nullopt;
}

std::optional<NativeEditor::Hit> NativeEditor::hitSynth(float x,float y) const noexcept {
    for(int i=0;i<2;++i){
        const auto rr=synthTabRect(i);
        if(rr.contains(x,y))
            return Hit{HitKind::SynthTab,i,-1,rr};
    }

    const auto b=bodyRect();
    const auto tab=synthTabRect(0);
    const float gap=std::clamp(b.w*0.014f,9.0f,13.0f);
    const float contentTop=tab.y+tab.h+gap;
    if(y<contentTop)return std::nullopt;

    if(matrixMode_){
        for(int m=0;m<6;++m){
            for(int carrier=0;carrier<6;++carrier){
                const auto rr=matrixRect(m,carrier);
                if(rr.contains(x,y))
                    return Hit{HitKind::Matrix,m,carrier,rr};
            }
        }
        return std::nullopt;
    }

    const Patch p=ProjectCore::instance().selectedPatch();
    for(int op=0;op<6;++op){
        const auto wave=operatorWaveFieldRect(op);
        if(wave.contains(x,y))
            return Hit{HitKind::WaveSelect,op,-1,wave};

        const auto toggle=operatorCardToggleRect(op);
        if(toggle.contains(x,y))
            return Hit{HitKind::OperatorToggle,op,-1,toggle};

        for(int param=0;param<8;++param){
            const auto rr=operatorCardParamRect(op,param);
            if(rr.contains(x,y))
                return Hit{HitKind::OperatorParam,op,param,rr};
        }

        if(p.ops[static_cast<size_t>(op)].wave==Wave::Custom){
            for(int partial=0;partial<16;++partial){
                const auto rr=operatorCardHarmonicRect(op,partial);
                if(rr.contains(x,y))
                    return Hit{HitKind::Harmonic,op,partial,rr};
            }
        }
    }
    return std::nullopt;
}

std::optional<NativeEditor::Hit> NativeEditor::hitFx(float x,float y) const noexcept {
    const auto rack=ProjectCore::instance().fxRack(false);
    for(int i=0;i<rack.count;++i){
        const auto r=fxRackStageRect(i);
        const float bw=std::clamp(r.w*.10f,39.0f,67.0f);
        const float right=r.x+r.w;
        const HitKind actions[4]={
            HitKind::RackUp,HitKind::RackDown,HitKind::RackBypass,HitKind::RackDelete};
        for(int j=0;j<4;++j){
            const Rect cell{right-(4-j)*bw,r.y+5,bw-3,r.h-10};
            if(cell.contains(x,y))return Hit{actions[j],i,-1,cell};
        }
        if(r.contains(x,y))return Hit{HitKind::RackExpand,i,-1,r};
        if(i==fxRackExpanded_){
            for(size_t param=0;param<fxParamCount(rack.chain[i].type);++param){
                const auto pr=fxRackParamRect(i,static_cast<int>(param));
                if(pr.contains(x,y))return Hit{HitKind::RackParameter,i,static_cast<int>(param),pr};
            }
        }
    }
    if(const auto r=fxRackAddRect(true);r.contains(x,y))
        return Hit{HitKind::RackAdd,-1,-1,r};
    // The addable FX view does not expose retired filter / link controls.
    if(fxWorkspace_==3)return std::nullopt;
    const auto card=fxGroupRect(0);
    const Rect editFilter{card.x+card.w-200,card.y+12,180,52};
    if(editFilter.contains(x,y))
        return Hit{HitKind::FilterOpen,0,-1,editFilter};
    const auto filterType=filterTypeRect(0);
    if(!ProjectCore::instance().selectedPatch().filter.evolving.configured&&
        filterType.contains(x,y))
        return Hit{HitKind::FilterType,0,-1,filterType};

    for(int i=0;i<static_cast<int>(kFilterParams.size())&&
        !ProjectCore::instance().selectedPatch().filter.evolving.configured;++i){
        const auto rr=fxSectionParamRect(0,i);
        if(rr.contains(x,y))
            return Hit{
                HitKind::PatchParam,
                static_cast<int>(kFilterParams[static_cast<size_t>(i)]),
                -1,rr};
    }
    for(int button=0;button<2;++button){
        const auto rr=peakToggleRect(button);
        if(rr.contains(x,y))return Hit{button==0?HitKind::PeakToggle:HitKind::PeakMode,0,-1,rr};
    }
    const auto p=ProjectCore::instance().selectedPatch();
    const PatchParam peakParams[3]={
        p.peak.followNote?PatchParam::PeakRatio:PatchParam::PeakFrequency,
        PatchParam::PeakQ,PatchParam::PeakGain};
    for(int row=0;row<3;++row){
        const auto rr=peakParamRect(row);
        if(rr.contains(x,y))
            return Hit{HitKind::PatchParam,static_cast<int>(peakParams[row]),-1,rr};
    }

    for(int i=0;i<static_cast<int>(kAmpParams.size());++i){
        const auto rr=fxSectionParamRect(1,i);
        if(rr.contains(x,y))
            return Hit{
                HitKind::PatchParam,
                static_cast<int>(kAmpParams[static_cast<size_t>(i)]),
                -1,rr};
    }

    const auto target=lfoTargetRect(0);
    if(target.contains(x,y))
        return Hit{HitKind::LfoTarget,0,-1,target};

    for(int i=0;i<static_cast<int>(kLfoParams.size());++i){
        const auto rr=fxSectionParamRect(2,i);
        if(rr.contains(x,y))
            return Hit{
                HitKind::PatchParam,
                static_cast<int>(kLfoParams[static_cast<size_t>(i)]),
                -1,rr};
    }

    auto& project=ProjectCore::instance();
    if(linkSourceRect(LinkSource::Velocity).contains(x,y))
        return Hit{HitKind::LinkSourceV,-1,-1,linkSourceRect(LinkSource::Velocity)};
    if(linkSourceRect(LinkSource::Mod).contains(x,y))
        return Hit{HitKind::LinkSourceM,-1,-1,linkSourceRect(LinkSource::Mod)};
    const int count=project.selectedModSlotCount(linkSource_);
    for(int slot=0;slot<count;++slot){
        const auto targetRect=modPartRect(slot,1);
        if(targetRect.contains(x,y))
            return Hit{HitKind::ModTargetNext,slot,-1,targetRect};

        const auto mn=modPartRect(slot,3);
        if(mn.contains(x,y))
            return Hit{HitKind::ModMin,slot,-1,mn};

        const auto mx=modPartRect(slot,4);
        if(mx.contains(x,y))
            return Hit{HitKind::ModMax,slot,-1,mx};

        const auto del=modPartRect(slot,5);
        if(del.contains(x,y))
            return Hit{HitKind::ModDelete,slot,-1,del};
    }

    const auto add=modAddRect();
    if(add.contains(x,y))
        return Hit{HitKind::ModAdd,-1,-1,add};

    return std::nullopt;
}

bool NativeEditor::applyHit(const Hit& hit,float x,float y){
    auto& project=ProjectCore::instance();
    const float nx=hit.rect.w>0
        ?std::clamp((x-hit.rect.x)/hit.rect.w,0.0f,1.0f)
        :0.5f;
    const float ny=hit.rect.h>0
        ?std::clamp((y-hit.rect.y)/hit.rect.h,0.0f,1.0f)
        :0.5f;

    switch(hit.kind){
        case HitKind::RackExpand:
            fxRackExpanded_=fxRackExpanded_==hit.a?-1:hit.a;
            return false;
        case HitKind::RackUp:
        case HitKind::RackDown:{
            const int shift=hit.kind==HitKind::RackUp?-1:1;
            if(project.moveFxStage(false,hit.a,shift)){
                if(fxRackExpanded_==hit.a)fxRackExpanded_+=shift;
                else if(fxRackExpanded_==hit.a+shift)fxRackExpanded_=hit.a;
                return true;
            }
            return false;
        }
        case HitKind::RackBypass:{
            const Fx rack=project.fxRack(false);
            if(hit.a<0||hit.a>=rack.count)return false;
            return project.setFxStageEnabled(false,hit.a,!rack.chain[hit.a].enabled);
        }
        case HitKind::RackDelete:{
            if(!project.removeFxStage(false,hit.a))return false;
            if(fxRackExpanded_==hit.a)fxRackExpanded_=-1;
            else if(fxRackExpanded_>hit.a)--fxRackExpanded_;
            return true;
        }
        case HitKind::RackParameter:{
            const Fx rack=project.fxRack(false);
            if(hit.a<0||hit.a>=rack.count)return false;
            const auto d=fxParamDesc(rack.chain[hit.a].type,static_cast<size_t>(hit.b));
            const float lw=std::clamp(hit.rect.w*.27f,115.0f,182.0f),vw=71.0f;
            const float range=std::max(24.0f,hit.rect.w-lw-vw-9.0f);
            const float fraction=std::clamp((x-(hit.rect.x+lw))/range,0.0f,1.0f);
            return project.setFxStageParameter(false,hit.a,hit.b,
                d.lo+fraction*(d.hi-d.lo));
        }
        case HitKind::RackAdd:{
            if(!project.addFxStage(false,fxRackNewType_))return false;
            fxRackExpanded_=project.fxRack(false).count-1;
            return true;
        }
        case HitKind::SynthTab:
            matrixMode_=hit.a==1;
            if(matrixMode_)synthScrollY_=0.0f;
            return false;

        case HitKind::OperatorSelect:
            selectedOperator_=std::clamp(hit.a,0,5);
            return false;

        case HitKind::WaveSelect:{
            const int op=std::clamp(hit.a,0,5);
            const auto p=project.selectedPatch();
            const int current=std::clamp(
                static_cast<int>(p.ops[static_cast<size_t>(op)].wave),
                0,static_cast<int>(kWaveformLabels.size())-1);
            return project.setSelectedOperatorWave(
                op,static_cast<Wave>((current+1)%static_cast<int>(kWaveformLabels.size())));
        }

        case HitKind::OperatorToggle:{
            const int op=std::clamp(hit.a,0,5);
            const auto p=project.selectedPatch();
            return project.setSelectedOperatorEnabled(
                op,!p.ops[static_cast<size_t>(op)].enabled);
        }

        case HitKind::OperatorParam:{
            const int op=std::clamp(hit.a,0,5);
            const int paramIndex=std::clamp(hit.b,0,7);
            const auto param=kOperatorParams[static_cast<size_t>(paramIndex)];

            // The left portion of the row is the label in the HTML layout.
            const float labelFraction=30.0f/std::max(1.0f,hit.rect.w);
            const float controlNx=std::clamp(
                (nx-labelFraction)/std::max(0.01f,1.0f-labelFraction),
                0.0f,1.0f);

            float value=denormalized(
                controlNx,operatorRange(param));
            if(param==OperatorParam::Semitone||
               param==OperatorParam::Detune)
                value=std::round(value);
            return project.setSelectedOperatorParam(
                op,param,value);
        }

        case HitKind::Harmonic:
            return project.setSelectedHarmonic(
                std::clamp(hit.a,0,5),
                std::clamp(hit.b,0,15),
                1.0f-ny,
                false);

        case HitKind::Matrix:
            return project.setSelectedMatrixAmount(hit.a,hit.b,nx);

        case HitKind::FxGroup:
            fxGroup_=std::clamp(hit.a,0,3);
            return false;

        case HitKind::FilterOpen:
            filterSurface.open=true;
            filterSurface.view=bodyRect();
            return false;
        case HitKind::FilterType:{
            const auto p=project.selectedPatch();
            const int current=std::clamp(
                static_cast<int>(p.filter.type),0,2);
            return project.setSelectedFilterType(
                static_cast<FilterType>((current+1)%3));
        }

        case HitKind::PeakToggle:{
            const auto p=project.selectedPatch();
            return project.setSelectedPeakEnabled(!p.peak.enabled);
        }
        case HitKind::PeakMode:{
            const auto p=project.selectedPatch();
            return project.setSelectedPeakFollowNote(!p.peak.followNote);
        }

        case HitKind::LfoTarget:{
            const auto p=project.selectedPatch();
            const int current=std::clamp(
                static_cast<int>(p.lfo.target),0,3);
            return project.setSelectedLfoTarget(
                static_cast<LfoTarget>((current+1)%4));
        }

        case HitKind::PatchParam:{
            const auto param=static_cast<PatchParam>(hit.a);
            const float labelFraction=
                std::clamp(120.0f/std::max(1.0f,hit.rect.w),0.12f,0.36f);
            const float valueFraction=
                std::clamp(58.0f/std::max(1.0f,hit.rect.w),0.08f,0.22f);
            const float controlNx=std::clamp(
                (nx-labelFraction)/
                    std::max(0.01f,1.0f-labelFraction-valueFraction),
                0.0f,1.0f);
            return project.setSelectedPatchParam(
                param,denormalized(controlNx,patchRange(param)));
        }

        case HitKind::LinkSourceV:linkSource_=LinkSource::Velocity;return false;
        case HitKind::LinkSourceM:linkSource_=LinkSource::Mod;return false;
        case HitKind::ModTargetPrev:
        case HitKind::ModTargetNext:{
            const auto s=project.selectedModSlot(hit.a,linkSource_);
            const int dir=hit.kind==HitKind::ModTargetNext?1:-1;
            auto target=s.target;
            const auto patch=project.selectedPatch();
            for(int attempt=0;attempt<=static_cast<int>(ModTarget::PeakGain);++attempt){
                target=cycleModTarget(target,dir);
                bool used=false;
                for(size_t j=0;j<patch.modSlotCount&&j<patch.modSlots.size();++j)
                    used|=patch.modSlots[j].target==target;
                for(size_t j=0;j<patch.velocitySlotCount&&j<patch.velocitySlots.size();++j)
                    used|=patch.velocitySlots[j].target==target;
                if(!used)break;
            }
            const auto rr=modRange(target);
            const float mid=(rr.lo+rr.hi)*0.5f;
            return project.setSelectedModSlot(hit.a,target,mid,mid,linkSource_);
        }

        case HitKind::ModMin:
        case HitKind::ModMax:
            // Numeric link values are edited with the OS keyboard rather
            // than an invisible horizontal slider.
            return false;

        case HitKind::ModDelete:
            return project.removeSelectedModSlot(hit.a,linkSource_);

        case HitKind::ModAdd:
            return project.addSelectedModSlot(ModTarget::Cutoff,linkSource_);

        case HitKind::None:
            return false;
    }
    return false;
}

std::optional<PatchTransferAction> NativeEditor::hitPatchTransfer(
    EditorPage page,float x,float y) const noexcept {
    if(page==EditorPage::Fx)return std::nullopt;

    const auto copy=patchTransferRect(page,1);
    if(copy.contains(x,y))return PatchTransferAction::CopyPatch;

    const auto paste=patchTransferRect(page,2);
    if(paste.contains(x,y))return PatchTransferAction::PastePatch;

    return std::nullopt;
}

bool NativeEditor::pointerDown(EditorPage page,float x,float y){
    activePage_=page;
    changed_=false;
    scrolling_=false;
    activeHit_.reset();

    opSurface.view=bodyRect();
    filterSurface.view=bodyRect();
    unisonSurface.view=bodyRect();
    if(page==EditorPage::Fx){
        for(int i=0;i<4;++i)if(fxWorkspaceTabRect(i).contains(x,y)){
            fxWorkspace_=i;fxScrollY_=0;return true;
        }
        if(fxWorkspace_==0||fxWorkspace_==1){
            if(scrollGutterRect().contains(x,y)){
                scrolling_=true;scrollLastY_=y;return true;
            }
            const bool hit=fxWorkspace_==0?
                filterSurface.down(x,y):unisonSurface.down(x,y);
            if(hit){
                activeHit_=Hit{fxWorkspace_==0?HitKind::FilterEditor:
                    HitKind::UnisonEditor,-1,-1,{}};
                return true;
            }
            return true;
        }
        if(fxWorkspace_==2){
            const auto target=lfoWorkspaceTargetRect();
            if(target.contains(x,y)){
                const auto p=ProjectCore::instance().selectedPatch();
                changed_=ProjectCore::instance().setSelectedLfoTarget(
                    static_cast<LfoTarget>((static_cast<int>(p.lfo.target)+1)%4));
                return true;
            }
            for(int i=0;i<4;++i)if(lfoWorkspaceRow(i).contains(x,y)){
                activeHit_=Hit{HitKind::PatchParam,
                    static_cast<int>(kLfoParams[static_cast<size_t>(i)]),
                    -1,lfoWorkspaceRow(i)};
                changed_=applyHit(*activeHit_,x,y);
                return true;
            }
            return true;
        }
    }
    // The spectrum lane is the dedicated vertical scroll control.
    if(scrollGutterRect().contains(x,y)){
        scrolling_=true;
        scrollLastY_=y;
        return true;
    }

     if(page==EditorPage::Synth&&!matrixMode_){
        for(int i=0;i<2;++i){
            const auto tab=synthTabRect(i);
            if(tab.contains(x,y)){matrixMode_=i==1;return true;}
        }
        if(opSurface.down(x,y)){activeHit_=Hit{HitKind::OperatorSelect,-1,-1,{}};return true;}
        // The hidden six-card controls must never receive touches in the
        // new operator editor viewport.
        if(bodyRect().contains(x,y))return true;
    }
    activeHit_=page==EditorPage::Synth
        ?hitSynth(x,y)
        :hitFx(x,y);

    if(activeHit_){
        changed_=applyHit(*activeHit_,x,y)||changed_;
        return true;
    }

    return false;
}

bool NativeEditor::pointerMove(float x,float y){
    if(scrolling_){
        if(activePage_==EditorPage::Synth&&!matrixMode_){
            opSurface.scrollBy(scrollLastY_-y);
        }else if(activePage_==EditorPage::Fx&&fxWorkspace_==0){
            filterSurface.scrollBy(scrollLastY_-y);
        }else if(activePage_==EditorPage::Fx&&fxWorkspace_==1){
            unisonSurface.scrollBy(scrollLastY_-y);
        }else{
            scrollEditor(activePage_,scrollLastY_-y);
        }
        scrollLastY_=y;
        return true;
    }

    if(!activeHit_)return false;
    if(activePage_==EditorPage::Fx&&
       activeHit_->kind==HitKind::FilterEditor){
        filterSurface.move(x,y);return true;
    }
    if(activePage_==EditorPage::Fx&&
       activeHit_->kind==HitKind::UnisonEditor){
        unisonSurface.move(x,y);return true;
    }
    if(activePage_==EditorPage::Synth&&!matrixMode_&&
       activeHit_->kind==HitKind::OperatorSelect){
        opSurface.move(x,y);return true;
    }
    switch(activeHit_->kind){
        case HitKind::OperatorParam:
        case HitKind::Harmonic:
        case HitKind::Matrix:
        case HitKind::PatchParam:
        case HitKind::RackParameter:
            changed_=applyHit(*activeHit_,x,y)||changed_;
            return true;

        default:
            return true;
    }
}

bool NativeEditor::pointerUp(){
    if(activePage_==EditorPage::Fx&&activeHit_&&
       activeHit_->kind==HitKind::FilterEditor)
        changed_=filterSurface.up()||changed_;
    if(activePage_==EditorPage::Fx&&activeHit_&&
       activeHit_->kind==HitKind::UnisonEditor)
        changed_=unisonSurface.up()||changed_;
    if(activePage_==EditorPage::Synth&&!matrixMode_&&activeHit_&&
       activeHit_->kind==HitKind::OperatorSelect)
        changed_=opSurface.up()||changed_;
    const bool changed=changed_;
    activeHit_.reset();
    scrolling_=false;
    changed_=false;
    return changed;
}

void NativeEditor::cancel() noexcept {
    activeHit_.reset();
    scrolling_=false;
    changed_=false;
}

} // namespace aiora
