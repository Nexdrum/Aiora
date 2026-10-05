package com.nexdrum.aiora

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.abs
import kotlin.math.ceil
import kotlin.math.floor
import kotlin.math.max

private enum class RollMode(val label: String, val curveKind: Int?) {
    Notes("Notes", null), Bend("∿", 0), Velocity("V", 1), Mod("M", 2)
}

private data class RollNote(
    val index: Int,
    val midi: Int,
    val start: Float,
    val length: Float
)

private data class RollPoint(
    val index: Int,
    val step: Float,
    val value: Float,
    val free: Boolean
)

@Composable
fun NativePianoRollView(revision: Int, onChanged: () -> Unit) {
    val track = NativeBridge.selectedTrack()
    if (track < 0 || NativeBridge.trackCount() == 0) {
        Text("Add and select a track first.", color = Color(0xFF9AA3B5))
        return
    }

    var mode by remember { mutableStateOf(RollMode.Notes) }
    val columns = remember(track, revision) { rollColumns(track) }
    val notes = remember(track, revision) { readNotes(track) }
    val rows = max(64, NativeBridge.playLengthSteps() + 32)
    val density = LocalDensity.current
    val textMeasurer = rememberTextMeasurer()
    val hScroll = rememberScrollState()
    val vScroll = rememberScrollState()

    val cellDp = 28.dp
    val gutterDp = 40.dp
    val headerDp = 44.dp
    val canvasWidth = gutterDp + cellDp * columns.size
    val canvasHeight = headerDp + cellDp * rows

    Column(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
        Row(horizontalArrangement = Arrangement.spacedBy(5.dp)) {
            RollMode.entries.forEach { candidate ->
                FilterChip(
                    selected = mode == candidate,
                    onClick = { mode = candidate },
                    label = { Text(candidate.label) }
                )
            }
            Spacer(Modifier.weight(1f))
            Text(
                NativeBridge.trackName(track),
                color = Color(0xFF9AA3B5),
                style = MaterialTheme.typography.labelMedium,
                modifier = Modifier.padding(top = 10.dp)
            )
        }
        Text(
            when (mode) {
                RollMode.Notes -> "Tap empty = one-step note • tap note = delete"
                RollMode.Bend -> "Tap a note span to add/delete a snapped bend point • long-press toggles free"
                RollMode.Velocity -> "Tap across the note width to set velocity • long-press toggles free"
                RollMode.Mod -> "Tap across the note width to set M • long-press toggles free"
            },
            color = Color(0xFF9AA3B5),
            style = MaterialTheme.typography.bodySmall
        )

        Box(
            Modifier.fillMaxSize()
                .horizontalScroll(hScroll)
                .verticalScroll(vScroll)
        ) {
            Canvas(
                Modifier.requiredSize(canvasWidth, canvasHeight)
                    .pointerInput(track, revision, mode, columns) {
                        detectTapGestures(
                            onTap = { pos ->
                                handleRollTap(track, mode, columns, notes, pos, density.density, false)
                                onChanged()
                            },
                            onLongPress = { pos ->
                                handleRollTap(track, mode, columns, notes, pos, density.density, true)
                                onChanged()
                            }
                        )
                    }
            ) {
                val cell = cellDp.toPx()
                val gutter = gutterDp.toPx()
                val header = headerDp.toPx()
                val barLen = max(1, NativeBridge.beats() * NativeBridge.divisions())
                val beatLen = max(1, NativeBridge.divisions())

                drawRect(Color(0xFF0C0E12))

                // Pitch header.
                columns.forEachIndexed { col, midi ->
                    val x = gutter + col * cell
                    val pc = AioraNotation.pitchColor(midi)
                    drawRect(Color(0xFF1A1F29), Offset(x, 0f), Size(cell - 1f, header - 2f))
                    drawRect(pc.copy(alpha = 0.9f), Offset(x, header - 3f), Size(cell - 1f, 2f))
                    val coord = AioraNotation.pitchCoord(midi)
                    drawText(
                        textMeasurer,
                        coord,
                        topLeft = Offset(x + 2f, 7f),
                        style = TextStyle(color = pc, fontSize = 8.sp)
                    )
                    if (midi % 12 == 2) {
                        val o = AioraNotation.dOctave(midi)
                        drawText(
                            textMeasurer,
                            if (o > 0) "+$o" else o.toString(),
                            topLeft = Offset(x + 5f, 23f),
                            style = TextStyle(color = Color(0xFF9AA3B5), fontSize = 8.sp)
                        )
                    }
                }

                // Time grid.
                for (row in 0 until rows) {
                    val y = header + row * cell
                    val isBar = row % barLen == 0
                    val isBeat = row % beatLen == 0
                    val rowColor = when {
                        isBar -> Color(0xFF242C3A)
                        isBeat -> Color(0xFF202634)
                        else -> Color(0xFF1A1F29)
                    }
                    drawRect(rowColor, Offset(0f, y), Size(gutter - 2f, cell - 1f))
                    val label = if (isBar) {
                        val bar = row / barLen + 1
                        if (NativeBridge.dozenal()) "b${AioraNotation.dozenalInt(bar)}" else "b$bar"
                    } else row.toString()
                    drawText(
                        textMeasurer,
                        label,
                        topLeft = Offset(3f, y + 7f),
                        style = TextStyle(color = if (isBar) Color.White else Color(0xFF8B93A5), fontSize = 8.sp)
                    )
                    columns.indices.forEach { col ->
                        val x = gutter + col * cell
                        drawRect(rowColor, Offset(x, y), Size(cell - 1f, cell - 1f))
                    }
                }

                // Notes and their automation overlays.
                notes.forEach { note ->
                    val col = columns.indexOf(note.midi)
                    if (col < 0) return@forEach
                    val x = gutter + col * cell
                    val y = header + note.start * cell
                    val h = max(cell, note.length * cell)
                    val pc = AioraNotation.pitchColor(note.midi)
                    drawRect(pc, Offset(x + 1f, y + 1f), Size(cell - 3f, h - 2f))
                    drawRect(Color(0x88001414), Offset(x + 1f, y + h - 4f), Size(cell - 3f, 3f))

                    drawNoteCurves(track, note, mode, x, y, cell, textMeasurer)
                }
            }
        }
    }
}

private fun rollColumns(track: Int): List<Int> {
    if (!NativeBridge.trackIsDrums(track)) return (14..110).toList()
    val pitches = sortedSetOf<Int>()
    repeat(NativeBridge.padCount(track)) { p ->
        val lo = NativeBridge.padLow(track, p)
        val hi = NativeBridge.padHigh(track, p)
        if (lo >= 0 && hi >= 0) for (m in minOf(lo, hi)..maxOf(lo, hi)) pitches += m
    }
    return if (pitches.isEmpty()) (14..110).toList() else pitches.toList()
}

private fun readNotes(track: Int): List<RollNote> = buildList {
    repeat(NativeBridge.noteCount(track)) { i ->
        add(
            RollNote(
                index = i,
                midi = NativeBridge.noteMidi(track, i),
                start = NativeBridge.noteStart(track, i),
                length = NativeBridge.noteLength(track, i)
            )
        )
    }
}

private fun readPoints(track: Int, note: Int, kind: Int): List<RollPoint> = buildList {
    repeat(NativeBridge.curvePointCount(track, note, kind)) { p ->
        add(
            RollPoint(
                index = p,
                step = NativeBridge.curvePointStep(track, note, kind, p),
                value = NativeBridge.curvePointValue(track, note, kind, p),
                free = NativeBridge.curvePointFree(track, note, kind, p)
            )
        )
    }
}

private fun handleRollTap(
    track: Int,
    mode: RollMode,
    columns: List<Int>,
    notes: List<RollNote>,
    pos: Offset,
    density: Float,
    longPress: Boolean
) {
    val cell = 28f * density
    val gutter = 40f * density
    val header = 44f * density
    if (pos.x < gutter || pos.y < header) return
    val col = floor((pos.x - gutter) / cell).toInt()
    val step = floor((pos.y - header) / cell).toInt()
    if (col !in columns.indices || step < 0) return
    val midi = columns[col]
    val note = notes.firstOrNull { it.midi == midi && step.toFloat() >= it.start && step.toFloat() < it.start + it.length }

    if (mode == RollMode.Notes) {
        if (note != null) NativeBridge.deleteNote(track, note.index)
        else NativeBridge.addNote(track, midi, step.toFloat(), 1f)
        return
    }
    note ?: return
    val kind = mode.curveKind ?: return
    val relativeStep = (step.toFloat() - note.start).coerceIn(0f, max(0f, note.length - 1f))
    val existing = readPoints(track, note.index, kind).minByOrNull { abs(it.step - relativeStep) }
    if (existing != null && abs(existing.step - relativeStep) <= 0.3f) {
        if (longPress) {
            NativeBridge.updateCurvePoint(track, note.index, kind, existing.index, existing.step, existing.value, !existing.free)
        } else {
            NativeBridge.deleteCurvePoint(track, note.index, kind, existing.index)
        }
        return
    }
    val colLeft = gutter + col * cell
    val normalizedAcross = ((pos.x - colLeft) / cell).coerceIn(0f, 1f)
    val value = when (mode) {
        RollMode.Bend -> 0f
        RollMode.Velocity, RollMode.Mod -> abs(normalizedAcross - 0.5f) * 2f
        else -> 0f
    }
    NativeBridge.addCurvePoint(track, note.index, kind, relativeStep, value, longPress)
}

private fun androidx.compose.ui.graphics.drawscope.DrawScope.drawNoteCurves(
    track: Int,
    note: RollNote,
    mode: RollMode,
    x: Float,
    y: Float,
    cell: Float,
    textMeasurer: androidx.compose.ui.text.TextMeasurer
) {
    val kinds = if (mode.curveKind != null) listOf(mode.curveKind) else listOf(0, 1, 2)
    kinds.filterNotNull().forEach { kind ->
        val points = readPoints(track, note.index, kind)
        if (points.isEmpty()) return@forEach
        val color = when (kind) {
            0 -> Color(0xFF00FFFF)
            1 -> Color(0xFFE8ECF1)
            else -> Color(0xFFC98EFF)
        }
        if (kind == 0) {
            val path = Path()
            points.forEachIndexed { i, p ->
                val px = x + cell / 2f + p.value * cell
                val py = y + (p.step + 0.5f) * cell
                if (i == 0) path.moveTo(px, py) else path.lineTo(px, py)
                drawCircle(color, if (p.free) 4.5f else 3.2f, Offset(px, py))
            }
            drawPath(path, color, style = androidx.compose.ui.graphics.drawscope.Stroke(width = 2f))
        } else {
            val left = Path()
            val right = Path()
            points.forEachIndexed { i, p ->
                val half = p.value.coerceIn(0f, 1f) * (cell / 2f - 2f)
                val py = y + (p.step + 0.5f) * cell
                val lx = x + cell / 2f - half
                val rx = x + cell / 2f + half
                if (i == 0) { left.moveTo(lx, py); right.moveTo(rx, py) }
                else { left.lineTo(lx, py); right.lineTo(rx, py) }
                drawCircle(color, if (p.free) 4.5f else 3.0f, Offset(lx, py))
                drawCircle(color, if (p.free) 4.5f else 3.0f, Offset(rx, py))
            }
            drawPath(left, color, style = androidx.compose.ui.graphics.drawscope.Stroke(width = 1.6f))
            drawPath(right, color, style = androidx.compose.ui.graphics.drawscope.Stroke(width = 1.6f))
        }
    }
}
