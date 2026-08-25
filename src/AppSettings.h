#pragma once

#include <string>

struct AppSettings
{
    std::string systemPrompt = "你是一名严谨、友好的中文助手。回答要准确、清楚，必要时给出步骤。";
    float temperature = 0.80f;
    float topP = 0.95f;
    int topK = 40;
    float repeatPenalty = 1.10f;
    int maxTokens = 512;
    int contextSize = 4096;
};
