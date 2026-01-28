#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>

using namespace geode::prelude;

// Definimos la clase de nuestro Mod Menu
class MyModMenu : public FLAlertLayer {
public:
    static MyModMenu* create() {
        auto ret = new MyModMenu();
        if (ret && ret->init(150, 200, "GJ_square01.png", "Mi Mod Menu")) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

    bool init(float w, float h, const char* bg, const char* title) {
        if (!FLAlertLayer::init(w, h, bg, title)) return false;

        auto menu = CCMenu::create();
        this->m_mainLayer->addChild(menu);

        // Aquí añadirías los botones de "No Clip", "Speedhack", etc.
        auto label = CCLabelBMFont::create("Opciones aqui", "bigFont.fnt");
        label->setScale(0.5f);
        menu->addChild(label);

        return true;
    }
};

// Inyectamos el botón en el menú principal
class $modify(MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        auto myButton = CCMenuItemSpriteExtra::create(
            CCSprite::createWithSpriteFrameName("GJ_settingsBtn_001.png"),
            this,
            menu_selector(MenuLayer::onMyMenu)
        );

        auto menu = this->getChildByID("bottom-menu");
        menu->addChild(myButton);
        
        return true;
    }

    void onMyMenu(CCObject*) {
        MyModMenu::create()->show();
    }
};
