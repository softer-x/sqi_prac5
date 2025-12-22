#pragma once
#include <string>
#include <optional>
#include <nlohmann/json.hpp>

struct AiConfig {
    std::string host;
    std::string port = "443";
    std::string api_key;
};

class AiAgent {
public:
    // Load config from JSON file
    bool loadConfig(const std::string& path, std::string* err = nullptr);
    
    // Load config from JSON string
    bool loadConfigFromJson(const std::string& jsonStr, std::string* err = nullptr);
    
    // Load config directly
    void setConfig(const AiConfig& config) { cfg_ = config; }
    
    // Load prompt from JSON file
    bool loadPrompt(const std::string& path, std::string* err = nullptr);
    
    // Load prompt from JSON string
    bool loadPromptFromJson(const std::string& jsonStr, std::string* err = nullptr);
    
    // Load prompt directly
    void setPrompt(const std::string& prompt) { prompt_ = prompt; }
    
    // Execute request
    std::optional<std::string> ask(std::string* outErr = nullptr) const;

private:
    static std::optional<std::string> httpsPostGenerate(
        const AiConfig& cfg, const std::string& jsonBody, std::string* err);
    
    static std::string extractTextFromJsonBody(const std::string& body);
    static bool readWholeFile(const std::string& path, std::string& out, std::string* err);

private:
    AiConfig cfg_;
    std::string prompt_;
};