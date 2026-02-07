#pragma once
#include <AdvancedLabel.hpp>
#include <cocos2d.h>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/Scrollbar.hpp>

#include "scroll-layer.hpp"
#include "utils.hpp"

class EmojiPicker final : public geode::Popup {
protected:
    CCTextInputNode* m_originalField = nullptr;
    ScrollLayer* m_scrollLayer = nullptr;
    ScrollLayer* m_sidebarPanel = nullptr;
    geode::Scrollbar* m_scrollbar = nullptr;
    CCLayer* m_inputLayer = nullptr;
    bool m_isClosing = false;

public:
    static EmojiPicker* create(CCTextInputNode* input);

protected:
    bool init(CCTextInputNode* input);

    static CCNode* createEmojiSprite(std::string_view emoji);
    static CCNode* encloseInContainer(CCNode* node, float size);

    static std::vector<EmojiCategory> const& getEmojiCategories();
    static std::vector<std::string> getFrequentlyUsedEmojis();
    static std::vector<std::string> getFavoriteEmojis();

    static void incrementEmojiUsage(geode::ZStringView emoji);
    static void toggleFavoriteEmoji(std::string const& emoji);

    void recreateGroups() const;

    CCNode* appendGroup(EmojiCategory const& category) const;
    void updateScrollLayer() const;

    void beginClose();
    void endClose() { this->onClose(nullptr); }

    bool ccTouchBegan(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
};
