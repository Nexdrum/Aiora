// Persistent spectral XY performance controller: no allocation in draw.
void NativeUi::drawExpressionPad() const noexcept {
    const auto box=expressionShellRect(),p=expressionPadRect();
    auto& o=NativeOverlay::instance();
    fillRect(box,mix(kTop,kPanel,.65f));
    o.addRect({box.x,box.y,box.w,box.h},overlayColor(kCyan,.82f));
    o.addText("EXPRESSION X / Y",box.x+10,box.y+8,.84f,overlayColor(kWhite));
    fillRect({p.x-3,p.y-3,p.w+6,p.h+6},mix(kCyan,kPurple,.43f));
    fillRect(p,mix(kRollBg,kPanel,.16f));
    for(int i=1;i<5;++i){
        const float t=i/5.f;
        fillRect({p.x+p.w*t,p.y,1.3f,p.h},mix(kPanel,kCyan,.52f));
        fillRect({p.x,p.y+p.h*t,p.w,1.3f},mix(kPanel,kPurple,.48f));
    }
    const float x=p.x+p.w*expressionV_,y=p.y+p.h*expressionM_;
    o.addLine(x,p.y,x,p.y+p.h,1.5f,overlayColor(kCyan,.72f));
    o.addLine(p.x,y,p.x+p.w,y,1.5f,overlayColor(kPurple,.72f));
    o.addFilledCircle(x,y,19,overlayColor(kCyan,.15f));
    o.addFilledCircle(x,y,11,overlayColor(kPurple,.88f));
    o.addFilledCircle(x,y,5.5f,overlayColor(kWhite));
    o.addText("V "+std::to_string(static_cast<int>(std::lround(expressionV_*100))),
        box.x+12,p.y+p.h+10,.90f,overlayColor(kCyan));
    o.addText("M "+std::to_string(static_cast<int>(std::lround(expressionM_*100))),
        box.x+box.w*.55f,p.y+p.h+10,.90f,overlayColor(kPurple));
}
