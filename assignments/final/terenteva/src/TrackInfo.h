#ifndef TRACKINFO_H
#define TRACKINFO_H

#include <string>

struct TrackInfo {
    std::string id;
    std::string title;
    std::string artist;
    std::string album;
    std::string genre;
    std::string url;
    int duration = 0;
    int year = 0;
};

#endif