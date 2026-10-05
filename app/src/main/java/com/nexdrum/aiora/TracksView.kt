package com.nexdrum.aiora

import androidx.compose.foundation.border
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp

@Composable
fun NativeTracksView(
    revision: Int,
    onChanged: () -> Unit,
    onTrackSelected: (Int) -> Unit
) {
    val count = NativeBridge.trackCount()
    val selected = NativeBridge.selectedTrack()
    Column(
        Modifier.fillMaxSize().verticalScroll(rememberScrollState()),
        verticalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        SongPanel(revision, onChanged)
        Text("TRACKS", style = MaterialTheme.typography.labelLarge, color = Color(0xFF9AA3B5))
        if (count == 0) {
            Text("No tracks yet. AIORA starts blank, matching the web build.", color = Color(0xFF9AA3B5))
        }
        repeat(count) { index ->
            NativeTrackCard(index, index == selected, revision, onChanged, onTrackSelected)
        }
        Button(
            onClick = {
                val i = NativeBridge.addTrack(false)
                if (i >= 0) onTrackSelected(i)
                onChanged()
            },
            enabled = count < 10,
            modifier = Modifier.fillMaxWidth()
        ) { Text("+ Add track") }
    }
}

@Composable
private fun SongPanel(revision: Int, onChanged: () -> Unit) {
    var bpm by remember(revision) { mutableFloatStateOf(NativeBridge.bpm()) }
    var beats by remember(revision) { mutableFloatStateOf(NativeBridge.beats().toFloat()) }
    var divisions by remember(revision) { mutableFloatStateOf(NativeBridge.divisions().toFloat()) }
    var dozenal by remember(revision) { mutableStateOf(NativeBridge.dozenal()) }
    var masterVol by remember(revision) { mutableFloatStateOf(NativeBridge.masterVolume()) }
    var masterRev by remember(revision) { mutableFloatStateOf(NativeBridge.masterReverb()) }

    ElevatedCard(colors = CardDefaults.elevatedCardColors(containerColor = Color(0xFF1B212C))) {
        Column(Modifier.padding(10.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text("SONG", style = MaterialTheme.typography.titleSmall)
                Spacer(Modifier.weight(1f))
                Text(if (dozenal) AioraNotation.dozenalInt(bpm.toInt()) else bpm.toInt().toString(), color = Color(0xFF9AA3B5))
                Text(" BPM", color = Color(0xFF9AA3B5))
            }
            Slider(
                value = bpm,
                onValueChange = { bpm = it; NativeBridge.setBpm(it) },
                onValueChangeFinished = onChanged,
                valueRange = 12f..288f
            )
            Text("Signature  ${beats.toInt()} / ${divisions.toInt()}", color = Color(0xFF9AA3B5))
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Slider(
                    value = beats,
                    onValueChange = { beats = it; NativeBridge.setSignature(it.toInt(), divisions.toInt()) },
                    onValueChangeFinished = onChanged,
                    valueRange = 1f..12f,
                    steps = 10,
                    modifier = Modifier.weight(1f)
                )
                Slider(
                    value = divisions,
                    onValueChange = { divisions = it; NativeBridge.setSignature(beats.toInt(), it.toInt()) },
                    onValueChangeFinished = onChanged,
                    valueRange = 1f..12f,
                    steps = 10,
                    modifier = Modifier.weight(1f)
                )
            }
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text("Dozenal numbers")
                Spacer(Modifier.weight(1f))
                Switch(checked = dozenal, onCheckedChange = { dozenal = it; NativeBridge.setDozenal(it); onChanged() })
            }
            Text("Master volume", color = Color(0xFF9AA3B5))
            Slider(value = masterVol, onValueChange = { masterVol = it; NativeBridge.setMasterVolume(it) }, onValueChangeFinished = onChanged)
            Text("Master reverb", color = Color(0xFF9AA3B5))
            Slider(value = masterRev, onValueChange = { masterRev = it; NativeBridge.setMasterReverb(it) }, onValueChangeFinished = onChanged)
        }
    }
}

@Composable
private fun NativeTrackCard(
    index: Int,
    selected: Boolean,
    revision: Int,
    onChanged: () -> Unit,
    onTrackSelected: (Int) -> Unit
) {
    var name by remember(index, revision) { mutableStateOf(NativeBridge.trackName(index)) }
    var mute by remember(index, revision) { mutableStateOf(NativeBridge.trackMute(index)) }
    var solo by remember(index, revision) { mutableStateOf(NativeBridge.trackSolo(index)) }
    var volume by remember(index, revision) { mutableFloatStateOf(NativeBridge.trackVolume(index)) }
    var pan by remember(index, revision) { mutableFloatStateOf(NativeBridge.trackPan(index)) }
    val drums = NativeBridge.trackIsDrums(index)
    val border = if (selected) Color(0xFF66FFFF) else Color(0xFF2C3342)

    Column(
        Modifier.fillMaxWidth().border(1.dp, border, RoundedCornerShape(8.dp)).padding(7.dp),
        verticalArrangement = Arrangement.spacedBy(5.dp)
    ) {
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            Text(if (NativeBridge.dozenal()) AioraNotation.dozenalInt(index + 1) else (index + 1).toString())
            OutlinedTextField(
                value = name,
                onValueChange = { if (it.length <= 24) name = it },
                singleLine = true,
                modifier = Modifier.weight(1f),
                textStyle = MaterialTheme.typography.bodySmall
            )
            Text(if (drums) "KIT" else "SYNTH", color = Color(0xFF9AA3B5), style = MaterialTheme.typography.labelSmall)
            TextButton(onClick = { NativeBridge.deleteTrack(index); onChanged() }) { Text("×") }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(5.dp), verticalAlignment = Alignment.CenterVertically) {
            FilterChip(selected = mute, onClick = { mute = !mute; NativeBridge.setTrackMute(index, mute); onChanged() }, label = { Text("M") })
            FilterChip(selected = solo, onClick = { solo = !solo; NativeBridge.setTrackSolo(index, solo); onChanged() }, label = { Text("S") })
            TextButton(onClick = {
                NativeBridge.setTrackName(index, name)
                NativeBridge.selectTrack(index)
                onTrackSelected(index)
                onChanged()
            }) { Text(if (selected) "Selected" else "Select") }
        }
        Text("Volume", style = MaterialTheme.typography.labelSmall, color = Color(0xFF9AA3B5))
        Slider(value = volume, onValueChange = { volume = it; NativeBridge.setTrackVolume(index, it) }, onValueChangeFinished = onChanged)
        Text("Pan", style = MaterialTheme.typography.labelSmall, color = Color(0xFF9AA3B5))
        Slider(value = pan, onValueChange = { pan = it; NativeBridge.setTrackPan(index, it) }, onValueChangeFinished = onChanged, valueRange = -1f..1f)
    }
}
