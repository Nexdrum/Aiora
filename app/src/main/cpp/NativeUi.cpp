#include "NativeUi.h"
#include "NativeEditor.h"
#include "NativeOverlay.h"

#include <GLES3/gl3.h>

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <cmath>

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
constexpr NativeUi::Rgb kOrange{1.0f, 0.6667f, 0.0f};
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

NativeUi::Rgb mix(NativeUi::Rgb a, NativeUi::Rgb b, float amount) noexcept {
    amount = std::clamp(amount, 0.0f, 1.0f);
    return {
        a.r + (b.r - a.r) * amount,
        a.g + (b.g - a.g) * amount,
        a.b + (b.b - a.b) * amount
    };
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

NativeUi::Rect NativeUi::headerScopeRect() const noexcept {
    const float usableW=std::max(0,width_-safeLeft_-safeRight_);
    const float margin=std::clamp(usableW*0.018f,10.0f,18.0f);
    const float gap=std::clamp(usableW*0.010f,7.0f,12.0f);
    const float headerH=std::clamp(usableW*0.142f,94.0f,116.0f);
    const float brand=std::clamp(usableW*0.205f,138.0f,176.0f);
    const float play=std::clamp(headerH*0.78f,64.0f,82.0f);
    const float left=
        static_cast<float>(safeLeft_)+margin+brand+gap+play+gap;
    const float right=
        static_cast<float>(safeLeft_)+usableW-margin;
    return {
        left,
        static_cast<float>(safeTop_)+(headerH-44.0f)*0.5f,
        std::max(80.0f,right-left),
        44.0f
    };
}

NativeUi::Rect NativeUi::headerControlRect(int index) const noexcept {
    if(index!=0)return {};
    const float usableW=std::max(0,width_-safeLeft_-safeRight_);
    const float margin=std::clamp(usableW*0.018f,10.0f,18.0f);
    const float gap=std::clamp(usableW*0.010f,7.0f,12.0f);
    const float headerH=std::clamp(usableW*0.142f,94.0f,116.0f);
    const float brand=std::clamp(usableW*0.205f,138.0f,176.0f);
    const float play=std::clamp(headerH*0.78f,64.0f,82.0f);
    return {
        static_cast<float>(safeLeft_)+margin+brand+gap,
        static_cast<float>(safeTop_)+(headerH-play)*0.5f,
        play,play
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
    if(page_==NativePage::Tracks)return r;
    const float gap=std::max(5.0f,std::min(width_,height_)*0.007f);
    const float h=trackSwitchRect(0).h;
    r.y+=h+gap;
    r.h=std::max(0.0f,r.h-h-gap);
    return r;
}

NativeUi::Rect NativeUi::trackSwitchRect(int part) const noexcept {
    const auto content=contentRect();
    const float gap=std::max(5.0f,std::min(width_,height_)*0.007f);
    const float h=std::clamp(content.w*0.090f,68.0f,82.0f);
    const float prevW=h,nextW=h;

    if(page_==NativePage::Roll){
        const float modeW=h*0.90f;
        const float mainW=std::max(
            90.0f,
            content.w-prevW-nextW-modeW*3.0f-gap*5.0f);
        float x=content.x;
        if(part==0)return {x,content.y,prevW,h};
        x+=prevW+gap;
        if(part==1)return {x,content.y,mainW,h};
        x+=mainW+gap;
        if(part==2)return {x,content.y,nextW,h};
        x+=nextW+gap;
        if(part>=3&&part<=5)
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
    const auto content=contentRect();
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
        case 3: return {card.x+pad,row1,w,64.0f};         // Demo
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

NativeUi::Rect NativeUi::drumKitCardRect() const noexcept {
    const auto view=bodyContentRect();
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

NativeUi::Rect NativeUi::rollModeRect(int index) const noexcept {
    if(index<0||index>2)return {};
    return trackSwitchRect(index+3);
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

void NativeUi::scrollRoll(int pitchDelta, int stepDelta) noexcept {
    const auto columns = rollColumns();
    const auto viewport = rollViewportRect();
    const float cell = rollCellPixels();
    const int visibleCols = std::max(
        1, static_cast<int>(std::floor((viewport.w - rollGutterPixels()) / cell)));
    const int visibleRows = std::max(
        1, static_cast<int>(std::floor((viewport.h - rollHeaderPixels()) / cell)));

    const int maxPitch = std::max(0, static_cast<int>(columns.size()) - visibleCols);
    const int maxStep = std::max(0, rollTotalRows() - visibleRows);
    rollPitchOffset_ = std::clamp(rollPitchOffset_ + pitchDelta, 0, maxPitch);
    rollStepOffset_ = std::clamp(rollStepOffset_ + stepDelta, 0, maxStep);
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
    }
}

bool NativeUi::hitScrollableBody(float x,float y) const noexcept {
    if(page_==NativePage::Tracks)return contentRect().contains(x,y);
    if(page_==NativePage::Drums)return bodyContentRect().contains(x,y);
    return false;
}

NativeUi::Rgb NativeUi::pitchColor(int midi) const noexcept {
    const int pc = ((midi % 12) + 12) % 12;
    return kPitchColors[static_cast<size_t>(pc)];
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

void NativeUi::drawScope() const noexcept {
    const auto r=headerScopeRect();
    if(r.w<56.0f||r.h<20.0f)return;

    std::array<float,AudioEngine::kScopeReadSamples> samples{};
    AudioEngine::instance().copyScope(samples);

    fillRect(r,mix(kTop,kBg,0.38f));
    fillRect({r.x,r.y+r.h*0.5f,r.w,1.0f},mix(kMuted,kTop,0.35f));

    size_t trigger=0;
    const size_t searchEnd=samples.size()/2;
    for(size_t i=1;i<searchEnd;++i){
        if(samples[i-1]<=0.0f&&samples[i]>0.0f){
            trigger=i;
            break;
        }
    }

    const int points=std::clamp(static_cast<int>(r.w/2.0f),24,96);
    float prevY=r.y+r.h*0.5f;
    for(int i=0;i<points;++i){
        const float t=points>1?static_cast<float>(i)/static_cast<float>(points-1):0.0f;
        const size_t available=samples.size()-trigger;
        const size_t idx=trigger+std::min(
            available-1,
            static_cast<size_t>(t*static_cast<float>(available-1)));
        const float sample=std::clamp(samples[idx],-1.0f,1.0f);
        const float y=r.y+r.h*0.5f-sample*(r.h*0.43f);
        const float x=r.x+t*r.w;

        if(i>0){
            const float top=std::min(prevY,y);
            const float bottom=std::max(prevY,y);
            const auto color=pitchColor((i*12)/std::max(1,points));
            fillRect({x-2.0f,top,3.0f,std::max(2.0f,bottom-top)},mix(kTop,color,0.46f));
            fillRect({x-0.75f,top,1.5f,std::max(1.5f,bottom-top)},color);
        }
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
        const RollMode modes[3]{RollMode::Bend,RollMode::Velocity,RollMode::Mod};
        for(int i=0;i<3;++i){
            const auto rr=rollModeRect(i);
            const bool active=rollMode_==modes[i];
            fillRect(rr,active?kCyan:kButton);
            const auto col=overlayColor(active?kBg:kWhite);
            if(i==0){
                const float cx=rr.x+rr.w*0.5f,cy=rr.y+rr.h*0.5f;
                float px=cx-rr.w*0.22f,py=cy;
                for(int s=1;s<=8;++s){
                    const float u=static_cast<float>(s)/8.0f;
                    const float x=cx-rr.w*0.22f+u*rr.w*0.44f;
                    const float y=cy+std::sin(u*6.28318530718f)*rr.h*0.13f;
                    ov.addLine(px,py,x,y,std::max(2.0f,rr.h*0.045f),col);
                    px=x;py=y;
                }
            }else{
                ov.addTextCentered(i==1?"V":"M",{rr.x,rr.y,rr.w,rr.h},
                    std::max(0.85f,rr.h/35.0f),col);
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
            overlay.addPitchGlyph(
                midi%12,
                {rect.x+glyphInset,rect.y+glyphInset,rect.w-glyphInset*2.0f,rect.h-glyphInset*2.0f},
                overlayColor(unavailable?kMuted:(active?mix(color,kWhite,0.30f):color)));
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
                0.90f,overlayColor(kWhite));
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
    const Rect patchField{
        patchCard.x+20.0f,patchCard.y+96.0f,
        patchCard.w-40.0f,56.0f};
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
        0.90f,overlayColor(kWhite));
    const auto aiBtn=trackUtilityRect(1);
    fillRect(aiBtn,kButton);
    overlay.addTextCentered(
        "Tune synth",{aiBtn.x,aiBtn.y,aiBtn.w,aiBtn.h},
        0.86f,overlayColor(kWhite));

    // SONG
    const auto song=songCardRect();
    drawCard(song,"SONG");
    const auto demo=trackUtilityRect(3);
    const auto clear=trackUtilityRect(2);
    const auto save=trackUtilityRect(4);
    const auto load=trackUtilityRect(5);
    const auto exp=projectTransferRect(0);
    const auto imp=projectTransferRect(1);
    for(const auto& rr:{demo,clear,save,load,exp,imp})fillRect(rr,kButton);
    overlay.addTextCentered("Demo",{demo.x,demo.y,demo.w,demo.h},0.92f,overlayColor(kWhite));
    overlay.addTextCentered("Clear trk",{clear.x,clear.y,clear.w,clear.h},0.88f,overlayColor(kWhite));
    overlay.addTextCentered("Save",{save.x,save.y,save.w,save.h},0.92f,overlayColor(kWhite));
    overlay.addTextCentered("Load",{load.x,load.y,load.w,load.h},0.92f,overlayColor(kWhite));
    overlay.addTextCentered("Export",{exp.x,exp.y,exp.w,exp.h},0.92f,overlayColor(kWhite));
    overlay.addTextCentered("Import",{imp.x,imp.y,imp.w,imp.h},0.92f,overlayColor(kWhite));

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
    auto& project=ProjectCore::instance();trackControlChanged_=false;trackActiveSlider_=-1;trackActiveIndex_=-1;

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
        for(int p=0;p<5;++p){
            const auto r=trackPartRect(t,count,p);if(!r.contains(x,y))continue;
            if(p<2){
                const float labelW=std::min(16.0f,r.w*0.18f);
                const float start=r.x+labelW+3.0f;
                const float width=std::max(2.0f,r.w-labelW-7.0f);
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

bool NativeUi::trackPointerMove(float x,float){
    auto& project=ProjectCore::instance();
    if(page_!=NativePage::Tracks||trackActiveSlider_<0)return false;

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
    trackControlChanged_=false;trackActiveSlider_=-1;trackActiveIndex_=-1;
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
            row.x+14.0f,row.y+13.0f,
            46.0f,46.0f};
        ov.addDrumIcon(
            iconIndex,
            {iconR.x,iconR.y,iconR.w,iconR.h},
            overlayColor(pc));

        const float glyphSize=38.0f;
        const float glyphY=row.y+10.0f;
        const float glyphX=row.x+76.0f;
        ov.addPitchGlyph(
            lo%12,
            {glyphX,glyphY,glyphSize,glyphSize},
            overlayColor(kWhite));
        if(lo!=hi){
            ov.addTextCentered(
                "-",
                {glyphX+glyphSize+2.0f,glyphY,22.0f,glyphSize},
                0.95f,overlayColor(kOrange));
            ov.addPitchGlyph(
                hi%12,
                {glyphX+glyphSize+26.0f,glyphY,glyphSize,glyphSize},
                overlayColor(kWhite));
        }

        const float infoX=row.x+176.0f;
        std::string name=drumIconName(iconId);
        std::string range=pitchCoord(lo);
        if(lo!=hi)range+="–"+pitchCoord(hi);
        std::string patch=project.padPatchName(track,i);
        if(patch.size()>25)patch.resize(25);

        ov.addText(
            name,
            infoX,row.y+13.0f,
            0.93f,overlayColor(kWhite));
        ov.addText(
            range+(patch.empty()?std::string{}:" · "+patch),
            infoX,row.y+39.0f,
            0.70f,overlayColor(kMuted));

        const Rect del{
            row.x+row.w-48.0f,
            row.y+12.0f,
            34.0f,34.0f};
        fillRect(del,kButton);
        ov.addTextCentered(
            "×",{del.x,del.y,del.w,del.h},
            0.95f,overlayColor(kWhite));

        // Mirror the HTML per-pad Vol/Pan rows. The selected pad is live-editable;
        // the other rows remain visible so the mixer state is always readable.
        const float vals[2]={
            project.padVolume(track,i),
            (project.padPan(track,i)+1.0f)*0.5f};
        const char* labels[2]={"Vol","Pan"};
        const float sliderTop=row.y+row.h*0.64f;
        const float sliderLeft=row.x+18.0f;
        const float available=row.w-36.0f;
        const float pairGap=24.0f;
        const float pairW=(available-pairGap)*0.5f;
        for(int s=0;s<2;++s){
            const float sx=sliderLeft+s*(pairW+pairGap);
            ov.addText(
                labels[s],sx,sliderTop,
                0.66f,overlayColor(kMuted));
            const float barX=sx+42.0f;
            const float barW=pairW-42.0f;
            const float cy=sliderTop+12.0f;
            fillRect({barX,cy-3.0f,barW,6.0f},kMuted);
            fillRect({
                barX,cy-3.0f,
                barW*std::clamp(vals[s],0.0f,1.0f),6.0f},
                kCyan);
            const float knob=14.0f;
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
        "PAD",card.x+18.0f,card.y+16.0f,
        1.05f,overlayColor(kWhite));
    ov.addText(
        "Note",card.x+18.0f,card.y+47.0f,
        0.88f,overlayColor(kWhite));

    if(track<0||!project.trackIsDrums(track))return;
    const int pad=project.selectedPad(track);
    const int count=project.padCount(track);

    const auto range=drumActionRect(0);
    fillRect(range,drumRangeMode()?mix(kButton,kCyan,0.70f):kButton);
    ov.addTextCentered(
        drumRangeMode()?"Range: on":"Range",
        {range.x,range.y,range.w,range.h},
        0.84f,overlayColor(kWhite));

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
        const float icon=std::min(rr.w,rr.h)*0.46f;
        ov.addDrumIcon(
            i,
            {rr.x+(rr.w-icon)*0.5f,rr.y+5.0f,icon,icon},
            overlayColor(sel?kWhite:kMuted));
        ov.addTextCentered(
            names[i],
            {rr.x+2.0f,rr.y+rr.h-25.0f,rr.w-4.0f,22.0f},
            0.62f,overlayColor(sel?kWhite:kMuted));
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

    auto& project=ProjectCore::instance();
    auto& ov=NativeOverlay::instance();
    const int track=project.selectedTrack();

    const auto kit=drumKitCardRect();
    fillRect(kit,kButton);
    fillRect({
        kit.x+2.0f,kit.y+2.0f,
        kit.w-4.0f,kit.h-4.0f},kPanel);
    ov.addText(
        "DRUM KIT",kit.x+18.0f,kit.y+16.0f,
        1.05f,overlayColor(kWhite));
    ov.addText(
        "Kit",kit.x+18.0f,kit.y+54.0f,
        0.90f,overlayColor(kWhite));

    const Rect kitField{
        kit.x+72.0f,kit.y+42.0f,
        kit.w-90.0f,42.0f};
    fillRect(kitField,kRollBg);
    ov.addText(
        (track>=0&&project.trackIsDrums(track))
            ?"Nexdrum":"Drum kit",
        kitField.x+12.0f,kitField.y+11.0f,
        0.92f,overlayColor(kWhite));

    drawPadQuick();

    const auto add=drumActionRect(1);
    fillRect(add,kButton);
    ov.addTextCentered(
        "+ Add Pad",{add.x,add.y,add.w,add.h},
        0.96f,overlayColor(kWhite));

    drawDrumEditor();
    drawGrid();
}

bool NativeUi::drumPointerDown(float x,float y){
    if(page_!=NativePage::Drums)return false;
    auto& project=ProjectCore::instance();const int track=project.selectedTrack();
    if(track<0||!project.trackIsDrums(track))return false;
    drumControlChanged_=false;drumActiveSlider_=-1;

    const int initialCount=project.padCount(track);
    for(int p=0;p<initialCount;++p){
        const auto row=padQuickRect(p,initialCount);
        const Rect del{
            row.x+row.w-48.0f,
            row.y+12.0f,
            34.0f,34.0f};
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

bool NativeUi::drumPointerMove(float x,float){
    if(page_!=NativePage::Drums||drumActiveSlider_<0)return false;
    auto& project=ProjectCore::instance();const int track=project.selectedTrack();
    if(track<0||!project.trackIsDrums(track))return false;
    const int pad=project.selectedPad(track);if(pad<0||pad>=project.padCount(track))return false;
    const auto r=drumSliderRect(drumActiveSlider_);
    const float start=r.x+42.0f;
    const float width=std::max(1.0f,r.w-42.0f);
    const float n=std::clamp((x-start)/width,0.0f,1.0f);
    if(drumActiveSlider_==0)project.setPadVolume(track,pad,n);else project.setPadPan(track,pad,n*2.0f-1.0f);
    drumControlChanged_=true;return true;
}

bool NativeUi::drumPointerUp(){
    const bool changed=drumControlChanged_;
    drumControlChanged_=false;drumActiveSlider_=-1;
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
    auto& project = ProjectCore::instance();
    const auto content = bodyContentRect();
    fillRect(content, kPanel);

    const int track = project.selectedTrack();
    if (track < 0) return;

    const auto viewport = rollViewportRect();
    fillRect(viewport, kRollBg);

    const auto columns = rollColumns();
    const float cell = rollCellPixels();
    const float gutter = rollGutterPixels();
    const float header = rollHeaderPixels();
    const int visibleCols = std::max(
        1, static_cast<int>(std::floor((viewport.w - gutter) / cell)));
    const int visibleRows = std::max(
        1, static_cast<int>(std::floor((viewport.h - header) / cell)));

    const int pitchOffset = std::clamp(
        rollPitchOffset_, 0, std::max(0, static_cast<int>(columns.size()) - visibleCols));
    const int stepOffset = std::clamp(
        rollStepOffset_, 0, std::max(0, rollTotalRows() - visibleRows));

    fillRect({viewport.x, viewport.y, gutter - 1.0f, header - 1.0f}, kPanel);

    for (int c = 0; c < visibleCols && pitchOffset + c < static_cast<int>(columns.size()); ++c) {
        const int midi = columns[static_cast<size_t>(pitchOffset + c)];
        const float x = viewport.x + gutter + c * cell;
        fillRect({x, viewport.y, cell - 1.0f, header - 2.0f}, kPanel);
        const auto pcColor=pitchColor(midi);
        fillRect({x, viewport.y + header - 4.0f, cell - 1.0f, 3.0f}, pcColor);
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
                const float iconS=std::min(cell*0.50f,header*0.40f);
                rollOv.addDrumIcon(
                    iconIndex,
                    {x+(cell-iconS)*0.5f,viewport.y+4.0f,iconS,iconS},
                    overlayColor(pcColor));
            }
            const float glyphS=std::min(cell*0.62f,header*0.42f);
            rollOv.addPitchGlyph(
                midi%12,
                {x+(cell-glyphS)*0.5f,viewport.y+header*0.48f,glyphS,glyphS},
                overlayColor(pcColor));
        }else{
            rollOv.addPitchGlyph(
                midi%12,
                {x+cell*0.19f,viewport.y+3.0f,cell*0.62f,std::min(cell*0.62f,header*0.68f)},
                overlayColor(pcColor));
        }
        if (((midi % 12) + 12) % 12 == 2) {
            const int octave=(midi-62)/12;
            const std::string label=octave>0?("+"+std::to_string(octave)):std::to_string(octave);
            NativeOverlay::instance().addText(
                label,
                x+cell*0.61f,
                viewport.y+header*(project.trackIsDrums(track)?0.82f:0.66f),
                0.68f,
                overlayColor(kMuted));
        }
    }

    const int beatLen = std::max(1, project.divisions());
    const int barLen = std::max(1, project.beats()) * beatLen;

    for (int r = 0; r < visibleRows; ++r) {
        const int step = stepOffset + r;
        const float y = viewport.y + header + r * cell;
        const Rgb rowColor =
            step % barLen == 0 ? kBar :
            step % beatLen == 0 ? kBeat : kCell;

        fillRect({viewport.x, y, gutter - 2.0f, cell - 1.0f}, rowColor);
        if(step%barLen==0){
            const int bar=step/barLen+1;
            NativeOverlay::instance().addText(
                "b"+std::to_string(bar),
                viewport.x+4.0f,
                y+cell*0.31f,
                0.78f,
                overlayColor(kWhite));
        }
        for (int c = 0; c < visibleCols && pitchOffset + c < static_cast<int>(columns.size()); ++c) {
            const float x = viewport.x + gutter + c * cell;
            fillRect({x, y, cell - 1.0f, cell - 1.0f}, rowColor);
        }
    }

    const int notes = project.noteCount(track);
    for (int n = 0; n < notes; ++n) {
        const int midi = project.noteMidi(track, n);
        const auto it = std::lower_bound(columns.begin(), columns.end(), midi);
        if (it == columns.end() || *it != midi) continue;
        const int colIndex = static_cast<int>(std::distance(columns.begin(), it));
        const int visibleCol = colIndex - pitchOffset;
        if (visibleCol < 0 || visibleCol >= visibleCols) continue;

        const float start = project.noteStart(track, n);
        const float length = std::max(1.0f, project.noteLength(track, n));
        if (start + length <= stepOffset || start >= stepOffset + visibleRows) continue;

        const float x = viewport.x + gutter + visibleCol * cell + 1.0f;
        const float y = viewport.y + header + (start - stepOffset) * cell + 1.0f;
        const float h = std::max(cell, length * cell) - 2.0f;
        fillRect({x, y, cell - 3.0f, h}, pitchColor(midi));
        fillRect({x, y + h - 3.0f, cell - 3.0f, 3.0f}, kTop);

        if (rollMode_ == RollMode::Notes) continue;
        const int kind = static_cast<int>(rollMode_) - 1;
        const int points = project.curvePointCount(track, n, kind);
        const Rgb pointColor =
            rollMode_ == RollMode::Bend ? kCyan :
            rollMode_ == RollMode::Velocity ? kWhite : kPurple;

        auto& ov=NativeOverlay::instance();
        const float center=x+(cell-3.0f)*0.5f;
        float prevLX=0.0f,prevRX=0.0f,prevY=0.0f;
        bool havePrev=false;
        for (int p = 0; p < points; ++p) {
            const float relStep = project.curvePointStep(track, n, kind, p);
            const float value = project.curvePointValue(track, n, kind, p);
            const bool free = project.curvePointFree(track, n, kind, p);
            const float py = viewport.y + header + (start + relStep + 0.5f - stepOffset) * cell;
            if (py < viewport.y + header-cell || py > viewport.y + viewport.h+cell) continue;

            const float radius=free?std::max(7.0f,cell*0.13f):std::max(6.0f,cell*0.11f);
            const float thick=std::max(2.0f,cell*0.045f);
            if (rollMode_ == RollMode::Bend) {
                const float px=center+value*cell;
                if(havePrev)ov.addLine(prevLX,prevY,px,py,thick,overlayColor(pointColor));
                ov.addCircle(px,py,radius,thick,overlayColor(pointColor));
                if(free){
                    ov.addLine(px-radius,py,px,py-radius,thick,overlayColor(pointColor));
                    ov.addLine(px,py-radius,px+radius,py,thick,overlayColor(pointColor));
                    ov.addLine(px+radius,py,px,py+radius,thick,overlayColor(pointColor));
                    ov.addLine(px,py+radius,px-radius,py,thick,overlayColor(pointColor));
                }
                prevLX=px;
            } else {
                const float half = std::clamp(value, 0.0f, 1.0f) * (cell * 0.48f);
                const float lx=center-half,rx=center+half;
                if(havePrev){
                    ov.addLine(prevLX,prevY,lx,py,thick,overlayColor(pointColor));
                    ov.addLine(prevRX,prevY,rx,py,thick,overlayColor(pointColor));
                }
                ov.addCircle(lx,py,radius,thick,overlayColor(pointColor));
                ov.addCircle(rx,py,radius,thick,overlayColor(pointColor));
                prevLX=lx;prevRX=rx;
            }
            prevY=py;havePrev=true;
        }
    }
    if(AudioEngine::instance().transportPlaying()){
        const int ph=AudioEngine::instance().playheadStep();
        if(ph>=stepOffset && ph<stepOffset+visibleRows){
            const float py=viewport.y+header+(ph-stepOffset)*cell;
            fillRect({viewport.x+gutter,py,std::max(0.0f,viewport.w-gutter),2.0f},kCyan);
        }
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
    const float headerH=std::clamp(usableW*0.118f,80.0f,102.0f);
    fillRect({
        static_cast<float>(safeLeft_),
        static_cast<float>(safeTop_),
        usableW,headerH},kTop);
    auto& overlay=NativeOverlay::instance();
    const float margin=std::clamp(usableW*0.018f,10.0f,18.0f);
    const float logoSize=std::clamp(headerH*0.62f,48.0f,64.0f);
    const NativeOverlay::Rect logoR{
        static_cast<float>(safeLeft_)+margin,
        static_cast<float>(safeTop_)+(headerH-logoSize)*0.5f,
        logoSize,logoSize};
    overlay.addLogo(
        {logoR.x-3.0f,logoR.y,logoR.w,logoR.h},
        overlayColor(kPurple,0.52f));
    overlay.addLogo(
        {logoR.x+3.0f,logoR.y+1.0f,logoR.w,logoR.h},
        overlayColor(kOrange,0.48f));
    overlay.addLogo(
        {logoR.x,logoR.y-2.0f,logoR.w,logoR.h},
        overlayColor(kCyan,0.68f));
    overlay.addLogo(logoR,overlayColor(kWhite,0.88f));
    overlay.addText(
        "A I O R A",
        static_cast<float>(safeLeft_)+margin+logoSize+10.0f,
        static_cast<float>(safeTop_)+headerH*0.36f,
        1.02f,overlayColor(kWhite));

    auto& project=ProjectCore::instance();
    auto& audio=AudioEngine::instance();
    const auto playR=headerControlRect(0);
    fillRect(playR,audio.transportPlaying()?kOrange:kGreen);
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
    drawScope();

    for (int i = 0; i < 6; ++i) {
        const auto page = static_cast<NativePage>(i);
        const auto nr=navRect(i);
        fillRect(nr,page==page_?kCyan:kButton);
        const float inset=std::max(3.0f,nr.h*0.08f);
        overlay.addNavIcon(
            i,
            {nr.x+inset,nr.y+inset,nr.w-inset*2.0f,nr.h-inset*2.0f},
            overlayColor(page==page_?kBg:kWhite));
    }

    if(page_!=NativePage::Tracks)drawTrackSwitchBar();

    const auto pageClip=page_==NativePage::Tracks?contentRect():bodyContentRect();
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
    glDisable(GL_SCISSOR_TEST);
    NativeOverlay::instance().flush();
}

std::optional<HeaderAction> NativeUi::hitHeader(float x,float y) const noexcept {
    if(headerControlRect(0).contains(x,y))return HeaderAction::TransportToggle;
    return std::nullopt;
}

std::optional<NativePage> NativeUi::hitNav(float x, float y) const noexcept {
    for (int i = 0; i < 6; ++i) {
        if (navRect(i).contains(x, y)) {
            return static_cast<NativePage>(i);
        }
    }
    return std::nullopt;
}

std::optional<TrackSwitchAction> NativeUi::hitTrackSwitch(float x,float y) const noexcept {
    if(page_==NativePage::Tracks)return std::nullopt;
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

std::optional<TrackUtilityAction> NativeUi::hitTrackUtility(float x,float y) const noexcept {
    if(page_!=NativePage::Tracks)return std::nullopt;
    if(trackUtilityRect(0).contains(x,y))return TrackUtilityAction::DozenalToggle;
    if(trackUtilityRect(1).contains(x,y))return TrackUtilityAction::AiFromClipboard;
    if(trackUtilityRect(2).contains(x,y))return TrackUtilityAction::ClearTrack;
    if(trackUtilityRect(3).contains(x,y))return TrackUtilityAction::Demo;
    if(trackUtilityRect(4).contains(x,y))return TrackUtilityAction::SaveProject;
    if(trackUtilityRect(5).contains(x,y))return TrackUtilityAction::LoadProject;
    return std::nullopt;
}

std::optional<ProjectTransferAction> NativeUi::hitProjectTransfer(float x,float y) const noexcept {
    if(page_!=NativePage::Tracks)return std::nullopt;
    if(projectTransferRect(0).contains(x,y))return ProjectTransferAction::CopyProject;
    if(projectTransferRect(1).contains(x,y))return ProjectTransferAction::PasteProject;
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

std::optional<RollMode> NativeUi::hitRollMode(float x, float y) const noexcept {
    if(page_!=NativePage::Roll)return std::nullopt;
    const RollMode modes[3]{RollMode::Bend,RollMode::Velocity,RollMode::Mod};
    for(int i=0;i<3;++i)if(rollModeRect(i).contains(x,y))return modes[i];
    return std::nullopt;
}

bool NativeUi::hitRollPitchHeader(float x,float y) const noexcept {
    if(page_!=NativePage::Roll)return false;
    const auto viewport=rollViewportRect();
    const float gutter=rollGutterPixels();
    const float header=rollHeaderPixels();
    return x>=viewport.x+gutter && x<viewport.x+viewport.w &&
           y>=viewport.y && y<viewport.y+header;
}

bool NativeUi::hitRollBeatGutter(float x,float y) const noexcept {
    if(page_!=NativePage::Roll)return false;
    const auto viewport=rollViewportRect();
    const float gutter=rollGutterPixels();
    const float header=rollHeaderPixels();
    return x>=viewport.x && x<viewport.x+gutter &&
           y>=viewport.y+header && y<viewport.y+viewport.h;
}

bool NativeUi::hitRollNoteArea(float x,float y) const noexcept {
    if(page_!=NativePage::Roll)return false;
    const auto viewport=rollViewportRect();
    const float gutter=rollGutterPixels();
    const float header=rollHeaderPixels();
    return x>=viewport.x+gutter && x<viewport.x+viewport.w &&
           y>=viewport.y+header && y<viewport.y+viewport.h;
}

std::optional<RollCellHit> NativeUi::hitRollCell(float x, float y) const noexcept {
    if (page_ != NativePage::Roll) return std::nullopt;

    const auto viewport = rollViewportRect();
    const float cell = rollCellPixels();
    const float gutter = rollGutterPixels();
    const float header = rollHeaderPixels();
    if (x < viewport.x + gutter || y < viewport.y + header ||
        x >= viewport.x + viewport.w || y >= viewport.y + viewport.h) {
        return std::nullopt;
    }

    const auto columns = rollColumns();
    const int visibleCol = static_cast<int>(std::floor((x - viewport.x - gutter) / cell));
    const int visibleRow = static_cast<int>(std::floor((y - viewport.y - header) / cell));
    const int colIndex = rollPitchOffset_ + visibleCol;
    const int step = rollStepOffset_ + visibleRow;
    if (visibleCol < 0 || visibleRow < 0 ||
        colIndex < 0 || colIndex >= static_cast<int>(columns.size()) ||
        step < 0 || step >= rollTotalRows()) {
        return std::nullopt;
    }

    const float cellLeft = viewport.x + gutter + visibleCol * cell;
    const float cellTop = viewport.y + header + visibleRow * cell;
    return RollCellHit{
        columns[static_cast<size_t>(colIndex)],
        step,
        std::clamp((x - cellLeft) / cell, 0.0f, 1.0f),
        std::clamp((y - cellTop) / cell, 0.0f, 1.0f)
    };
}

bool NativeUi::rollAutomationPosition(
    float x,float y,int noteIndex,int curveKind,bool free,
    float& step,float& value) const noexcept {

    auto& project=ProjectCore::instance();
    const int track=project.selectedTrack();
    if(track<0||noteIndex<0||noteIndex>=project.noteCount(track))return false;

    const auto viewport=rollViewportRect();
    const float cell=rollCellPixels();
    const float gutter=rollGutterPixels();
    const float header=rollHeaderPixels();
    const auto columns=rollColumns();

    const int midi=project.noteMidi(track,noteIndex);
    const auto it=std::lower_bound(columns.begin(),columns.end(),midi);
    if(it==columns.end()||*it!=midi)return false;
    const int colIndex=static_cast<int>(std::distance(columns.begin(),it));
    const int visibleCol=colIndex-rollPitchOffset_;
    const float baseX=viewport.x+gutter+(visibleCol+0.5f)*cell;

    const float start=project.noteStart(track,noteIndex);
    const float length=std::max(1.0f,project.noteLength(track,noteIndex));
    const float maxStep=std::max(0.0f,length-1.0f);

    if(free){
        const float absolute=
            rollStepOffset_+
            (y-viewport.y-header)/cell-0.5f;
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
            value=std::clamp((x-baseX)/cell,-12.0f,12.0f);
        }else{
            const auto hit=hitRollCell(x,y);
            if(!hit)return false;
            value=std::clamp(
                static_cast<float>(hit->midi-midi),
                -12.0f,12.0f);
        }
    }else{
        value=std::clamp(
            std::fabs(x-baseX)/(cell*0.48f),
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
    const float cell=rollCellPixels();
    const float gutter=rollGutterPixels();
    const float header=rollHeaderPixels();
    const auto columns=rollColumns();
    const float radius=std::max(20.0f,cell*0.42f);
    const float radius2=radius*radius;

    int bestNote=-1,bestPoint=-1;
    float bestDist=radius2;
    float bestStep=0.0f,bestValue=0.0f;
    bool bestFree=false;

    const int noteCount=project.noteCount(track);
    for(int n=0;n<noteCount;++n){
        const int midi=project.noteMidi(track,n);
        const auto it=std::lower_bound(columns.begin(),columns.end(),midi);
        if(it==columns.end()||*it!=midi)continue;
        const int visibleCol=
            static_cast<int>(std::distance(columns.begin(),it))-rollPitchOffset_;
        const float baseX=viewport.x+gutter+(visibleCol+0.5f)*cell;
        const float start=project.noteStart(track,n);

        const int points=project.curvePointCount(track,n,curveKind);
        for(int p=0;p<points;++p){
            const float rel=project.curvePointStep(track,n,curveKind,p);
            const float v=project.curvePointValue(track,n,curveKind,p);
            const float py=
                viewport.y+header+
                (start+rel+0.5f-rollStepOffset_)*cell;

            if(curveKind==0){
                const float px=baseX+v*cell;
                const float dx=px-x,dy=py-y,dd=dx*dx+dy*dy;
                if(dd<=bestDist){
                    bestDist=dd;bestNote=n;bestPoint=p;
                    bestStep=rel;bestValue=v;
                    bestFree=project.curvePointFree(track,n,curveKind,p);
                }
            }else{
                const float half=std::clamp(v,0.0f,1.0f)*(cell*0.48f);
                for(float px:{baseX-half,baseX+half}){
                    const float dx=px-x,dy=py-y,dd=dx*dx+dy*dy;
                    if(dd<=bestDist){
                        bestDist=dd;bestNote=n;bestPoint=p;
                        bestStep=rel;bestValue=v;
                        bestFree=project.curvePointFree(track,n,curveKind,p);
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

    int note=-1;
    float noteDistance=1.0e9f;
    for(int n=0;n<noteCount;++n){
        const float start=project.noteStart(track,n);
        const float length=std::max(1.0f,project.noteLength(track,n));
        if(static_cast<float>(cellHit->step)<start||
           static_cast<float>(cellHit->step)>start+length-1.0f)continue;
        const float d=std::fabs(
            static_cast<float>(project.noteMidi(track,n)-cellHit->midi));
        const float limit=curveKind==0?3.0f:1.0f;
        if(d<=limit&&d<noteDistance){
            noteDistance=d;note=n;
        }
    }
    if(note<0)return std::nullopt;

    float step=0.0f,value=0.0f;
    if(!rollAutomationPosition(
        x,y,note,curveKind,false,step,value))return std::nullopt;
    return RollAutomationHit{note,-1,step,value,false};
}

int NativeUi::padIndexForMidi(int midi) noexcept {
    const int clamped = std::clamp(midi, kGridLow, kGridHigh);
    const int lowToHighBlock = (clamped - kGridLow) / 7;
    return 6 - lowToHighBlock;
}

} // namespace aiora
