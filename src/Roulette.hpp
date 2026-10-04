#pragma once

#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>

using namespace geode::prelude;

// ---------------------------------------------------------------------------
// Tipos de datos
// ---------------------------------------------------------------------------

enum class RouletteMode : int {
    Difficulty = 0,
    Demonlist = 1,
    Special = 2,
};

// Filtro de tipo de nivel
enum class LengthFilter : int {
    Classic = 0,
    Platformer = 1,
    Both = 2,
};

// Indices propios de dificultad (1..10). Ver diffInfo() en Roulette.cpp
constexpr int DIFF_COUNT = 10;

struct DiffInfo {
    char const* name;   // nombre a mostrar
    int spriteFrame;    // frame para GJDifficultySprite
    char const* search; // parametro "diff" de la busqueda de GD
    int demonFilter;    // 0 si no es demon, 1..5 si lo es
    int demonValue;     // valor esperado de GJGameLevel::m_demonDifficulty (-1 = no comprobar)
};

DiffInfo const& diffInfo(int diff);

// Ruletas especiales
enum class SpecialType : int {
    Recent = 0,
    RandomAll = 1,
    Friends = 2,
    Cube = 3,
    Ship = 4,
    Ball = 5,
    Ufo = 6,
    Wave = 7,
    Robot = 8,
    Spider = 9,
    Swing = 10,
    ChallengeList = 11, // The Challenge List (challengelist.gd)
};
constexpr int SPECIAL_COUNT = 12;

struct SpecialInfo {
    char const* name;  // nombre a mostrar
    char const* icon;  // frame del icono (nullptr = icono del mod)
    char const* query; // texto de busqueda (challenges)
    char const* regex; // filtro sobre el nombre del nivel (challenges)
};

SpecialInfo const& specialInfo(int special);

struct RouletteConfig {
    RouletteMode mode = RouletteMode::Difficulty;
    int difficulty = 6; // Easy Demon por defecto
    int topFrom = 1;
    int topTo = 50;
    int special = 0;
    LengthFilter length = LengthFilter::Classic;
    bool popular = false;        // dificultad: solo niveles populares (mas descargados)
    int minDownloads = 100000;   // minimo de descargas en modo popular
    int count = 25;

    // Las listas (Demonlist, Challenge List) solo tienen niveles clasicos
    bool isListMode() const {
        return mode == RouletteMode::Demonlist
            || (mode == RouletteMode::Special && special == (int)SpecialType::ChallengeList);
    }
    // Filtro efectivo
    LengthFilter effectiveLength() const {
        return this->isListMode() ? LengthFilter::Classic : length;
    }
    // Clave para agrupar estadisticas ("diff:6", "top:1-50", "sp:7|L1")
    std::string categoryKey() const;
    // Nombre legible ("Easy Demon", "Main List", "Wave Challenge (Plataformas)")
    std::string categoryName() const;
};

struct LevelResult {
    int levelID = 0;
    std::string name;
    std::string creator;
    int percent = 0;
    int position = 0; // posicion en pointercrate (0 si no aplica)
};

struct DemonEntry {
    int position = 0;
    int levelID = 0;
    std::string name;
    std::string publisher;
};

struct SessionStats {
    int played = 0;
    int total = 0;
    double mean = 0.0;
    double stddev = 0.0;
    int best = 0;
    int worst = 0;
    int completions = 0; // niveles al 100%
};

SessionStats computeStats(std::vector<LevelResult> const& results);

struct RouletteSession {
    RouletteConfig config;
    std::vector<LevelResult> results;
    std::vector<int> usedIDs;
    std::vector<DemonEntry> pool; // solo modo demonlist
    int64_t startedAt = 0;
    // Nivel que toca jugar ahora (0 si aun no se ha elegido)
    LevelResult current;

    SessionStats stats() const { return computeStats(results); }
    bool isFinished() const { return (int)results.size() >= config.count; }
};

struct HistoryEntry {
    int64_t id = 0;
    RouletteConfig config;
    std::vector<LevelResult> results;
    int64_t startedAt = 0;
    int64_t endedAt = 0;
    bool completed = false;
    bool locked = false;

    SessionStats stats() const { return computeStats(results); }
};

// Que records hay que conservar al borrar en bloque
struct KeepOptions {
    bool best = true;
    bool worst = true;
    bool mostCompletions = true;
};

// Records de un grupo (misma categoria y mismo numero de niveles)
struct GroupRecords {
    int64_t bestID = 0;
    int64_t worstID = 0;
    int64_t mostCompletionsID = 0;
};

// ---------------------------------------------------------------------------
// Gestor principal
// ---------------------------------------------------------------------------

class RouletteManager {
public:
    using LevelCallback = std::function<void(GJGameLevel*, std::string const& error)>;

    static RouletteManager& get();

    void load();

    // --- sesion ---
    bool hasSession() const { return m_session.has_value(); }
    RouletteSession* session() { return m_session ? &*m_session : nullptr; }
    RouletteConfig lastConfig() const { return m_lastConfig; }

    // Empieza una sesion nueva y busca el primer nivel
    void startSession(RouletteConfig const& config, LevelCallback cb);
    // Continua una sesion existente (busca/recupera el nivel actual)
    void continueSession(LevelCallback cb);
    // Abandona la sesion actual (se guarda en el historial como incompleta)
    void abandonSession();
    // Cancela cualquier busqueda en curso
    void cancelFetch();
    // Busca un nivel cualquiera por ID (para abrirlo desde el historial)
    void fetchLevel(int id, LevelCallback cb) { this->fetchLevelByID(id, std::move(cb)); }

    // --- integracion con el juego ---
    bool isCurrentLevel(GJGameLevel* level) const;
    // Devuelve true si este LevelInfoLayer debe mostrar el panel de la ruleta
    bool isSessionLevel(GJGameLevel* level) const;
    // Registra el intento del nivel actual
    void recordAttempt(GJGameLevel* level, int percent);
    // Si el nivel es el que se acaba de jugar y el siguiente ya esta listo,
    // devuelve el siguiente (y lo marca como actual)
    GJGameLevel* consumeAdvance(GJGameLevel* level);
    bool isAwaitingAdvance() const { return m_awaitingAdvance; }
    bool isNextReady() const { return m_nextLevel != nullptr; }
    bool nextFailed() const { return m_nextFailed; }
    void retryNext();
    int lastPlayedID() const { return m_lastPlayedID; }
    GJGameLevel* currentLevel() const { return m_currentLevel; }

    // Ultimo resultado + sesion terminada pendiente de mostrar resumen
    std::optional<LevelResult> const& lastResult() const { return m_lastResult; }
    std::optional<HistoryEntry> takeFinishedSummary();
    bool hasFinishedSummary() const { return m_finishedSummary.has_value(); }
    bool lastFinishWasRecord() const { return m_lastFinishRecord; }

    // --- historial ---
    std::vector<HistoryEntry> const& history() const { return m_history; }
    HistoryEntry const* findEntry(int64_t id) const;

    // Mejor total entre sesiones completadas de la categoria (-1 si no hay)
    int recordTotal(std::string const& categoryKey, int count) const;
    // Records de cada grupo (clave: "categoria#niveles")
    std::unordered_map<std::string, GroupRecords> computeRecords() const;

    void setLocked(int64_t id, bool locked);
    void deleteEntry(int64_t id);
    // Cuantas ruletas se borrarian / borra en bloque. categoryKey vacio = todas
    int countBulkDelete(std::string const& categoryKey, KeepOptions const& keep) const;
    int bulkDelete(std::string const& categoryKey, KeepOptions const& keep);

    // --- overlay OBS ---
    void writeOverlayFiles();

private:
    RouletteManager() = default;

    void saveSession();
    void saveHistory();
    void finishSession(bool completed);
    std::vector<int64_t> bulkDeleteIDs(std::string const& categoryKey, KeepOptions const& keep) const;

    void fetchNextLevel(LevelCallback cb);
    void fetchFromSearch(
        std::string cacheKey, std::function<GJSearchObject*(int page)> makeSearch,
        std::function<bool(GJGameLevel*)> filter, int maxPages, int attempt, LevelCallback cb,
        bool favorTop = false, int minDownloads = 0
    );
    void fetchDifficultyLevel(LevelCallback cb);
    void fetchSpecialLevel(LevelCallback cb);
    void fetchRandomAllLevel(int attempt, LevelCallback cb);
    void fetchDemonlistLevel(int attempt, LevelCallback cb);
    void fetchDemonPool(std::string baseUrl, int after, int remaining, std::function<void(std::string const&)> done);
    void fetchLevelByID(int id, LevelCallback cb);
    void setCurrent(GJGameLevel* level, int position);
    bool isUsed(int id) const;

    std::optional<RouletteSession> m_session;
    RouletteConfig m_lastConfig;
    std::vector<HistoryEntry> m_history;

    Ref<GJGameLevel> m_currentLevel;
    Ref<GJGameLevel> m_nextLevel;
    bool m_awaitingAdvance = false;
    bool m_nextFailed = false;
    int m_lastPlayedID = 0;
    std::optional<LevelResult> m_lastResult;
    std::optional<HistoryEntry> m_finishedSummary;
    bool m_lastFinishRecord = false;

    std::unordered_map<std::string, int> m_totalLevels; // total de niveles por busqueda
    int m_maxLevelID = 0;
    int m_generation = 0;
    bool m_loaded = false;
    async::TaskHolder<web::WebResponse> m_webTask;
};

// 100000 -> "100K", 1500000 -> "1.5M"
std::string formatCount(int n);

// Formatea una marca de tiempo como "dd/mm/aaaa hh:mm"
std::string formatDate(int64_t timestamp);
int64_t nowTimestamp();
