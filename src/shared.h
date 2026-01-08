#ifndef DEF_SHARED
#define DEF_SHARED

#include <SDL.h>
#include <SDL_ttf.h>
#include <string>
#include <vector>
#include <deque>
#include "meta.cpp"

struct MusicItem
{
    std::string id, title, artist, coverId;
    int length;
    bool local;
    SDL_Texture* art;

    MusicItem (std::string _id, std::string _t, std::string _a, int _len= 0, std::string _c= "", bool _loc= false)
    : id(_id), title(_t), artist(_a), length(_len), coverId(_c), local(_loc), art(nullptr)
    {}

    void clearArt ()
    {
        if (art)
        { SDL_DestroyTexture(art); art= nullptr; }
    }
};

struct ApplicationState
{
    AppView view= HOME;
    AppView nextView= HOME;
    float animPos= 0.0f;
    bool animating= false;

    std::vector<MusicItem> items;
    std::deque<MusicItem> queue;
    int queuePos= 0;

    int index= 0;
    int scroll= 0;
    int page= 0;
    int menuIdx= 0;

    bool active= false;
    bool scrobbled= false;
    bool busy= false;
    bool downloading= false;
    Uint32 startTime= 0;
};

extern Configuration config;
extern ApplicationState state;
extern MusicItem activeTrack;
extern SDL_Texture* activeArt;

#endif
