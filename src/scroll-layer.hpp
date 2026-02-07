// https://github.com/EclipseMenu/EclipseMenu/blob/main/src/modules/gui/cocos/popup/scroll-layer.hpp
#pragma once
#include <cocos2d.h>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/binding/CCScrollLayerExt.hpp>
#include <Geode/binding/CCScrollLayerExtDelegate.hpp>

// taken from Object Workshop (a hybrid of robtops cocos and geode)
class ScrollLayer : public CCScrollLayerExt, public CCScrollLayerExtDelegate {
protected:
    cocos2d::CCTouch* m_touchStart{};
    cocos2d::CCPoint m_touchStartPosition2;
    cocos2d::CCPoint m_touchPosition2;
    float m_touchLastY{};
    bool m_scrollWheelEnabled;
    bool m_touchMoved{};
    bool m_cancellingTouches{};

    void cancelAndStoleTouch(cocos2d::CCTouch*, cocos2d::CCEvent*);
    void checkBoundaryOfContent(float);
    void claimTouch(cocos2d::CCTouch*);
    void touchFinish(cocos2d::CCTouch*);

    bool ccTouchBegan(cocos2d::CCTouch*, cocos2d::CCEvent*) override;
    void ccTouchMoved(cocos2d::CCTouch*, cocos2d::CCEvent*) override;

    void ccTouchEnded(cocos2d::CCTouch*, cocos2d::CCEvent*) override;
    void ccTouchCancelled(cocos2d::CCTouch*, cocos2d::CCEvent*) override;
    void scrollWheel(float, float) override;
    void visit() override;
    void registerWithTouchDispatcher() override;
    CCMenuItemSpriteExtra* itemForTouch(cocos2d::CCTouch*);

    ScrollLayer(cocos2d::CCRect const& rect, bool scrollWheelEnabled, bool vertical);
    ~ScrollLayer() override;

public:
    void scrollToTop() const;
    void scrollTo(CCNode* node) const;
    static ScrollLayer* create(cocos2d::CCRect const& rect, bool scrollWheelEnabled = true, bool vertical = true);
    static ScrollLayer* create(cocos2d::CCSize const& size, bool scrollWheelEnabled = true, bool vertical = true);
};