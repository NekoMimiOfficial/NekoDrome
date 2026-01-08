#ifndef DEF_UI
#define DEF_UI

#include <SDL_ttf.h>
#include <SDL_image.h>
#include <psp2/ime_dialog.h>
#include <psp2/common_dialog.h>
#include "shared.h"

void print (SDL_Renderer* ren, TTF_Font* f, std::string str, int x, int y, SDL_Color c, bool centered= false)
{
    if (str.empty()) { return; }
    SDL_Surface* s= TTF_RenderUTF8_Blended(f, str.c_str(), c);
    if (s)
    {
        SDL_Texture* t= SDL_CreateTextureFromSurface(ren, s);
        SDL_Rect d= { centered ? x- (s->w/ 2) : x, y, s->w, s->h };
        SDL_RenderCopy(ren, t, NULL, &d);
        SDL_FreeSurface(s);
        SDL_DestroyTexture(t);
    }
}

void render (SDL_Renderer* ren, TTF_Font* f, AppView v, int xOff, float vScroll)
{
    if (v == PLAYER)
    {
        if (activeArt)
        {
            SDL_Rect r= { xOff+ 330, 70, 300, 300 };
            SDL_RenderCopy(ren, activeArt, NULL, &r);
            if (state.scrobbled)
            {
                SDL_SetRenderDrawColor(ren, CP_GREEN.r, CP_GREEN.g, CP_GREEN.b, 255);
                SDL_Rect border= {xOff+ 328, 68, 304, 304};
                SDL_RenderDrawRect(ren, &border);
            }
        }
        print(ren, f, activeTrack.title, xOff+ 480, 385, CP_TEXT, true);
        print(ren, f, activeTrack.artist, xOff+ 480, 420, CP_SUBTEXT0, true);
        return;
    }

    std::string header= "";
    if (v == HOME) { header= "Albums (Page: " + std::to_string(state.page+ 1) + ")"; }
    else if (v == STARRED) { header= "Starred Tracks"; }
    else if (v == OFFLINE) { header= "Offline Library"; }
    else if (v == TRACKS) { header= "Tracks / Search"; }
    else if (v == SETTINGS) { header= "Settings"; }

    print(ren, f, header, xOff+ 20, 50, CP_MAUVE);

    if (v == SETTINGS)
    {
        std::string menu[]= {"IP: " + config.host, "Port: " + config.port, "User: " + config.user, "Pass: *****", "SAVE & CONNECT"};
        for (int i= 0; i < 5; i++)
        {
            SDL_Rect r= {xOff+ 280, 100+ (i* 65), 400, 45};
            SDL_SetRenderDrawColor(ren, (state.menuIdx == i ? CP_MAUVE.r : CP_SURFACE0.r), (state.menuIdx == i ? CP_MAUVE.g : CP_SURFACE0.g), (state.menuIdx == i ? CP_MAUVE.b : CP_SURFACE0.b), 255);
            SDL_RenderFillRect(ren, &r);
            print(ren, f, menu[i], xOff+ 480, 105+ (i* 65), CP_TEXT, true);
        }
        return;
    }

    for (int i= 0; i < (int)state.items.size(); i++)
    {
        float y= 90+ ( (i- vScroll)* 60 );
        if (y < 40 || y > 470) { continue; }

        MusicItem &it= state.items[i];
        SDL_Color bg= (i% 2 == 0) ? CP_MANTLE : CP_SURFACE0;
        SDL_SetRenderDrawColor(ren, (i == state.index ? CP_MAUVE.r : bg.r), (i == state.index ? CP_MAUVE.g : bg.g), (i == state.index ? CP_MAUVE.b : bg.b), (i == state.index ? 100 : 255));

        SDL_Rect row= { xOff+ 10, (int)y, 940, 58 };
        SDL_RenderFillRect(ren, &row);

        if (!it.coverId.empty())
        {
            if (it.art == nullptr) { it.art= fetchArt(ren, it.coverId, 50); }
            if (it.art) { SDL_Rect img= { xOff+ 15, (int)y+ 4, 50, 50 }; SDL_RenderCopy(ren, it.art, NULL, &img); }
        }
        print(ren, f, it.title, xOff+ 80, (int)y+ 15, (i == state.index) ? CP_BASE : CP_TEXT);
        if (it.local) { SDL_SetRenderDrawColor(ren, CP_SAPPHIRE.r, CP_SAPPHIRE.g, CP_SAPPHIRE.b, 255); SDL_Rect dot= { xOff+ 910, (int)y+ 18, 22, 22 }; SDL_RenderFillRect(ren, &dot); }
    }
}

std::string prompt (SDL_Renderer* ren, TTF_Font* f, std::string label)
{
    SceWChar16 title[64], buf[256]= {0};
    for (size_t i= 0; i < label.length(); i++) { title[i]= label[i]; }
    title[label.length()]= 0;

    SceImeDialogParam p;
    sceImeDialogParamInit(&p);
    p.supportedLanguages= 0x0001FFFF;
    p.type= SCE_IME_TYPE_DEFAULT;
    p.title= title;
    p.maxTextLength= 255;
    p.inputTextBuffer= buf;

    if (sceImeDialogInit(&p) < 0) { return ""; }
    while (true)
    {
        sceCommonDialogUpdate(0);
        if (sceImeDialogGetStatus() == SCE_COMMON_DIALOG_STATUS_FINISHED)
        {
            SceImeDialogResult res;
            memset(&res, 0, sizeof(res));
            sceImeDialogGetResult(&res);
            char out[256];
            for (int i= 0; i < 256; i++) { out[i]= (char)buf[i]; if (buf[i] == 0) { break; } }
            sceImeDialogTerm();
            return (res.button == SCE_IME_DIALOG_BUTTON_ENTER) ? std::string(out) : "";
        }
        SDL_SetRenderDrawColor(ren, CP_BASE.r, CP_BASE.g, CP_BASE.b, 255);
        SDL_RenderClear(ren);
        print(ren, f, "Entering " + label + "...", 480, 250, CP_TEXT, true);
        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }
}

#endif
