// MINIMAL STAND-IN for the Geode/cocos2d API, used only by tests/syntax_check.sh to type-check the glue code
// in src/ (DecoManager, popups, ...) for internal consistency. It encodes my ASSUMPTIONS about the real API
// and therefore cannot prove the real SDK matches them - only a real `geode build` can.
#pragma once
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <format>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

typedef unsigned char GLubyte;
namespace cocos2d {
struct CCPoint { float x = 0, y = 0; CCPoint() = default; CCPoint(float a, float b) : x(a), y(b) {} };
struct CCSize { float width = 0, height = 0; CCSize() = default; CCSize(float w, float h) : width(w), height(h) {} };
struct CCRect {
    CCPoint origin; CCSize size;
    float getMinX() const { return origin.x; } float getMaxX() const { return origin.x + size.width; }
    float getMinY() const { return origin.y; } float getMaxY() const { return origin.y + size.height; }
};
struct ccColor3B { GLubyte r, g, b; };
struct ccColor4B { GLubyte r, g, b, a; };
inline CCPoint ccp(float x, float y) { return {x, y}; }
inline ccColor4B ccc4(GLubyte r, GLubyte g, GLubyte b, GLubyte a) { return {r, g, b, a}; }
enum CCTextAlignment { kCCTextAlignmentLeft, kCCTextAlignmentCenter };

class CCObject { public: virtual ~CCObject() = default; void autorelease() {} void retain() {} void release() {} };
typedef void (CCObject::*SEL_MenuHandler)(CCObject*);
typedef void (CCObject::*SEL_SCHEDULE)(float);
#define menu_selector(f) static_cast<cocos2d::SEL_MenuHandler>(&f)
#define schedule_selector(f) static_cast<cocos2d::SEL_SCHEDULE>(&f)
}  // namespace cocos2d

namespace geode {
enum class Anchor { Center, BottomLeft, Bottom };
template <class T> class Ref {
    T* p = nullptr;
public:
    Ref() = default; Ref(T* t) : p(t) {} Ref(Ref const&) = default; Ref& operator=(Ref const&) = default;
    Ref& operator=(T* t) { p = t; return *this; }
    T* data() const { return p; } T* operator->() const { return p; } operator T*() const { return p; }
};
}  // namespace geode

namespace cocos2d {
class CCNode : public CCObject {
public:
    static CCNode* create() { return new CCNode(); }
    virtual bool init() { return true; }
    virtual void onExit() {}
    void addChild(CCNode*, int = 0) {} void removeFromParent() {} void removeFromParentAndCleanup(bool) {}
    void removeAllChildren() {}
    void setPosition(CCPoint const&) {} CCPoint getPosition() const { return {}; }
    void setContentSize(CCSize const&) {} CCSize getContentSize() const { return {}; }
    void setAnchorPoint(CCPoint const&) {} void ignoreAnchorPointForPosition(bool) {}
    void setScale(float) {} float getScale() const { return 1; } float getScaleX() const { return 1; } float getScaleY() const { return 1; }
    void setVisible(bool) {} CCNode* getParent() const { return nullptr; } CCRect boundingBox() const { return {}; }
    void setTag(int) {} int getTag() const { return 0; }
    void setID(std::string const&) {} CCNode* getChildByID(std::string const&) const { return nullptr; }
    void scheduleOnce(SEL_SCHEDULE, float) {} void unschedule(SEL_SCHEDULE) {}
    void addChildAtPosition(CCNode*, geode::Anchor, CCPoint = {}, bool = true) {}
};
class CCNodeRGBA : public CCNode { public: void setColor(ccColor3B const&) {} void setOpacity(GLubyte) {} };
class CCLayer : public CCNode {};
class CCLayerColor : public CCLayer { public: static CCLayerColor* create(ccColor4B const&, float, float) { return new CCLayerColor(); } };
class CCLayerGradient : public CCLayer {
public: static CCLayerGradient* create(ccColor4B const&, ccColor4B const&, CCPoint const&) { return new CCLayerGradient(); } };
class CCSprite : public CCNodeRGBA { public: static CCSprite* createWithSpriteFrameName(char const*) { return new CCSprite(); } };
class CCLabelBMFont : public CCNodeRGBA {
public:
    static CCLabelBMFont* create(char const*, char const*) { return new CCLabelBMFont(); }
    static CCLabelBMFont* create(char const*, char const*, float, CCTextAlignment) { return new CCLabelBMFont(); }
    void setString(char const*) {} void limitLabelWidth(float, float, float) {}
};
class CCMenu : public CCLayer { public: static CCMenu* create() { return new CCMenu(); } void setTouchPriority(int) {} };
class CCMenuItem : public CCNode { public: void setEnabled(bool) {} };
class CCMenuItemSpriteExtra : public CCMenuItem {
public: static CCMenuItemSpriteExtra* create(CCNode*, CCNode*, CCObject*, SEL_MenuHandler) { return new CCMenuItemSpriteExtra(); }
        static CCMenuItemSpriteExtra* create(CCNode*, CCObject*, SEL_MenuHandler) { return new CCMenuItemSpriteExtra(); } };
class CCMenuItemToggler : public CCMenuItem {
public: static CCMenuItemToggler* createWithStandardSprites(CCObject*, SEL_MenuHandler, float) { return new CCMenuItemToggler(); }
        void toggle(bool) {} bool isToggled() const { return false; } };
class CCScene : public CCNode {};
class CCDirector { public: static CCDirector* get() { static CCDirector d; return &d; } CCSize getWinSize() { return {568, 320}; } CCScene* getRunningScene() { return nullptr; } };
class CCSpriteFrame { public: CCSize getOriginalSize() const { return {30, 30}; } };
class CCSpriteFrameCache { public: static CCSpriteFrameCache* get() { static CCSpriteFrameCache c; return &c; } CCSpriteFrame* spriteFrameByName(char const*) { return nullptr; } };
class CCArray : public CCObject { public: unsigned count() const { return 0; } std::vector<CCObject*> v; };
namespace extension { class CCScale9Sprite : public CCNodeRGBA { public: static CCScale9Sprite* create(char const*) { return new CCScale9Sprite(); } }; }
}  // namespace cocos2d

namespace geode {
template <class T> struct CCArrayExt {
    cocos2d::CCArray* a; explicit CCArrayExt(cocos2d::CCArray* x) : a(x) {}
    struct It { cocos2d::CCObject** p; T operator*() const { return static_cast<T>(*p); } It& operator++() { ++p; return *this; } bool operator!=(It const& o) const { return p != o.p; } };
    It begin() { return {a->v.data()}; } It end() { return {a->v.data() + a->v.size()}; }
};
template <class T, class U> T typeinfo_cast(U* u) { return dynamic_cast<T>(u); }
}  // namespace geode
inline std::string operator""_spr(char const* s, size_t n) { return "yourname.auto_decorator/" + std::string(s, n); }
#define $modify(Name, Base) Name : public Base

// ---- game classes ----
enum class GameObjectType { Solid, Hazard, Decoration, Slope, Other };
struct GJSpriteColor { int m_colorID = 0; };
class GameObject : public cocos2d::CCNodeRGBA {
public:
    GameObjectType m_objectType = GameObjectType::Solid; int m_objectID = 0;
    GJSpriteColor* m_baseColor = nullptr; GJSpriteColor* m_detailColor = nullptr;
    static GameObject* createWithKey(int) { return new GameObject(); }
};
class EffectGameObject : public GameObject {};
class ObjectToolbox { public: static ObjectToolbox* sharedState() { static ObjectToolbox t; return &t; } char const* intKeyToFrame(int) { return ""; } };
class EditorUI; 
class LevelEditorLayer : public cocos2d::CCLayer {
public:
    cocos2d::CCArray* getAllObjects() { return nullptr; }
    cocos2d::CCArray* createObjectsFromString(std::string const&, bool, bool) { return nullptr; }
    void removeObject(GameObject*, bool) {}
    void runColorEffect(EffectGameObject*, int, float, float, bool) {}
    cocos2d::CCLayer* m_objectLayer = nullptr; EditorUI* m_editorUI = nullptr;
};
class EditorUI : public cocos2d::CCLayer {
public:
    virtual bool init(LevelEditorLayer*) { return true; }
    cocos2d::CCPoint getLimitedPosition(cocos2d::CCPoint p) { return p; }
    cocos2d::CCMenuItemSpriteExtra *m_buildModeBtn = nullptr, *m_editModeBtn = nullptr, *m_deleteModeBtn = nullptr;
    LevelEditorLayer* m_editorLayer = nullptr;
};
class ButtonSprite : public cocos2d::CCSprite { public: static ButtonSprite* create(char const*, int, bool, char const*, char const*, float, float) { return new ButtonSprite(); } };

// ---- geode utilities ----
namespace geode {
using namespace cocos2d; using namespace cocos2d::extension;
struct Version { std::string toVString() const { return "v1.0.0"; } };
template <class T> struct Result { bool isOk() const { return true; } bool isErr() const { return false; } T unwrap() const { return {}; } };
class Mod {
public:
    static Mod* get() { static Mod m; return &m; }
    template <class T> T getSettingValue(std::string const&) { return T{}; }
    template <class T> T getSavedValue(std::string const&, T def = T{}) { return def; }
    template <class T> void setSavedValue(std::string const&, T) {}
    std::filesystem::path getResourcesDir() { return {}; } std::filesystem::path getSaveDir() { return {}; }
    Version getVersion() { return {}; }
};
namespace utils::file { inline Result<std::string> readString(std::filesystem::path const&) { return {}; } }
namespace log { template <class... A> void info(std::format_string<A...> f, A&&... a) { (void)std::vformat(f.get(), std::make_format_args(a...)); }
                template <class... A> void warn(std::format_string<A...> f, A&&... a) { (void)std::vformat(f.get(), std::make_format_args(a...)); } }
enum class NotificationIcon { None, Success, Error, Warning };
struct Notification { static Notification* create(std::string const&, NotificationIcon, float) { return new Notification(); } void show() {} };
struct MDPopup { static MDPopup* create(char const*, std::string const&, char const*) { return new MDPopup(); } void show() {} };
inline void openSettingsPopup(Mod*, bool) {}
class Popup : public cocos2d::CCNode {
public:
    void show() {}
protected:
    bool init(float, float) { return true; }
    virtual void onClose(cocos2d::CCObject*) {}
    cocos2d::CCSize m_size; cocos2d::CCNode* m_mainLayer = nullptr; cocos2d::CCMenu* m_buttonMenu = nullptr;
    cocos2d::extension::CCScale9Sprite* m_bgSprite = nullptr; cocos2d::CCMenuItemSpriteExtra* m_closeBtn = nullptr;
};
class TextInput : public cocos2d::CCNode {
public:
    static TextInput* create(float, std::string const&, std::string const&) { return new TextInput(); }
    void setFilter(std::string const&) {} void setMaxCharCount(size_t) {} void setString(std::string const&) {}
    std::string getString() const { return {}; } void setCallback(std::function<void(std::string const&)>) {}
};
class ColorPickPopupDelegate { public: virtual ~ColorPickPopupDelegate() = default; virtual void updateColor(cocos2d::ccColor4B const&) {} };
class ColorPickPopup { public: static ColorPickPopup* create(cocos2d::ccColor4B const&) { return new ColorPickPopup(); } void setDelegate(ColorPickPopupDelegate*) {} void show() {} };
namespace prelude { using namespace geode; using namespace cocos2d; using namespace cocos2d::extension; }
}  // namespace geode
namespace fmt { template <class... A> std::string format(std::format_string<A...> f, A&&... a) { return std::format(f, std::forward<A>(a)...); } }
