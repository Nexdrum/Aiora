package com.nexdrum.aiora

import androidx.compose.ui.graphics.Color

object AioraNotation {
    private const val DOZENAL = "0123456789XE"

    val pitchColors = listOf(
        Color(0xFF3AFF00), Color(0xFF00FFEC), Color(0xFF008FFF), Color(0xFF0F00FB),
        Color(0xFF6300BE), Color(0xFF6E0080), Color(0xFF980000), Color(0xFFC80000),
        Color(0xFFF30000), Color(0xFFFF7800), Color(0xFFFFEF00), Color(0xFFAAFF00)
    )

    fun pitchColor(midi: Int): Color = pitchColors[((midi % 12) + 12) % 12]

    /** D4 = 0, matching AIORA rather than scientific octave numbering. */
    fun dOctave(midi: Int): Int = (midi - 62) / 12

    /** Sign-magnitude dozenal coordinate centered on D4 = 0. */
    fun pitchCoord(midi: Int): String {
        val delta = midi - 62
        val negative = delta < 0
        val magnitude = kotlin.math.abs(delta)
        val whole = magnitude / 12
        val fraction = magnitude % 12
        return buildString {
            if (negative) append('-')
            append(DOZENAL[whole.coerceIn(0, DOZENAL.lastIndex)])
            if (fraction != 0) {
                append('.')
                append(DOZENAL[fraction])
            }
        }
    }

    fun dozenalInt(value: Int): String {
        if (value == 0) return "0"
        val negative = value < 0
        var n = kotlin.math.abs(value)
        val out = StringBuilder()
        while (n > 0) {
            out.append(DOZENAL[n % 12])
            n /= 12
        }
        if (negative) out.append('-')
        return out.reverse().toString()
    }
}
