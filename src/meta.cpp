#ifndef DEF_META
#define DEF_META

#include <SDL.h>
#include <string>

const short BUILD_VER_MAJ= 1;
const short BUILD_VER_MIN= 01;
const long BUILD_ID= 1024;

const int SCREEN_W= 960;
const int SCREEN_H= 544;

const SDL_Color CP_BASE      = {30, 30, 46, 255};
const SDL_Color CP_MANTLE    = {24, 24, 37, 255};
const SDL_Color CP_TEXT      = {205, 214, 244, 255};
const SDL_Color CP_MAUVE     = {203, 166, 247, 255};
const SDL_Color CP_GREEN     = {166, 227, 161, 255};
const SDL_Color CP_SAPPHIRE  = {116, 199, 236, 255};
const SDL_Color CP_SURFACE0  = {49, 50, 68, 255};
const SDL_Color CP_SUBTEXT0  = {166, 173, 200, 255};

const std::string CONFIG_FILE= "ux0:data/nekodrome/config.json";
const std::string ASSET_DIR= "ux0:data/nekodrome/music/";

struct Configuration
{
    std::string host;
    std::string port= "4533";
    std::string user;
    std::string pass;
    std::string query;
};

enum AppView
{ HOME, STARRED, OFFLINE, TRACKS, PLAYER, SETTINGS };

#endif
