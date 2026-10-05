package com.nexdrum.aiora

import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp

@Composable
fun NativeDrumsView(revision: Int, onChanged: () -> Unit) {
    val track = NativeBridge.selectedTrack()
    if (track < 0 || NativeBridge.trackCount() == 0) {
        Text("Add a track first.", color = Color(0xFF9AA3B5))
        return
    }
    if (!NativeBridge.trackIsDrums(track)) {
        Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("${NativeBridge.trackName(track)} is a melodic track.", color = Color(0xFF9AA3B5))
            Button(onClick = { NativeBridge.loadNexdrumKit(track); onChanged() }) { Text("Load Nexdrum kit") }
        }
        return
    }

    val count = NativeBridge.padCount(track)
    var selected by remember(track, revision) {
        mutableIntStateOf(NativeBridge.selectedPad(track).coerceIn(0, (count - 1).coerceAtLeast(0)))
    }
    var rangeMode by remember(track, selected) { mutableStateOf(false) }
    var rangeStart by remember(track, selected) { mutableStateOf<Int?>(null) }
    var volume by remember(track, selected, revision) { mutableFloatStateOf(if (count > 0) NativeBridge.padVolume(track, selected) else 1f) }
    var pan by remember(track, selected, revision) { mutableFloatStateOf(if (count > 0) NativeBridge.padPan(track, selected) else 0f) }

    if (count == 0) {
        Text("This drum track has no pads yet.", color = Color(0xFF9AA3B5))
        return
    }

    Column(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Text(NativeBridge.trackName(track), style = MaterialTheme.typography.titleMedium)
        Row(
            Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
            horizontalArrangement = Arrangement.spacedBy(6.dp)
        ) {
            repeat(count) { i ->
                val icon = NativeBridge.padIcon(track, i)
                FilterChip(
                    selected = selected == i,
                    onClick = {
                        selected = i
                        NativeBridge.selectPad(track, i)
                        rangeMode = false
                        rangeStart = null
                        onChanged()
                    },
                    label = { Text(icon.replaceFirstChar { it.uppercase() }) }
                )
            }
        }

        val low = NativeBridge.padLow(track, selected)
        val high = NativeBridge.padHigh(track, selected)
        val provisionalLow = rangeStart?.let { minOf(it, low) } ?: low
        val provisionalHigh = rangeStart?.let { maxOf(it, high) } ?: high
        Text(
            "${NativeBridge.padIcon(track, selected).replaceFirstChar { it.uppercase() }}  " +
                "${AioraNotation.pitchCoord(low)} — ${AioraNotation.pitchCoord(high)}",
            color = Color(0xFF9AA3B5)
        )
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(onClick = {
                rangeMode = !rangeMode
                rangeStart = null
            }) { Text(if (rangeMode) "Cancel range" else "Range") }
            Text(
                when {
                    !rangeMode -> "Tap/hold the grid to preview"
                    rangeStart == null -> "Tap the first endpoint"
                    else -> "Tap the second endpoint"
                },
                color = Color(0xFF9AA3B5),
                modifier = Modifier.padding(top = 12.dp)
            )
        }

        AioraPitchGrid(
            modifier = Modifier.fillMaxWidth(),
            selectedLow = if (rangeStart != null) provisionalLow else low,
            selectedHigh = if (rangeStart != null) provisionalHigh else high,
            onPitchDown = { midi ->
                if (rangeMode) {
                    val first = rangeStart
                    if (first == null) {
                        rangeStart = midi
                    } else {
                        if (NativeBridge.setPadRange(track, selected, first, midi)) onChanged()
                        rangeStart = null
                        rangeMode = false
                    }
                    -1
                } else {
                    NativeBridge.noteOnPad(nativeGridPadIndex(midi), midi, 0.85f)
                }
            },
            onPitchUp = { voice -> if (voice >= 0) NativeBridge.noteOff(voice) }
        )

        Text("Pad volume", color = Color(0xFF9AA3B5))
        Slider(value = volume, onValueChange = { volume = it; NativeBridge.setPadVolume(track, selected, it) }, onValueChangeFinished = onChanged)
        Text("Pad pan", color = Color(0xFF9AA3B5))
        Slider(value = pan, onValueChange = { pan = it; NativeBridge.setPadPan(track, selected, it) }, onValueChangeFinished = onChanged, valueRange = -1f..1f)
    }
}
