#pragma once
#include <string>
#include <optional>
#include <vector>
#include <nlohmann/json.hpp>
#include "TrackInfo.h"

struct MusicConfig {
    std::string yandex_music_token;
    std::string python_path = "python3";
    std::string music_script_path = "yandex_music_client.py";
};

class MusicAgent 
{
public:
    bool loadConfig(const std::string &path, std::string *err = nullptr);
    std::optional<std::vector<TrackInfo>> getMusicRecommendation(
        const std::vector<std::string> &search,
        std::string *outErr = nullptr
    );
    std::optional<std::vector<TrackInfo>> findTrack(const std::string &search, std::string *outErr);
private:
    std::optional<std::string> executePython(
        const std::vector<std::string> &search,
        std::string *outErr) const;
    
    std::optional<std::vector<TrackInfo>> parseMusic(
        const std::string &jsonResponse,
        std::string *outErr) const;
    
    static bool readWholeFile(const std::string &path, std::string &out, std::string *err);
private:
    MusicConfig config_;
};



