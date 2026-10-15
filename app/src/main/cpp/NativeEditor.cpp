#include "NativeEditor.h"
#include "NativeOverlay.h"

#include <GLES3/gl3.h>

#include <algorithm>
#include <array>
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
    const float h=70.0f;
    const float w=(e.w-gap*3.0f)*0.5f;

    float top=e.y+gap;
    if(page==EditorPage::Synth){
        const auto last=operatorCardRect(5);
        top=last.y+last.h+gap+46.0f;
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

NativeEditor::Rect NativeEditor::fxRackRect() const noexcept {
    const auto b=bodyRect();const float gap=std::clamp(b.w*.014f,9.0f,13.0f);
    const Fx rack=ProjectCore::instance().fxRack(false);
    float height=175;
    for(int i=0;i<rack.count;++i){height+=82;if(i==fxRackExpanded_)height+=64.0f*fxParamCount(rack.chain[i].type);}
    return {b.x+gap,b.y+gap-fxScrollY_,b.w-2*gap,height};
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
    if(page==EditorPage::Synth&&matrixMode_)return 0.0f;

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

void NativeEditor::renderSynth() const noexcept {
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

    // Filter
    drawSection(0);
    {
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
    }
    static constexpr const char* filterLabels[7]={
        "Cutoff","Reso","Env→Cut","A","D","S","R"};
    for(int i=0;i<7;++i)
        drawParam(0,i,filterLabels[i],
            kFilterParams[static_cast<size_t>(i)],
            i<3?kCyan:kOrange);

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
        ov.addText(
            shortValue(s.min),
            mn.x+10.0f,mn.y+31.0f,
            0.82f,overlayColor(kWhite));
        ov.addText(
            shortValue(s.max),
            mx.x+10.0f,mx.y+31.0f,
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
        for(int op=0;op<6;++op){
            const auto rr=operatorWaveFieldRect(op);
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

std::optional<int> NativeEditor::hitOperatorRatio(float x,float y) const noexcept {
    const auto hit=hitSynth(x,y);
    if(!hit||hit->kind!=HitKind::OperatorParam||hit->b!=0)
        return std::nullopt;
    return std::clamp(hit->a,0,5);
}

std::optional<int> NativeEditor::hitOperatorSemitone(float x,float y) const noexcept {
    const auto hit=hitSynth(x,y);
    if(!hit||hit->kind!=HitKind::OperatorParam||hit->b!=1)
        return std::nullopt;
    return std::clamp(hit->a,0,5);
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
    const auto filterType=filterTypeRect(0);
    if(filterType.contains(x,y))
        return Hit{HitKind::FilterType,0,-1,filterType};

    for(int i=0;i<static_cast<int>(kFilterParams.size());++i){
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
        case HitKind::ModMax:{
            const auto s=project.selectedModSlot(hit.a,linkSource_);
            const auto rr=modRange(s.target);
            const float v=denormalized(nx,rr);
            return hit.kind==HitKind::ModMin
                ?project.setSelectedModSlot(hit.a,s.target,v,s.max,linkSource_)
                :project.setSelectedModSlot(hit.a,s.target,s.min,v,linkSource_);
        }

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

    // The spectrum lane is the dedicated vertical scroll control.
    if(scrollGutterRect().contains(x,y)){
        scrolling_=true;
        scrollLastY_=y;
        return true;
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
        scrollEditor(activePage_,scrollLastY_-y);
        scrollLastY_=y;
        return true;
    }

    if(!activeHit_)return false;

    switch(activeHit_->kind){
        case HitKind::OperatorParam:
        case HitKind::Harmonic:
        case HitKind::Matrix:
        case HitKind::PatchParam:
        case HitKind::RackParameter:
        case HitKind::ModMin:
        case HitKind::ModMax:
            changed_=applyHit(*activeHit_,x,y)||changed_;
            return true;

        default:
            return true;
    }
}

bool NativeEditor::pointerUp(){
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
