#include "emoji-picker.hpp"
#include "emojis.hpp"
#include <Geode/binding/CCTextInputNode.hpp>
#include <Geode/ui/Notification.hpp>

constexpr float ScrollViewHeight = 200.f;
constexpr float ScrollViewWidth = 300.f;
constexpr float ScrollGap = 2.5f;

using namespace geode::prelude;

// https://github.com/TheSillyDoggo/Comment-Emojis/blob/main/src/CCProxyNode.cpp
class ProxyNode final : public CCNode {
public:
    static ProxyNode* create(CCNode* node) {
        auto ret = new ProxyNode();
        ret->m_node = node;
        ret->autorelease();
        return ret;
    }

    void visit() override {
        if (!m_node) return;
        this->setContentSize(m_node->getContentSize());
        CCNode::visit();
    }

    void draw() override {
        auto originalPos = m_node->getPosition();
        m_node->setPosition(
            m_node->isIgnoreAnchorPointForPosition()
                ? CCSize {0.f, 0.f}
                : m_node->getContentSize() * m_node->getAnchorPoint()
        );

        m_node->visit();
        m_node->setPosition(originalPos);
    }

    CCNode* getNode() const { return m_node; }

protected:
    Ref<CCNode> m_node = nullptr;
};

class FeedbackButton final : public CCMenuItemSpriteExtra {
public:
    static FeedbackButton* create(
        CCNode* node, Function<void()>&& callback,
        Function<void()>&& onHold = nullptr
    ) {
        auto ret = new FeedbackButton();
        if (ret->init(node, std::forward<Function<void()>>(callback), std::forward<Function<void()>>(onHold))) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

protected:
    void onClicked(CCObject*) {
        if (m_cancelledTouch) return;
        m_callback();
    }

    void onHold() {
        m_cancelledTouch = true;
        CCMenuItemSpriteExtra::unselected();
        m_onHold();
    }

    void selected() override {
        m_cancelledTouch = false;
        if (m_bEnabled) {
            auto action = CCSequence::create(
                CCDelayTime::create(1.5f),
                CCCallFunc::create(this, callfunc_selector(FeedbackButton::onHold)),
                nullptr
            );
            action->setTag(0x1337);
            this->runAction(action);
        }
        CCMenuItemSpriteExtra::selected();
    }

    void unselected() override {
        this->stopActionByTag(0x1337);
        CCMenuItemSpriteExtra::unselected();
    }

    bool init(CCNode* node, Function<void()> callback, Function<void()> onHold) {
        m_callback = std::move(callback);
        m_onHold = std::move(onHold);

        if (!CCMenuItemSpriteExtra::init(node, node, this, menu_selector(FeedbackButton::onClicked))) {
            return false;
        }

        return true;
    }

protected:
    Function<void()> m_callback = nullptr;
    Function<void()> m_onHold = nullptr;
    bool m_cancelledTouch = false;
};

int64_t getUIScale() {
    static int64_t val = (listenForSettingChanges<int64_t>("ui-scale", [](int64_t value) {
        val = value;
    }), Mod::get()->getSettingValue<int64_t>("ui-scale"));
    return val;
}

static float getUIScaleF() {
    switch (getUIScale()) {
        case 1: default: return 1.f;
        case 2: return 1.2f;
        case 3: return 1.548f;
    }
}

static NineSlice* createBackground(float width, float height) {
    auto background = NineSlice::create("square02_001.png");
    background->setScale(0.5f);
    background->setContentSize({ width * 2.f, height * 2.f });
    background->setOpacity(60);
    return background;
}

EmojiPicker* EmojiPicker::create(CCTextInputNode* input) {
    auto ret = new EmojiPicker();
    if (ret->init(input)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool EmojiPicker::init(CCTextInputNode* input) {
    if (!Popup::init(360.f, ScrollViewHeight + 11.f, "geode.loader/GE_square01.png")) {
        return false;
    }

    m_originalField = input;

    m_sidebarPanel = ::ScrollLayer::create({ 27.f, ScrollViewHeight - 5.f }, false);
    m_sidebarPanel->setID("sidebar-panel"_spr);
    m_mainLayer->addChildAtPosition(m_sidebarPanel, Anchor::BottomLeft, { 8.f, 8.f });

    m_scrollLayer = ::ScrollLayer::create({ ScrollViewWidth, ScrollViewHeight });
    m_scrollLayer->setID("scroll-layer"_spr);
    m_mainLayer->addChildAtPosition(m_scrollLayer, Anchor::BottomLeft, { 40.f, 5.5f });

    recreateGroups();

    auto bg = createBackground(27.f, ScrollViewHeight - 5.f);
    bg->setID("sidebar-bg"_spr);
    bg->setAnchorPoint({ 0, 0 });
    m_mainLayer->addChildAtPosition(bg, Anchor::BottomLeft, { 8.f, 8.f });

    m_scrollLayer->m_contentLayer->setLayout(
        ColumnLayout::create()
            ->setAxisReverse(true)
            ->setAutoScale(false)
            ->setAxisAlignment(AxisAlignment::End)
            ->setCrossAxisAlignment(AxisAlignment::Center)
            ->setCrossAxisLineAlignment(AxisAlignment::Center)
            ->setCrossAxisOverflow(false)
            ->ignoreInvisibleChildren(false)
            ->setGap(ScrollGap)
    );

    m_sidebarPanel->scrollToTop();
    m_scrollLayer->scrollToTop();

    m_scrollbar = Scrollbar::create(m_scrollLayer);
    m_scrollbar->setID("scrollbar"_spr);
    m_mainLayer->addChildAtPosition(m_scrollbar, Anchor::Right, { -10.f, 0.f });

    m_buttonMenu->setVisible(false);
    m_mainLayer->setPositionY(m_mainLayer->getPositionY() - 40.f);

    // create input field zone
    auto background = NineSlice::create("geode.loader/GE_square03.png");
    background->setContentSize({ 360.f, 80.f });

    auto layer = CCMenu::create();
    m_inputLayer = layer;

    auto winSize = CCDirector::get()->getWinSize();
    layer->setPosition(winSize.width * 0.5f, winSize.height * 0.5f + 110.f);
    layer->setContentSize({ 360.f, 80.f });
    layer->setID("input-field-layer"_spr);

    auto inputContainer = ProxyNode::create(input);
    inputContainer->setContentSize({ 360.f, 80.f });
    inputContainer->setID("input-container"_spr);

    auto dummyButtonSprite = CCSprite::create("GJ_button_01.png");
    dummyButtonSprite->setContentSize({ 360.f, 80.f });
    dummyButtonSprite->setVisible(false);

    auto dummyButton = CCMenuItemExt::createSpriteExtra(
        dummyButtonSprite, [this](auto) {
            m_originalField->onClickTrackNode(true);
        }
    );

    layer->addChildAtPosition(background, Anchor::Center);
    layer->addChildAtPosition(inputContainer, Anchor::Center);
    layer->addChildAtPosition(dummyButton, Anchor::Center);

    this->addChild(layer);

    // Animate entry
    m_noElasticity = true;
    this->setOpacity(0);
    this->runAction(CCEaseExponentialOut::create(CCFadeTo::create(0.3f, 105)));

    auto targetPos = m_mainLayer->getPosition();
    m_mainLayer->setPositionY(targetPos.y - 150.f);
    m_mainLayer->runAction(CCEaseExponentialOut::create(CCMoveTo::create(0.5f, targetPos)));

    auto targetPos2 = layer->getPosition();
    layer->setPositionY(targetPos2.y + 50.f);
    layer->runAction(CCEaseExponentialOut::create(CCMoveTo::create(0.5f, targetPos2)));

    return true;
}

void EmojiPicker::recreateGroups() const {
    // remove all children
    m_scrollLayer->m_contentLayer->removeAllChildren();
    m_sidebarPanel->m_contentLayer->removeAllChildren();

    auto sidebarMenu = CCMenu::create();
    sidebarMenu->setAnchorPoint({ 0, 0 });
    sidebarMenu->setPosition({ 0, 0 });
    sidebarMenu->ignoreAnchorPointForPosition(true);

    // add dummy node to pad the top
    auto dummy = CCNode::create();
    dummy->setContentSize({ 27, 0 });
    sidebarMenu->addChild(dummy);

    for (auto& category : getEmojiCategories()) {
        if (category.emojis.empty()) {
            continue;
        }

        CCNode* first = nullptr;

        if (category.name == "Frequently Used") {
            first = CCSprite::createWithSpriteFrameName("GJ_timeIcon_001.png");
        } else if (category.name == "Favorites") {
            first = CCSprite::createWithSpriteFrameName("GJ_starsIcon_001.png");
        } else {
            first = createEmojiSprite(category.icon);
        }

        if (!first) { continue; }
        first = encloseInContainer(first, 18.f);

        auto groupTop = appendGroup(category);
        auto btn = CCMenuItemExt::createSpriteExtra(
            first, [this, groupTop](auto) {
                m_scrollLayer->scrollTo(groupTop);
            }
        );
        sidebarMenu->addChild(btn);
    }

    // add dummy node to pad the bottom
    dummy = CCNode::create();
    dummy->setContentSize({ ScrollViewWidth, 0 });
    m_scrollLayer->m_contentLayer->addChild(dummy);

    // calculate required height
    float height = -ScrollGap;
    for (auto child : m_scrollLayer->m_contentLayer->getChildrenExt()) {
        height += child->getContentSize().height + ScrollGap;
    }
    m_scrollLayer->m_contentLayer->setContentHeight(std::max(height, ScrollViewHeight));

    height = -5.f;
    for (auto child : sidebarMenu->getChildrenExt()) {
        height += child->getContentSize().height + 5.f;
    }
    sidebarMenu->setContentSize({ 27, std::max(height, ScrollViewHeight - 5.f) });

    sidebarMenu->setLayout(
        ColumnLayout::create()
            ->setAxisReverse(true)
            ->setAutoScale(false)
            ->setAxisAlignment(AxisAlignment::End)
            ->setCrossAxisAlignment(AxisAlignment::Center)
            ->setCrossAxisLineAlignment(AxisAlignment::Center)
            ->setCrossAxisOverflow(false)
    );

    m_sidebarPanel->setZOrder(1);
    m_sidebarPanel->m_contentLayer->setContentHeight(std::max(height, ScrollViewHeight - 5.f));
    m_sidebarPanel->m_contentLayer->addChild(sidebarMenu);
}

CCNode* EmojiPicker::createEmojiSprite(std::string_view emoji) {
    std::string_view emojiView;
    bool found = false;
    for (auto& [name, content] : EmojiReplacements) {
        if (name == emoji) {
            emojiView = content;
            found = true;
            break;
        }
    }

    if (!found) {
        return nullptr;
    }

    auto utf32Res = string::utf8ToUtf32(emojiView);
    if (!utf32Res) {
        log::warn("Failed to convert emoji {} to UTF-32", emoji);
        return nullptr;
    }

    auto utf32 = std::move(utf32Res).unwrap();
    auto utf32_raw = std::u32string_view(utf32.data(), utf32.size());
    if (auto it = EmojiSheet.find(utf32_raw); it != EmojiSheet.end()) {
        return CCSprite::createWithSpriteFrameName(it->second);
    }

    if (auto it = CustomNodeSheet.find(utf32_raw); it != CustomNodeSheet.end()) {
        uint32_t index = 0;
        return it->second(U"", index);
    }

    return nullptr;
}

CCNode* EmojiPicker::encloseInContainer(CCNode* node, float size) {
    auto contentSize = node->getContentSize();
    if (contentSize.width != size || contentSize.height != size) {
        // scale to fit
        auto scale = size / std::max(contentSize.width, contentSize.height);
        node->setScale(scale);

        // enclose in a container if not square
        if (contentSize.width != contentSize.height) {
            auto container = CCNode::create();
            container->setContentSize({ size, size });
            container->addChild(node);
            node->setPosition(size / 2, size / 2);
            return container;
        }
    }
    return node;
}

std::vector<EmojiCategory> const& EmojiPicker::getEmojiCategories() {
    static std::vector<EmojiCategory> categories = []() {
        std::vector<EmojiCategory> res = {
            { "Favorites", "", {} },
            { "Frequently Used", "", {} }
        };
        PopulateCategoryInfos(EmojiGroups, res);
        return res;
    }();

    categories[0].emojis = getFavoriteEmojis();
    categories[1].emojis = getFrequentlyUsedEmojis();

    return categories;
}

std::vector<std::string> EmojiPicker::getFrequentlyUsedEmojis() {
    auto emojis = Mod::get()->getSavedValue<StringMap<uint64_t>>("frequently-used-emojis", {});

    std::vector<std::pair<std::string, uint64_t>> sortedEmojis(emojis.begin(), emojis.end());
    std::ranges::sort(sortedEmojis, [](auto const& a, auto const& b) {
        return a.second > b.second;
    });

    auto size = std::min<int64_t>(sortedEmojis.size(), Mod::get()->getSettingValue<int64_t>("frequently-used-emojis-limit"));

    // return the top N emojis
    std::vector<std::string> result;
    result.reserve(size);
    for (int64_t i = 0; i < size; ++i) {
        result.push_back(std::move(sortedEmojis[i].first));
    }

    return result;
}

std::vector<std::string> EmojiPicker::getFavoriteEmojis() {
    return Mod::get()->getSavedValue<std::vector<std::string>>("favorite-emojis", {});
}

void EmojiPicker::incrementEmojiUsage(ZStringView emoji) {
    auto emojis = Mod::get()->getSavedValue<StringMap<uint64_t>>("frequently-used-emojis", {});
    emojis[emoji] += 1;
    Mod::get()->setSavedValue("frequently-used-emojis", emojis);
}

void EmojiPicker::toggleFavoriteEmoji(std::string const& emoji) {
    auto favorites = getFavoriteEmojis();
    auto it = std::ranges::find(favorites, emoji);
    bool found = it != favorites.end();
    if (found) {
        favorites.erase(it);
    } else {
        favorites.push_back(emoji);
    }

    // save changes
    Mod::get()->setSavedValue("favorite-emojis", favorites);

    Notification::create(
        found ? "Removed from favorites" : "Added to favorites",
        CCSprite::createWithSpriteFrameName("GJ_starsIcon_001.png")
    )->show();
}

CCNode* EmojiPicker::appendGroup(EmojiCategory const& category) const {
    auto contentLayer = m_scrollLayer->m_contentLayer;

    auto title = Label::create(category.name, "chatFont.fnt");
    title->setID("emoji-category-title"_spr);
    title->setScale(0.7f);
    title->setAnchorPoint({ 0.f, 0.f });

    auto titleMenu = CCMenu::create();
    titleMenu->setContentSize({ ScrollViewWidth, title->getContentHeight() });
    titleMenu->setAnchorPoint({ 0, 1 });

    auto menu = CCMenu::create();
    menu->setPosition(0, 0);
    menu->setAnchorPoint({ 0, 0 });
    menu->ignoreAnchorPointForPosition(true);
    menu->setContentSize({ ScrollViewWidth - 5.f, 200.f });

    for (auto& emoji : category.emojis) {
        auto sprite = createEmojiSprite(emoji);
        if (!sprite) {
            log::warn("Emoji {} not found", emoji);
            continue;
        }

        sprite = encloseInContainer(sprite, 18.f * getUIScaleF());

        auto item = FeedbackButton::create(
            sprite, [this, emoji] {
                auto cursorPos = m_originalField->m_textField->m_uCursorPos;
                std::string originalText = m_originalField->getString();
                std::string_view sw = originalText;

                // check if text limit is reached
                if (sw.size() + emoji.size() >= 190) {
                    return Notification::create(
                        "Text limit reached",
                        NotificationIcon::Warning
                    )->show();
                }

                // insert emoji at cursor position
                if (cursorPos >= 0) {
                    m_originalField->setString(fmt::format(
                        "{}{}{}",
                        sw.substr(0, cursorPos),
                        emoji, sw.substr(cursorPos)
                    ));

                    auto newCursor = cursorPos + emoji.size();
                    m_originalField->m_textField->m_uCursorPos = newCursor;
                    m_originalField->updateBlinkLabelToChar(newCursor);
                } else {
                    // or append to the end
                    m_originalField->setString(originalText + emoji);
                }

                incrementEmojiUsage(emoji);
            }, [this, emoji] {
                toggleFavoriteEmoji(emoji);
                this->recreateGroups();
                this->updateScrollLayer();
            }
        );
        item->setID(emoji);
        menu->addChild(item);
    }

    auto isCollapsed = Mod::get()->getSaveContainer()["collapsed"][category.name].asBool().unwrapOr(false);
    auto collapseBtnSprite = CCSprite::createWithSpriteFrameName("edit_downBtn_001.png");
    collapseBtnSprite->setScale(0.6f);

    auto collapseBtnNode = CCNode::create();
    collapseBtnNode->setContentSize({
        collapseBtnSprite->getContentSize().width + title->getContentSize().width,
        title->getContentSize().height
    });

    collapseBtnNode->addChild(title);
    collapseBtnNode->addChild(collapseBtnSprite);

    collapseBtnNode->setLayout(
        RowLayout::create()
            ->setAutoScale(false)
            ->setAxisAlignment(AxisAlignment::Start)
            ->setCrossAxisAlignment(AxisAlignment::Center)
            ->setCrossAxisLineAlignment(AxisAlignment::Center)
            ->setGrowCrossAxis(false)
    );

    auto menuContainer = CCNode::create();
    menu->setLayout(
        RowLayout::create()
            ->setAutoScale(false)
            ->setAxisAlignment(AxisAlignment::Start)
            ->setCrossAxisAlignment(AxisAlignment::Even)
            ->setCrossAxisLineAlignment(AxisAlignment::Even)
            ->setGrowCrossAxis(true)
    );

    auto menuHeight = menu->getContentHeight();
    float extraHeight = menuHeight > 18.f ? 5.f : 9.f;
    auto bgHeight = menuHeight + extraHeight;
    menuContainer->setContentSize({ ScrollViewWidth, bgHeight });
    menu->setPosition(2.5f, extraHeight * 0.5f);

    auto bg = createBackground(ScrollViewWidth, bgHeight);
    bg->setPosition(menuContainer->getContentSize() * 0.5f);
    bg->setID("emoji-category-bg"_spr);

    auto collapseBtn = CCMenuItemExt::createSpriteExtra(
        collapseBtnNode, [this, menu, bg, menuContainer, collapseBtnSprite, bgHeight, category = category.name](auto) {
            bool visible = !menu->isVisible();
            Mod::get()->getSaveContainer()["collapsed"][category] = !visible;
            menu->setVisible(visible);
            bg->setVisible(visible);
            collapseBtnSprite->runAction(CCRotateTo::create(0.1f, visible ? 0.f : -90.f));
            menuContainer->setContentHeight(visible ? bgHeight : 0.f );
            this->updateScrollLayer();
        }
    );
    collapseBtn->m_scaleMultiplier = 1.0f;
    collapseBtn->setPosition(collapseBtn->getContentSize() * 0.5f);
    collapseBtn->setID("collapse-btn"_spr);
    titleMenu->addChild(collapseBtn);

    menuContainer->addChild(bg);
    menuContainer->addChild(menu);

    if (isCollapsed) {
        menu->setVisible(false);
        bg->setVisible(false);
        collapseBtnSprite->setRotation(-90.f);
        menuContainer->setContentHeight(0.f);
    }

    contentLayer->addChild(titleMenu);
    contentLayer->addChild(menuContainer);

    return titleMenu;
}

void EmojiPicker::updateScrollLayer() const {
    auto oldHeight = m_scrollLayer->m_contentLayer->getContentHeight();

    // calculate required height
    float height = -ScrollGap;
    for (auto child : m_scrollLayer->m_contentLayer->getChildrenExt()) {
        height += child->getContentSize().height + ScrollGap;
    }

    m_scrollLayer->m_contentLayer->setContentHeight(std::max(height, ScrollViewHeight));

    auto diff = -m_scrollLayer->m_contentLayer->getContentHeight() + oldHeight;

    m_scrollLayer->m_contentLayer->updateLayout();
    m_scrollLayer->scrollLayer(diff);
    m_scrollbar->setTarget(m_scrollLayer);
}

void EmojiPicker::beginClose() {
    if (m_isClosing) {
        return;
    }

    m_isClosing = true;
    m_mainLayer->runAction(CCEaseElasticOut::create(
        CCMoveBy::create(0.5f, { 0, -275.f })
    ));
    m_inputLayer->runAction(CCEaseElasticOut::create(
        CCMoveBy::create(0.5f, { 0, 150.f })
    ));
    this->runAction(CCSequence::create(
        CCFadeTo::create(0.35f, 0),
        CCCallFunc::create(this, callfunc_selector(EmojiPicker::endClose)),
        nullptr
    ));
}

bool EmojiPicker::ccTouchBegan(CCTouch* touch, CCEvent* event) {
    if (!Popup::ccTouchBegan(touch, event)) {
        return false;
    }

    auto middle = m_inputLayer->getPositionX();
    auto location = touch->getLocation();

    constexpr float radius = 200.f;
    if (location.x < middle - radius || location.x > middle + radius) {
        this->beginClose();
    }

    return true;
}
