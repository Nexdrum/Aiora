package com.nexdrum.aiora

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
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

@Composable
private fun AioraApp() {
    var page by remember { mutableStateOf("Tracks") }
    var voice by remember { mutableIntStateOf(-1) }
    val pages = listOf("Tracks", "Drums", "Roll", "Synth", "FX", "Play")

    MaterialTheme(colorScheme = darkColorScheme(primary = AioraCyan, background = AioraBg, surface = AioraPanel)) {
        Column(Modifier.fillMaxSize().background(AioraBg)) {
            Row(
                Modifier.fillMaxWidth().background(Color(0xFF14171D)).padding(8.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text("AIORA", style = MaterialTheme.typography.titleMedium)
                Spacer(Modifier.weight(1f))
                Text("Native • ${NativeBridge.sampleRate()} Hz", style = MaterialTheme.typography.labelSmall, color = Color(0xFF9AA3B5))
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
                Modifier.fillMaxWidth().padding(8.dp),
                shape = RoundedCornerShape(10.dp),
                colors = CardDefaults.cardColors(containerColor = AioraPanel)
            ) {
                Column(Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                    Text(page, style = MaterialTheme.typography.titleLarge)
                    Text("Native shell is live. Web AIORA remains the behavioral reference while each page is ported into this surface.", color = Color(0xFF9AA3B5))
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        Button(onClick = { if (voice < 0) voice = NativeBridge.noteOn(62, 0.85f) }) { Text("Preview D") }
                        OutlinedButton(onClick = { if (voice >= 0) { NativeBridge.noteOff(voice); voice = -1 } }) { Text("Release") }
                        OutlinedButton(onClick = { NativeBridge.panic(); voice = -1 }) { Text("Panic") }
                    }
                }
            }
        }
    }
}
