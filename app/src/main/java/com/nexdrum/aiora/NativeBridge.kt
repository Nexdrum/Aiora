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

    external fun projectReset()
    external fun addTrack(drums: Boolean): Int
    external fun deleteTrack(index: Int): Boolean
    external fun selectTrack(index: Int): Boolean
    external fun selectedTrack(): Int
    external fun trackCount(): Int
    external fun trackName(index: Int): String
    external fun setTrackName(index: Int, name: String)
    external fun trackIsDrums(index: Int): Boolean
    external fun trackMute(index: Int): Boolean
    external fun trackSolo(index: Int): Boolean
    external fun trackVolume(index: Int): Float
    external fun trackPan(index: Int): Float
    external fun setTrackMute(index: Int, value: Boolean)
    external fun setTrackSolo(index: Int, value: Boolean)
    external fun setTrackVolume(index: Int, value: Float)
    external fun setTrackPan(index: Int, value: Float)

    external fun loadNexdrumKit(trackIndex: Int): Boolean
    external fun padCount(trackIndex: Int): Int
    external fun selectedPad(trackIndex: Int): Int
    external fun selectPad(trackIndex: Int, padIndex: Int): Boolean
    external fun padIcon(trackIndex: Int, padIndex: Int): String
    external fun padPatchName(trackIndex: Int, padIndex: Int): String
    external fun padCenter(trackIndex: Int, padIndex: Int): Int
    external fun padLow(trackIndex: Int, padIndex: Int): Int
    external fun padHigh(trackIndex: Int, padIndex: Int): Int
    external fun padVolume(trackIndex: Int, padIndex: Int): Float
    external fun padPan(trackIndex: Int, padIndex: Int): Float
    external fun setPadVolume(trackIndex: Int, padIndex: Int, value: Float)
    external fun setPadPan(trackIndex: Int, padIndex: Int, value: Float)
    external fun setPadRange(trackIndex: Int, padIndex: Int, low: Int, high: Int): Boolean

    external fun bpm(): Float
    external fun beats(): Int
    external fun divisions(): Int
    external fun dozenal(): Boolean
    external fun masterVolume(): Float
    external fun masterReverb(): Float
    external fun setBpm(value: Float)
    external fun setSignature(beats: Int, divisions: Int)
    external fun setDozenal(enabled: Boolean)
    external fun setMasterVolume(value: Float)
    external fun setMasterReverb(value: Float)
}
