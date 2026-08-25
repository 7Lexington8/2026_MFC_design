#pragma once

#include "AppSettings.h"
#include "ConversationManager.h"

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

struct llama_model;
struct llama_context;
struct llama_vocab;

struct GenerationStats
{
    int promptTokens = 0;
    int generatedTokens = 0;
    double elapsedSeconds = 0.0;
    double tokensPerSecond = 0.0;
};

class LLMEngine
{
public:
    using TokenCallback = std::function<void(const std::string&)>;

    LLMEngine();
    ~LLMEngine();

    LLMEngine(const LLMEngine&) = delete;
    LLMEngine& operator=(const LLMEngine&) = delete;

    bool LoadModel(const std::string& pathUtf8, int contextSize, std::string& error);
    void UnloadModel();

    bool IsLoaded() const;
    bool IsBusy() const;
    std::string ModelDescription() const;

    void RequestStop();

    bool Generate(
        const std::vector<ChatMessage>& history,
        const AppSettings& settings,
        const TokenCallback& onToken,
        std::string& fullResponse,
        GenerationStats& stats,
        std::string& error);

private:
    std::string BuildPrompt(const std::vector<ChatMessage>& history, const std::string& systemPrompt, std::string& error) const;
    std::vector<int32_t> Tokenize(const std::string& text, bool addSpecial, std::string& error) const;
    std::string TokenToPiece(int32_t token) const;

    llama_model* model_ = nullptr;
    llama_context* ctx_ = nullptr;
    const llama_vocab* vocab_ = nullptr;

    std::atomic_bool stopRequested_{ false };
    std::atomic_bool busy_{ false };
    mutable std::mutex mutex_;
};
