#include "pch.h"
#include "LLMEngine.h"

#include "llama.h"

#include <array>
#include <cmath>
#include <thread>

LLMEngine::LLMEngine()
{
    // Load available llama.cpp backends (CPU and, when compiled in, CUDA/Vulkan/etc.).
    ggml_backend_load_all();
}

LLMEngine::~LLMEngine()
{
    RequestStop();
    UnloadModel();
}

bool LLMEngine::LoadModel(const std::string& pathUtf8, int contextSize, std::string& error)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (busy_) {
        error = "模型正在推理，不能重新加载。";
        return false;
    }

    if (ctx_) {
        llama_free(ctx_);
        ctx_ = nullptr;
    }
    if (model_) {
        llama_model_free(model_);
        model_ = nullptr;
    }
    vocab_ = nullptr;

    llama_model_params modelParams = llama_model_default_params();
    // If this llama.cpp build supports GPU offload, try to offload as many layers as possible.
    // On a CPU-only build this remains 0 and works normally.
    modelParams.n_gpu_layers = llama_supports_gpu_offload() ? 999 : 0;

    model_ = llama_model_load_from_file(pathUtf8.c_str(), modelParams);
    if (!model_) {
        error = "无法加载 GGUF 模型。请确认文件路径、模型格式和 llama.cpp 版本。";
        return false;
    }

    llama_context_params ctxParams = llama_context_default_params();
    ctxParams.n_ctx = static_cast<uint32_t>(std::clamp(contextSize, 512, 32768));
    ctxParams.n_batch = static_cast<uint32_t>(std::min(512, std::clamp(contextSize, 512, 32768)));

    const unsigned hc = std::thread::hardware_concurrency();
    const int threads = static_cast<int>(hc > 2 ? hc - 1 : std::max(1u, hc));
    ctxParams.n_threads = threads;
    ctxParams.n_threads_batch = threads;
    ctxParams.no_perf = false;

    ctx_ = llama_init_from_model(model_, ctxParams);
    if (!ctx_) {
        llama_model_free(model_);
        model_ = nullptr;
        error = "模型已读取，但创建推理上下文失败。可能是内存不足或上下文长度过大。";
        return false;
    }

    vocab_ = llama_model_get_vocab(model_);
    if (!vocab_) {
        llama_free(ctx_);
        ctx_ = nullptr;
        llama_model_free(model_);
        model_ = nullptr;
        error = "无法读取模型词表。";
        return false;
    }

    stopRequested_ = false;
    error.clear();
    return true;
}

void LLMEngine::UnloadModel()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (busy_) return;

    if (ctx_) {
        llama_free(ctx_);
        ctx_ = nullptr;
    }
    if (model_) {
        llama_model_free(model_);
        model_ = nullptr;
    }
    vocab_ = nullptr;
}

bool LLMEngine::IsLoaded() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return model_ != nullptr && ctx_ != nullptr && vocab_ != nullptr;
}

bool LLMEngine::IsBusy() const
{
    return busy_.load();
}

std::string LLMEngine::ModelDescription() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!model_) return {};
    std::array<char, 512> buf{};
    const int n = llama_model_desc(model_, buf.data(), buf.size());
    if (n <= 0) return {};
    return std::string(buf.data(), static_cast<size_t>(std::min<int>(n, static_cast<int>(buf.size() - 1))));
}

void LLMEngine::RequestStop()
{
    stopRequested_ = true;
}

std::string LLMEngine::BuildPrompt(
    const std::vector<ChatMessage>& history,
    const std::string& systemPrompt,
    std::string& error) const
{
    if (!model_) {
        error = "模型未加载。";
        return {};
    }

    struct OwnedMessage {
        std::string role;
        std::string content;
    };

    std::vector<OwnedMessage> owned;
    owned.reserve(history.size() + 1);
    if (!systemPrompt.empty()) {
        owned.push_back({ "system", systemPrompt });
    }
    for (const auto& m : history) {
        owned.push_back({ m.role == ChatRole::User ? "user" : "assistant", m.content });
    }

    std::vector<llama_chat_message> msgs;
    msgs.reserve(owned.size());
    for (const auto& m : owned) {
        msgs.push_back({ m.role.c_str(), m.content.c_str() });
    }

    const char* tmpl = llama_model_chat_template(model_, nullptr);
    if (tmpl && !msgs.empty()) {
        int needed = llama_chat_apply_template(tmpl, msgs.data(), msgs.size(), true, nullptr, 0);
        if (needed >= 0) {
            std::vector<char> formatted(static_cast<size_t>(needed) + 1, '\0');
            const int written = llama_chat_apply_template(
                tmpl, msgs.data(), msgs.size(), true, formatted.data(), static_cast<int32_t>(formatted.size()));
            if (written >= 0) {
                return std::string(formatted.data(), static_cast<size_t>(written));
            }
        }
    }

    // Fallback for GGUF files without a usable embedded chat template.
    // The supplied SenseNova GGUF is expected to contain a chat template; this fallback
    // keeps the GUI usable with other models, though response quality may vary.
    std::string prompt;
    if (!systemPrompt.empty()) {
        prompt += "System: " + systemPrompt + "\n\n";
    }
    for (const auto& m : history) {
        prompt += (m.role == ChatRole::User ? "User: " : "Assistant: ");
        prompt += m.content;
        prompt += "\n\n";
    }
    prompt += "Assistant: ";
    return prompt;
}

std::vector<int32_t> LLMEngine::Tokenize(const std::string& text, bool addSpecial, std::string& error) const
{
    if (!vocab_) {
        error = "词表未初始化。";
        return {};
    }

    int n = llama_tokenize(vocab_, text.data(), static_cast<int32_t>(text.size()), nullptr, 0, addSpecial, true);
    if (n == 0) return {};
    if (n > 0) {
        error = "llama_tokenize 返回了异常的预估长度。";
        return {};
    }

    n = -n;
    std::vector<llama_token> tokens(static_cast<size_t>(n));
    const int actual = llama_tokenize(
        vocab_, text.data(), static_cast<int32_t>(text.size()), tokens.data(), static_cast<int32_t>(tokens.size()), addSpecial, true);
    if (actual < 0) {
        error = "Prompt 分词失败。";
        return {};
    }
    tokens.resize(static_cast<size_t>(actual));

    std::vector<int32_t> out;
    out.reserve(tokens.size());
    for (llama_token t : tokens) out.push_back(static_cast<int32_t>(t));
    return out;
}

std::string LLMEngine::TokenToPiece(int32_t token) const
{
    std::array<char, 256> smallBuffer{};
    int n = llama_token_to_piece(
        vocab_, static_cast<llama_token>(token), smallBuffer.data(), static_cast<int32_t>(smallBuffer.size()), 0, true);
    if (n >= 0) {
        return std::string(smallBuffer.data(), static_cast<size_t>(n));
    }

    std::vector<char> large(static_cast<size_t>(-n));
    n = llama_token_to_piece(vocab_, static_cast<llama_token>(token), large.data(), static_cast<int32_t>(large.size()), 0, true);
    if (n < 0) return {};
    return std::string(large.data(), static_cast<size_t>(n));
}

bool LLMEngine::Generate(
    const std::vector<ChatMessage>& history,
    const AppSettings& settings,
    const TokenCallback& onToken,
    std::string& fullResponse,
    GenerationStats& stats,
    std::string& error)
{
    std::unique_lock<std::mutex> lock(mutex_);
    if (!model_ || !ctx_ || !vocab_) {
        error = "请先加载 GGUF 模型。";
        return false;
    }
    if (busy_.exchange(true)) {
        error = "已有一个推理任务正在运行。";
        return false;
    }

    struct BusyGuard {
        std::atomic_bool& flag;
        ~BusyGuard() { flag = false; }
    } guard{ busy_ };

    stopRequested_ = false;
    fullResponse.clear();
    stats = {};

    std::string prompt = BuildPrompt(history, settings.systemPrompt, error);
    if (!error.empty()) return false;

    std::vector<int32_t> rawTokens = Tokenize(prompt, true, error);
    if (!error.empty() || rawTokens.empty()) {
        if (error.empty()) error = "Prompt 为空或无法分词。";
        return false;
    }

    std::vector<llama_token> promptTokens;
    promptTokens.reserve(rawTokens.size());
    for (int32_t t : rawTokens) promptTokens.push_back(static_cast<llama_token>(t));

    const int nCtx = static_cast<int>(llama_n_ctx(ctx_));
    const int maxNew = std::clamp(settings.maxTokens, 1, 4096);
    if (static_cast<int>(promptTokens.size()) + 1 >= nCtx) {
        error = "当前会话已超过上下文长度。请新建会话、减少历史内容，或提高 contextSize。";
        return false;
    }

    const int allowedNew = std::min(maxNew, nCtx - static_cast<int>(promptTokens.size()) - 1);
    stats.promptTokens = static_cast<int>(promptTokens.size());

    llama_memory_clear(llama_get_memory(ctx_), true);

    const uint32_t nBatch = std::max<uint32_t>(1, llama_n_batch(ctx_));
    size_t offset = 0;
    while (offset < promptTokens.size()) {
        if (stopRequested_) {
            error = "已停止。";
            return false;
        }
        const size_t take = std::min<size_t>(nBatch, promptTokens.size() - offset);
        llama_batch batch = llama_batch_get_one(promptTokens.data() + offset, static_cast<int32_t>(take));
        const int rc = llama_decode(ctx_, batch);
        if (rc != 0) {
            error = "Prompt 解码失败，llama_decode 返回 " + std::to_string(rc) + "。";
            return false;
        }
        offset += take;
    }

    llama_sampler* sampler = llama_sampler_chain_init(llama_sampler_chain_default_params());
    if (!sampler) {
        error = "创建采样器失败。";
        return false;
    }
    struct SamplerGuard {
        llama_sampler* p;
        ~SamplerGuard() { if (p) llama_sampler_free(p); }
    } samplerGuard{ sampler };

    const int penaltyLastN = std::min(64, nCtx);
    llama_sampler_chain_add(sampler, llama_sampler_init_penalties(
        llama_vocab_n_tokens(vocab_),
        penaltyLastN,
        std::clamp(settings.repeatPenalty, 0.5f, 2.0f),
        0.0f,
        0.0f));

    llama_sampler_chain_add(sampler, llama_sampler_init_top_k(std::max(0, settings.topK)));
    llama_sampler_chain_add(sampler, llama_sampler_init_top_p(std::clamp(settings.topP, 0.0f, 1.0f), 1));

    if (settings.temperature <= 0.0f) {
        llama_sampler_chain_add(sampler, llama_sampler_init_greedy());
    } else {
        llama_sampler_chain_add(sampler, llama_sampler_init_temp(std::clamp(settings.temperature, 0.01f, 2.0f)));
        llama_sampler_chain_add(sampler, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));
    }

    // Seed repetition-penalty history with the prompt itself.
    for (llama_token t : promptTokens) {
        llama_sampler_accept(sampler, t);
    }

    const auto t0 = std::chrono::steady_clock::now();

    for (int i = 0; i < allowedNew; ++i) {
        if (stopRequested_) break;

        // Current llama.cpp llama_sampler_sample() also accepts the selected token
        // into the sampler state, so do not call llama_sampler_accept() a second time here.
        llama_token token = llama_sampler_sample(sampler, ctx_, -1);

        if (llama_vocab_is_eog(vocab_, token)) break;

        std::string piece = TokenToPiece(static_cast<int32_t>(token));
        fullResponse += piece;
        ++stats.generatedTokens;
        if (onToken && !piece.empty()) onToken(piece);

        llama_batch batch = llama_batch_get_one(&token, 1);
        const int rc = llama_decode(ctx_, batch);
        if (rc != 0) {
            error = "生成阶段 llama_decode 失败，返回 " + std::to_string(rc) + "。";
            return false;
        }
    }

    const auto t1 = std::chrono::steady_clock::now();
    stats.elapsedSeconds = std::chrono::duration<double>(t1 - t0).count();
    if (stats.elapsedSeconds > 0.0) {
        stats.tokensPerSecond = stats.generatedTokens / stats.elapsedSeconds;
    }

    if (stopRequested_) {
        error = "已停止。";
        return false;
    }

    error.clear();
    return true;
}
