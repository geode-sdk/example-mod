/**
 * Include the Geode headers.
 */
#include <Geode/Geode.hpp>
// Required to use $modify
#include <Geode/Modify.hpp>

/**
 * Brings cocos2d and all Geode namespaces 
 * to the current scope.
 */
USE_GEODE_NAMESPACE();

/**
 * `$modify` lets you extend and modify GD's 
 * classes; to hook a function in Geode, 
 * simply $modify the class and write a new 
 * function definition with the signature of 
 * the one you want to hook.
 */
class $modify(MenuLayer) {
	/**
	 * MenuLayer::onMoreGames is a GD function, 
	 * so we hook it by simply writing its name 
	 * inside a $modified MenuLayer class.
	 * 
	 * Note that for all hooks, your signature 
	 * has to match exactly - `bool onMoreGames` 
	 * would not place a hook!
	 */
	void onMoreGames(CCObject*) {
		FLAlertLayer::create("Geode", "Hello from my custom mod!", "OK")->show(); 
	} 
};
