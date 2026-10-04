#include "Roulette.hpp"

#include <Geode/utils/file.hpp>
#include <Geode/utils/random.hpp>
#include <ctime>
#include <regex>

// ---------------------------------------------------------------------------
// Dificultades y especiales
// ---------------------------------------------------------------------------

static DiffInfo const DIFFS[DIFF_COUNT + 1] = {
    { "?",             0,  "-",  0, -1 },
    { "Easy",          1,  "1",  0, -1 },
    { "Normal",        2,  "2",  0, -1 },
    { "Hard",          3,  "3",  0, -1 },
    { "Harder",        4,  "4",  0, -1 },
    { "Insane",        5,  "5",  0, -1 },
    { "Easy Demon",    7,  "-2", 1, 3 },
    { "Medium Demon",  8,  "-2", 2, 4 },
    { "Hard Demon",    6,  "-2", 3, 0 },
    { "Insane Demon",  9,  "-2", 4, 5 },
    { "Extreme Demon", 10, "-2", 5, 6 },
};

DiffInfo const& diffInfo(int diff) {
    if (diff < 1 || diff > DIFF_COUNT) return DIFFS[0];
    return DIFFS[diff];
}

static SpecialInfo const SPECIALS[SPECIAL_COUNT] = {
    { "Recent",           "GJ_sRecentIcon_001.png",  nullptr,            nullptr },
    { "Random (all of GD)", nullptr,                 nullptr,            nullptr },
    { "Friends",          "GJ_sFriendsIcon_001.png", nullptr,            nullptr },
    { "Cube Challenge",   "gj_iconBtn_on_001.png",   "cube challenge",   R"(\bcube\b.*\bchall)" },
    { "Ship Challenge",   "gj_shipBtn_on_001.png",   "ship challenge",   R"(\bship\b.*\bchall)" },
    { "Ball Challenge",   "gj_ballBtn_on_001.png",   "ball challenge",   R"(\bball\b.*\bchall)" },
    { "UFO Challenge",    "gj_birdBtn_on_001.png",   "ufo challenge",    R"(\bufo\b.*\bchall)" },
    { "Wave Challenge",   "gj_dartBtn_on_001.png",   "wave challenge",   R"(\bwave\b.*\bchall)" },
    { "Robot Challenge",  "gj_robotBtn_on_001.png",  "robot challenge",  R"(\brobot\b.*\bchall)" },
    { "Spider Challenge", "gj_spiderBtn_on_001.png", "spider challenge", R"(\bspider\b.*\bchall)" },
    { "Swing Challenge",  "gj_swingBtn_on_001.png",  "swing challenge",  R"(\bswing\b.*\bchall)" },
    { "Challenge List",   "rankIcon_top10_001.png",  nullptr,            nullptr },
};

SpecialInfo const& specialInfo(int special) {
    return SPECIALS[std::clamp(special, 0, SPECIAL_COUNT - 1)];
}

std::string RouletteConfig::categoryKey() const {
    std::string key;
    switch (mode) {
        case RouletteMode::Difficulty: key = fmt::format("diff:{}", difficulty); break;
        case RouletteMode::Demonlist: key = fmt::format("top:{}-{}", topFrom, topTo); break;
        case RouletteMode::Special: key = fmt::format("sp:{}", special); break;
    }
    auto len = this->effectiveLength();
    if (len != LengthFilter::Classic) key += fmt::format("|L{}", (int)len);
    if (mode == RouletteMode::Difficulty && popular) key += fmt::format("|P{}", minDownloads);
    return key;
}

std::string RouletteConfig::categoryName() const {
    std::string name;
    switch (mode) {
        case RouletteMode::Difficulty: name = diffInfo(difficulty).name; break;
        case RouletteMode::Special: name = specialInfo(special).name; break;
        case RouletteMode::Demonlist: {
            if (topFrom == 1 && topTo == 75) name = "Main List";
            else if (topFrom == 76 && topTo == 150) name = "Extended List";
            else if (topFrom == 151 && topTo >= 9999) name = "Legacy List";
            else if (topTo >= 9999) name = fmt::format("Top {}+", topFrom);
            else if (topFrom <= 1) name = fmt::format("Top {}", topTo);
            else name = fmt::format("Top {} - {}", topFrom, topTo);
        } break;
    }
    switch (this->effectiveLength()) {
        case LengthFilter::Platformer: name += " (Plat.)"; break;
        case LengthFilter::Both: name += " (Classic+Plat.)"; break;
        default: break;
    }
    if (mode == RouletteMode::Difficulty && popular) name += fmt::format(" (Popular {}+)", formatCount(minDownloads));
    return name;
}

SessionStats computeStats(std::vector<LevelResult> const& results) {
    SessionStats s;
    s.played = (int)results.size();
    if (results.empty()) return s;
    s.best = 0;
    s.worst = 100;
    for (auto& r : results) {
        s.total += r.percent;
        s.best = std::max(s.best, r.percent);
        s.worst = std::min(s.worst, r.percent);
        if (r.percent >= 100) s.completions++;
    }
    s.mean = (double)s.total / s.played;
    double acc = 0.0;
    for (auto& r : results) {
        double d = r.percent - s.mean;
        acc += d * d;
    }
    s.stddev = std::sqrt(acc / s.played);
    return s;
}

int64_t nowTimestamp() {
    return (int64_t)std::time(nullptr);
}

std::string formatCount(int n) {
    if (n >= 1000000) {
        auto s = fmt::format("{:.1f}", n / 1000000.0);
        if (s.ends_with(".0")) s.resize(s.size() - 2);
        return s + "M";
    }
    if (n >= 1000) {
        auto s = fmt::format("{:.1f}", n / 1000.0);
        if (s.ends_with(".0")) s.resize(s.size() - 2);
        return s + "K";
    }
    return std::to_string(n);
}

std::string formatDate(int64_t timestamp) {
    if (timestamp <= 0) return "-";
    std::time_t t = (std::time_t)timestamp;
    std::tm tm {};
#ifdef GEODE_IS_WINDOWS
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    return fmt::format("{:02}/{:02}/{} {:02}:{:02}", tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900, tm.tm_hour, tm.tm_min);
}

static int64_t newEntryID() {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    return (int64_t)ms * 1000 + utils::random::generate<int>(0, 1000);
}

static char const* lengthSearch(LengthFilter len) {
    switch (len) {
        case LengthFilter::Platformer: return "5";
        case LengthFilter::Both: return "-";
        default: return "0,1,2,3,4";
    }
}

static bool lengthMatches(LengthFilter len, GJGameLevel* level) {
    bool plat = level->m_levelLength >= 5;
    switch (len) {
        case LengthFilter::Platformer: return plat;
        case LengthFilter::Both: return true;
        default: return !plat;
    }
}

// ---------------------------------------------------------------------------
// JSON
// ---------------------------------------------------------------------------

static int jsonInt(matjson::Value const& v, std::string_view key, int def = 0) {
    if (!v.contains(key)) return def;
    return (int)v[key].asInt().unwrapOr(def);
}
static int64_t jsonInt64(matjson::Value const& v, std::string_view key) {
    if (!v.contains(key)) return 0;
    return (int64_t)v[key].asInt().unwrapOr(0);
}
static std::string jsonStr(matjson::Value const& v, std::string_view key) {
    if (!v.contains(key)) return "";
    return v[key].asString().unwrapOr("");
}
static bool jsonBool(matjson::Value const& v, std::string_view key) {
    if (!v.contains(key)) return false;
    return v[key].asBool().unwrapOr(false);
}

static matjson::Value configToJson(RouletteConfig const& c) {
    auto v = matjson::Value::object();
    v["mode"] = (int)c.mode;
    v["difficulty"] = c.difficulty;
    v["from"] = c.topFrom;
    v["to"] = c.topTo;
    v["special"] = c.special;
    v["length"] = (int)c.length;
    v["popular"] = c.popular;
    v["minDl"] = c.minDownloads;
    v["count"] = c.count;
    return v;
}
static RouletteConfig configFromJson(matjson::Value const& v) {
    RouletteConfig c;
    c.mode = (RouletteMode)std::clamp(jsonInt(v, "mode"), 0, 2);
    c.difficulty = std::clamp(jsonInt(v, "difficulty", 6), 1, DIFF_COUNT);
    c.topFrom = std::max(1, jsonInt(v, "from", 1));
    c.topTo = std::max(c.topFrom, jsonInt(v, "to", 50));
    c.special = std::clamp(jsonInt(v, "special", 0), 0, SPECIAL_COUNT - 1);
    c.length = (LengthFilter)std::clamp(jsonInt(v, "length", 0), 0, 2);
    c.popular = jsonBool(v, "popular");
    c.minDownloads = std::max(0, jsonInt(v, "minDl", 100000));
    c.count = std::max(1, jsonInt(v, "count", 25));
    return c;
}

static matjson::Value resultToJson(LevelResult const& r) {
    auto v = matjson::Value::object();
    v["id"] = r.levelID;
    v["name"] = r.name;
    v["creator"] = r.creator;
    v["pct"] = r.percent;
    v["pos"] = r.position;
    return v;
}
static LevelResult resultFromJson(matjson::Value const& v) {
    LevelResult r;
    r.levelID = jsonInt(v, "id");
    r.name = jsonStr(v, "name");
    r.creator = jsonStr(v, "creator");
    r.percent = std::clamp(jsonInt(v, "pct"), 0, 100);
    r.position = jsonInt(v, "pos");
    return r;
}

static matjson::Value resultsToJson(std::vector<LevelResult> const& results) {
    auto arr = matjson::Value::array();
    for (auto& r : results) arr.push(resultToJson(r));
    return arr;
}
static std::vector<LevelResult> resultsFromJson(matjson::Value const& v, std::string_view key) {
    std::vector<LevelResult> out;
    if (!v.contains(key) || !v[key].isArray()) return out;
    for (auto& r : v[key]) out.push_back(resultFromJson(r));
    return out;
}

static std::filesystem::path dataPath(char const* name) {
    return Mod::get()->getSaveDir() / name;
}

// ---------------------------------------------------------------------------
// Busqueda de niveles en los servidores de GD
// ---------------------------------------------------------------------------

class LevelSearcher : public CCObject, public LevelManagerDelegate {
public:
    using Callback = std::function<void(CCArray*, int total)>;

    static LevelSearcher* get() {
        static LevelSearcher* instance = [] {
            auto s = new LevelSearcher();
            s->autorelease();
            s->retain();
            return s;
        }();
        return instance;
    }

    void search(GJSearchObject* obj, Callback cb) {
        m_callback = std::move(cb);
        m_key = obj->getKey();
        m_total = -1;
        m_handled = false;
        auto glm = GameLevelManager::get();
        glm->m_levelManagerDelegate = this;
        glm->getOnlineLevels(obj);
    }

    void cancel() {
        m_callback = nullptr;
        m_key.clear();
        auto glm = GameLevelManager::get();
        if (glm->m_levelManagerDelegate == this) glm->m_levelManagerDelegate = nullptr;
    }

    void loadLevelsFinished(CCArray* levels, char const* key) override { this->handle(levels, key); }
    void loadLevelsFinished(CCArray* levels, char const* key, int) override { this->handle(levels, key); }
    void loadLevelsFailed(char const* key) override { this->handle(nullptr, key); }
    void loadLevelsFailed(char const* key, int) override { this->handle(nullptr, key); }

    void setupPageInfo(gd::string info, char const* key) override {
        if (!key || m_key != key) return;
        std::string s = info;
        auto pos = s.find(':');
        m_total = numFromString<int>(s.substr(0, pos)).unwrapOr(-1);
    }

private:
    void handle(CCArray* levels, char const* key) {
        if (!key || m_key != key || m_handled) return;
        m_handled = true;
        Ref<CCArray> arr = levels;
        // se difiere un frame para que llegue tambien setupPageInfo
        queueInMainThread([this, arr, key = m_key] {
            if (m_key != key) return;
            auto glm = GameLevelManager::get();
            if (glm->m_levelManagerDelegate == this) glm->m_levelManagerDelegate = nullptr;
            auto cb = std::move(m_callback);
            m_callback = nullptr;
            m_key.clear();
            if (cb) cb(arr, m_total);
        });
    }

    Callback m_callback;
    std::string m_key;
    int m_total = -1;
    bool m_handled = false;
};

// ---------------------------------------------------------------------------
// RouletteManager: carga y guardado
// ---------------------------------------------------------------------------

RouletteManager& RouletteManager::get() {
    static RouletteManager instance;
    if (!instance.m_loaded) instance.load();
    return instance;
}

void RouletteManager::load() {
    m_loaded = true;

    if (auto str = file::readString(dataPath("config.json"))) {
        if (auto json = matjson::parse(str.unwrap())) {
            m_lastConfig = configFromJson(json.unwrap());
        }
    }

    if (auto str = file::readString(dataPath("history.json"))) {
        if (auto json = matjson::parse(str.unwrap()); json && json.unwrap().isArray()) {
            int index = 0;
            bool missingIDs = false;
            for (auto& h : json.unwrap()) {
                HistoryEntry e;
                e.id = jsonInt64(h, "uid");
                e.config = configFromJson(h["config"]);
                e.results = resultsFromJson(h, "results");
                e.startedAt = jsonInt64(h, "start");
                e.endedAt = jsonInt64(h, "end");
                e.completed = jsonBool(h, "completed");
                e.locked = jsonBool(h, "locked");
                if (e.id == 0) {
                    e.id = e.startedAt * 1000 + (index % 1000);
                    missingIDs = true;
                }
                index++;
                m_history.push_back(std::move(e));
            }
            if (missingIDs) this->saveHistory();
        }
    }

    if (auto str = file::readString(dataPath("session.json"))) {
        if (auto json = matjson::parse(str.unwrap()); json && json.unwrap().isObject()) {
            auto& v = json.unwrap();
            RouletteSession s;
            s.config = configFromJson(v["config"]);
            s.results = resultsFromJson(v, "results");
            s.startedAt = jsonInt64(v, "start");
            if (v.contains("current")) s.current = resultFromJson(v["current"]);
            if (v.contains("used") && v["used"].isArray()) {
                for (auto& id : v["used"]) s.usedIDs.push_back((int)id.asInt().unwrapOr(0));
            }
            if (v.contains("pool") && v["pool"].isArray()) {
                for (auto& d : v["pool"]) {
                    s.pool.push_back({ jsonInt(d, "pos"), jsonInt(d, "id"), jsonStr(d, "name"), jsonStr(d, "pub") });
                }
            }
            if (!s.isFinished()) m_session = std::move(s);
        }
    }
}

void RouletteManager::saveSession() {
    if (!m_session) {
        std::error_code ec;
        std::filesystem::remove(dataPath("session.json"), ec);
        return;
    }
    auto& s = *m_session;
    auto v = matjson::Value::object();
    v["config"] = configToJson(s.config);
    v["results"] = resultsToJson(s.results);
    v["start"] = s.startedAt;
    v["current"] = resultToJson(s.current);
    auto used = matjson::Value::array();
    for (auto id : s.usedIDs) used.push(id);
    v["used"] = used;
    auto pool = matjson::Value::array();
    for (auto& d : s.pool) {
        auto e = matjson::Value::object();
        e["pos"] = d.position;
        e["id"] = d.levelID;
        e["name"] = d.name;
        e["pub"] = d.publisher;
        pool.push(e);
    }
    v["pool"] = pool;
    (void)file::writeStringSafe(dataPath("session.json"), v.dump(matjson::NO_INDENTATION));
}

void RouletteManager::saveHistory() {
    auto arr = matjson::Value::array();
    for (auto& e : m_history) {
        auto v = matjson::Value::object();
        v["uid"] = e.id;
        v["config"] = configToJson(e.config);
        v["results"] = resultsToJson(e.results);
        v["start"] = e.startedAt;
        v["end"] = e.endedAt;
        v["completed"] = e.completed;
        v["locked"] = e.locked;
        arr.push(v);
    }
    (void)file::writeStringSafe(dataPath("history.json"), arr.dump(matjson::NO_INDENTATION));
}

// ---------------------------------------------------------------------------
// Historial: records, candados y borrado
// ---------------------------------------------------------------------------

HistoryEntry const* RouletteManager::findEntry(int64_t id) const {
    for (auto& e : m_history) {
        if (e.id == id) return &e;
    }
    return nullptr;
}

int RouletteManager::recordTotal(std::string const& categoryKey, int count) const {
    int best = -1;
    for (auto& e : m_history) {
        if (!e.completed || e.config.count != count) continue;
        if (e.config.categoryKey() != categoryKey) continue;
        best = std::max(best, e.stats().total);
    }
    return best;
}

std::unordered_map<std::string, GroupRecords> RouletteManager::computeRecords() const {
    struct Acc {
        GroupRecords ids;
        int best = -1;
        int worst = INT_MAX;
        int most = -1;
    };
    std::unordered_map<std::string, Acc> acc;
    for (auto& e : m_history) {
        if (!e.completed) continue;
        auto st = e.stats();
        auto& a = acc[fmt::format("{}#{}", e.config.categoryKey(), e.config.count)];
        if (st.total > a.best) { a.best = st.total; a.ids.bestID = e.id; }
        if (st.total < a.worst) { a.worst = st.total; a.ids.worstID = e.id; }
        if (st.completions > a.most) { a.most = st.completions; a.ids.mostCompletionsID = e.id; }
    }
    std::unordered_map<std::string, GroupRecords> out;
    for (auto& [key, a] : acc) out[key] = a.ids;
    return out;
}

void RouletteManager::setLocked(int64_t id, bool locked) {
    for (auto& e : m_history) {
        if (e.id == id) e.locked = locked;
    }
    this->saveHistory();
}

void RouletteManager::deleteEntry(int64_t id) {
    std::erase_if(m_history, [id](auto const& e) { return e.id == id; });
    this->saveHistory();
}

std::vector<int64_t> RouletteManager::bulkDeleteIDs(std::string const& categoryKey, KeepOptions const& keep) const {
    auto records = this->computeRecords();
    std::vector<int64_t> ids;
    for (auto& e : m_history) {
        if (!categoryKey.empty() && e.config.categoryKey() != categoryKey) continue;
        if (e.locked) continue;
        if (e.completed) {
            auto it = records.find(fmt::format("{}#{}", e.config.categoryKey(), e.config.count));
            if (it != records.end()) {
                auto& r = it->second;
                if (keep.best && r.bestID == e.id) continue;
                if (keep.worst && r.worstID == e.id) continue;
                if (keep.mostCompletions && r.mostCompletionsID == e.id) continue;
            }
        }
        ids.push_back(e.id);
    }
    return ids;
}

int RouletteManager::countBulkDelete(std::string const& categoryKey, KeepOptions const& keep) const {
    return (int)this->bulkDeleteIDs(categoryKey, keep).size();
}

int RouletteManager::bulkDelete(std::string const& categoryKey, KeepOptions const& keep) {
    auto ids = this->bulkDeleteIDs(categoryKey, keep);
    std::unordered_set<int64_t> set(ids.begin(), ids.end());
    std::erase_if(m_history, [&](auto const& e) { return set.contains(e.id); });
    this->saveHistory();
    return (int)ids.size();
}

// ---------------------------------------------------------------------------
// Sesiones
// ---------------------------------------------------------------------------

void RouletteManager::startSession(RouletteConfig const& config, LevelCallback cb) {
    if (m_session) this->finishSession(false);

    m_lastConfig = config;
    (void)file::writeStringSafe(dataPath("config.json"), configToJson(config).dump());

    RouletteSession s;
    s.config = config;
    s.startedAt = nowTimestamp();
    m_session = std::move(s);
    m_currentLevel = nullptr;
    m_nextLevel = nullptr;
    m_awaitingAdvance = false;
    m_nextFailed = false;
    m_lastResult.reset();
    m_finishedSummary.reset();
    m_generation++;
    this->saveSession();
    this->writeOverlayFiles();

    this->fetchNextLevel([this, cb](GJGameLevel* level, std::string const& err) {
        if (level) this->setCurrent(level, level->getTag());
        cb(level, err);
    });
}

void RouletteManager::continueSession(LevelCallback cb) {
    if (!m_session) return cb(nullptr, "There is no roulette in progress");

    if (m_awaitingAdvance) {
        if (m_nextLevel) {
            m_awaitingAdvance = false;
            this->setCurrent(m_nextLevel, m_nextLevel->getTag());
            m_nextLevel = nullptr;
            return cb(m_currentLevel, "");
        }
        m_awaitingAdvance = false;
        m_generation++;
        return this->fetchNextLevel([this, cb](GJGameLevel* level, std::string const& err) {
            if (level) this->setCurrent(level, level->getTag());
            cb(level, err);
        });
    }

    if (m_currentLevel) return cb(m_currentLevel, "");

    auto& cur = m_session->current;
    if (cur.levelID > 0) {
        int pos = cur.position;
        return this->fetchLevelByID(cur.levelID, [this, cb, pos](GJGameLevel* level, std::string const& err) {
            if (level) this->setCurrent(level, pos);
            cb(level, err);
        });
    }

    this->fetchNextLevel([this, cb](GJGameLevel* level, std::string const& err) {
        if (level) this->setCurrent(level, level->getTag());
        cb(level, err);
    });
}

void RouletteManager::abandonSession() {
    if (!m_session) return;
    this->finishSession(false);
    m_finishedSummary.reset();
}

void RouletteManager::cancelFetch() {
    m_generation++;
    LevelSearcher::get()->cancel();
    m_webTask.cancel();
}

void RouletteManager::finishSession(bool completed) {
    if (!m_session) return;
    this->cancelFetch();

    if (!m_session->results.empty()) {
        HistoryEntry e;
        e.id = newEntryID();
        e.config = m_session->config;
        e.results = m_session->results;
        e.startedAt = m_session->startedAt;
        e.endedAt = nowTimestamp();
        e.completed = completed;

        m_lastFinishRecord = false;
        if (completed) {
            int prev = this->recordTotal(e.config.categoryKey(), e.config.count);
            m_lastFinishRecord = e.stats().total > prev;
            m_finishedSummary = e;
        }
        m_history.push_back(std::move(e));
        this->saveHistory();
    }

    m_session.reset();
    m_currentLevel = nullptr;
    m_nextLevel = nullptr;
    m_awaitingAdvance = false;
    m_nextFailed = false;
    this->saveSession();
    this->writeOverlayFiles();
}

std::optional<HistoryEntry> RouletteManager::takeFinishedSummary() {
    auto ret = std::move(m_finishedSummary);
    m_finishedSummary.reset();
    return ret;
}

void RouletteManager::setCurrent(GJGameLevel* level, int position) {
    if (!m_session) return;
    m_currentLevel = level;
    auto& cur = m_session->current;
    cur.levelID = level->m_levelID.value();
    cur.name = level->m_levelName;
    cur.creator = level->m_creatorName;
    cur.percent = 0;
    cur.position = position;
    if (!this->isUsed(cur.levelID)) m_session->usedIDs.push_back(cur.levelID);
    this->saveSession();
    this->writeOverlayFiles();
}

bool RouletteManager::isUsed(int id) const {
    if (!m_session) return false;
    auto& used = m_session->usedIDs;
    return std::find(used.begin(), used.end(), id) != used.end();
}

bool RouletteManager::isCurrentLevel(GJGameLevel* level) const {
    if (!m_session || !level || m_awaitingAdvance) return false;
    return m_session->current.levelID != 0 && level->m_levelID.value() == m_session->current.levelID;
}

bool RouletteManager::isSessionLevel(GJGameLevel* level) const {
    if (!level) return false;
    int id = level->m_levelID.value();
    if (m_finishedSummary && id == m_lastPlayedID) return true;
    if (!m_session) return false;
    if (m_awaitingAdvance && id == m_lastPlayedID) return true;
    return id == m_session->current.levelID;
}

void RouletteManager::recordAttempt(GJGameLevel* level, int percent) {
    if (!this->isCurrentLevel(level)) return;

    LevelResult r = m_session->current;
    r.percent = std::clamp(percent, 0, 100);
    m_session->results.push_back(r);
    m_lastResult = r;
    m_lastPlayedID = r.levelID;
    m_session->current = LevelResult {};
    m_currentLevel = nullptr;

    if (m_session->isFinished()) {
        this->finishSession(true);
        return;
    }

    m_awaitingAdvance = true;
    m_nextFailed = false;
    m_nextLevel = nullptr;
    this->saveSession();
    this->writeOverlayFiles();

    // Se busca el siguiente nivel mientras el jugador vuelve al menu
    this->retryNext();
}

void RouletteManager::retryNext() {
    m_nextFailed = false;
    m_generation++;
    this->fetchNextLevel([this](GJGameLevel* level, std::string const& err) {
        if (!m_awaitingAdvance) return;
        if (level) {
            m_nextLevel = level;
        } else {
            m_nextFailed = true;
            log::warn("Could not get the next level: {}", err);
        }
    });
}

GJGameLevel* RouletteManager::consumeAdvance(GJGameLevel* level) {
    if (!m_session || !m_awaitingAdvance || !m_nextLevel || !level) return nullptr;
    if (level->m_levelID.value() != m_lastPlayedID) return nullptr;
    Ref<GJGameLevel> next = m_nextLevel;
    m_nextLevel = nullptr;
    m_awaitingAdvance = false;
    this->setCurrent(next, next->getTag());
    return next;
}

// ---------------------------------------------------------------------------
// Busqueda del siguiente nivel
// ---------------------------------------------------------------------------

void RouletteManager::fetchNextLevel(LevelCallback cb) {
    if (!m_session) return cb(nullptr, "No session");
    switch (m_session->config.mode) {
        case RouletteMode::Difficulty: return this->fetchDifficultyLevel(std::move(cb));
        case RouletteMode::Demonlist: return this->fetchDemonlistLevel(0, std::move(cb));
        case RouletteMode::Special: return this->fetchSpecialLevel(std::move(cb));
    }
}

static GJSearchObject* makeSearch(
    SearchType type, std::string const& query, char const* diff, int demonFilter, LengthFilter len, int page, bool star
) {
    return GJSearchObject::create(
        type, query, diff, lengthSearch(len), page,
        star,  // star (solo niveles rateados)
        false, // uncompleted
        false, // featured
        0,     // songID
        false, // original
        false, // twoPlayer
        false, // customSong
        false, // songFilter
        false, // noStar
        false, // coins
        false, // epic
        false, // legendary
        false, // mythic
        false, // onlyCompleted
        demonFilter, 0, 0
    );
}

// Busqueda generica: pide una pagina al azar entre TODAS las paginas de la
// busqueda (todas con la misma probabilidad) y elige un nivel que cumpla el
// filtro. Si la pagina falla o no tiene ninguno valido, se prueba otra pagina
// al azar (sin encoger el rango hacia las primeras paginas).
// maxPages limita las paginas consideradas (0 = sin limite)
void RouletteManager::fetchFromSearch(
    std::string cacheKey, std::function<GJSearchObject*(int page)> makeSearchFn,
    std::function<bool(GJGameLevel*)> filter, int maxPages, int attempt, LevelCallback cb,
    bool favorTop, int minDownloads
) {
    if (!m_session) return;
    // con minimo de descargas hacen falta algunos intentos mas mientras se
    // averigua hasta que pagina llegan los niveles que lo cumplen
    if (attempt >= (minDownloads > 0 ? 10 : 6)) {
        return cb(nullptr, "No levels found (or the GD servers are rate limiting requests). Wait a bit and press Retry");
    }

    int gen = m_generation;
    auto len = m_session->config.effectiveLength();

    auto pickFrom = [this, filter, len, minDownloads](CCArray* arr) -> GJGameLevel* {
        if (!arr || !m_session) return nullptr;
        std::vector<GJGameLevel*> valid;
        for (auto level : CCArrayExt<GJGameLevel*>(arr)) {
            int id = level->m_levelID.value();
            if (id <= 0 || this->isUsed(id)) continue;
            if (!lengthMatches(len, level)) continue;
            if (minDownloads > 0 && level->m_downloads < minDownloads) continue;
            if (filter && !filter(level)) continue;
            valid.push_back(level);
        }
        if (valid.empty()) return nullptr;
        auto level = valid[utils::random::generate<size_t>(0, valid.size())];
        level->setTag(0);
        return level;
    };

    auto pageCount = [this, cacheKey, maxPages]() {
        int total = m_totalLevels.contains(cacheKey) ? m_totalLevels[cacheKey] : 10;
        int pages = std::max(1, (total + 9) / 10);
        if (maxPages > 0) pages = std::min(pages, maxPages);
        return pages;
    };

    // Pagina al azar. Con favorTop se inclina hacia las primeras paginas (en orden de
    // "mas gustados" son los niveles mas populares), pero cualquiera puede salir
    auto randomPage = [pageCount, favorTop, minDownloads]() {
        int pages = pageCount();
        if (!favorTop || minDownloads > 0) return utils::random::generate<int>(0, pages);
        double u = utils::random::generate<double>(0.0, 1.0);
        return std::clamp((int)(pages * std::pow(u, 3.0)), 0, pages - 1);
    };

    auto retry = [this, cacheKey, makeSearchFn, filter, maxPages, attempt, cb, favorTop, minDownloads]() {
        // con favorTop (sin minimo), como al principio: si falla se encoge el rango hacia las primeras paginas
        if (favorTop && minDownloads <= 0 && m_totalLevels.contains(cacheKey)) {
            m_totalLevels[cacheKey] = std::max(10, m_totalLevels[cacheKey] / 3);
        }
        this->fetchFromSearch(cacheKey, makeSearchFn, filter, maxPages, attempt + 1, cb, favorTop, minDownloads);
    };

    // Con minimo de descargas (orden "mas descargados"): si ningun nivel de la
    // pagina llega al minimo, las paginas siguientes tampoco, asi que el rango
    // se corta ahi. Devuelve false si ya no queda ningun nivel posible
    auto learnFromPage = [this, cacheKey, minDownloads](int page, CCArray* arr) {
        if (minDownloads <= 0 || !arr || arr->count() == 0) return true;
        int maxDl = 0;
        for (auto level : CCArrayExt<GJGameLevel*>(arr)) maxDl = std::max(maxDl, level->m_downloads);
        if (maxDl >= minDownloads) return true;
        if (page == 0) return false;
        m_totalLevels[cacheKey] = page * 10;
        return true;
    };

    auto noneLeft = [cb, minDownloads]() {
        cb(nullptr, fmt::format("There are no levels of this difficulty with at least {} downloads", formatCount(minDownloads)));
    };

    auto searchPage = [this, gen, makeSearchFn, pickFrom, learnFromPage, noneLeft, retry, cb](int page) {
        LevelSearcher::get()->search(makeSearchFn(page), [this, gen, page, pickFrom, learnFromPage, noneLeft, retry, cb](CCArray* arr, int) {
            if (gen != m_generation) return;
            if (auto level = pickFrom(arr)) return cb(level, "");
            if (!learnFromPage(page, arr)) return noneLeft();
            retry();
        });
    };

    if (!m_totalLevels.contains(cacheKey)) {
        // Primero se pide la pagina 0 para saber cuantos niveles hay
        LevelSearcher::get()->search(makeSearchFn(0), [this, gen, cacheKey, pickFrom, randomPage, learnFromPage, noneLeft, searchPage, retry, cb](CCArray* arr, int total) {
            if (gen != m_generation) return;
            if (!arr) return retry();
            m_totalLevels[cacheKey] = total > 0 ? total : 1000;
            if (!learnFromPage(0, arr)) return noneLeft();
            int page = randomPage();
            if (page == 0) {
                if (auto level = pickFrom(arr)) return cb(level, "");
            }
            searchPage(page);
        });
    } else {
        searchPage(randomPage());
    }
}

// Por dificultad: niveles rateados ordenados por fecha de rate ("Rateados") y
// pagina al azar entre todas, asi cualquier nivel rateado de esa dificultad
// tiene la misma probabilidad de salir, sea famoso o no
void RouletteManager::fetchDifficultyLevel(LevelCallback cb) {
    auto& cfg = m_session->config;
    int diff = cfg.difficulty;
    auto len = cfg.effectiveLength();
    auto const& info = diffInfo(diff);
    int expectedDemon = info.demonValue;
    // "Force popular levels": orden "mas descargados" y solo niveles con un minimo
    // de descargas (sin minimo, paginas inclinadas hacia los mas populares);
    // si no, cualquier nivel rateado al azar
    bool popular = cfg.popular;
    int minDownloads = popular ? cfg.minDownloads : 0;
    auto sort = popular ? SearchType::Downloaded : SearchType::Awarded;

    this->fetchFromSearch(
        popular ? fmt::format("diffP{}:{}:{}", minDownloads, diff, (int)len) : fmt::format("diffR:{}:{}", diff, (int)len),
        [diff, len, sort](int page) {
            auto& info = diffInfo(diff);
            return makeSearch(sort, "", info.search, info.demonFilter, len, page, true);
        },
        // solo niveles de la dificultad exacta
        [diff, expectedDemon](GJGameLevel* level) {
            if (expectedDemon >= 0 && level->m_demonDifficulty != expectedDemon) return false;
            if (diff == 1 && level->m_stars.value() <= 1) return false; // en "Easy" GD tambien mete los Auto
            return true;
        },
        0, 0, std::move(cb), popular, minDownloads
    );
}

void RouletteManager::fetchSpecialLevel(LevelCallback cb) {
    auto& cfg = m_session->config;
    auto len = cfg.effectiveLength();
    auto type = (SpecialType)cfg.special;
    auto key = fmt::format("sp:{}:{}", cfg.special, (int)len);

    switch (type) {
        case SpecialType::Recent: {
            // los ~100 niveles mas recientes
            this->fetchFromSearch(key, [len](int page) {
                return makeSearch(SearchType::Recent, "", "-", 0, len, page, false);
            }, nullptr, 10, 0, std::move(cb));
        } break;

        case SpecialType::Friends: {
            this->fetchFromSearch(key, [len](int page) {
                return makeSearch(SearchType::Friends, "", "-", 0, len, page, false);
            }, nullptr, 0, 0, [cb](GJGameLevel* level, std::string const& err) {
                if (!level) return cb(nullptr, "No friend levels found (you need to be logged in and have friends with levels)");
                cb(level, err);
            });
        } break;

        case SpecialType::ChallengeList: {
            this->fetchDemonlistLevel(0, std::move(cb));
        } break;

        case SpecialType::RandomAll: {
            this->fetchRandomAllLevel(0, std::move(cb));
        } break;

        default: {
            auto& info = specialInfo(cfg.special);
            std::string query = info.query;
            auto re = std::make_shared<std::regex>(info.regex, std::regex::icase | std::regex::ECMAScript);
            this->fetchFromSearch(key, [query, len](int page) {
                return makeSearch(SearchType::Search, query, "-", 0, len, page, false);
            }, [re](GJGameLevel* level) {
                std::string name = level->m_levelName;
                return std::regex_search(name, *re);
            }, 0, 0, std::move(cb));
        } break;
    }
}

// Niveles aleatorios de todo GD: se generan IDs al azar hasta el ID mas
// reciente y se piden en bloque (los que no existen simplemente no vuelven)
void RouletteManager::fetchRandomAllLevel(int attempt, LevelCallback cb) {
    if (!m_session) return;
    if (attempt >= 10) return cb(nullptr, "Could not find a valid random level");
    int gen = m_generation;

    if (m_maxLevelID <= 0) {
        auto search = makeSearch(SearchType::Recent, "", "-", 0, LengthFilter::Both, 0, false);
        LevelSearcher::get()->search(search, [this, gen, attempt, cb](CCArray* arr, int) {
            if (gen != m_generation) return;
            int maxID = 0;
            if (arr) {
                for (auto level : CCArrayExt<GJGameLevel*>(arr)) maxID = std::max(maxID, level->m_levelID.value());
            }
            if (maxID <= 0) return cb(nullptr, "Could not reach the GD servers");
            m_maxLevelID = maxID;
            this->fetchRandomAllLevel(attempt, cb);
        });
        return;
    }

    std::string ids;
    for (int i = 0; i < 10; i++) {
        if (i) ids += ",";
        ids += std::to_string(utils::random::generate<int>(128, m_maxLevelID + 1));
    }
    auto len = m_session->config.effectiveLength();
    auto search = GJSearchObject::create(SearchType::MapPackOnClick, ids);
    LevelSearcher::get()->search(search, [this, gen, attempt, len, cb](CCArray* arr, int) {
        if (gen != m_generation || !m_session) return;
        std::vector<GJGameLevel*> valid;
        if (arr) {
            for (auto level : CCArrayExt<GJGameLevel*>(arr)) {
                int id = level->m_levelID.value();
                if (id <= 0 || this->isUsed(id) || !lengthMatches(len, level)) continue;
                valid.push_back(level);
            }
        }
        if (valid.empty()) return this->fetchRandomAllLevel(attempt + 1, cb);
        auto level = valid[utils::random::generate<size_t>(0, valid.size())];
        level->setTag(0);
        cb(level, "");
    });
}

void RouletteManager::fetchDemonPool(std::string baseUrl, int after, int remaining, std::function<void(std::string const&)> done) {
    int gen = m_generation;
    int limit = std::min(100, remaining);
    auto url = fmt::format("{}?limit={}&after={}", baseUrl, limit, after);

    m_webTask.spawn(
        web::WebRequest().userAgent("OneAttemptRoulette/1.3 (Geode mod)").timeout(std::chrono::seconds(20)).get(url),
        [this, gen, baseUrl, after, remaining, limit, done](web::WebResponse res) {
            if (gen != m_generation || !m_session) return;
            if (!res.ok()) return done(fmt::format("Could not download the list (code {})", res.code()));
            auto json = res.json();
            if (!json || !json.unwrap().isArray()) return done("Invalid response from the list");

            int count = 0;
            int lastPos = after;
            for (auto& d : json.unwrap()) {
                count++;
                DemonEntry e;
                e.position = jsonInt(d, "position");
                e.name = jsonStr(d, "name");
                if (d.contains("level_id") && d["level_id"].isNumber()) {
                    e.levelID = (int)d["level_id"].asInt().unwrapOr(0);
                }
                if (d.contains("publisher") && d["publisher"].isObject()) {
                    e.publisher = jsonStr(d["publisher"], "name");
                }
                lastPos = std::max(lastPos, e.position);
                if (e.levelID > 0) m_session->pool.push_back(std::move(e));
            }

            int newRemaining = remaining - count;
            if (count < limit || newRemaining <= 0) return done("");
            this->fetchDemonPool(baseUrl, lastPos, newRemaining, done);
        }
    );
}

void RouletteManager::fetchDemonlistLevel(int attempt, LevelCallback cb) {
    if (!m_session) return;
    if (attempt >= 6) return cb(nullptr, "Could not load any level from the list");

    auto& cfg = m_session->config;
    if (m_session->pool.empty()) {
        int gen = m_generation;
        // Demonlist: rango elegido de Pointercrate. Challenge List: la lista entera
        bool challengeList = cfg.mode == RouletteMode::Special;
        std::string baseUrl = challengeList
            ? "https://challengelist.gd/api/v1/demons/"
            : "https://pointercrate.com/api/v2/demons/listed/";
        int from = challengeList ? 1 : cfg.topFrom;
        int to = challengeList ? 9999 : cfg.topTo;
        this->fetchDemonPool(baseUrl, from - 1, to - from + 1, [this, gen, attempt, cb](std::string const& err) {
            if (gen != m_generation || !m_session) return;
            if (!err.empty()) return cb(nullptr, err);
            if (m_session->pool.empty()) return cb(nullptr, "There are no levels in that range of the list");
            this->saveSession();
            this->fetchDemonlistLevel(attempt, cb);
        });
        return;
    }

    std::vector<DemonEntry const*> candidates;
    for (auto& d : m_session->pool) {
        if (!this->isUsed(d.levelID)) candidates.push_back(&d);
    }
    if (candidates.empty()) {
        // Se han jugado todos: se permite repetir
        for (auto& d : m_session->pool) candidates.push_back(&d);
    }

    auto demon = *candidates[utils::random::generate<size_t>(0, candidates.size())];
    this->fetchLevelByID(demon.levelID, [this, demon, attempt, cb](GJGameLevel* level, std::string const& err) {
        if (!m_session) return;
        if (!level) {
            log::warn("Could not load {} ({}): {}", demon.name, demon.levelID, err);
            m_session->usedIDs.push_back(demon.levelID);
            return this->fetchDemonlistLevel(attempt + 1, cb);
        }
        level->setTag(demon.position);
        cb(level, "");
    });
}

void RouletteManager::fetchLevelByID(int id, LevelCallback cb) {
    int gen = m_generation;
    auto search = GJSearchObject::create(SearchType::Search, std::to_string(id));
    LevelSearcher::get()->search(search, [this, gen, id, cb](CCArray* arr, int) {
        if (gen != m_generation) return;
        if (arr) {
            for (auto level : CCArrayExt<GJGameLevel*>(arr)) {
                if (level->m_levelID.value() == id) return cb(level, "");
            }
        }
        cb(nullptr, fmt::format("Level {} not found", id));
    });
}

// ---------------------------------------------------------------------------
// Archivos para el overlay de OBS
// ---------------------------------------------------------------------------

void RouletteManager::writeOverlayFiles() {
    if (!Mod::get()->getSettingValue<bool>("obs-export")) return;

    auto dir = Mod::get()->getSaveDir() / "obs";
    (void)file::createDirectoryAll(dir);

    auto write = [&](char const* name, std::string const& text) {
        (void)file::writeString(dir / name, text);
    };

    if (!m_session) {
        for (auto name : { "category.txt", "progress.txt", "total.txt", "mean.txt", "stddev.txt", "level.txt", "last.txt" }) {
            write(name, "");
        }
        write("roulette.txt", "");
        return;
    }

    auto& s = *m_session;
    auto st = s.stats();
    int index = std::min(st.played + 1, s.config.count);
    std::string level = s.current.levelID ? s.current.name : "...";

    write("category.txt", s.config.categoryName());
    write("progress.txt", fmt::format("{}/{}", index, s.config.count));
    write("total.txt", fmt::format("{}%", st.total));
    write("mean.txt", fmt::format("{:.2f}%", st.mean));
    write("stddev.txt", fmt::format("{:.2f}", st.stddev));
    write("level.txt", level);
    write("last.txt", m_lastResult ? fmt::format("{} - {}%", m_lastResult->name, m_lastResult->percent) : "");
    write("roulette.txt", fmt::format(
        "Roulette: {}\nLevel {}/{}: {}\nTotal: {}%\nMean: {:.2f}%  |  Std. dev.: {:.2f}",
        s.config.categoryName(), index, s.config.count, level, st.total, st.mean, st.stddev
    ));
}
