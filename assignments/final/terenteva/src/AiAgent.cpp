#include "AiAgent.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <cstring>
#include <iostream>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#ifdef _WIN32
    #include <Windows.h>
#else
    #include <unistd.h>
    #include <sys/wait.h>
#endif

using nlohmann::json;

// --------- utils IO ----------
bool AiAgent::readWholeFile(const std::string& path, std::string& out, std::string* err) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { if (err) *err = "Cannot open file: " + path; return false; }
    std::ostringstream ss; ss << f.rdbuf();
    out = std::move(ss).str();
    return true;
}

// --------- JSON loaders ----------
bool AiAgent::loadConfig(const std::string& path, std::string* err) {
    std::string s;
    if (!readWholeFile(path, s, err)) return false;
    try {
        auto j = json::parse(s);
        // обязательные поля через at(); port можно оставить как есть, если отсутствует
        
        cfg_.mode = j.at("mode").get<std::string>();
        current_mode_ = cfg_.mode;
        if (j.contains("hurated")) {
            auto h = j["hurated"];
            cfg_.hurated.host   = h.at("host").get<std::string>();
            if (h.contains("port")) cfg_.hurated.port = h.at("port").get<std::string>();
            cfg_.hurated.api_key = h.at("api_key").get<std::string>();
        }
        if (j.contains("local")) {
            auto l = j["local"];
            cfg_.local.backend = l.value("backend", "llama.cpp");
            cfg_.local.model_path = l.at("model_path").get<std::string>();
            cfg_.local.llama_cli_path = l.value("llama_cli_path", "llama_cli");
            cfg_.local.context_length = l.value("context_lengh", 2048);
            cfg_.local.temperature = l.value("temperature", 0.7f);
            cfg_.local.threads = l.value("threads", 4);
        }
        std::cout << "Выбран режим: " << cfg_.mode << std::endl;
        return true;
    } catch (const std::exception& e) {
        if (err) *err = std::string("Config parse error: ") + e.what();
        return false;
    }
}

bool AiAgent::setMode(const std::string &mode, std::string *err) {
    std::string mode_lower = mode;
    std::transform(mode_lower.begin(), mode_lower.end(), mode_lower.begin(), ::tolower);
    if (mode_lower != "hurated" && mode_lower != "local") {
        if (err) *err = "Invalid mode. Must be 'hurated' or 'local'";
        return false;
    }
    current_mode_ = mode_lower;
    std::cout << "Mode changed to: " << current_mode_ << std::endl;
    return true;
}

bool AiAgent::loadPrompt(const std::string& path, std::string* err) 
{
    std::string s;
    if (!readWholeFile(path, s, err)) return false;
    try {
        // Допускаем, что файл — либо строка JSON, либо объект с ключом "prompt"
        json j = json::parse(s);
        if (j.is_string()) {
            prompt_ = j.get<std::string>();
        } else if (j.is_object()) {
            prompt_ = j.at("prompt").get<std::string>();
        } else {
            if (err) *err = "Prompt JSON must be string or object with key 'prompt'";
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        if (err) *err = std::string("Prompt parse error: ") + e.what();
        return false;
    }
}

std::vector <std::string> AiAgent::parseJsonArrayResponse(const std::string &response)
{
    std::vector<std::string> res;
    try {
        auto j = json::parse(response);
        if (j.is_array()) {
            for (const auto &item : j) {
                res.push_back(item.get<std::string>());
            }
        }
    } catch (...) {
        size_t start = response.find('[');
        size_t end = response.find(']');
        if (start != std::string::npos && end != std::string::npos && end > start) {
            std::string json_part = response.substr(start, end - start + 1);
            try {
                auto j = json::parse(json_part);
                if (j.is_array()) {
                    for (const auto &item : j) {
                        res.push_back(item.get<std::string>());
                    }
                }
            } catch (...) {

            }
        }
    }
    return res;
}

// ------- Простейший разбор JSON: ожидаем { "text": "<строка>" } -------
std::string AiAgent::extractTextFromJsonBody(const std::string& body) {
    // Если вместе с HTTP-хедерами — отрежем их
    const auto p = body.find("\r\n\r\n");
    const std::string json_part = (p != std::string::npos) ? body.substr(p + 4) : body;

    try {
        auto j = json::parse(json_part);
        return j.at("text").get<std::string>();  // строго ожидаем поле "text"
    } catch (...) {
        return {};
    }
}

// -------- Низкоуровневый HTTPS POST на /api/generate --------
std::optional<std::string> AiAgent::httpsPostGenerate(
        const HuratedConfig& cfg, const std::string& jsonBody, std::string* err) {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) { if (err) *err = "SSL_CTX_new failed"; return std::nullopt; }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { if (err) *err = "socket failed"; SSL_CTX_free(ctx); return std::nullopt; }

    struct addrinfo hints = {}, *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(cfg.host.c_str(), cfg.port.c_str(), &hints, &res) != 0) {
        if (err) *err = "getaddrinfo failed";
        close(sock); SSL_CTX_free(ctx); return std::nullopt;
    }

    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
        if (err) *err = "connect failed";
        freeaddrinfo(res); close(sock); SSL_CTX_free(ctx); return std::nullopt;
    }
    freeaddrinfo(res);

    SSL* ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sock);
    if (SSL_connect(ssl) <= 0) {
        if (err) *err = "SSL_connect failed";
        SSL_free(ssl); close(sock); SSL_CTX_free(ctx); return std::nullopt;
    }

    // HTTP запрос
    std::ostringstream req;
    req << "POST /api/generate HTTP/1.1\r\n"
        << "Host: " << cfg.host << "\r\n"
        << "Content-Type: application/json\r\n"
        << "Connection: close\r\n";
    if (!cfg.api_key.empty()) req << "x-api-key: " << cfg.api_key << "\r\n";
    req << "Content-Length: " << jsonBody.size() << "\r\n\r\n"
        << jsonBody;

    const std::string request_str = req.str();
    if (SSL_write(ssl, request_str.c_str(), (int)request_str.size()) <= 0) {
        if (err) *err = "SSL_write failed";
        SSL_free(ssl); close(sock); SSL_CTX_free(ctx); return std::nullopt;
    }

    char buf[4096];
    std::string response;
    int bytes;
    while ((bytes = SSL_read(ssl, buf, sizeof(buf)-1)) > 0) {
        buf[bytes] = '\0';
        response += buf;
    }

    SSL_free(ssl);
    close(sock);
    SSL_CTX_free(ctx);

    // ----- Используем nlohmann::json для извлечения "text" -----
    std::string text = extractTextFromJsonBody(response);
    if (text.empty()) {
        if (err) *err = "Cannot extract \"text\" from JSON response";
        return std::nullopt;
    }
    return text;
}

std::optional<std::string> AiAgent::llamaCliGenerate(
    const LocalConfig &cfg, const std::string &prompt, std::string *err)
{
    std::ifstream model_file(cfg.model_path);
    if (!model_file) {
        if (err) *err = "Model file not found: " + cfg.model_path;
        return std::nullopt;
    }

    std::string formatted_prompt = "<|im_start|>user\n" + prompt + "<|im_end|>\n<|im_start|>assistant\n";
    std::string escaped_prompt;
    for (char c : formatted_prompt) {
        if (c == '"' || c == '\\' || c == '$' || c == '`') {
            escaped_prompt.push_back('\\');
        }
        escaped_prompt.push_back(c);
    }

    std::string command = cfg.llama_cli_path 
                        + " -m \"" + cfg.model_path + "\"" 
                        + " -p \"" + escaped_prompt + "\""
                        + " --single-turn"
                        + " --no-display-prompt"
                        + " --log-disable"
                        + " -n 512"
                        + " 2>&1";
    std::cout << "Local model starting: " << cfg.model_path << std::endl;

    std::array<char, 8192> buffer;
    std::string res;
    #ifdef _WIN32
        FILE* pipe = _popen(command.c_str(), "r");
    #else
        FILE* pipe = popen(command.c_str(), "r");
    #endif

    if (!pipe) {
        if (err) *err = "Failed to execute llama.cpp CLI";
        return std::nullopt;
    }
    bool in_banner = true;
    bool found_assistant = false;
    char *line = nullptr;
    try {
        while ((line = fgets(buffer.data(), buffer.size(), pipe)) != nullptr) {
            res += line;
        }
    } catch (...) {
        #ifdef _WIN32
            _pclose(pipe);
        #else
            pclose(pipe);
        #endif
        if (err) *err = "Error reading from llama.cpp";
        return std::nullopt;
    }

    #ifdef _WIN32
        int returnCode = _pclose(pipe);
    #else
        int returnCode = pclose(pipe);
    #endif
    if (returnCode != 0) {
        if (err) *err = "llama.cpp CLI failed with code: " + std::to_string(returnCode);
        return std::nullopt;
    }

    size_t prompt_pos = res.find(escaped_prompt);
    if (prompt_pos != std::string::npos) {
        res = res.substr(prompt_pos + escaped_prompt.length());
    }

    size_t start = res.find_first_not_of(" \n\r\t");
    size_t end = res.find_last_not_of(" \n\r\t");
    if (start != std::string::npos && end != std::string::npos) {
        res = res.substr(start, end - start + 1);
    }

    return res;
}

std::optional<std::string> AiAgent::generateResponse(const std::string &prompt, std::string *err)
{
    if (current_mode_ == "hurated") {
        std::cout << "Используется Hurated API" << std::endl;
        
        if (cfg_.hurated.host.empty() || cfg_.hurated.api_key.empty()) {
            if (err) *err = "Hurated API configuration is incomplete";
            return std::nullopt;
        }
        
        json payload = {{"prompt", prompt}};
        std::string body = payload.dump();
        
        return httpsPostGenerate(cfg_.hurated, body, err);
        
    } else if (current_mode_ == "local") {
        std::cout << "[Используется локальная модель" << std::endl;
        
        if (cfg_.local.model_path.empty()) {
            if (err) *err = "Local model path is not specified";
            return std::nullopt;
        }
        
        return llamaCliGenerate(cfg_.local, prompt, err);
        
    } else {
        if (err) *err = "Unknown mode: " + current_mode_;
        return std::nullopt;
    }
}

std::optional<std::vector<std::string>> AiAgent::generateMusic(const std::string &mood_description,
                                                                    std::string *outErr)
{
    if (prompt_.empty()) {
        if (outErr) *outErr = "Prompt is empty (load it first)";
        return std::nullopt;
    }

    std::string finalprompt = prompt_;
    size_t pos = finalprompt.find("{mood_description}");
    if (pos != std::string::npos) {
        finalprompt.replace(pos, 18, mood_description);
    }
    
    auto AiResponse = generateResponse(finalprompt, outErr);
    std::cout << "Found!" << std::endl;
    if (!AiResponse) {
        return std::nullopt;
    }
    try {
        auto j = json::parse(*AiResponse);
        std::vector<std::string> queries;
        for (const auto &item : j) {
            queries.push_back(item.get<std::string>());
        }
        return queries;
    } catch (const std::exception &e) {
        if (outErr) {
            *outErr = std::string("Failed to parse Ai responses: ") + e.what();
        }
        return std::nullopt;
    }
}

std::optional<bool> AiAgent::printResult(const std::vector<TrackInfo> &tracks, const std::string &mood, std::string *outErr)
{
    if (tracks.empty()) {
        if (outErr) *outErr = "No tracks to display";
        return std::nullopt;
    }
    if (!mood.empty()) {
        std::cout << "Подобрал для тебя подходящие треки! Генерирую красивый вывод, надо подождать:)\n";
    } else {
        std::cout << "Нашел твой трек!\n";
    }
    
    json tracksJson = json::array();
    for (const auto &track : tracks){
        json trackJson = {
            {"id", track.id},
            {"title", track.title},
            {"artist", track.artist},
            {"album", track.album},
            {"genre", track.genre},
            {"duration", track.duration},
            {"url", track.url}
        };
        tracksJson.push_back(trackJson);
    }
    std::cout << tracksJson.size() << std::endl;
    std::string printprompt = R"(
        Ты - музыкальный ассистент. Тебе нужно ПРОФИЛЬТРОВАТЬ и красиво оформить список треков.
        ВАЖНЫЕ ИНСТРУКЦИИ ПО ФИЛЬТРАЦИИ ДУБЛИКАТОВ:
        1. УДАЛИ ВСЕ ДУБЛИКАТЫ - оставь только УНИКАЛЬНЫЕ треки
        2. Дубликатами считаются:
        - Треки с одинаковым исполнителем И названием (основной критерий)
        - Ремиксы, каверы, караоке-версии оригинальных треков
        - Треки с одинаковым названием, исполнителем, длительностью (±5 секунд) и жанром
        - Live-версии, если есть студийная версия
        3. ПРИОРИТЕТЫ при выборе какой трек оставить:
        - Оригинальные студийные версии
        - Более качественные записи (известные альбомы)
        - Более полные версии (длиннее по времени)

        ПРАВИЛА ВЫВОДА:
        1. Ничего не упоминай про фильтрацию, пользователю нужен только результат, а не то, как он получен
        2. Для каждого трека выводи в формате:
        №. Исполнитель - Название трека
            Альбом: Название альбома
            Жанр: жанр
            Длительность: минуты:секунды
            Ссылка: url
        {{MOOD_EXPLANATION}}
        4. Используй эмодзи для оформления

        ПРЕДУПРЕЖДЕНИЕ: Если ты выведешь дубликаты, пользователь будет расстроен!
        Список всех найденных треков (включая дубликаты для фильтрации):
        {{TRACKS_DATA}}
        
        Отфильтруй и выведи только уникальные треки:
    )";

    std::string jsonTracksStr = tracksJson.dump();
    size_t pos = printprompt.find("{{TRACKS_DATA}}");
    if (pos != std::string::npos) {
        printprompt.replace(pos, 15, jsonTracksStr);
    }
    std::string moodExp;
    if (!mood.empty()) {
        moodExp = "3. В конце объясни, почему эти конкретные треки подходят для настроения: \"" +
        mood + "\". При этом само настроение в кавычках не повторяй";
    } else {
        moodExp = "3. Не объясняй, почему треки подходят, просто выведи их по инструкции";
    }
    pos = printprompt.find("{{MOOD_EXPLANATION}}");
    if (pos != std::string::npos) {
            printprompt.replace(pos, 20, moodExp);
    }
    json payload = {{"prompt", printprompt}};
    const std::string body = payload.dump();
    auto AiResponse = generateResponse(printprompt, outErr);
    if (!AiResponse) {
        std::cout << "AI request failed\n";
        return false;
    }
    if (AiResponse->empty()) {
        std::cout << "AI returned empty response\n";
        return false;
    }
    std::cout << "\n" << *AiResponse << "\n";
    std::cout << "Приятного прослушивания!\n";
    return true;
}

std::string AiAgent::getBackendName() const {
    if (current_mode_ == "hurated") {
        return "Hurated API";
    } else if (current_mode_ == "local") {
        return "Local Model (" + cfg_.local.model_path + ")";
    }
    return "Unknown";
}

std::string AiAgent::generateLocalRecommendationsPrompt(const std::string &mood) const {
    return R"(Ты - музыкальный эксперт с глубокими знаниями о музыке всех жанров и эпох.

    Пользователь описывает свое настроение: ")" + mood + R"("

    Пожалуйста, предложи подборку из 8-10 музыкальных треков, которые идеально подойдут под это настроение.

    Для каждого трека укажи:
    1. Исполнитель - Название трека
    2. Краткое описание почему этот трек подходит к настроению
    3. Основной жанр
    4. Примерное время выхода (год или десятилетие)

    Оформи ответ красиво с эмодзи, но сохрани информативность. Не используй markdown разметку, только простой текст.

    Пример формата:
    Radiohead - Creep
    • Почему подходит: Тема изоляции и самокритики идеально ложится на меланхоличное настроение
    • Жанр: Альтернативный рок
    • Период: 1990-е

    Начни с небольшого вступления о том, как музыка может усилить или изменить настроение, а затем представь подборку.)";
}

std::optional<bool> AiAgent::generateAndPrintRecommendations(const std::string &mood,
                                                              std::string *outErr) {
    std::string prompt = generateLocalRecommendationsPrompt(mood);
    
    std::cout << "Генерирую музыкальные рекомендации...\n\n";
    
    auto response = generateResponse(prompt, outErr);
    if (!response) {
        return std::nullopt;
    }
    
    return true;
}