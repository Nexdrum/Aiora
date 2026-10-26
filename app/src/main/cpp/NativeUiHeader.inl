// Header and navigation are a single coordinated spectral UI shell.
drawExpressionPad();
const auto transport=transportShellRect();
fillRect(transport,mix(kTop,kPanel,.55f));
auto& overlay=NativeOverlay::instance();
overlay.addRect({transport.x,transport.y,transport.w,transport.h},
                overlayColor(kPurple,.75f));
const float logoSize=std::clamp(transport.h*.18f,28.f,58.f);
const NativeOverlay::Rect logoR{
    transport.x+transport.w*.11f,transport.y+transport.h*.065f,
    logoSize,logoSize};
overlay.addSpectrumLogo(logoR);
overlay.addText("A I O R A",logoR.x+logoSize+9.f,
                logoR.y+logoSize*.25f,.92f,overlayColor(kWhite));
auto& project=ProjectCore::instance();
auto& audio=AudioEngine::instance();
const auto startR=headerControlRect(0),playR=headerControlRect(1),
           endR=headerControlRect(2);
const auto bezel=[&](Rect r,Rgb accent){
    fillRect(r,accent);
    fillRect({r.x+3,r.y+3,std::max(1.f,r.w-6),
             std::max(1.f,r.h-6)},kButton);
};
bezel(startR,kBlue);
bezel(playR,audio.transportPlaying()?kOrange:kGreen);
bezel(endR,kPurple);
overlay.addTextCentered("|<",{startR.x,startR.y,startR.w,startR.h},
                        1.32f,overlayColor(kWhite));
if(audio.transportPlaying()){
    const float a=playR.h*.28f;
    overlay.addRect({playR.x+(playR.w-a)*.5f,playR.y+(playR.h-a)*.5f,a,a},
                    overlayColor(kGreen));
}else overlay.addTextCentered("▶",{playR.x,playR.y,playR.w,playR.h},
                              1.65f,overlayColor(kWhite));
overlay.addTextCentered(">|",{endR.x,endR.y,endR.w,endR.h},
                        1.32f,overlayColor(kWhite));
drawScope();
static constexpr std::array<Rgb,7> accents{
    kCyan,kOrange,kPurple,kRed,kBlue,kGreen,kGold};
for(int i=0;i<7;++i){
    const auto nr=navRect(i);
    const bool active=kNavPages[static_cast<size_t>(i)]==page_;
    const auto accent=accents[static_cast<size_t>(i)];
    fillRect(nr,active?accent:mix(kButton,accent,.54f));
    const float rim=active?3.f:2.f;
    fillRect({nr.x+rim,nr.y+rim,std::max(1.f,nr.w-2*rim),
              std::max(1.f,nr.h-2*rim)},
             active?mix(kPanel,accent,.26f):kButton);
    const float inset=std::max(5.f,std::min(nr.w,nr.h)*.18f);
    overlay.addNavIcon(kNavIconIndices[static_cast<size_t>(i)],
                       {nr.x+inset,nr.y+inset,
                        nr.w-2*inset,nr.h-2*inset},
                       overlayColor(active?accent:kWhite));
}
