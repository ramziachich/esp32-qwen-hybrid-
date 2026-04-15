#include "BluetoothA2DPSource.h"

BluetoothA2DPSource a2dp_source;
static bool bt_connected = false;
static int16_t audio_buffer[160];

void init_bluetooth(const char* speaker_name) {
    Serial.printf("🔊 BT: %s\n", speaker_name);
    a2dp_source.set_connected_callback([](bool c) {
        bt_connected = c;
        Serial.println(c ? "✅ BT connecté" : "❌ BT déconnecté");
    });
    a2dp_source.set_volume(100);
    a2dp_source.start(speaker_name);
}

bool is_bluetooth_connected() {
    return bt_connected;
}

void play_notification() {
    if (!bt_connected) return;
    for (int bip = 0; bip < 2; bip++) {
        for (int i = 0; i < 160; i++) {
            float t = i / 16000.0f;
            audio_buffer[i] = (int16_t)(8000 * sin(2 * 3.14159 * 880 * t));
        }
        for (int r = 0; r < 30; r++) {
            a2dp_source.write((uint8_t*)audio_buffer, sizeof(audio_buffer));
        }
        delay(100);
    }
}

void speak_text_simple(const char* text) {
    if (!bt_connected) return;
    int len = strlen(text);
    if (len > 100) len = 100;
    for (int pos = 0; pos < len; pos++) {
        char c = text[pos];
        float freq = 200.0f + ((c % 60) * 10.0f);
        for (int i = 0; i < 160; i++) {
            float t = i / 16000.0f;
            float env = (i < 80) ? (i / 80.0f) : ((160 - i) / 80.0f);
            audio_buffer[i] = (int16_t)(6000 * sin(2 * 3.14159 * freq * t) * env);
        }
        for (int f = 0; f < 5; f++) {
            a2dp_source.write((uint8_t*)audio_buffer, sizeof(audio_buffer));
        }
        delay(30);
    }
}

void play_startup_jingle() {
    if (!bt_connected) return;
    float notes[] = {262, 330, 392, 523, 659, 784};
    for (float freq : notes) {
        for (int i = 0; i < 160; i++) {
            float t = i / 16000.0f;
            audio_buffer[i] = (int16_t)(4000 * sin(2 * 3.14159 * freq * t));
        }
        for (int r = 0; r < 40; r++) {
            a2dp_source.write((uint8_t*)audio_buffer, sizeof(audio_buffer));
        }
        delay(50);
    }
}
