package com.nexdrum.aiora

object NativeBridge {
    init { System.loadLibrary("aiora") }

    external fun startAudio(): Boolean
    external fun stopAudio()
    external fun noteOn(midi: Int, velocity: Float): Int
    external fun noteOnPad(padIndex: Int, midi: Int, velocity: Float): Int
    external fun noteOff(voiceId: Int)
    external fun panic()
    external fun setFactoryPreset(index: Int)
    external fun factoryPreset(): Int
    external fun sampleRate(): Int
}
