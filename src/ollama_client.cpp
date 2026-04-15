#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

const char* OLLAMA_HOST = "192.168.43.1";
const int OLLAMA_PORT = 11434;
const char* MODEL_NAME = "qwen2.5:0.5b";
const int HTTP_TIMEOUT = 30000;
const int MAX_TOKENS = 150;

struct OllamaResponse {
    String text;
    bool success;
    long response_time_ms;
};

bool check_ollama_connection() {
    HTTPClient http;
    String url = String("http://") + OLLAMA_HOST + ":" + OLLAMA_PORT + "/api/tags";
    http.begin(url);
    http.setTimeout(5000);
    int code = http.GET();
    http.end();
    return (code == 200);
}

OllamaResponse query_ollama(const char* prompt, const char* system_prompt) {
    OllamaResponse result = {"", false, 0};
    HTTPClient http;
    String url = String("http://") + OLLAMA_HOST + ":" + OLLAMA_PORT + "/api/generate";
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(HTTP_TIMEOUT);
    
    String full_prompt = "<|im_start|>system\n";
    full_prompt += system_prompt ? system_prompt : "Tu es un assistant utile et concis. Reponds en francais, maximum 2 phrases.";
    full_prompt += "<|im_end|>\n<|im_start|>user\n";
    full_prompt += prompt;
    full_prompt += "<|im_end|>\n<|im_start|>assistant\n";
    
    StaticJsonDocument<512> doc;
    doc["model"] = MODEL_NAME;
    doc["prompt"] = full_prompt;
    doc["stream"] = false;
    doc["options"]["temperature"] = 0.7;
    doc["options"]["top_p"] = 0.9;
    doc["options"]["num_predict"] = MAX_TOKENS;
    
    String requestBody;
    serializeJson(doc, requestBody);
    
    unsigned long start_time = millis();
    int httpCode = http.POST(requestBody);
    result.response_time_ms = millis() - start_time;
    
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        StaticJsonDocument<4096> response;
        DeserializationError error = deserializeJson(response, payload);
        
        if (!error) {
            result.text = response["response"].as<String>();
            result.text.replace("<|im_end|>", "");
            result.text.replace("<|endoftext|>", "");
            result.text.trim();
            result.success = true;
        }
    }
    
    http.end();
    return result;
}
