#include <Geode/modify/CommentCell.hpp>
#include <Geode/modify/GameManager.hpp>
#include <Geode/modify/ShareCommentLayer.hpp>

#include <Geode/Enums.hpp>
#include <Geode/binding/GJComment.hpp>

#include "emoji-picker.hpp"
#include "emojis.hpp"

using namespace geode::prelude;

constexpr bool isValidUsernameChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

std::string replaceEmojis(std::string_view text) {
    auto result = std::string(text);

    // replace emoji placeholders
    for (auto [name, emoji] : EmojiReplacements) {
        findAndReplace(result, name, emoji);
    }

    // parse @mentions
    for (auto it = result.begin(); it != result.end();) {
        if (it == result.end() || *it != '@') {
            ++it;
            continue;
        }

        auto start = it;
        ++it; // skip '@'
        auto nameStart = it;

        while (it != result.end() && isValidUsernameChar(*it)) {
            ++it;
        }

        auto nameLength = static_cast<char>(std::min<ptrdiff_t>(std::distance(start, it), 255));
        if (nameLength <= 1) {
            it = nameStart;
            continue;
        }

        std::string_view name(&*start, std::distance(start, it));
        auto replacement = fmt::format("{}{}{}", MentionCharStr, nameLength, name);

        auto startIndex = static_cast<size_t>(std::distance(result.begin(), start));
        result.replace(startIndex, static_cast<size_t>(nameLength), replacement);

        // restore the iterator
        it = result.begin() + startIndex + replacement.size();
    }

    return result;
}

class $modify(ClearFontCacheHook, GameManager) {
    void reloadAllStep5() {
        GameManager::reloadAllStep5();
        BMFontConfiguration::purgeCachedData();
    }
};

class $modify(CommentCellHook, CommentCell) {
    static void onModify(auto& self) {
        (void) self.setHookPriority("CommentCell::loadFromComment", geode::Priority::LatePost);
    }

    void loadFromComment(GJComment* comment) {
        CommentCell::loadFromComment(comment);

        if (!comment || comment->m_isSpam) {
            return;
        }

        Label* newText;
        cocos2d::ccColor3B changedColor;
        auto commentString = replaceEmojis(comment->m_commentString);
        float maxWidth = 315.f;
        float defaultScale = 1.f;

        if (auto oldText = static_cast<TextArea*>(m_mainLayer->getChildByID("comment-text-area"))) {
            oldText->setVisible(false);
            changedColor = getTextAreaColor(oldText);

            // rescale very long comments
            auto length = commentString.size();
            float scale = length > 64 ? 1.f - std::min((length - 64) * 0.05f, 0.3f) : 1.f;

            newText = Label::createWrapped("", "chatFont.fnt", scale, 315.f);
            newText->setExtraLineSpacing(12.f);
            newText->setBreakWords(48);
            newText->setAnchorPoint({0.f, 0.5f});
            newText->setPosition({11.f, m_accountComment ? 38.f : 33.f});
            newText->setID("comment-text-area"_spr);
        }
        else if (auto oldLabel = static_cast<cocos2d::CCLabelBMFont*>(m_mainLayer->getChildByID("comment-text-label"))) {
            oldLabel->setVisible(false);
            changedColor = oldLabel->getColor();

            newText = Label::create("", "chatFont.fnt");
            newText->setAnchorPoint({0.f, 0.5f});
            newText->setPosition({10.f, 9.f});
            newText->setID("comment-text-label"_spr);
            maxWidth = 270.f;
            defaultScale = 0.7f;
        }
        else {
            return;
        }

        newText->setColor(changedColor);
        newText->enableCustomNodes(&CustomNodeSheet);
        newText->enableEmojis("EmojiSheet.png"_spr, &EmojiSheet);
        newText->setString(commentString);
        newText->limitLabelWidth(maxWidth, defaultScale, 0.1f);

        m_mainLayer->addChild(newText);
    }
};

class $modify(ShareCommentLayerHook, ShareCommentLayer) {
    bool init(gd::string title, int charLimit, CommentType type, int ID, gd::string desc) {
        if (!ShareCommentLayer::init(std::move(title), charLimit, type, ID, std::move(desc))) {
            return false;
        }

        auto btnSprite = CCSprite::create("picker_icon.png"_spr);
        btnSprite->setScale(0.75f);
        btnSprite->setColor({ 0, 0, 0 });
        btnSprite->setOpacity(105);

        auto btn = CCMenuItemExt::createSpriteExtra(
            btnSprite, [this](auto) {
                EmojiPicker::create(m_commentInput)->show();
            }
        );
        btn->setID("emoji-picker"_spr);
        btn->setPosition(175.f, 36.5f);
        this->m_buttonMenu->addChild(btn);

        return true;
    }
};
