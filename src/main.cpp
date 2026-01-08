#include <psp2/display.h>
#include <psp2/power.h>
#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_mixer.h>
#include <SDL_image.h>
#include <curl/curl.h>
#include <jansson.h>
#include <psp2/ctrl.h>
#include <psp2/net/net.h>
#include <psp2/sysmodule.h>
#include <psp2/common_dialog.h>
#include <string>
#include <vector>
#include <deque>

#include "shared.h"
#include "utils.cpp"
#include "network.cpp"
#include "ui.cpp"

Configuration config;
ApplicationState state;
MusicItem activeTrack("", "No Track", "None", 0);
SDL_Texture* activeArt= nullptr;

void beginTrack (SDL_Renderer* ren)
{
    if (state.queue.empty() || state.queuePos >= (int)state.queue.size()) { return; }
    MusicItem t= state.queue[state.queuePos];
    activeTrack= t;
    if (activeArt) { SDL_DestroyTexture(activeArt); activeArt= nullptr; }
    activeArt= fetchArt(ren, t.coverId, 300);
    Mix_Music* m= nullptr;

    if (checkFile(ASSET_DIR+ t.id+ ".mp3")) { m= Mix_LoadMUS( (ASSET_DIR+ t.id+ ".mp3").c_str() ); }
    if (!m)
    {
        std::string stream= "http://" + config.host + ":" + config.port + "/rest/stream?" + config.query + "&id=" + t.id + "&format=mp3&maxBitRate=128";
        NetBuf b= {NULL, 0}; CURL* c= curl_easy_init();
        if (c)
        {
            curl_easy_setopt(c, CURLOPT_URL, stream.c_str());
            curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, netWrite);
            curl_easy_setopt(c, CURLOPT_WRITEDATA, &b);
            if (curl_easy_perform(c) == CURLE_OK && b.data) { m= Mix_LoadMUS_RW(SDL_RWFromMem(b.data, b.len), 1); }
            curl_easy_cleanup(c);
        }
    }
    if (m) { Mix_PlayMusic(m, 1); state.active= true; state.scrobbled= false; state.startTime= SDL_GetTicks(); switchView(PLAYER); }
}

const char* getFont ()
{
  if (checkFile("ux0:data/nekodrome/font.ttf"))
  { return "ux0:/data/nekodrome/font.ttf"; }else
  { return "app0:/data/font.ttf"; }
}

int main (int argc, char* argv[])
{
    sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    sceSysmoduleLoadModule(SCE_SYSMODULE_IME);
    SceNetInitParam np; np.memory= malloc(1024* 1024); np.size= 1024* 1024; np.flags= 0; sceNetInit(&np);

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG);
    Mix_Init(MIX_INIT_MP3); TTF_Init();
    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);

    SDL_Window* win= SDL_CreateWindow("NekoDrome", 0, 0, 960, 544, 0);
    SDL_Renderer* ren= SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    TTF_Font* f= TTF_OpenFont(getFont(), 24);

    if (!pullSettings()) { state.view= SETTINGS; } else { sync(HOME, 0); }

    bool running= true;
    uint32_t pressTime= 0, frameTime= SDL_GetTicks();
    float vScroll= 0.0f;

    while (running)
    {
        Uint32 now= SDL_GetTicks();
        SceCtrlData ctrl;
        if (sceCtrlPeekBufferPositive(0, &ctrl, 1) > 0 && now- pressTime > 150 && !state.animating)
        {
            bool used= false;
            if (state.view == SETTINGS)
            {
                if (ctrl.buttons & SCE_CTRL_DOWN) { state.menuIdx= (state.menuIdx+ 1)% 5; used= true; }
                if (ctrl.buttons & SCE_CTRL_UP) { state.menuIdx= (state.menuIdx+ 4)% 5; used= true; }
                if (ctrl.buttons & SCE_CTRL_CROSS)
                {
                    if (state.menuIdx == 0) { config.host= prompt(ren, f, "Server IP"); }
                    if (state.menuIdx == 1) { config.port= prompt(ren, f, "Port"); }
                    if (state.menuIdx == 2) { config.user= prompt(ren, f, "Username"); }
                    if (state.menuIdx == 3) { config.pass= prompt(ren, f, "Password"); }
                    if (state.menuIdx == 4 && !config.host.empty()) { commitSettings(); sync(HOME, 0); }
                    used= true;
                }
            }else
            {
                if (ctrl.buttons & SCE_CTRL_DOWN) { state.index++; used= true; }
                if (ctrl.buttons & SCE_CTRL_UP) { state.index--; used= true; }
                if (ctrl.buttons & SCE_CTRL_RIGHT && state.view == HOME) { sync(HOME, ++state.page); used= true; }
                if (ctrl.buttons & SCE_CTRL_LEFT && state.view == HOME && state.page > 0) { sync(HOME, --state.page); used= true; }
                if (ctrl.buttons & SCE_CTRL_RTRIGGER) { AppView n= (state.view == HOME) ? STARRED : (state.view == STARRED) ? OFFLINE : HOME; sync(n, 0); used= true; }
                if (ctrl.buttons & SCE_CTRL_LTRIGGER) { AppView n= (state.view == HOME) ? OFFLINE : (state.view == OFFLINE) ? STARRED : HOME; sync(n, 0); used= true; }
                if (ctrl.buttons & SCE_CTRL_SQUARE && !state.items.empty()) { saveLocal(state.items[state.index]); used= true; }
                if (ctrl.buttons & SCE_CTRL_SELECT && state.active) { switchView(PLAYER); used= true; }
                if (ctrl.buttons & SCE_CTRL_START) { switchView(SETTINGS); used= true; }
                if (ctrl.buttons & SCE_CTRL_TRIANGLE) { std::string q= prompt(ren, f, "Search"); if (!q.empty()) { syncTracks("", true, q); } used= true; }
                if (ctrl.buttons & SCE_CTRL_CROSS)
                {
                    if (state.view == PLAYER) { if (state.active) { Mix_PauseMusic(); }else { Mix_ResumeMusic(); } state.active= !state.active; }
                    else if (!state.items.empty())
                    {
                        if (state.view == HOME) { syncTracks(state.items[state.index].id); }
                        else { state.queue.clear(); for (auto &it : state.items) { state.queue.push_back(it); } state.queuePos= state.index; beginTrack(ren); }
                    }
                    used= true;
                }
                if (ctrl.buttons & SCE_CTRL_CIRCLE) { if (state.view != HOME) { sync(HOME, state.page); } used= true; }
            }
            if (used) { pressTime= now; }
        }

        if (now- frameTime >= 16)
        {
            if (state.active && !Mix_PlayingMusic())
            { state.queuePos++; if (state.queuePos < (int)state.queue.size()) { beginTrack(ren); } else { state.active= false; } }
            if (state.animating) { state.animPos += 0.1f; if (state.animPos >= 1.0f) { state.view= state.nextView; state.animating= false; } }
            else {
                if (state.index < 0) { state.index= 0; }
                if (!state.items.empty() && state.index >= (int)state.items.size()) { state.index= state.items.size()- 1; }
                if (state.index < state.scroll) { state.scroll= state.index; }
                if (state.index > state.scroll+ 5) { state.scroll= state.index- 5; }
                vScroll += (state.scroll- vScroll)* 0.15f;
            }

            Uint32 elapsed= (now- state.startTime)/ 1000;
            if (state.active && !state.scrobbled && state.startTime > 0) { if (elapsed >= 40) { fetch("scrobble", "id=" + activeTrack.id + "&submission=true"); state.scrobbled= true; } }

            SDL_SetRenderDrawColor(ren, CP_BASE.r, CP_BASE.g, CP_BASE.b, 255); SDL_RenderClear(ren);
            SDL_SetRenderDrawColor(ren, CP_MANTLE.r, CP_MANTLE.g, CP_MANTLE.b, 255); SDL_Rect bar= {0, 0, 960, 40}; SDL_RenderFillRect(ren, &bar);
            print(ren, f, "NekoDrome", 20, 5, CP_SUBTEXT0); print(ren, f, getClockTime(), 900, 5, CP_TEXT, true);

            if (state.busy) { print(ren, f, "Loading...", 480, 250, CP_MAUVE, true); }
            else if (state.downloading) { print(ren, f, "Downloading...", 480, 250, CP_GREEN, true); }
            else { if (state.animating) { render(ren, f, state.view, (int)( -state.animPos* SCREEN_W ), vScroll); render(ren, f, state.nextView, (int)( (1.0f- state.animPos)* SCREEN_W ), 0.0f); } else { render(ren, f, state.view, 0, vScroll); } }

            SDL_SetRenderDrawColor(ren, CP_MANTLE.r, CP_MANTLE.g, CP_MANTLE.b, 255); SDL_Rect foot= {0, 470, 960, 74}; SDL_RenderFillRect(ren, &foot);
            if (state.active)
            {
                if (activeArt && state.view != PLAYER) { SDL_Rect art= { 10, 477, 60, 60 }; SDL_RenderCopy(ren, activeArt, NULL, &art); }
                print(ren, f, activeTrack.title, 80, 480, CP_TEXT);
                if (activeTrack.length > 0 && state.startTime > 0)
                {
                    float p= (float)elapsed/ activeTrack.length; if (p > 1.0f) { p= 1.0f; }
                    SDL_SetRenderDrawColor(ren, CP_SURFACE0.r, CP_SURFACE0.g, CP_SURFACE0.b, 255); SDL_Rect full= {80, 520, 860, 6}; SDL_RenderFillRect(ren, &full);
                    SDL_SetRenderDrawColor(ren, CP_MAUVE.r, CP_MAUVE.g, CP_MAUVE.b, 255); SDL_Rect cur= {80, 520, (int)(860* p), 6}; SDL_RenderFillRect(ren, &cur);
                }
            }
            SDL_RenderPresent(ren); frameTime= now;
        }
        SDL_Delay(1);
    }
    return 0;
}
