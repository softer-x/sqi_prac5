#pragma once
#include <string>
#include <optional>
#include <nlohmann/json.hpp>
#include "TrackInfo.h"

struct HuratedConfig {
    std::string host;
    std::string port = "443";
    std::string api_key;
};

struct LocalConfig {
    std::string backend;
    std::string model_path;
    std::string llama_cli_path = "llama-cli";
    int context_length = 2048;
    float temperature = 0.7;
    int threads = 4;
};

struct AiConfig {
    std::string mode;
    HuratedConfig hurated;
    LocalConfig local;
};

class ModelWrapper {
public:
    virtual ~ModelWrapper () = default;
    virtual std::optional<std::string> generate(const std::string &prompt, std::string *err = nullptr) = 0;
    virtual std::string getName() const = 0;
};

class AiAgent {
public:
    // Загрузить конфиг (host, port, api_key) из JSON-файла
    bool loadConfig(const std::string& path, std::string* err = nullptr);

    bool setMode(const std::string& mode, std::string* err = nullptr);
    std::string getCurrentMode() const { return current_mode_; }
    // Загрузить промпт из JSON-файла (принимает либо строку, либо объект с ключом "prompt")
    bool loadPrompt(const std::string& path, std::string* err = nullptr);

    // Выполнить запрос и вернуть распарсенный "text" из ответа
    // Возвращает std::nullopt при ошибке (описание в outErr, если передан)
    std::optional<std::vector<std::string>> generateMusic(const std::string &mood_description,
                                                            std::string *outErr = nullptr);
    // Явно задать промпт программно (не из файла)
    void setPrompt(std::string p) { prompt_ = std::move(p); }
    std::optional<bool> printResult(const std::vector<TrackInfo> &tracks, const std::string &mood, std::string *outErr);
    
    std::optional<bool> generateAndPrintRecommendations(const std::string &mood, std::string *outErr);
    std::string getBackendName() const;
    std::optional<std::string> generateResponse(const std::string &prompt, std::string *err = nullptr);

private:
    // ---- низкоуровневые помощники ----
    static std::optional<std::string> httpsPostGenerate(
        const HuratedConfig& cfg, const std::string& jsonBody, std::string* err);

    static std::optional<std::string> llamaCliGenerate(
        const LocalConfig& cfg, const std::string& prompt, std::string* err);
        
    // Простой разбор JSON: ожидаем { "text": "<строка>" }
    static std::string extractTextFromJsonBody(const std::string& body);

    static bool readWholeFile(const std::string& path, std::string& out, std::string* err);

    bool initBackend(std::string *err = nullptr);
    std::vector<std::string> parseJsonArrayResponse(const std::string &response);

    std::string generateLocalRecommendationsPrompt(const std::string &mood) const;
    
private:
    AiConfig cfg_;
    std::string prompt_;
    std::string current_mode_;
};