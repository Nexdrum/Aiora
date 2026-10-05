package com.nexdrum.aiora

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.background
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        NativeBridge.startAudio()
        setContent { AioraApp() }
    }

    override fun onDestroy() {
        NativeBridge.stopAudio()
        super.onDestroy()
    }
}

private val AioraBg = Color(0xFF101216)
private val AioraPanel = Color(0xFF161A21)
private val AioraCyan = Color(0xFF00CCCC)
private val Muted = Color(0xFF9AA3B5)
private val presets = listOf("Spectrachord Init", "Subula", "Spectrello", "Nebular", "Nexdrum")

private data class NativePad(val label: String, val center: Int, val low: Int, val high: Int)
private val nexPads = listOf(
    NativePad("Crash", 80, 80, 86),
    NativePad("Ride", 73, 73, 79),
    NativePad("Bongo", 66, 66, 72),
    NativePad("Snare", 59, 59, 65),
    NativePad("Tom", 52, 52, 58),
    NativePad("Floor tom", 45, 45, 51),
    NativePad("Kick", 38, 38, 44)
)

@Composable
private fun AioraApp() {
    var page by remember { mutableStateOf("Tracks") }
    var preset by remember { mutableIntStateOf(NativeBridge.factoryPreset()) }
    var selectedPad by remember { mutableIntStateOf(6) }
    var revision by remember { mutableIntStateOf(0) }
    val changed = { revision++ }
    val pages = listOf("Tracks", "Drums", "Roll", "Synth", "FX", "Play")

    MaterialTheme(colorScheme = darkColorScheme(primary = AioraCyan, background = AioraBg, surface = AioraPanel)) {
        Column(Modifier.fillMaxSize().background(AioraBg)) {
            Row(
                Modifier.fillMaxWidth().background(Color(0xFF14171D)).padding(8.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text("AIORA", style = MaterialTheme.typography.titleMedium)
                Spacer(Modifier.weight(1f))
                Text("Oboe • ${NativeBridge.sampleRate()} Hz", style = MaterialTheme.typography.labelSmall, color = Muted)
            }
            Row(Modifier.fillMaxWidth().padding(horizontal = 6.dp, vertical = 4.dp), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                pages.forEach { p ->
                    Button(
                        onClick = { page = p },
                        modifier = Modifier.weight(1f),
                        contentPadding = PaddingValues(horizontal = 2.dp, vertical = 8.dp),
                        colors = ButtonDefaults.buttonColors(containerColor = if (page == p) AioraCyan else Color(0xFF232833))
                    ) { Text(p.take(5), style = MaterialTheme.typography.labelSmall) }
                }
            }
            Card(
                Modifier.fillMaxWidth().weight(1f).padding(8.dp),
                shape = RoundedCornerShape(10.dp),
                colors = CardDefaults.cardColors(containerColor = AioraPanel)
            ) {
                Column(Modifier.fillMaxSize().padding(12.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                    Text(page, style = MaterialTheme.typography.titleLarge)
                    if (page == "Synth" || page == "FX") {
                        PresetStrip(preset) { i ->
                            preset = i
                            NativeBridge.setFactoryPreset(i)
                        }
                    }
                    if ((page == "Synth" || page == "FX") && preset == 4) {
                        PadQuickSelect(selectedPad) { selectedPad = it }
                    }
                    when (page) {
                        "Tracks" -> NativeTracksView(
                            revision = revision,
                            onChanged = changed,
                            onTrackSelected = { track ->
                                if (track >= 0 && NativeBridge.trackIsDrums(track) && NativeBridge.padCount(track) > 0) {
                                    selectedPad = NativeBridge.selectedPad(track).coerceAtLeast(0)
                                }
                            }
                        )
                        "Drums" -> NativeDrumsView(revision = revision, onChanged = changed)
                        "Roll" -> NativePianoRollView(revision = revision, onChanged = changed)
                        "Play" -> PerformanceGrid(preset)
                        "Synth", "FX" -> EditorPreview(preset, selectedPad)
                    }
                }
            }
        }
    }
}

@Composable
private fun PresetStrip(selected: Int, onSelect: (Int) -> Unit) {
    Row(
        Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
        horizontalArrangement = Arrangement.spacedBy(6.dp)
    ) {
        presets.forEachIndexed { i, name ->
            FilterChip(selected = selected == i, onClick = { onSelect(i) }, label = { Text(name) })
        }
    }
}

@Composable
private fun PadQuickSelect(selected: Int, onSelect: (Int) -> Unit) {
    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
        Text("Pad", color = Muted, style = MaterialTheme.typography.labelMedium)
        Row(
            Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
            horizontalArrangement = Arrangement.spacedBy(6.dp)
        ) {
            nexPads.forEachIndexed { i, pad ->
                FilterChip(selected = selected == i, onClick = { onSelect(i) }, label = { Text(pad.label) })
            }
        }
    }
}

@Composable
private fun PerformanceGrid(preset: Int) {
    Text(if (preset == 4) "Nexdrum • each row is one shared 7-pitch pad family" else presets[preset], color = Muted)
    AioraPitchGrid(
        modifier = Modifier.fillMaxWidth(),
        onPitchDown = { midi ->
            if (preset == 4) NativeBridge.noteOnPad(nativeGridPadIndex(midi), midi, 0.85f)
            else NativeBridge.noteOn(midi, 0.85f)
        },
        onPitchUp = { voice -> NativeBridge.noteOff(voice) }
    )
}

@Composable
private fun EditorPreview(preset: Int, selectedPad: Int) {
    val pad = nexPads[selectedPad.coerceIn(nexPads.indices)]
    Text(
        if (preset == 4) "Editing ${pad.label}; preview is routed through that pad's own native Spectrachord patch."
        else "Editing ${presets[preset]} in the native Spectrachord engine.",
        color = Muted
    )
    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        var voice by remember(preset, selectedPad) { mutableIntStateOf(-1) }
        Button(onClick = {
            if (voice < 0) {
                voice = if (preset == 4) NativeBridge.noteOnPad(selectedPad, pad.center, 0.8f)
                else NativeBridge.noteOn(62, 0.8f)
            }
        }) { Text("Preview") }
        OutlinedButton(onClick = {
            if (voice >= 0) {
                NativeBridge.noteOff(voice)
                voice = -1
            }
        }) { Text("Release") }
        OutlinedButton(onClick = {
            NativeBridge.panic()
            voice = -1
        }) { Text("Panic") }
    }
}
