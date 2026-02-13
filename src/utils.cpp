#ifndef DEF_UTILS
#define DEF_UTILS

#include <psp2/rtc.h>
#include <psp2/io/stat.h>
#include <curl/curl.h>
#include <sstream>
#include <iomanip>
#include <jansson.h>
#include "shared.h"

bool checkFile (std::string path)
{
    SceIoStat s;
    return (sceIoGetstat(path.c_str(), &s) >= 0);
}

std::string getClockTime ()
{
    SceDateTime dt;
    sceRtcGetCurrentClockLocalTime(&dt);
    char out[16];
    snprintf(out, 16, "%02d:%02d", dt.hour, dt.minute);
    return std::string(out);
}

std::string escape (std::string str)
{
    std::ostringstream ss;
    ss.fill('0');
    for (char c : str)
    {
        if (isalnum( (unsigned char)c ) || c == '-' || c == '_' || c == '.' || c == '~')
        { ss << c; }else
        { ss << '%' << std::setw(2) << std::hex << (int)( (unsigned char)c ); }
    }
    return ss.str();
}

void flushItems ()
{
    for (auto &i : state.items)
    { i.clearArt(); }
    state.items.clear();
}

std::string escape(CURL* c, const std::string& value) {
    char* output = curl_easy_escape(c, value.c_str(), (int) value.length());
    if (output) {
        std::string result(output);
        curl_free(output);
        return result;
    }
    return "";
}

bool commitSettings ()
{
    CURL* c= curl_easy_init ();
    if (!c)
    { return false; }

    sceIoMkdir ("ux0:data/nekodrome", 0777);
    sceIoMkdir (ASSET_DIR.c_str(), 0777);
    json_t *j= json_object ();
    json_object_set_new (j, "ip", json_string(config.host.c_str()));
    json_object_set_new (j, "port", json_string(config.port.c_str()));
    json_object_set_new (j, "user", json_string(config.user.c_str()));
    json_object_set_new (j, "pass", json_string(config.pass.c_str()));
    json_dump_file (j, CONFIG_FILE.c_str(), 0);
    json_decref (j);

    std::string encUser = escape(c, config.user);
    std::string encPass = escape(c, config.pass);

    config.query= "u="+ encUser+"&p=" +encPass +"&v=1.16.1&c=NekoDrome&f=json";
    curl_easy_cleanup (c);
    return true;
}

bool pullSettings() {
    CURL* c= curl_easy_init ();
    if (!c)
    { return false; }

    json_error_t err;
    json_t *j= json_load_file (CONFIG_FILE.c_str(), 0, &err);
    if (!j)
    {
        curl_easy_cleanup (c);
        return false;
    }

    config.host= json_string_value (json_object_get(j, "ip"));
    const char* p= json_string_value (json_object_get(j, "port"));
    config.port= p ? p : "4533";
    config.user= json_string_value (json_object_get(j, "user"));
    config.pass= json_string_value (json_object_get(j, "pass"));

    std::string encUser = escape(c, config.user);
    std::string encPass = escape(c, config.pass);

    config.query= "u="+ encUser+"&p=" +encPass +"&v=1.16.1&c=NekoDrome&f=json";

    json_decref (j);
    curl_easy_cleanup (c);
    return true;
}

#endif
