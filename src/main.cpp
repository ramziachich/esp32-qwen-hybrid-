#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include "ollama_client.h"
extern void init_bluetooth(const char* speaker_name);
extern bool is_bluetooth_connected();
extern void play_notification();
extern void play_startup_jingle();
extern void speak_text_simple(const char* text);
extern bool check_ollama_connection();
extern struct OllamaResponse query_ollama(const char* prompt, const char* system_prompt);

// ========== VOS PARAMÈTRES ==========
const char* WIFI_SSID = "ESP";
const char* WIFI_PASSWORD = "12345678";
const char* BT_SPEAKER_NAME = "SY-SPEAKER";
const char* DEVICE_HOSTNAME = "qwen-assistant";

WebServer server(80);
bool system_ready = false;
String history[10];
int history_index = 0;
unsigned long last_heartbeat = 0;

void print_menu();

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n╔══════════════════════════════════════╗");
    Serial.println("║     QWEN HYBRID ASSISTANT            ║");
    Serial.println("║     ESP32 + Smartphone Ollama        ║");
    Serial.println("╚══════════════════════════════════════╝\n");
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    Serial.print("WiFi connexion");
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n✅ WiFi: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("\n❌ WiFi échoué");
    }
    
    ArduinoOTA.setHostname(DEVICE_HOSTNAME);
    ArduinoOTA.onStart([]() { Serial.println("OTA start..."); });
    ArduinoOTA.onEnd([]() { Serial.println("\nOTA done!"); });
    ArduinoOTA.onProgress([](unsigned int p, unsigned int t) {
        Serial.printf("%u%%\r", (p / (t / 100)));
    });
    ArduinoOTA.begin();
    
    init_bluetooth(BT_SPEAKER_NAME);
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nTest Ollama...");
        if (check_ollama_connection()) {
            system_ready = true;
            Serial.println("✅ Ollama OK!");
            delay(2000);
            if (is_bluetooth_connected()) {
                play_startup_jingle();
                speak_text_simple("Pret");
            }
        } else {
            Serial.println("⚠️ Ollama non accessible");
            Serial.println("💡 Verifiez: OLLAMA_HOST=0.0.0.0 ollama serve");
        }
    }
    
    setup_web();
    server.begin();
    print_menu();
}

void setup_web() {
    server.on("/", HTTP_GET, []() {
        String html = "<!DOCTYPE html><html><head>";
        html += "<meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
        html += "<title>Qwen Assistant</title>";
        html += "<style>";
        html += "body{font-family:system-ui;background:#1a1a2e;color:#eee;max-width:600px;margin:0 auto;padding:20px;}";
        html += "h1{color:#00d4aa;text-align:center;}";
        html += ".status{display:flex;justify-content:space-around;background:rgba(255,255,255,0.1);padding:15px;border-radius:15px;margin-bottom:20px;}";
        html += ".chat{background:rgba(0,0,0,0.3);border-radius:15px;height:300px;overflow-y:auto;padding:15px;margin-bottom:15px;}";
        html += ".msg{margin:10px 0;padding:12px;border-radius:10px;}";
        html += ".user{background:#0f3460;margin-left:20%;}";
        html += ".bot{background:#e94560;margin-right:20%;}";
        html += ".input{display:flex;gap:10px;}";
        html += "input{flex:1;padding:15px;border:none;border-radius:10px;background:#0f3460;color:white;}";
        html += "button{background:#00d4aa;color:#1a1a2e;border:none;padding:15px 25px;border-radius:10px;font-weight:bold;}";
        html += "</style></head><body>";
        html += "<h1>🤖 Qwen Assistant</h1>";
        html += "<div class='status'>";
        html += "<div>WiFi: " + String(WiFi.status() == WL_CONNECTED ? "🟢" : "🔴") + "</div>";
        html += "<div>BT: " + String(is_bluetooth_connected() ? "🟢" : "🔴") + "</div>";
        html += "<div>Ollama: " + String(check_ollama_connection() ? "🟢" : "🔴") + "</div>";
        html += "</div>";
        html += "<div class='chat' id='chat'><div class='msg bot'>Bonjour! Je suis pret.</div></div>";
        html += "<div class='input'><input type='text' id='msg' placeholder='Votre question...'>";
        html += "<button onclick='send()'>Envoyer</button></div>";
        html += "<script>";
        html += "function send(){";
        html += "var q=document.getElementById('msg').value;";
        html += "fetch('/chat?q='+encodeURIComponent(q)).then(r=>r.text()).then(t=>{";
        html += "document.getElementById('chat').innerHTML+='<div class=\"msg user\">'+q+'</div>';";
        html += "document.getElementById('chat').innerHTML+='<div class=\"msg bot\">'+t+'</div>';";
        html += "});}";
        html += "</script></body></html>";
        server.send(200, "text/html", html);
    });
    
    server.on("/chat", HTTP_GET, []() {
        if (!server.hasArg("q")) {
            server.send(400, "text/plain", "Missing q");
            return;
        }
        String q = server.arg("q");
        auto resp = query_ollama(q.c_str(), nullptr);
        if (resp.success && is_bluetooth_connected()) {
            play_notification();
            speak_text_simple(resp.text.c_str());
        }
        server.send(200, "text/plain", resp.success ? resp.text : "Erreur Ollama");
    });
}

void loop() {
    ArduinoOTA.handle();
    server.handleClient();
    
    if (millis() - last_heartbeat > 30000) {
        last_heartbeat = millis();
        Serial.printf("💓 Heartbeat | WiFi:%d dBm | BT:%s\n", 
            WiFi.RSSI(), is_bluetooth_connected() ? "OK" : "NOK");
    }
    
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        
        if (cmd.startsWith("ASK:")) {
            String q = cmd.substring(4);
            Serial.printf("\n👤 %s\n", q.c_str());
            auto r = query_ollama(q.c_str(), nullptr);
            if (r.success) {
                Serial.printf("🤖 %s\n", r.text.c_str());
                if (is_bluetooth_connected()) {
                    play_notification();
                    speak_text_simple(r.text.c_str());
                }
            } else {
                Serial.println("❌ Erreur Ollama");
            }
        }
        else if (cmd == "STATUS") {
            Serial.printf("WiFi: %s | BT: %s | Ollama: %s\n",
                WiFi.status() == WL_CONNECTED ? "OK" : "NOK",
                is_bluetooth_connected() ? "OK" : "NOK",
                check_ollama_connection() ? "OK" : "NOK");
            Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
        }
        else if (cmd == "TEST") {
            if (is_bluetooth_connected()) {
                play_notification();
                speak_text_simple("Test OK");
                Serial.println("🔊 Test audio");
            } else {
                Serial.println("❌ BT non connecte");
            }
        }
        else if (cmd == "HELP") {
            print_menu();
        }
    }
    delay(10);
}

void print_menu() {
    Serial.println("\n📋 COMMANDES:");
    Serial.println("  ASK:<question>  -> Question a Qwen");
    Serial.println("  STATUS          -> Etat systeme");
    Serial.println("  TEST            -> Test audio BT");
    Serial.println("  HELP            -> Ce menu");
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("🌐 http://%s\n", WiFi.localIP().toString().c_str());
    }
}
