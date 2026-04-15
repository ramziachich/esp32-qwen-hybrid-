#ifndef OLLAMA_CLIENT_H
#define OLLAMA_CLIENT_H

#include <Arduino.h>

struct OllamaResponse {
    String text;
    bool success;
    long response_time_ms;
};

bool check_ollama_connection();
OllamaResponse query_ollama(const char* prompt, const char* system_prompt);

#endif
