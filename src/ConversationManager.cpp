#include "pch.h"
#include "ConversationManager.h"
#include "Utf8.h"

namespace
{
    constexpr char kMagic[] = "LSNV1";

    void WriteU32(std::ofstream& out, uint32_t v)
    {
        out.write(reinterpret_cast<const char*>(&v), sizeof(v));
    }

    bool ReadU32(std::ifstream& in, uint32_t& v)
    {
        return static_cast<bool>(in.read(reinterpret_cast<char*>(&v), sizeof(v)));
    }

    void WriteString(std::ofstream& out, const std::string& s)
    {
        WriteU32(out, static_cast<uint32_t>(s.size()));
        if (!s.empty()) out.write(s.data(), static_cast<std::streamsize>(s.size()));
    }

    bool ReadString(std::ifstream& in, std::string& s)
    {
        uint32_t n = 0;
        if (!ReadU32(in, n)) return false;
        if (n > 32u * 1024u * 1024u) return false;
        s.assign(n, '\0');
        if (n > 0 && !in.read(s.data(), n)) return false;
        return true;
    }
}

ConversationManager::ConversationManager()
{
    NewConversation();
}

size_t ConversationManager::Count() const { return conversations_.size(); }
size_t ConversationManager::CurrentIndex() const { return current_; }

bool ConversationManager::SetCurrent(size_t index)
{
    if (index >= conversations_.size()) return false;
    current_ = index;
    return true;
}

size_t ConversationManager::NewConversation()
{
    Conversation c;
    c.title = L"新会话 " + std::to_wstring(conversations_.size() + 1);
    conversations_.push_back(std::move(c));
    current_ = conversations_.size() - 1;
    return current_;
}

bool ConversationManager::DeleteConversation(size_t index)
{
    if (index >= conversations_.size()) return false;
    conversations_.erase(conversations_.begin() + static_cast<std::ptrdiff_t>(index));
    if (conversations_.empty()) {
        NewConversation();
        return true;
    }
    if (current_ >= conversations_.size()) current_ = conversations_.size() - 1;
    return true;
}

Conversation& ConversationManager::Current() { return conversations_.at(current_); }
const Conversation& ConversationManager::Current() const { return conversations_.at(current_); }
Conversation& ConversationManager::At(size_t index) { return conversations_.at(index); }
const Conversation& ConversationManager::At(size_t index) const { return conversations_.at(index); }

void ConversationManager::AddMessage(size_t index, ChatRole role, const std::string& content)
{
    conversations_.at(index).messages.push_back({ role, content });
}

void ConversationManager::AutoTitleFromFirstUser(size_t index)
{
    auto& c = conversations_.at(index);
    if (c.messages.size() != 1 || c.messages.front().role != ChatRole::User) return;

    bool ok = false;
    std::wstring w = Utf8ToWide(c.messages.front().content, &ok);
    if (!ok || w.empty()) return;
    if (w.size() > 14) w = w.substr(0, 14) + L"…";
    c.title = w;
}

bool ConversationManager::Save(const std::wstring& filePath) const
{
    std::ofstream out(std::filesystem::path(filePath), std::ios::binary | std::ios::trunc);
    if (!out) return false;

    out.write(kMagic, sizeof(kMagic));
    WriteU32(out, static_cast<uint32_t>(conversations_.size()));
    WriteU32(out, static_cast<uint32_t>(current_));

    for (const auto& c : conversations_) {
        WriteString(out, WideToUtf8(c.title));
        WriteU32(out, static_cast<uint32_t>(c.messages.size()));
        for (const auto& m : c.messages) {
            const uint8_t role = static_cast<uint8_t>(m.role);
            out.write(reinterpret_cast<const char*>(&role), sizeof(role));
            WriteString(out, m.content);
        }
    }
    return static_cast<bool>(out);
}

bool ConversationManager::Load(const std::wstring& filePath)
{
    std::ifstream in(std::filesystem::path(filePath), std::ios::binary);
    if (!in) return false;

    char magic[sizeof(kMagic)]{};
    if (!in.read(magic, sizeof(magic))) return false;
    if (std::memcmp(magic, kMagic, sizeof(kMagic)) != 0) return false;

    uint32_t count = 0, current = 0;
    if (!ReadU32(in, count) || !ReadU32(in, current)) return false;
    if (count == 0 || count > 1000) return false;

    std::vector<Conversation> tmp;
    tmp.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        std::string title8;
        if (!ReadString(in, title8)) return false;
        bool ok = false;
        std::wstring title = Utf8ToWide(title8, &ok);
        if (!ok) return false;

        uint32_t nMessages = 0;
        if (!ReadU32(in, nMessages) || nMessages > 100000) return false;

        Conversation c;
        c.title = std::move(title);
        c.messages.reserve(nMessages);
        for (uint32_t j = 0; j < nMessages; ++j) {
            uint8_t role = 0;
            if (!in.read(reinterpret_cast<char*>(&role), sizeof(role))) return false;
            if (role > static_cast<uint8_t>(ChatRole::Assistant)) return false;
            std::string content;
            if (!ReadString(in, content)) return false;
            c.messages.push_back({ static_cast<ChatRole>(role), std::move(content) });
        }
        tmp.push_back(std::move(c));
    }

    conversations_ = std::move(tmp);
    current_ = std::min<size_t>(current, conversations_.size() - 1);
    return true;
}
