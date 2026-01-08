#ifndef DEF_NETWORK
#define DEF_NETWORK

#include <curl/curl.h>
#include <jansson.h>
#include <psp2/io/dirent.h>
#include <SDL_image.h>
#include "shared.h"

struct NetBuf { char* data; size_t len; };

size_t netWrite (void* p, size_t s, size_t n, void* u)
{
    size_t sz= s* n;
    NetBuf* b= (NetBuf*)u;
    char* next= (char*)realloc(b->data, b->len+ sz+ 1);
    if (!next) { return 0; }
    b->data= next;
    memcpy(&(b->data[b->len]), p, sz);
    b->len += sz;
    b->data[b->len]= 0;
    return sz;
}

std::string fetch (std::string req, std::string args)
{
    CURL* c= curl_easy_init();
    std::string res;
    if (c)
    {
        std::string target= "http://" + config.host + ":" + config.port + "/rest/" + req + "?" + config.query + "&" + args;
        auto cb= [] (void* data, size_t s, size_t n, std::string* out) -> size_t
        { out->append( (char*)data, s* n ); return s* n; };
        curl_easy_setopt(c, CURLOPT_URL, target.c_str());
        curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, (size_t (*) (void*, size_t, size_t, std::string*) )cb);
        curl_easy_setopt(c, CURLOPT_WRITEDATA, &res);
        curl_easy_perform(c);
        curl_easy_cleanup(c);
    }
    return res;
}

SDL_Texture* fetchArt (SDL_Renderer* ren, std::string id, int sz)
{
    if (id.empty()) { return nullptr; }
    std::string path= ASSET_DIR+ id+ ".jpg";
    if (checkFile(path)) { return IMG_LoadTexture(ren, path.c_str()); }

    NetBuf b= {NULL, 0};
    CURL* c= curl_easy_init();
    SDL_Texture* t= nullptr;
    if (c)
    {
        std::string target= "http://" + config.host + ":" + config.port + "/rest/getCoverArt?" + config.query + "&id=" + id + "&size=" + std::to_string(sz);
        curl_easy_setopt(c, CURLOPT_URL, target.c_str());
        curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, netWrite);
        curl_easy_setopt(c, CURLOPT_WRITEDATA, (void*)&b);
        if (curl_easy_perform(c) == CURLE_OK && b.data)
        {
            SDL_RWops* rw= SDL_RWFromMem(b.data, b.len);
            t= IMG_LoadTexture_RW(ren, rw, 1);
        }
        curl_easy_cleanup(c);
        if (b.data) { free(b.data); }
    }
    return t;
}

void switchView (AppView v)
{
    if (state.view == v) { return; }
    state.nextView= v;
    state.animating= true;
    state.animPos= 0.0f;
}

void sync (AppView v, int p)
{
    state.busy= true;
    flushItems();
    state.page= p;
    json_t* j= NULL;

    if (v == HOME)
    {
        std::string raw= fetch("getAlbumList", "type=newest&size=50&offset=" + std::to_string(p* 50));
        j= json_loads(raw.c_str(), 0, NULL);
        if (j)
        {
            json_t* list= json_object_get(json_object_get(json_object_get(j, "subsonic-response"), "albumList"), "album");
            for (size_t i= 0; i < json_array_size(list); i++)
            {
                json_t* item= json_array_get(list, i);
                state.items.emplace_back(
                    json_string_value(json_object_get(item, "id")),
                                         json_string_value(json_object_get(item, "title")),
                                         json_string_value(json_object_get(item, "artist")), 0,
                                         json_string_value(json_object_get(item, "coverArt")), false
                );
            }
        }
    }else if (v == STARRED)
    {
        std::string raw= fetch("getStarred", "");
        j= json_loads(raw.c_str(), 0, NULL);
        if (j)
        {
            json_t* songs= json_object_get(json_object_get(json_object_get(j, "subsonic-response"), "starred"), "song");
            for (size_t i= 0; i < json_array_size(songs); i++)
            {
                json_t* s= json_array_get(songs, i);
                std::string sid= json_string_value(json_object_get(s, "id"));
                state.items.emplace_back(sid,
                                         json_string_value(json_object_get(s, "title")),
                                         json_string_value(json_object_get(s, "artist")),
                                         (int)json_integer_value(json_object_get(s, "duration")),
                                         json_string_value(json_object_get(s, "coverArt")),
                                         checkFile(ASSET_DIR+ sid+ ".mp3")
                );
            }
        }
    }else if (v == OFFLINE)
    {
        SceIoDirent ent;
        SceUID d= sceIoDopen(ASSET_DIR.c_str());
        if (d >= 0)
        {
            while (sceIoDread(d, &ent) > 0)
            {
                std::string fn= ent.d_name;
                if (fn.length() > 4 && fn.substr(fn.length()- 4) == ".mp3")
                {
                    std::string sid= fn.substr(0, fn.length()- 4);
                    std::string mPath= ASSET_DIR+ sid+ ".meta";
                    std::string t= "Offline Track", a= "Unknown", c= "";
                    json_t* m= json_load_file(mPath.c_str(), 0, NULL);
                    if (m)
                    {
                        t= json_string_value(json_object_get(m, "title"));
                        a= json_string_value(json_object_get(m, "artist"));
                        if (json_object_get(m, "coverId"))
                        { c= json_string_value(json_object_get(m, "coverId")); }
                        json_decref(m);
                    }
                    state.items.emplace_back(sid, t, a, 0, c, true);
                }
            }
            sceIoDclose(d);
        }
    }

    if (j) { json_decref(j); }
    state.index= 0;
    state.scroll= 0;
    state.busy= false;
    switchView(v);
}

void syncTracks (std::string id, bool searching= false, std::string q= "")
{
    state.busy= true;
    flushItems();
    json_t* j= NULL;

    if (searching)
    {
        std::string raw= fetch("search3", "query=" + escape(q) + "&trackCount=50");
        j= json_loads(raw.c_str(), 0, NULL);
        if (j)
        {
            json_t* list= json_object_get(json_object_get(json_object_get(j, "subsonic-response"), "searchResult3"), "song");
            for (size_t i= 0; i < json_array_size(list); i++)
            {
                json_t* t= json_array_get(list, i);
                std::string tid= json_string_value(json_object_get(t, "id"));
                state.items.emplace_back(tid,
                                         json_string_value(json_object_get(t, "title")),
                                         json_string_value(json_object_get(t, "artist")),
                                         (int)json_integer_value(json_object_get(t, "duration")),
                                         json_string_value(json_object_get(t, "coverArt")),
                                         checkFile(ASSET_DIR+ tid+ ".mp3")
                );
            }
        }
    }else
    {
        std::string raw= fetch("getMusicDirectory", "id=" + id);
        j= json_loads(raw.c_str(), 0, NULL);
        if (j)
        {
            json_t* list= json_object_get(json_object_get(json_object_get(j, "subsonic-response"), "directory"), "child");
            for (size_t i= 0; i < json_array_size(list); i++)
            {
                json_t* t= json_array_get(list, i);
                std::string tid= json_string_value(json_object_get(t, "id"));
                state.items.emplace_back(tid,
                                         json_string_value(json_object_get(t, "title")),
                                         json_string_value(json_object_get(t, "artist")),
                                         (int)json_integer_value(json_object_get(t, "duration")),
                                         json_string_value(json_object_get(t, "coverArt")),
                                         checkFile(ASSET_DIR+ tid+ ".mp3")
                );
            }
        }
    }

    if (j) { json_decref(j); }
    state.index= 0;
    state.scroll= 0;
    state.busy= false;
    switchView(TRACKS);
}

void saveLocal (MusicItem i)
{
    if (checkFile(ASSET_DIR+ i.id+ ".mp3")) { return; }
    state.downloading= true;

    std::string url= "http://" + config.host + ":" + config.port + "/rest/stream?" + config.query + "&id=" + i.id + "&format=mp3";
    NetBuf m= {NULL, 0};
    CURL* c= curl_easy_init();
    if (c)
    {
        curl_easy_setopt(c, CURLOPT_URL, url.c_str());
        curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, netWrite);
        curl_easy_setopt(c, CURLOPT_WRITEDATA, &m);
        if (curl_easy_perform(c) == CURLE_OK && m.data)
        {
            FILE* f= fopen( (ASSET_DIR+ i.id+ ".mp3").c_str(), "wb" );
            if (f) { fwrite(m.data, 1, m.len, f); fclose(f); }
        }

        if (!i.coverId.empty())
        {
            NetBuf cvr= {NULL, 0};
            std::string cvrUrl= "http://" + config.host + ":" + config.port + "/rest/getCoverArt?" + config.query + "&id=" + i.coverId + "&size=600";
            curl_easy_setopt(c, CURLOPT_URL, cvrUrl.c_str());
            curl_easy_setopt(c, CURLOPT_WRITEDATA, &cvr);
            if (curl_easy_perform(c) == CURLE_OK && cvr.data)
            {
                FILE* f= fopen( (ASSET_DIR+ i.coverId+ ".jpg").c_str(), "wb" );
                if (f) { fwrite(cvr.data, 1, cvr.len, f); fclose(f); }
                free(cvr.data);
            }
        }
        curl_easy_cleanup(c);
        if (m.data) { free(m.data); }
    }

    json_t* meta= json_object();
    json_object_set_new(meta, "title", json_string(i.title.c_str()));
    json_object_set_new(meta, "artist", json_string(i.artist.c_str()));
    json_object_set_new(meta, "coverId", json_string(i.coverId.c_str()));
    json_dump_file(meta, (ASSET_DIR+ i.id+ ".meta").c_str(), 0);
    json_decref(meta);

    for (auto &it : state.items)
    {
        if (it.id == i.id) { it.local= true; }
    }
    state.downloading= false;
}

#endif
