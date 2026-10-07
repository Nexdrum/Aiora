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
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.abs
import kotlin.math.ceil
import kotlin.math.floor
import kotlin.math.max
import kotlin.math.min
import kotlin.math.sin

private enum class RollMode(val label: String, val curveKind: Int?) {
    Notes("Notes", null), Bend("∿", 0), Velocity("V", 1), Mod("M", 2)
}

private data class RollPoint(
    val index: Int,
    val step: Float,
    val value: Float,
    val free: Boolean
)

private data class RollNote(
    val track: Int,
    val index: Int,
    val midi: Int,
    val start: Float,
    val length: Float,
    val bend: List<RollPoint>,
    val velocity: List<RollPoint>,
    val mod: List<RollPoint>,
    val bendLow: Float?,
    val bendHigh: Float?
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
    val shadowSources = remember(revision) { readAllNotes() }
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
                        if (NativeBridge.dozenal()) "b" + AioraNotation.dozenalInt(bar) else "b$bar"
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

                // Harmonic shadows: one aggregate path means overlapping shadows never
                // become darker than the fixed shadow opacity.
                val shadowPath = Path()
                shadowSources.forEach { source ->
                    val pc = positivePitchClass(source.midi)
                    columns.forEach { targetMidi ->
                        if (positivePitchClass(targetMidi) == pc) {
                            appendRibbon(
                                path = shadowPath,
                                note = source,
                                targetMidi = targetMidi,
                                columns = columns,
                                gutter = gutter,
                                header = header,
                                cell = cell,
                                seed = ribbonSeed(source)
                            )
                        }
                    }
                }
                drawPath(shadowPath, Color(0x52000000))

                // Selected-track notes are permanent performance ribbons:
                // bend = center path, velocity = width, M = edge roughness.
                notes.forEach { note ->
                    val col = columns.indexOf(note.midi)
                    if (col < 0) return@forEach
                    val pc = AioraNotation.pitchColor(note.midi)
                    val notePath = Path()
                    val tail = appendRibbon(
                        path = notePath,
                        note = note,
                        targetMidi = note.midi,
                        columns = columns,
                        gutter = gutter,
                        header = header,
                        cell = cell,
                        seed = ribbonSeed(note)
                    )
                    drawPath(notePath, pc)
                    drawLine(
                        Color(0x88001414),
                        start = tail.first,
                        end = tail.second,
                        strokeWidth = 3f
                    )
                    drawNoteCurves(note, mode, columns, gutter, header, cell)
                }
            }
        }
    }
}

private fun positivePitchClass(midi: Int): Int = ((midi % 12) + 12) % 12

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
        val midi = NativeBridge.noteMidi(track, i)
        val bounds = drumBendBounds(track, midi)
        add(
            RollNote(
                track = track,
                index = i,
                midi = midi,
                start = NativeBridge.noteStart(track, i),
                length = NativeBridge.noteLength(track, i),
                bend = readPoints(track, i, 0),
                velocity = readPoints(track, i, 1),
                mod = readPoints(track, i, 2),
                bendLow = bounds?.first,
                bendHigh = bounds?.second
            )
        )
    }
}

private fun readAllNotes(): List<RollNote> = buildList {
    repeat(NativeBridge.trackCount()) { track ->
        addAll(readNotes(track))
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
}.sortedBy { it.step }

private fun drumBendBounds(track: Int, midi: Int): Pair<Float, Float>? {
    if (!NativeBridge.trackIsDrums(track)) return null
    repeat(NativeBridge.padCount(track)) { p ->
        val lo = min(NativeBridge.padLow(track, p), NativeBridge.padHigh(track, p))
        val hi = max(NativeBridge.padLow(track, p), NativeBridge.padHigh(track, p))
        if (midi in lo..hi) {
            return (lo - 0.5f - midi) to (hi + 0.5f - midi)
        }
    }
    return null
}

private fun curveLength(note: RollNote): Float = max(0f, max(1f, note.length) - 1f)

private fun clampBend(note: RollNote, value: Float): Float {
    val v = value.coerceIn(-12f, 12f)
    val lo = note.bendLow
    val hi = note.bendHigh
    return if (lo != null && hi != null) v.coerceIn(lo, hi) else v
}

private fun lerpAt(
    x: Float,
    x0: Float,
    y0: Float,
    x1: Float,
    y1: Float
): Float {
    if (abs(x1 - x0) < 1.0e-5f) return y1
    val t = ((x - x0) / (x1 - x0)).coerceIn(0f, 1f)
    return y0 + (y1 - y0) * t
}

private fun bendValue(note: RollNote, step: Float): Float {
    val l = curveLength(note)
    val x = step.coerceIn(0f, l)
    if (note.bend.isEmpty()) return 0f

    var prevStep = 0f
    var prevValue = 0f
    note.bend.forEach { point ->
        val ps = point.step.coerceIn(0f, l)
        val pv = clampBend(note, point.value)
        if (abs(ps - prevStep) < 1.0e-5f) {
            prevValue = pv
        } else {
            if (x <= ps) return lerpAt(x, prevStep, prevValue, ps, pv)
            prevStep = ps
            prevValue = pv
        }
    }
    return if (prevStep < l) lerpAt(x, prevStep, prevValue, l, 0f) else prevValue
}

private fun levelValue(points: List<RollPoint>, note: RollNote, step: Float, fallback: Float): Float {
    if (points.isEmpty()) return fallback
    val l = curveLength(note)
    val x = step.coerceIn(0f, l)
    var prevStep = 0f
    var prevValue = points.first().value.coerceIn(0f, 1f)
    points.forEach { point ->
        val ps = point.step.coerceIn(0f, l)
        val pv = point.value.coerceIn(0f, 1f)
        if (abs(ps - prevStep) < 1.0e-5f) {
            prevValue = pv
        } else {
            if (x <= ps) return lerpAt(x, prevStep, prevValue, ps, pv)
            prevStep = ps
            prevValue = pv
        }
    }
    return prevValue
}

private fun velocityValue(note: RollNote, step: Float): Float =
    levelValue(note.velocity, note, step, 1f)

private fun modValue(note: RollNote, step: Float): Float =
    levelValue(note.mod, note, step, 0f)

private fun pitchXFor(columns: List<Int>, pitch: Float, gutter: Float, cell: Float): Float {
    if (columns.isEmpty()) return gutter
    val firstCenter = gutter + cell / 2f
    val lastCenter = gutter + (columns.lastIndex + 0.5f) * cell
    if (pitch <= columns.first()) return firstCenter
    if (pitch >= columns.last()) return lastCenter
    for (i in 0 until columns.lastIndex) {
        val a = columns[i].toFloat()
        val b = columns[i + 1].toFloat()
        if (pitch <= b) {
            val t = if (abs(b - a) < 1.0e-5f) 0f else ((pitch - a) / (b - a)).coerceIn(0f, 1f)
            return gutter + (i + 0.5f + t) * cell
        }
    }
    return lastCenter
}

private fun pitchForX(columns: List<Int>, x: Float, gutter: Float, cell: Float): Float {
    if (columns.isEmpty()) return 62f
    val visual = (x - gutter) / cell - 0.5f
    if (visual <= 0f) return columns.first().toFloat()
    if (visual >= columns.lastIndex) return columns.last().toFloat()
    val i = floor(visual).toInt().coerceIn(0, columns.lastIndex - 1)
    val t = visual - i
    val a = columns[i].toFloat()
    val b = columns[i + 1].toFloat()
    return a + (b - a) * t
}

private fun ribbonSeed(note: RollNote): Int =
    note.track * 73856093 xor note.index * 19349663 xor note.midi * 83492791

private fun edgeNoise(seed: Int, sample: Int, side: Int): Float {
    val s = seed.toDouble()
    val i = sample.toDouble()
    val a = sin(s * 0.0000137 + i * 1.731 + side * 2.137)
    val b = sin(s * 0.0000311 + i * 3.117 + side * 5.071)
    return (a * 0.68 + b * 0.32).toFloat().coerceIn(-1f, 1f)
}

private fun visualCurveStep(note: RollNote, fraction: Float): Float {
    val l = curveLength(note)
    if (l <= 0f) return 0f
    val visualSteps = max(1f, note.length)
    return (fraction.coerceIn(0f, 1f) * visualSteps - 0.5f).coerceIn(0f, l)
}

private fun ribbonHalfWidth(note: RollNote, step: Float, cell: Float): Float {
    val minHalf = max(1.25f, cell * 0.055f)
    val maxHalf = max(minHalf, cell / 2f - 2f)
    return minHalf + (maxHalf - minHalf) * velocityValue(note, step)
}

private fun appendRibbon(
    path: Path,
    note: RollNote,
    targetMidi: Int,
    columns: List<Int>,
    gutter: Float,
    header: Float,
    cell: Float,
    seed: Int
): Pair<Offset, Offset> {
    val length = max(1f, note.length)
    val top = header + note.start * cell
    val height = length * cell
    val segments = ceil(length * 5f).toInt().coerceIn(5, 320)
    val left = ArrayList<Offset>(segments + 1)
    val right = ArrayList<Offset>(segments + 1)

    for (i in 0..segments) {
        val fraction = i.toFloat() / segments.toFloat()
        val step = visualCurveStep(note, fraction)
        val center = pitchXFor(columns, targetMidi + bendValue(note, step), gutter, cell)
        val half = ribbonHalfWidth(note, step, cell)
        val roughness = modValue(note, step) * cell * 0.11f
        val leftHalf = max(1.1f, half + edgeNoise(seed, i, 0) * roughness)
        val rightHalf = max(1.1f, half + edgeNoise(seed, i, 1) * roughness)
        val y = top + fraction * height
        left += Offset(center - leftHalf, y)
        right += Offset(center + rightHalf, y)
    }

    path.moveTo(left.first().x, left.first().y)
    for (i in 1 until left.size) path.lineTo(left[i].x, left[i].y)
    for (i in right.indices.reversed()) path.lineTo(right[i].x, right[i].y)
    path.close()
    return left.last() to right.last()
}

private fun noteAtPosition(
    notes: List<RollNote>,
    columns: List<Int>,
    pos: Offset,
    gutter: Float,
    header: Float,
    cell: Float
): RollNote? {
    if (pos.x < gutter || pos.y < header) return null
    val absoluteStep = (pos.y - header) / cell
    var best: RollNote? = null
    var bestDistance = Float.MAX_VALUE
    notes.forEach { note ->
        if (absoluteStep < note.start || absoluteStep >= note.start + max(1f, note.length)) return@forEach
        val localFraction = ((pos.y - (header + note.start * cell)) / (max(1f, note.length) * cell)).coerceIn(0f, 1f)
        val step = visualCurveStep(note, localFraction)
        val center = pitchXFor(columns, note.midi + bendValue(note, step), gutter, cell)
        val tolerance = ribbonHalfWidth(note, step, cell) + modValue(note, step) * cell * 0.11f + 5f
        val distance = abs(pos.x - center)
        if (distance <= tolerance && distance < bestDistance) {
            best = note
            bestDistance = distance
        }
    }
    return best
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
    if (col !in columns.indices) return
    val absoluteStep = (pos.y - header) / cell
    if (absoluteStep < 0f) return
    val snappedStep = floor(absoluteStep).toInt()
    val midi = columns[col]
    val note = noteAtPosition(notes, columns, pos, gutter, header, cell)

    if (mode == RollMode.Notes) {
        if (note != null) NativeBridge.deleteNote(track, note.index)
        else NativeBridge.addNote(track, midi, snappedStep.toFloat(), 1f)
        return
    }
    note ?: return
    val kind = mode.curveKind ?: return
    val l = curveLength(note)
    val freeRelative = (absoluteStep - note.start - 0.5f).coerceIn(0f, l)
    val snappedRelative = (snappedStep.toFloat() - note.start).coerceIn(0f, l)
    val relativeStep = if (longPress) freeRelative else snappedRelative
    val points = when (kind) {
        0 -> note.bend
        1 -> note.velocity
        else -> note.mod
    }
    val existing = points.minByOrNull { abs(it.step - relativeStep) }
    if (existing != null && abs(existing.step - relativeStep) <= 0.3f) {
        if (longPress) {
            NativeBridge.updateCurvePoint(track, note.index, kind, existing.index, existing.step, existing.value, !existing.free)
        } else {
            NativeBridge.deleteCurvePoint(track, note.index, kind, existing.index)
        }
        return
    }

    val value = when (mode) {
        RollMode.Bend -> {
            val pitch = if (longPress) pitchForX(columns, pos.x, gutter, cell) else midi.toFloat()
            clampBend(note, pitch - note.midi)
        }
        RollMode.Velocity, RollMode.Mod -> {
            val center = pitchXFor(columns, note.midi + bendValue(note, relativeStep), gutter, cell)
            (abs(pos.x - center) / max(1f, cell / 2f - 2f)).coerceIn(0f, 1f)
        }
        else -> 0f
    }
    NativeBridge.addCurvePoint(track, note.index, kind, relativeStep, value, longPress)
}

private fun androidx.compose.ui.graphics.drawscope.DrawScope.drawNoteCurves(
    note: RollNote,
    mode: RollMode,
    columns: List<Int>,
    gutter: Float,
    header: Float,
    cell: Float
) {
    if (mode == RollMode.Notes) return

    val yTop = header + note.start * cell
    val length = max(1f, note.length)
    val height = length * cell
    val segments = ceil(length * 5f).toInt().coerceIn(5, 320)

    if (mode == RollMode.Bend) {
        val color = Color(0xFF00FFFF)
        val centerPath = Path()
        for (i in 0..segments) {
            val fraction = i.toFloat() / segments.toFloat()
            val step = visualCurveStep(note, fraction)
            val px = pitchXFor(columns, note.midi + bendValue(note, step), gutter, cell)
            val py = yTop + fraction * height
            if (i == 0) centerPath.moveTo(px, py) else centerPath.lineTo(px, py)
        }
        drawPath(centerPath, color, style = Stroke(width = 2f))
        note.bend.forEach { p ->
            val px = pitchXFor(columns, note.midi + clampBend(note, p.value), gutter, cell)
            val py = yTop + (p.step + 0.5f).coerceIn(0f, length) * cell
            drawCircle(color, if (p.free) 4.5f else 3.2f, Offset(px, py))
        }
        return
    }

    val points = if (mode == RollMode.Velocity) note.velocity else note.mod
    val color = if (mode == RollMode.Velocity) Color(0xFFE8ECF1) else Color(0xFFC98EFF)
    val centerPath = Path()
    for (i in 0..segments) {
        val fraction = i.toFloat() / segments.toFloat()
        val step = visualCurveStep(note, fraction)
        val px = pitchXFor(columns, note.midi + bendValue(note, step), gutter, cell)
        val py = yTop + fraction * height
        if (i == 0) centerPath.moveTo(px, py) else centerPath.lineTo(px, py)
    }
    drawPath(centerPath, color.copy(alpha = 0.28f), style = Stroke(width = 1f))
    if (points.isEmpty()) return

    val left = Path()
    val right = Path()
    for (i in 0..segments) {
        val fraction = i.toFloat() / segments.toFloat()
        val step = visualCurveStep(note, fraction)
        val center = pitchXFor(columns, note.midi + bendValue(note, step), gutter, cell)
        val value = if (mode == RollMode.Velocity) velocityValue(note, step) else modValue(note, step)
        val half = value * max(1f, cell / 2f - 2f)
        val py = yTop + fraction * height
        if (i == 0) {
            left.moveTo(center - half, py)
            right.moveTo(center + half, py)
        } else {
            left.lineTo(center - half, py)
            right.lineTo(center + half, py)
        }
    }
    drawPath(left, color, style = Stroke(width = 1.6f))
    drawPath(right, color, style = Stroke(width = 1.6f))

    points.forEach { p ->
        val step = p.step.coerceIn(0f, curveLength(note))
        val center = pitchXFor(columns, note.midi + bendValue(note, step), gutter, cell)
        val half = p.value.coerceIn(0f, 1f) * max(1f, cell / 2f - 2f)
        val py = yTop + (p.step + 0.5f).coerceIn(0f, length) * cell
        drawCircle(color, if (p.free) 4.5f else 3.0f, Offset(center - half, py))
        drawCircle(color, if (p.free) 4.5f else 3.0f, Offset(center + half, py))
    }
}
