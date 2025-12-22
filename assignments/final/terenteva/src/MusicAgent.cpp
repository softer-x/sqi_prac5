#include "MusicAgent.h"
#include <fstream>
#include <sstream>
#include <cstring>
#include <memory>
#include <array>
#include <iostream>

#ifdef _WIN32
    #include <Windows.h>
#else
    #include <unistd.h>
    #include <sys/wait.h>
#endif

using nlohmann::json;

bool MusicAgent::readWholeFile(const std::string &path, std::string &out, std::string *err)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        if (err) {
            *err = "Cannot open file: " + path;
        }
        return false;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    out = std::move(ss).str();
    return true;
}

bool MusicAgent::loadConfig(const std::string &path, std::string *err)
{
    std::string configStr;
    if (!readWholeFile(path, configStr, err)) {
        return false;
    }
    try {
        auto j = json::parse(configStr);
        if (j.contains("music")) {
            auto music = j["music"];
            config_.yandex_music_token = music.at("yandex_music_token").get<std::string>();

            if (j.contains("python_path")) {
                config_.python_path = music.at("python_path").get<std::string>();
            }
            if (j.contains("music_script_path")) {
                config_.music_script_path = music.at("music_script_path").get<std::string>();
            }
        } else if (j.contains("yandex_music_toke")) {
            config_.yandex_music_token = j.at("yandex_music_token").get<std::string>();
            if (j.contains("python_path")) {
                config_.python_path = j.at("python_path").get<std::string>();
            }
            if (j.contains("music_script_path")) {
                config_.music_script_path = j.at("music_script_path").get<std::string>();
            }
        } else {
            if (err) *err = "Wrong configuration";
            return false;
        }
       
        return true;
    } catch (const std::exception &e) {
        if (err) {
            *err = std::string("Config parse error: ") + e.what();
        }
        return false;
    }
}

std::optional<std::string> MusicAgent::executePython(
    const std::vector<std::string> &search,
    std::string *outErr) const
{
    std::ifstream scriptFile(config_.music_script_path);
    if (!scriptFile) {
        if (outErr) {
            *outErr = "Python script not found: " + config_.music_script_path;
        }
        return std::nullopt;
    }
    std::string queriesStr = "";
    for (size_t i = 0; i < search.size(); ++i) {
        if (i > 0) queriesStr += "|";
        std::string cleanQuery;
        for (unsigned char c : search[i]) {
            if (!(c >= 0xD8 && c <= 0xDF)) {
                cleanQuery += c;
            }
        }
        queriesStr += cleanQuery;
    }   
    std::string command = config_.python_path + " " + config_.music_script_path + " \"" + config_.yandex_music_token + "\" \"" +
    queriesStr + "\"";

    std::array<char, 128> buffer;
    std::string result;

    #ifdef _WIN32 
        FILE *pipe = _popen(command.c_str(), "r");
        if (!pipe) {
            if (outErr) {
                *outErr = "Failed to execute Python script";
            }
            return std::nullopt;
        }
        while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
            result += buffer.data();
        }
        int returnCode = _pclose(pipe);
    #else
        FILE *pipe = popen(command.c_str(), "r");
        if (!pipe) {
            if (outErr) {
                *outErr = "Failed to execute Python script";
            }
            return std::nullopt;
        }
        while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
            result += buffer.data();
        }
        int returnCode = pclose(pipe);
    #endif

    if (returnCode != 0) {
        if (outErr) {
            *outErr = "Python code execution failed with code: " + std::to_string(returnCode);
        }
        return std::nullopt;
    }
    return result;
}

std::optional<std::vector<TrackInfo>> MusicAgent::parseMusic(
    const std::string &jsonResponse,
    std::string *outErr) const
{
    try {
        auto j = json::parse(jsonResponse);
        if (!j.at("success").get<bool>()) {
            if (outErr) {
                if (j.at("no_tracks").get<bool>()) {
                    *outErr = "Такого трека в Яндекс.Музыке нет:(";
                } else {
                    *outErr = "Python script error: " + j.value("error", "Unknown error");
                }
                
            }
            return std::nullopt;
        }
        std::vector<TrackInfo> tracks;
        auto tracksJson = j.at("tracks");

        for (const auto &trackJson : tracksJson) {
            TrackInfo track;
            track.id = trackJson.at("id").get<std::string>();
            track.title = trackJson.at("title").get<std::string>();
            track.artist = trackJson.at("artist").get<std::string>();
            track.album = trackJson.at("album").get<std::string>();
            track.genre = trackJson.at("genre").get<std::string>();
            track.duration = trackJson.at("duration").get<int>();
            track.year = trackJson.value("year", 0);
            track.url = trackJson.value("url", "");

            tracks.push_back(track);
        }
        return tracks;
    } catch (const std::exception &e) {
        if (outErr) {
            *outErr = std::string("JSON parse error: ") + e.what();
        }
        return std::nullopt;
    }
}

std::optional<std::vector<TrackInfo>> MusicAgent::findTrack(const std::string &search, std::string *outErr)
{
    if (config_.yandex_music_token.empty()) {
        if (outErr) *outErr = "Yandex Music token not found";
        return std::nullopt;
    }
    if (search.empty()) {
        if (outErr) *outErr = "Search queries are empty";
        return std::nullopt;
    }
    std::vector<std::string> search1;
    search1.push_back(search);
    auto pythonResult = executePython(search1, outErr);
    if (!pythonResult) {
        return std::nullopt;
    }
    return parseMusic(*pythonResult, outErr);
}

std::optional<std::vector<TrackInfo>> MusicAgent::getMusicRecommendation(
    const std::vector<std::string> &search,
    std::string *outErr)
{
    if (config_.yandex_music_token.empty()) {
        if (outErr) *outErr = "Yandex Music token not found";
        return std::nullopt;
    }
    if (search.empty()) {
        if (outErr) *outErr = "Search queries are empty";
        return std::nullopt;
    }
    auto pythonResult = executePython(search, outErr);
    if (!pythonResult) {
        return std::nullopt;
    }
    return parseMusic(*pythonResult, outErr);
}

