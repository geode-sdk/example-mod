#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <deque>
#include <sstream>
#include <vector>

using namespace geode::prelude;

// ===============================================================
// Afterflow v0.3 — SIN COMPILAR. Si algo falla al compilar o en el
// juego, el culpable más probable es la simulación (findDelay).
// ===============================================================

struct Click { float x; bool down; int button; bool p1; };
struct PendingInput { int ticksLeft; bool down; int button; bool player1; };

namespace af {
    // ---- estado de grabación ----
    std::vector<Click> committed;   // clics guardados del nivel
    std::vector<Click> tentative;   // grabación de la pasada actual (se recorta en checkpoints, como un bot)
    std::vector<size_t> cpSizes;    // cuántos clics llevaba la grabación en cada checkpoint
    std::deque<PendingInput> pending;

    // ---- banderas ----
    bool simulating = false;  // dentro de una simulación (ensayo)
    bool simDied = false;     // el ensayo terminó en muerte
    bool internalCp = false;  // checkpoint creado por nosotros para la simulación
    bool replaying = false;   // aplicando un input retrasado
    int levelID = 0;

    constexpr float kTick = 1.f / 240.f;
    using Clock = std::chrono::steady_clock;

    std::string key() { return fmt::format("clicks-{}", levelID); }

    bool near(float x, bool down, int button, bool p1, float tol) {
        for (auto& c : committed) {
            if (c.down == down && c.button == button && c.p1 == p1 && std::fabs(c.x - x) <= tol) return true;
        }
        return false;
    }

    void add(std::vector<Click>& v, Click c) {
        for (auto& o : v) {
            if (o.down == c.down && o.button == c.button && o.p1 == c.p1 && std::fabs(o.x - c.x) < 1.f) return;
        }
        v.push_back(c);
    }

    // Como un bot de macros en práctica: al volver a un checkpoint se borra
    // lo grabado después de él; sin checkpoints se empieza de cero.
    void trimToCheckpoint() {
        tentative.resize(cpSizes.empty() ? 0 : std::min(cpSizes.back(), tentative.size()));
    }

    void commit() {
        for (auto& c : tentative) add(committed, c);
        tentative.clear();
    }

    void save(bool flush = true) {
        std::ostringstream ss;
        for (auto& c : committed) ss << c.x << ',' << c.down << ',' << c.button << ',' << c.p1 << ';';
        Mod::get()->setSavedValue<std::string>(key(), ss.str());
        if (flush) (void)Mod::get()->saveData();
    }

    void load() {
        committed.clear();
        tentative.clear();
        auto s = Mod::get()->getSavedValue<std::string>(key(), "");
        std::istringstream ss(s);
        std::string item;
        while (std::getline(ss, item, ';')) {
            Click c{}; int d = 0, p = 0; char sep;
            std::istringstream is(item);
            if (is >> c.x >> sep >> d >> sep >> c.button >> sep >> p) {
                c.down = d; c.p1 = p;
                committed.push_back(c);
            }
        }
    }

    // ---------------------------------------------------------------
    // Simulación ("jugador fantasma"): guarda el estado con un checkpoint,
    // prueba aplicar el input con retraso 0,1,2... y restaura el estado.
    // Devuelve: 0 = aplicar ahora (o abortado por tiempo), d>0 = retrasar d ticks,
    //          -1 = ningún retraso te salva.
    // ---------------------------------------------------------------
    int findDelay(PlayLayer* pl, bool down, int button, bool p1) {
        int window = (int)Mod::get()->getSettingValue<int64_t>("window-ticks");
        int look = (int)Mod::get()->getSettingValue<int64_t>("lookahead-ticks");
        auto cap = std::chrono::milliseconds(Mod::get()->getSettingValue<int64_t>("time-cap-ms"));

        auto start = Clock::now();
        simulating = true;
        simDied = false;

        // Guardar estado actual como checkpoint temporal
        internalCp = true;
        pl->createCheckpoint();
        internalCp = false;

        auto arr = pl->m_checkpointArray;
        if (!arr || arr->count() == 0) { simulating = false; return 0; }
        auto cp = static_cast<CheckpointObject*>(arr->lastObject());
        cp->retain();
        arr->removeLastObject();   // que no aparezca como checkpoint real

        int result = -1;
        bool aborted = false;

        for (int d = 0; d <= window && !aborted; ++d) {
            simDied = false;
            for (int t = 0; t < d + look && !simDied; ++t) {
                if (Clock::now() - start > cap) { aborted = true; break; }
                if (t == d) pl->handleButton(down, button, p1); // simulating=true: pasa directo
                pl->update(kTick);                              // 1 tick de física
            }
            pl->loadFromCheckpoint(cp);                         // volver al estado real
            if (aborted) break;
            if (!simDied) { result = d; break; }
        }

        cp->release();
        simulating = false;
        simDied = false;
        return aborted ? 0 : result;
    }
}

class $modify(AfterflowLayer, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool player1) {
        using namespace af;

        auto pl = PlayLayer::get();
        bool inPractice = pl && pl->m_isPracticeMode;
        float x = m_player1 ? m_player1->getPositionX() : 0.f;

        // 1) Grabar (solo input real del jugador, en práctica)
        if (inPractice && !simulating && !replaying && Mod::get()->getSettingValue<bool>("record")) {
            add(tentative, {x, down, button, player1});
        }

        bool on = Mod::get()->getSettingValue<bool>("enabled");
        bool fixThis = down ? Mod::get()->getSettingValue<bool>("fix-click")
                            : Mod::get()->getSettingValue<bool>("fix-release");
        bool practiceOnly = Mod::get()->getSettingValue<bool>("practice-only");

        if (!pl || !on || !fixThis || simulating || replaying || (practiceOnly && !inPractice)) {
            GJBaseGameLayer::handleButton(down, button, player1);
            return;
        }

        // 2) Mantener el orden: si ya hay inputs retrasados, este va detrás
        if (!pending.empty()) {
            pending.push_back({pending.back().ticksLeft + 1, down, button, player1});
            return;
        }

        // 3) Solo ayudar cerca de un clic grabado
        if (Mod::get()->getSettingValue<bool>("only-recorded")) {
            float tol = (float)Mod::get()->getSettingValue<int64_t>("tolerance-units");
            if (!near(x, down, button, player1, tol)) {
                GJBaseGameLayer::handleButton(down, button, player1);
                return;
            }
        }

        // 4) Simular y decidir
        int d = findDelay(pl, down, button, player1);
        if (d > 0) {
            pending.push_back({d, down, button, player1});
            return;
        }
        GJBaseGameLayer::handleButton(down, button, player1);
    }

    void processCommands(float dt) {
        using namespace af;
        if (!simulating) {
            for (auto& p : pending) p.ticksLeft--;
            while (!pending.empty() && pending.front().ticksLeft <= 0) {
                auto p = pending.front();
                pending.pop_front();
                replaying = true;
                GJBaseGameLayer::handleButton(p.down, p.button, p.player1);
                replaying = false;
            }
        }
        GJBaseGameLayer::processCommands(dt);
    }
};

class $modify(AfterflowPlay, PlayLayer) {
    struct Fields { CCMenuItemSpriteExtra* btn = nullptr; CCMenuItemSpriteExtra* recBtn = nullptr; };

    static const char* labelFor(bool on) { return on ? "AF ON" : "AF OFF"; }
    static const char* recLabelFor(bool on) { return on ? "REC ON" : "REC OFF"; }

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        af::levelID = level->m_levelID.value();   // si da error: usa (int)level->m_levelID
        af::pending.clear();
        af::cpSizes.clear();
        af::load();

        auto menu = CCMenu::create();
        menu->setPosition({0.f, 0.f});
        auto win = CCDirector::get()->getWinSize();

        bool on = Mod::get()->getSettingValue<bool>("enabled");
        auto spr = ButtonSprite::create(labelFor(on));
        spr->setScale(.5f);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(AfterflowPlay::onToggleAfterflow));
        m_fields->btn = btn;
        btn->setPosition({win.width - 45.f, win.height - 70.f});
        menu->addChild(btn);

        bool rec = Mod::get()->getSettingValue<bool>("record");
        auto recSpr = ButtonSprite::create(recLabelFor(rec));
        recSpr->setScale(.5f);
        auto recBtn = CCMenuItemSpriteExtra::create(recSpr, this, menu_selector(AfterflowPlay::onToggleRecord));
        m_fields->recBtn = recBtn;
        recBtn->setPosition({win.width - 45.f, win.height - 105.f});
        menu->addChild(recBtn);

        this->addChild(menu, 1000);
        return true;
    }

    void onToggleAfterflow(CCObject*) {
        bool on = !Mod::get()->getSettingValue<bool>("enabled");
        Mod::get()->setSettingValue<bool>("enabled", on);
        if (auto spr = typeinfo_cast<ButtonSprite*>(m_fields->btn->getNormalImage())) {
            spr->setString(labelFor(on));
        }
    }

    void onToggleRecord(CCObject*) {
        bool rec = !Mod::get()->getSettingValue<bool>("record");
        Mod::get()->setSettingValue<bool>("record", rec);
        if (!rec) { af::tentative.clear(); af::cpSizes.clear(); }
        if (auto spr = typeinfo_cast<ButtonSprite*>(m_fields->recBtn->getNormalImage())) {
            spr->setString(recLabelFor(rec));
        }
    }

    // Muerte virtual: durante un ensayo no se mata al jugador de verdad.
    void destroyPlayer(PlayerObject* player, GameObject* object) {
        if (af::simulating) { af::simDied = true; return; }
        PlayLayer::destroyPlayer(player, object);
    }

    // Al poner un checkpoint se anota cuántos clics llevaba la grabación.
    void createCheckpoint() {
        PlayLayer::createCheckpoint();
        if (!af::internalCp) af::cpSizes.push_back(af::tentative.size());
    }

    void removeLastCheckpoint() {
        PlayLayer::removeLastCheckpoint();
        if (!af::cpSizes.empty()) af::cpSizes.pop_back();
    }

    // Al reaparecer: se recorta la grabación hasta el último checkpoint.
    void resetLevel() {
        af::trimToCheckpoint();
        af::pending.clear();
        PlayLayer::resetLevel();
    }

    void resetLevelFromStart() {
        af::cpSizes.clear();
        af::tentative.clear();
        af::pending.clear();
        PlayLayer::resetLevelFromStart();
    }

    void togglePracticeMode(bool practice) {
        if (!practice) {
            af::trimToCheckpoint();
            af::commit();
            af::save();
            af::cpSizes.clear();
        }
        PlayLayer::togglePracticeMode(practice);
    }

    void onQuit() {
        if (this->m_isPracticeMode) {
            af::trimToCheckpoint();
            af::commit();
            af::save();
        }
        af::cpSizes.clear();
        af::tentative.clear();
        af::pending.clear();
        PlayLayer::onQuit();
    }

    void levelComplete() {
        af::commit();
        af::save();
        PlayLayer::levelComplete();
    }
};
