package com.nexdrum.aiora

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.material3.Text

private const val GRID_LOW = 38
private const val GRID_HIGH = 86

@Composable
fun AioraPitchGrid(
    modifier: Modifier = Modifier,
    selectedLow: Int? = null,
    selectedHigh: Int? = null,
    onPitchDown: (Int) -> Int,
    onPitchUp: (Int) -> Unit
) {
    Column(modifier, verticalArrangement = Arrangement.spacedBy(4.dp)) {
        // Visual top row is the highest 7-note block. Bottom-left remains D2 (MIDI 38).
        for (visualRow in 0 until 7) {
            val block = 6 - visualRow
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                for (column in 0 until 7) {
                    val midi = GRID_LOW + block * 7 + column
                    PitchCell(
                        midi = midi,
                        selected = selectedLow != null && selectedHigh != null &&
                            midi in minOf(selectedLow, selectedHigh)..maxOf(selectedLow, selectedHigh),
                        onPitchDown = onPitchDown,
                        onPitchUp = onPitchUp,
                        modifier = Modifier.weight(1f)
                    )
                }
            }
        }
    }
}

@Composable
private fun PitchCell(
    midi: Int,
    selected: Boolean,
    onPitchDown: (Int) -> Int,
    onPitchUp: (Int) -> Unit,
    modifier: Modifier = Modifier
) {
    val pitchColor = AioraNotation.pitchColor(midi)
    val isD = midi % 12 == 2
    val shape = RoundedCornerShape(8.dp)
    Box(
        modifier
            .aspectRatio(1f)
            .background(if (selected) pitchColor.copy(alpha = 0.42f) else pitchColor.copy(alpha = 0.18f), shape)
            .border(if (selected) 2.dp else 1.dp, pitchColor.copy(alpha = if (selected) 1f else 0.65f), shape)
            .pointerInput(midi) {
                detectTapGestures(
                    onPress = {
                        val voice = onPitchDown(midi)
                        tryAwaitRelease()
                        onPitchUp(voice)
                    }
                )
            },
        contentAlignment = Alignment.Center
    ) {
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            // Temporary text coordinate until the owner-drawn glyph PNGs are installed as native resources.
            Text(
                AioraNotation.pitchCoord(midi),
                color = Color.White,
                fontWeight = if (isD) FontWeight.Bold else FontWeight.Medium
            )
            if (isD) {
                val octave = AioraNotation.dOctave(midi)
                Text(if (octave > 0) "+$octave" else octave.toString(), color = Color(0xFF9AA3B5))
            }
        }
    }
}

fun nativeGridPadIndex(midi: Int): Int {
    val clamped = midi.coerceIn(GRID_LOW, GRID_HIGH)
    val lowToHighBlock = (clamped - GRID_LOW) / 7
    // Native kit storage is high-to-low: crash, ride, bongo, snare, tom, floor tom, kick.
    return 6 - lowToHighBlock
}
