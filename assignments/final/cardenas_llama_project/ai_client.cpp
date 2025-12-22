#include "ai_agent.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <host> <prompt> [api_key]" << std::endl;
        std::cerr << "Example: " << argv[0] << " ai-api.hurated.com \"Hello world\"" << std::endl;
        return 1;
    }

    AiAgent agent;
    
    // Create config JSON
    nlohmann::json configJson;
    configJson["host"] = argv[1];
    configJson["port"] = "443";
    
    if (argc >= 4) {
        configJson["api_key"] = argv[3];
    } else {
        configJson["api_key"] = "";
    }
    
    // Create prompt JSON
    nlohmann::json promptJson;
    promptJson["prompt"] = argv[2];
    
    std::string err;
    
    // Load config from JSON string
    if (!agent.loadConfigFromJson(configJson.dump(), &err)) {
        std::cerr << "Config error: " << err << std::endl;
        return 1;
    }
    
    // Load prompt from JSON string
    if (!agent.loadPromptFromJson(promptJson.dump(), &err)) {
        std::cerr << "Prompt error: " << err << std::endl;
        return 1;
    }

    auto response = agent.ask(&err);
    if (!response) {
        std::cerr << "Request failed: " << err << std::endl;
        return 2;
    }

    // Output in JSON format
    nlohmann::json outputJson;
    outputJson["text"] = *response;
    std::cout << outputJson.dump() << std::endl;
    
    return 0;
}