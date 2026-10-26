// XY expression is independent of note-drawing touch identity and never
// enters project serialization. While recording, samples remain raw until
// the normal finishRollDraw() simplification runs.
void moveXYInput(NativeState& state,float x,float y){
    state.ui.setExpressionFromTouch(x,y);
    auto& draw=state.rollDraw;
    if(draw.active&&!draw.paused){
        draw.velocity=state.ui.expressionVelocity();
        draw.mod=state.ui.expressionMod();
        draw.vTouched=true;draw.mTouched=true;
        captureRollDraw(draw.raw[1],draw.cursor,draw.velocity);
        captureRollDraw(draw.raw[2],draw.cursor,draw.mod);
        showRollDraw(state);
    }
    aiora::AudioEngine::instance().setLiveExpression(
        state.ui.expressionVelocity(),state.ui.expressionMod());
}
