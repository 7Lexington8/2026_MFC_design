#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

enum class ChatRole : uint8_t
{
    User = 0,
    Assistant = 1
};

struct ChatMessage
{
    ChatRole role = ChatRole::User;
    std::string content;
};

struct Conversation
{
    std::wstring title;
    std::vector<ChatMessage> messages;
};

class ConversationManager
{
public:
    ConversationManager();

    size_t Count() const;
    size_t CurrentIndex() const;
    bool SetCurrent(size_t index);

    size_t NewConversation();
    bool DeleteConversation(size_t index);

    Conversation& Current();
    const Conversation& Current() const;
    Conversation& At(size_t index);
    const Conversation& At(size_t index) const;

    void AddMessage(size_t index, ChatRole role, const std::string& content);
    void AutoTitleFromFirstUser(size_t index);

    bool Save(const std::wstring& filePath) const;
    bool Load(const std::wstring& filePath);

private:
    std::vector<Conversation> conversations_;
    size_t current_ = 0;
};
