# NekoDrome  
<img src="https://nekomimi.tilde.team/res/misc/nekodrome.png" align="center">   
Since Neko was bored and wanted to play her music collection from her navidrome server but on her PS Vita, she searched for a navidrome client yet she found none for the vita, hence why she decided this would be a good time for one to exist thus NekoDrome emerged  
You can look at the screenshots btw in the screenshots folder :3  
  
To talk more about this project I guess there isn't much to say  
Does one simple task *playing music from navidrome* and that's it  
More QoL features would be added soon too  
*Plus many speed improvements cause honestly it needs a lot of them :'3*  

# Building  
This project requires [vitasdk](https://vitasdk.org/) please install it first  
There are 2 types of builds, OTA builds and VPK builds  
You are always supposed to install the VPK first before doing an OTA build  
Either install the VPK from the releases or build for VPK first before proceeding with the OTA build  

## VPK build  
```sh
cmake .
make -j 12
```

## OTA build  
1. Install [vita companion](https://github.com/devnoname120/vitacompanion)  
2. Edit `deploy.sh` with your vita's IP  
3. Run the following:  
```sh
./b
```

# Project Roadmap  
So there's a lot to talk about here and honestly we're still in the beginnings of this client  
There are some weird choices I picked like going with SDL2 for example but that's for a couple reasons, mainly because I want it to be easy to contrib to (someone please make a cool visualizer plz lul) and because I am already familiar with SDL2 in CPP and have a template for vitasdk SDL2 projects  
Now I will admit some of the choices where probably not the best especially for the hardware this app runs on sadly, yeah it's a music player but it's still a bad one for now  
currently there are 2 issues that make this app a bit slow, the assets not being caches (this actually isnt that bad since we expect you to be using a self hosted server on the same LAN) the second is kinda weird, for whatever reason it looks like the TTF font i've picked makes this app slower? using a single language TTF font makes this app way faster and less laggier so i have no idea what to do in this case cause i'd still like to support as many gliphs from many languages as possible, for that i've kinda hacked in a way to load custom fonts automatically...  

# Features  
- Scrobble: scrobbles your track after 40s  
- Download: downloads your track for offline play (on the app itself) (press SQUARE on a track)  
- Search: search you navidrome server with the `search3` API (press TRIANGLE)  
- Player and Queue system: NekoDrome will Queue all items in an album or your liked songs or offline songs, you also have a full screen player (accessed by pressing SELECT)  
- Settings: The app is configured with a simple settings view (accessed by pressing START)  
- Custom Font: You can set your own fonts by placing it as `ux0:/data/nekodrome/font.ttf`  

# Neko's Navidrome server  
huh? what of it?  
*no piracy!*  
have [this](https://github.com/NekoMimiOfficial/) instead :3   
