#include <Geode.hpp>

class $modify(MenuLayer) {
	void onMoreGames(cocos2d::CCObject*) {
		FLAlertLayer::create("Geode", "Hello from custom mod!", "OK")->show(); 
	} 
};
