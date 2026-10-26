// Predictive Buffer (remake) - UNTESTED SCAFFOLD.
// Lines marked VERIFY use game functions whose exact names/signatures must be
// checked against the Geode bindings for your GD version.

#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <algorithm>
#include <deque>

using namespace geode::prelude;

namespace {
    constexpr float TICK = 1.f / 240.f; // one physics step; 4 ticks = 1 frame @ 60fps
    bool g_probing = false;             // true while simulating the future
    bool g_probeDied = false;           // set if the simulated player dies
    bool g_levelReset = false;          // tells the layer to drop queued taps
}

struct Pending {
    bool down;
    int button;
    bool p1;
    double due; // seconds on our own clock
};

// Death detection: while probing, record the death instead of really dying.
class $modify(BufferPlayLayer, PlayLayer) {
    void destroyPlayer(PlayerObject* player, GameObject* obj) { // VERIFY
        if (g_probing) {
            g_probeDied = true;
            return;
        }
        PlayLayer::destroyPlayer(player, obj);
    }

    void resetLevel() { // VERIFY
        g_levelReset = true;
        PlayLayer::resetLevel();
    }
};

class $modify(BufferLayer, GJBaseGameLayer) {
    struct Fields {
        std::deque<Pending> queue;
        std::deque<double> presses; // recent press times, for the spam guard
        double clock = 0.0;
    };

    bool isSpam(bool down, double now) {
        auto mod = Mod::get();
        if (!mod->getSettingValue<bool>("spam-guard")) return false;
        auto& p = m_fields->presses;
        if (down) {
            p.push_back(now);
            while (p.size() > 3) p.pop_front();
        }
        if (p.size() < 3) return false;
        double gap = mod->getSettingValue<int64_t>("spam-interval") * TICK;
        return (p[2] - p[1]) < gap && (p[1] - p[0]) < gap;
    }

    // Simulate `look` ticks with the input applied after `delayTicks`; true if it kills us.
    bool probeDies(bool down, int button, bool p1, int delayTicks, int look) {
        auto pl = typeinfo_cast<PlayLayer*>(this);
        if (!pl) return false;

        auto snapshot = CheckpointObject::create();      // VERIFY
        snapshot->retain();
        pl->storeCheckpoint(snapshot);                   // VERIFY

        g_probing = true;
        g_probeDied = false;
        for (int t = 0; t <= look && !g_probeDied; ++t) {
            if (t == delayTicks) GJBaseGameLayer::handleButton(down, button, p1);
            GJBaseGameLayer::processCommands(TICK);
        }
        g_probing = false;

        pl->loadFromCheckpoint(snapshot);                // VERIFY
        snapshot->release();
        return g_probeDied;
    }

    void handleButton(bool down, int button, bool isPlayer1) {
        auto mod = Mod::get();
        auto pl = typeinfo_cast<PlayLayer*>(this);
        if (g_probing || !pl || button != 1 || !mod->getSettingValue<bool>("enabled")) {
            return GJBaseGameLayer::handleButton(down, button, isPlayer1);
        }

        auto player = isPlayer1 ? m_player1 : m_player2;
        bool wave = player && player->m_isDart;
        int maxTicks = mod->getSettingValue<int64_t>(wave ? "wave-ticks" : "normal-ticks");
        int look = mod->getSettingValue<int64_t>("look-ahead");
        double now = m_fields->clock;

        // Only presses matter outside wave; in wave, releases change direction too.
        bool spam = isSpam(down, now);
        if ((!down && !wave) || maxTicks <= 0 || spam) {
            return GJBaseGameLayer::handleButton(down, button, isPlayer1);
        }

        // Safe to press now? Then do nothing special.
        if (!probeDies(down, button, isPlayer1, 0, look)) {
            return GJBaseGameLayer::handleButton(down, button, isPlayer1);
        }

        // Find the smallest delay that survives.
        for (int k = 1; k <= maxTicks; ++k) {
            if (!probeDies(down, button, isPlayer1, k, look)) {
                Pending in{down, button, isPlayer1, now + k * TICK};
                auto& q = m_fields->queue;
                auto pos = std::upper_bound(q.begin(), q.end(), in.due,
                    [](double d, Pending const& e) { return d < e.due; });
                q.insert(pos, in);
                return;
            }
        }
        // No delay helps: let the tap through unchanged.
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }

    void processCommands(float dt) {
        if (g_probing) return GJBaseGameLayer::processCommands(dt);

        auto& f = *m_fields;
        if (g_levelReset) {
            f.queue.clear();
            g_levelReset = false;
        }

        double end = f.clock + dt;
        float done = 0.f;
        while (!f.queue.empty() && f.queue.front().due <= end) {
            auto in = f.queue.front();
            f.queue.pop_front();
            float off = std::clamp(static_cast<float>(in.due - f.clock), done, dt);
            if (off > done) {
                GJBaseGameLayer::processCommands(off - done);
                done = off;
            }
            GJBaseGameLayer::handleButton(in.down, in.button, in.p1);
        }
        if (dt - done > 0.f) GJBaseGameLayer::processCommands(dt - done);
        f.clock = end;
    }
};
