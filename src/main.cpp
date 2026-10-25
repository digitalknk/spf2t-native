// SPDX-License-Identifier: GPL-3.0-only
#include "game.hpp"
#include "audio.hpp"
#include "gem_art.hpp"
#include "presentation.hpp"
#include "match_feedback.hpp"
#include "match.hpp"
#include "runtime.hpp"
#include "attack_hud.hpp"
#include "gamepads.hpp"
#include "keyboard.hpp"
#include <cctype>
#include <SDL3/SDL.h>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <sstream>
using namespace puzzle_fighter::prototype;
using puzzle_fighter::SpritePacketGroup;
using puzzle_fighter::SourceSoundCommand;

struct Options {
    std::string root="data",user_dir="user",capture,report,audio_wav,capture_break,capture_attack;
    std::vector<std::pair<unsigned,SDL_Scancode>> menu_keys;
    unsigned restart_at=0,start_at=0;GameSettings config;bool setup=true;
    unsigned frames=0;std::uint16_t seed=0x920d;
    bool demo=false,solo=false,headless=false,exercise=false,mute=false,music=true,save_options=true,original_layout=true,arcade=false;
};
static Options options(int argc,char**argv) {
    Options o;
    const char* base=SDL_GetBasePath();
    if(!base)throw std::runtime_error("Cannot locate game directory: "+std::string(SDL_GetError()));
    o.root=(std::filesystem::path(base)/"data").string();
    o.user_dir=(std::filesystem::path(base)/"user").string();
    o.config.legacy=false;bool explicit_config=false,legacy=false;
    bool batch=false;for(int i=1;i<argc;++i){if((std::string(argv[i])=="--root"||std::string(argv[i])=="--data-dir")&&i+1<argc)o.root=argv[i+1];if(std::string(argv[i])=="--user-dir"&&i+1<argc)o.user_dir=argv[i+1];if(std::string(argv[i])=="--headless"||std::string(argv[i])=="--demo")batch=true;}
    if(!batch)o.config.load(o.user_dir);else o.config.random_seed=false;
    const auto number=[](const std::string&v,unsigned limit){std::size_t used=0;const auto n=std::stoul(v,&used,0);if(used!=v.size()||n>limit)throw std::runtime_error("Invalid option value: "+v);return unsigned(n);};
    const auto character=[&](std::string name){if(!name.empty()&&std::isdigit(static_cast<unsigned char>(name[0])))return number(name,10);std::string clean;for(char c:name)if(std::isalnum(static_cast<unsigned char>(c)))clean+=char(std::tolower(static_cast<unsigned char>(c)));for(unsigned c=0;c<11;++c){std::string canonical;for(char ch:std::string(character_name(c)))if(std::isalnum(static_cast<unsigned char>(ch)))canonical+=char(std::tolower(static_cast<unsigned char>(ch)));if(clean==canonical)return c;}throw std::runtime_error("Unknown original character: "+name);};
    for(int i=1;i<argc;++i) {
        std::string a=argv[i];
        auto value=[&](){if(++i>=argc)throw std::runtime_error("Missing argument after "+a);return std::string(argv[i]);};
        if(a=="--root"||a=="--data-dir")o.root=value();else if(a=="--user-dir")o.user_dir=value();else if(a=="--frames")o.frames=std::stoul(value());
        else if(a=="--seed"){o.seed=number(value(),65535);o.config.chosen_seed=o.seed;o.config.random_seed=false;}
        else if(a=="--random-seed")o.config.random_seed=true;
        else if(a=="--capture")o.capture=value();
        else if(a=="--report")o.report=value();else if(a=="--restart-at")o.restart_at=std::stoul(value());else if(a=="--demo")o.demo=true;
        else if(a=="--audio-wav")o.audio_wav=value();else if(a=="--mute")o.mute=true;else if(a=="--no-music")o.music=false;
        else if(a=="--arcade"){o.arcade=true;o.setup=false;explicit_config=true;o.config.legacy=false;}
        else if(a=="--native-layout")o.original_layout=false;else if(a=="--original-layout")o.original_layout=true;
        else if(a=="--solo"){o.solo=true;o.config.opponent=1;}else if(a=="--classic"){o.config.opponent=2;explicit_config=true;}
        else if(a=="--versus"){o.config.opponent=0;explicit_config=true;}
        else if(a=="--p1"||a=="--p2"){o.config.characters[a=="--p2"]=character(value());explicit_config=true;}
        else if(a=="--difficulty"){o.config.difficulty=number(value(),7);explicit_config=true;}
        else if(a=="--course"){auto v=value();if(v=="easy")o.config.course=0;else if(v=="normal")o.config.course=1;else if(v=="hard")o.config.course=2;else o.config.course=number(v,2);explicit_config=true;}
        else if(a=="--speed"){o.config.speed=number(value(),3);explicit_config=true;}
        else if(a=="--damage"){auto v=value();if(v=="low")o.config.damage=0;else if(v=="normal")o.config.damage=1;else if(v=="high")o.config.damage=2;else if(v=="maximum")o.config.damage=3;else o.config.damage=number(v,3);explicit_config=true;}
        else if(a=="--handicap1"||a=="--handicap2"){o.config.handicaps[a=="--handicap2"]=number(value(),4);explicit_config=true;}
        else if(a=="--rounds"){o.config.rounds_cpu=o.config.rounds_human=number(value(),5);explicit_config=true;}
        else if(a=="--new-game")explicit_config=true;else if(a=="--legacy-setup")legacy=true;
        else if(a=="--no-save-options")o.save_options=false;
        else if(a=="--quick-start")o.setup=false;else if(a=="--start-at")o.start_at=number(value(),1000000);
        else if(a=="--capture-break")o.capture_break=value();
        else if(a=="--capture-attack")o.capture_attack=value();
        else if(a=="--key-script") {
            const auto path=value();std::ifstream f(path);if(!f)throw std::runtime_error("Cannot read key script: "+path);
            unsigned frame;std::string key;
            while(f>>frame){std::getline(f>>std::ws,key);const auto code=SDL_GetScancodeFromName(key.c_str());if(!frame||code==SDL_SCANCODE_UNKNOWN)throw std::runtime_error("Invalid scheduled key");o.menu_keys.emplace_back(frame,code);}
            std::stable_sort(o.menu_keys.begin(),o.menu_keys.end(),[](auto a,auto b){return a.first<b.first;});
        }
        else if(a=="--headless")o.headless=true;else if(a=="--exercise")o.exercise=true;
        else if(a=="--help") {
            std::cout<<"Puzzle Fighter native prototype\nP1 gameplay: A/D move, S drop, C/V turn, Enter taunt\nP2 gameplay: arrows move/drop, J/K turn, keypad Enter taunt; both keyboard layouts control P1 in solo\nSetup: P1 WASD, C confirm, V cancel; P2 arrows, J confirm, K cancel; solo arrows also control P1, Tab edits CPU fighter\nBoth ready starts; Enter starts at any time. F8 original arcade, F9 original/native display, F10 enhanced options, R quick rematch (quick mode only), Space pause, Escape quit\n--solo uses a testing macro, not original AI; --classic uses original-derived CPU\nArcade: independent P1/P2 inputs; Enter / keypad Enter or gamepad Start joins/continues; 5 inserts a coin (6 uses chute2 when configured); original boot, title, attract, endings and scores are connected.\nOptions: --arcade --original-layout --native-layout --seed N --random-seed --solo --classic --versus --p1 NAME --p2 NAME --difficulty 0..7 --course easy/normal/hard --speed 0..3 --damage low/normal/high/maximum --handicap1 0..4 --handicap2 0..4 --rounds 1/3/5 --quick-start --new-game --legacy-setup --demo --exercise --frames N --headless --capture FILE.bmp --capture-break FILE.bmp --capture-attack FILE.bmp --report FILE.json --audio-wav FILE.wav --mute --no-music --data-dir PACK --user-dir STATE (--root aliases --data-dir)\nGamepads: D-pad/left stick; bottom/right face buttons turn and confirm/cancel; Start starts/pauses; Back options\nChecks: --key-script FILE (lines: frame scancode-name) --no-save-options --start-at N --restart-at N\n";std::exit(0);
        }else throw std::runtime_error("Unknown option: "+a);
    }
    if(o.headless&&!o.frames)throw std::runtime_error("Headless mode needs --frames N");
    if(o.headless&&!o.capture.empty())throw std::runtime_error("Capture needs the renderer; omit --headless");
    if(o.headless&&(!o.capture_break.empty()||!o.capture_attack.empty()))throw std::runtime_error("Effect capture needs the renderer");
    if(o.demo&&o.solo)throw std::runtime_error("Choose --solo or --demo, not both");
    if((o.demo&&!explicit_config)||legacy){const auto fixed=o.config.chosen_seed;const bool random=o.config.random_seed;o.config=GameSettings{};o.config.chosen_seed=fixed;o.config.random_seed=random;if(o.solo)o.config.opponent=1;o.setup=false;}
    o.config.validate();if(o.headless||o.demo)o.setup=false;
    o.seed=o.config.new_match_seed();
    return o;
}
static void exercise(Game&g) {
    auto&m=g.memory;
    // A declared native practice position, not an original captured fixture.
    auto field=m.read_long(Game::players[0]+0x164);
    for(unsigned x=0;x<6;++x)m.write_word(field+0xc2+2*x,x==5?9:1);
}
static std::array<std::uint16_t,2> demonstration(Game&g) {
    std::array<std::uint16_t,2> out{};
    for(unsigned i=0;i<2;++i) {
        auto p=Game::players[i];int target=int((g.locks[i]*5+i*2+1)%6);
        int col=g.memory.read_word(p+0x10);
        out[i]=4;if(col<target)out[i]|=1;if(col>target)out[i]|=2;
        if(g.ticks%83==i*7)out[i]|=0x200;
        if(g.ticks%127==i*11)out[i]|=0x100;
    }
    return out;
}
static void report(Game&g,const Options&o,const SoloMacro&macro,const BreakVisuals&visuals,const Match&match,const MatchFeedback&feedback,const NativeMatchRuntime*runtime=nullptr) {
    std::ostringstream s;
    s<<"{\n  \"runtime\": \"native recovered C++ prototype\",\n  \"seed\": "<<g.seed<<",\n  \"round_starts\": "<<g.restarts<<",\n  \"ticks\": "<<g.ticks<<",\n  \"players\": [\n";
    for(unsigned i=0;i<2;++i) {
        const auto p=Game::players[i];auto&m=g.memory;m.accesses=0;
        s<<"    {\"locks\": "<<g.locks[i]<<", \"spawns\": "<<g.spawns[i]<<", \"removed\": "<<g.clears[i]<<", \"formations\": "<<g.formations[i]<<", \"received\": "<<g.received[i]<<", \"moves\": "<<g.moves[i]<<", \"turns\": "<<g.turns[i]<<", \"score\": "<<m.read_long(p+0xaa)<<", \"step\": "<<unsigned(m.read_byte(p+0x17e))<<", \"piece_seed\": "<<m.read_word(p+0x144)<<", \"steps_visited\": [";
        bool comma=false;for(unsigned k=0;k<21;++k)if(g.visits[i][k]){if(comma)s<<", ";s<<k;comma=true;}
        s<<"], \"board\": ";auto field=m.read_long(p+0x164);if(field>=0xff0000){s<<"[";
        for(unsigned y=0;y<13;++y)for(unsigned x=0;x<6;++x){if(y||x)s<<", ";s<<m.read_word(field+y*16+x*2+2);}
        s<<"]";}else s<<"null";s<<"}"<<(i==0?",":"")<<"\n";
    }
    s<<"  ],\n  \"control_mode\": \""<<(g.settings.opponent==2?"classic_cpu":g.settings.opponent==1?"solo_macro":o.demo?"automated_demo":"human_versus_human")<<"\",\n  \"break_visuals\": {\"method\": \""<<(g.settings.legacy?"provisional host shatter driven by original marks":"original151F0 animation scripts and object consumers")<<"\", \"cells_triggered\": "<<visuals.bursts<<"},\n  \"macro_column_targets\": [";
    for(unsigned x=0;x<6;++x){if(x)s<<", ";s<<macro.targets[x];}s<<"],\n  \"match\": {\"round\": "<<match.round<<", \"completed_rounds\": "<<match.completed_rounds<<", \"wins\": ["<<match.wins[0]<<", "<<match.wins[1]<<"], \"finished\": "<<(match.finished?"true":"false")<<"},\n  \"characters\": ["<<g.settings.characters[0]<<", "<<g.settings.characters[1]<<"],\n  \"settings\": {\"legacy\": "<<(g.settings.legacy?"true":"false")<<", \"difficulty\": "<<g.settings.difficulty<<", \"speed\": "<<g.settings.speed<<", \"rounds_1p\": "<<g.settings.rounds_cpu<<", \"rounds_2p\": "<<g.settings.rounds_human<<", \"handicaps\": ["<<g.settings.handicaps[0]<<", "<<g.settings.handicaps[1]<<"], \"cpu_level\": "<<unsigned(g.memory.read_byte(Game::players[1]+0xa8))<<", \"fall_delays\": ["<<unsigned(g.memory.read_byte(Game::players[0]+0x18a))<<", "<<unsigned(g.memory.read_byte(Game::players[1]+0x18a))<<"]}\n}\n";
    std::ostringstream presentation;presentation<<",\n  \"cpu_setup\": {\"course\": "<<g.settings.course<<", \"computer_flag\": "<<unsigned(g.memory.read_byte(Game::players[1]+0x9e))<<", \"runtime_course\": "<<unsigned(g.memory.read_byte(0xff0c8c))<<"},\n  \"presentation\": {\"intro\": "<<(match.intro?"true":"false")<<", \"intro_frames\": "<<match.intro_elapsed<<", \"intermissions\": [";
    for(unsigned i=0;i<5;++i){if(i)presentation<<", ";presentation<<match.intermissions[i];}presentation<<"]}";
    presentation<<",\n  \"match_seed\": "<<(g.source_arcade?g.seed:o.seed)<<", \"seed_mode\": \""<<(g.settings.random_seed?(g.source_arcade?"source_lifecycle":"random"):"choose")<<"\"";
    presentation<<",\n  \"damage\": {\"setting\": "<<g.settings.damage<<", \"runtime\": "<<unsigned(g.memory.read_byte(Game::globals+0xa1))<<"}";
    presentation<<",\n  \"match_feedback\": {\"result_voices\": "<<feedback.voices<<", \"attack_events\": ["<<g.attack_events[0]<<", "<<g.attack_events[1]<<"], \"hit_events\": ["<<g.hit_events[0]<<", "<<g.hit_events[1]<<"]}";
    presentation<<",\n  \"unrecovered_source_tasks\": [";bool first_task=true;for(auto kind:g.unrecovered_task_kinds){if(!first_task)presentation<<", ";first_task=false;presentation<<kind;}presentation<<"]";
    if(runtime&&!g.settings.legacy)presentation<<",\n  \"source_runtime\": {\"irq_frames\": "<<runtime->irqs<<", \"main_cycles\": "<<runtime->scheduler.completed_main_cycles<<", \"sound_requests_dispatched\": "<<runtime->dispatched_sounds<<", \"pending_sound_queue\": "<<((g.memory.read_word(Game::globals+0x57a0)-g.memory.read_word(Game::globals+0x57a2))&0xff0)/16<<", \"scene_request\": "<<(runtime->scene_requested?"true":"false")<<"}";
    if(runtime&&runtime->arcade.active)presentation<<",\n  \"arcade\": {\"active\": true, \"completed_matches\": "<<runtime->arcade.completed_matches<<", \"joins\": "<<runtime->arcade.join_events<<", \"terminal\": "<<(runtime->arcade.terminal?"true":"false")<<", \"outer_scene\": "<<g.memory.read_word(Game::globals+4)<<", \"scene_step\": "<<g.memory.read_word(Game::globals+8)<<"}";
    if(runtime&&runtime->arcade.active)presentation<<",\n  \"cabinet\": {\"owner\": "<<unsigned(runtime->cabinet.owner)<<", \"cold_starts\": "<<runtime->cabinet.cold_starts<<", \"soft_returns\": "<<runtime->cabinet.soft_returns<<", \"idle_cycles\": "<<runtime->cabinet.idle_cycles<<", \"accepted_starts\": "<<runtime->cabinet.accepted_starts<<", \"seed_injections\": "<<runtime->cabinet.seed_injections<<", \"global_seed\": "<<g.memory.read_word(Game::globals+0xae)<<", \"last_game_seed\": "<<g.seed<<"}";
    if(runtime&&runtime->arcade.active)presentation<<",\n  \"coin_feedback\": {\"credits\": ["<<unsigned(g.memory.read_byte(Game::globals+0x76))<<", "<<unsigned(g.memory.read_byte(Game::globals+0x7e))<<"], \"physical_coins\": "<<g.memory.read_long(Game::globals+0xc0)<<", \"service_credits\": "<<g.memory.read_long(Game::globals+0xc4)<<", \"display_refreshes\": "<<runtime->cabinet.credit_refreshes<<", \"storage_transactions\": "<<runtime->cabinet.operator_store.transactions<<"}";
    auto output=s.str();output.insert(output.rfind("\n}"),presentation.str());
    if(!o.report.empty()){std::ofstream f(o.report);if(!f)throw std::runtime_error("Cannot write report: "+o.report);f<<output;}
    std::cout<<output;
}
struct View {
    SDL_Window*window=nullptr;SDL_Renderer*renderer=nullptr;
    SDL_Texture*character_canvas=nullptr;
    unsigned match_seed=0;bool original_layout=true;NativeArcadeSession*arcade=nullptr;
    std::unique_ptr<GemArt> art;Game*game=nullptr;BreakVisuals*breaks=nullptr;CharacterVisuals*characters=nullptr;Match*match=nullptr;AttackHud*attacks=nullptr;std::array<bool,2> controllers{};bool audio_playing=false,solo=false,visible_break=false;
    explicit View(const std::string&root) {
        if(!SDL_Init(SDL_INIT_VIDEO))throw std::runtime_error(SDL_GetError());
        window=SDL_CreateWindow("Puzzle Fighter | Native prototype",1040,800,SDL_WINDOW_RESIZABLE);
        if(!window)throw std::runtime_error(SDL_GetError());
        renderer=SDL_CreateRenderer(window,nullptr);if(!renderer)throw std::runtime_error(SDL_GetError());
        SDL_SetRenderLogicalPresentation(renderer,1040,800,SDL_LOGICAL_PRESENTATION_LETTERBOX);
        SDL_SetRenderVSync(renderer,1);art=std::make_unique<GemArt>(root);
        SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
    }
    ~View(){art.reset();if(character_canvas)SDL_DestroyTexture(character_canvas);if(renderer)SDL_DestroyRenderer(renderer);if(window)SDL_DestroyWindow(window);SDL_Quit();}
    void colour(unsigned r,unsigned g,unsigned b,unsigned a=255){SDL_SetRenderDrawColor(renderer,r,g,b,a);}
    void rect(float x,float y,float w,float h,bool fill=true){SDL_FRect r{x,y,w,h};if(fill)SDL_RenderFillRect(renderer,&r);else SDL_RenderRect(renderer,&r);}
    void text(float x,float y,const std::string&s,float scale=1.5f) {
        SDL_SetRenderScale(renderer,scale,scale);SDL_RenderDebugText(renderer,x/scale,y/scale,s.c_str());SDL_SetRenderScale(renderer,1,1);
    }
    void gem(float x,float y,std::uint16_t value,bool falling=false,float size=40) {
        if(!value||value==0xff)return;
        auto*t=art->texture(renderer,game->memory,value,game->ticks);
        SDL_FRect target{x,y,size,size};SDL_RenderTexture(renderer,t,nullptr,&target);
        if(falling){colour(247,247,239);rect(x+1,y+1,size-2,size-2,false);}
    }
    void shatter(float x,float y,const BreakVisuals::Cell&cell) {
        if(!cell.seen)return;
        const auto age=game->ticks-cell.started;if(age>=52)return;
        visible_break=visible_break||(age>=7&&age<=24);
        auto*t=art->texture(renderer,game->memory,cell.value,game->ticks);
        if(age<5) {
            SDL_FRect target{x,y,40,40};SDL_RenderTexture(renderer,t,nullptr,&target);
            colour(255,255,240,age%2?210:95);rect(x,y,40,40);return;
        }
        const float elapsed=float(age-5),fade=std::max(0.0f,1.0f-elapsed/40.0f);
        SDL_SetTextureAlphaMod(t,std::uint8_t(255*fade));
        for(unsigned part=0;part<4;++part) {
            const float dx=(part&1)?1.0f:-1.0f,dy=(part&2)?1.0f:-1.0f;
            SDL_FRect source{float((part&1)*8),float((part>>1)*8),8,8};
            SDL_FRect target{x+(part&1)*20+dx*elapsed*.65f,y+(part>>1)*20+dy*elapsed*.55f+elapsed*elapsed*.012f,20,20};
            SDL_RenderTexture(renderer,t,&source,&target);
        }
        SDL_SetTextureAlphaMod(t,255);
        colour(255,245,203,std::uint8_t(220*fade));
        for(unsigned n=0;n<4;++n)rect(x+18+((n&1)?1:-1)*elapsed*.9f,y+18+((n&2)?1:-1)*elapsed*.75f,3,3);
    }
    void source_packets(Game&g){
        auto&m=g.memory;std::array<unsigned,4> hud_parts{};
        bool pending=false;
        const auto flush=[&](){if(!pending)return;SDL_SetRenderTarget(renderer,nullptr);const SDL_Rect clip{316,270,408,310};SDL_SetRenderClipRect(renderer,&clip);SDL_FRect target{50,228,1024,1024};SDL_RenderTexture(renderer,character_canvas,nullptr,&target);SDL_SetRenderClipRect(renderer,nullptr);pending=false;};
        for(unsigned n=0;n<g.sprite_frame.packets.size();++n){const auto&o=g.sprite_frame.packets[n];const auto&origin=g.sprite_frame.origins[n];
            const int rx=o.x&0x3ff,ry=(o.y&0x3ff)>=512?int(o.y&0x3ff)-1024:int(o.y&0x3ff);
            const auto actor=origin.actor;const unsigned kind=actor?m.read_byte(actor+2):0,variant=actor?m.read_byte(actor+3):0;
            float x=0,y=0,sx=2.5f,sy=2.5f;SDL_Rect clip{0,88,1040,560};
            const bool gem=origin.group==SpritePacketGroup::board||origin.group==SpritePacketGroup::pair;
            if(gem){const bool next=(o.x&0xe000)==0xe000;const unsigned side=unsigned(rx>=256);x=next?(side?625.0f:365.0f)+(rx-int(202+side*92))*2.5f:(side?752.0f:48.0f)+(rx-int(80+side*256))*2.5f;y=next?63+ry*2.5f:116+(ry-16)*2.5f;clip=next?SDL_Rect{0,88,1040,560}:SDL_Rect{side?752:48,116,240,520};if(origin.group==SpritePacketGroup::board&&o.tile>=0x6400&&o.tile<0x6800)visible_break=true;}
            else if(origin.group==SpritePacketGroup::third_pool&&kind==1){
                const unsigned slot=(actor-(Game::globals+0x2100))/0x200,part=hud_parts.at(slot)++;const bool marker=part%6>=3;const unsigned side=unsigned(part>=6);
                x=(side?752.0f:48.0f)+(rx-64-int(side?272:16))*2.5f;y=76+(ry-16)*2.5f;
                if(marker){sx=sy=2;x=(side?752.0f:48.0f)+142+32*(part%6-3);y=658;}clip={0,36,1040,660};
            }else if(origin.group==SpritePacketGroup::first_pool&&(kind==3||kind==0x30)){
                const bool field=kind==0x30||variant==5||variant==6;const unsigned side=kind==0x30?unsigned(variant!=0):unsigned(variant==6);
                if(field){x=(side?752.0f:48.0f)+(rx-64-int(side?272:16))*2.5f;y=76+(ry-16)*2.5f;clip={side?752:48,76,240,560};}
                else {sx=sy=2;x=136+(rx-64)*2;y=92+(ry-16)*2;clip={0,88,1040,560};}
            }else if(origin.group==SpritePacketGroup::first_pool&&(kind==0||kind==1||kind==2||kind==6||kind==7||kind==0x15||kind==0x1b||kind==0x1c||kind==0x1d||kind==0x24||kind==0x31||kind==0x36)){
                sx=1040.0f/384;x=(rx-64)*sx;y=88+(ry-16)*sy;clip={296,88,448,560};
            }else if(origin.group==SpritePacketGroup::overlay||origin.group==SpritePacketGroup::raw||origin.group==SpritePacketGroup::service){sx=1040.0f/384;x=(rx-64)*sx;y=88+(ry-16)*sy;}
            else {
                // Assemble original-coordinate parts before the host transform.
                // Scaling part origins differently from their widths tore013
                // sprites apart; per-part clipping also resampled attack edges.
                if(!character_canvas){character_canvas=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_TARGET,512,512);if(!character_canvas)throw std::runtime_error(SDL_GetError());SDL_SetTextureScaleMode(character_canvas,SDL_SCALEMODE_NEAREST);SDL_SetTextureBlendMode(character_canvas,SDL_BLENDMODE_BLEND);}
                if(!pending){SDL_SetRenderTarget(renderer,character_canvas);SDL_SetRenderClipRect(renderer,nullptr);colour(0,0,0,0);SDL_RenderClear(renderer);pending=true;}
                SDL_FRect target{float(rx-64),float(ry-16),float((((o.attributes>>8)&15)+1)*16),float((((o.attributes>>12)&15)+1)*16)};SDL_RenderTexture(renderer,art->object(renderer,m,o.tile,o.attributes),nullptr,&target);continue;
            }
            flush();
            SDL_SetRenderClipRect(renderer,&clip);SDL_FRect target{x,y,float((((o.attributes>>8)&15)+1)*16)*sx,float((((o.attributes>>12)&15)+1)*16)*sy};SDL_RenderTexture(renderer,art->object(renderer,m,o.tile,o.attributes),nullptr,&target);
        }
        flush();SDL_SetRenderClipRect(renderer,nullptr);
    }
    void original_text(Memory&m){
        // Scroll1 is the original 8px text layer. The board viewport adapter
        // maps its 96x208 field to the accepted 240x520 native field.
        for(unsigned side=0;side<2;++side){
            const auto base=(m.read_byte(Game::globals+0x151)?0x902000u:0x900000u)+side*0x1000;
            for(unsigned row=0;row<26;++row)for(unsigned col=0;col<12;++col){
                const auto at=base+(col+2)*0x80+(row+2)*4;const auto tile=m.read_word(at),attr=m.read_word(at+2);
                if(!tile)continue;
                SDL_FRect target{float((side?752:48)+col*20),float(116+row*20),20,20};
                SDL_RenderTexture(renderer,art->glyph(renderer,m,tile,attr),nullptr,&target);
            }
        }
    }
    void draw_original(Game&g,bool paused,const std::string&error){
        game=&g;auto&m=g.memory;colour(12,30,35);SDL_RenderClear(renderer);colour(244,221,133);text(40,24,"PUZZLE FIGHTER",2.3f);colour(192,219,206);text(600,29,g.source_arcade?"ARCADE":"ROUND "+std::to_string(match->round)+" / BEST OF "+std::to_string(g.settings.best_of()),1.6f);
        if(error.empty()){SDL_FRect target{40,84,960,560};SDL_RenderTexture(renderer,art->source_frame(renderer,g),nullptr,&target);}
        visible_break=false;for(const auto&o:g.sprite_frame.packets)if(o.tile>=0x6400&&o.tile<0x6800)visible_break=true;
        for(unsigned side=0;side<2;++side){const float x=side?550:40;const bool cpu=g.source_arcade?!(m.read_byte(Game::globals+0xa8)&(1u<<side)):side&&g.settings.opponent;colour(cpu?170:side?125:245,cpu?170:side?199:154,cpu?170:side?245:139);text(x,662,std::string(cpu?"CPU":side?"P2":"P1")+"  "+character_name(g.settings.characters[side]),1.5f);colour(192,219,206);
            if(cpu){text(x,692,g.settings.opponent==2?std::string("Classic CPU / ")+difficulty_name(g.settings.difficulty):"Testing macro",1.2f);if(g.settings.opponent==2)text(x,715,"Level "+std::to_string(m.read_byte(Game::players[side]+0xa8))+" / "+course_name(g.settings.course)+" / "+speed_name(g.settings.speed),1.2f);}
            else{text(x,692,side?"Move: arrows  Drop: Down  Rotate: J/K":"Move: A/D  Drop: S  Rotate: C/V",1.2f);text(x,715,g.source_arcade?(side?"Up/Down select  Keypad Enter join/continue":"W/S select  Enter join/continue"):(side?"Taunt: keypad Enter":"Taunt: Enter (solo also uses arrows / J/K)"),1.15f);}
        }
        colour(192,219,206);text(40,756,g.source_arcade?"F9 Layout  F10 Quick options  Space Pause":"F8 Arcade  F9 Layout  F10 Options  Space Pause  R Quick rematch",1.2f);text(755,756,"Seed "+std::to_string(g.source_arcade?g.seed:match_seed),1.2f);
        if(g.source_arcade){const auto credit=m.read_byte(Game::globals+0x76);text(40,780,"Credits "+std::to_string((credit>>4)*10+(credit&15))+"  [5] insert coin",1.1f);}
        if(arcade&&arcade->active&&arcade->terminal){colour(7,18,24,245);rect(180,292,680,110);colour(255,221,150);text(220,315,arcade->destination==puzzle_fighter::ArcadeScene::ending?"COURSE COMPLETE":"GAME OVER",2.3f);text(220,360,"F8 Play arcade again / F10 Quick match",1.4f);}
        if(!g.source_arcade&&match->finished&&match->post_complete&&error.empty()&&!paused){colour(7,18,24,245);rect(180,292,680,110);colour(255,221,150);text(220,315,std::string(side_name(unsigned(match->winner),g.settings.opponent))+" WINS MATCH",2.3f);text(220,360,"R Rematch / F10 Characters and options",1.4f);}
        if(paused||!error.empty()){colour(7,18,24,245);rect(130,300,780,130);colour(255,221,150);text(155,320,paused?"PAUSED":error,paused?2.0f:1.0f);}
    }
    void draw(Game&g,bool paused,const std::string&error) {
        if((original_layout||(g.source_arcade&&(g.memory.read_word(Game::globals+4)!=4||g.memory.read_word(Game::globals+8)!=4)))&&!g.settings.legacy){draw_original(g,paused,error);return;}
        game=&g;visible_break=false;auto&m=g.memory;m.accesses=0;
        colour(12,40,43);SDL_RenderClear(renderer);
        for(unsigned y=0;y<800;y+=32){colour(16+y/45,51+y/28,52+y/35);rect(0,float(y),1040,32);}
        colour(12,26,35);rect(316,88,408,486);
        colour(35,54,65);rect(316,450,408,124);colour(42,91,84);rect(316,568,408,12);
        if(!g.settings.legacy){SDL_FRect target{0,88,1040,560};SDL_RenderTexture(renderer,art->stage(renderer,m),nullptr,&target);}
        colour(244,221,133);text(338,20,"PUZZLE FIGHTER",2.5f);
        colour(211,232,222);text(350,59,"ROUND "+std::to_string(match->round)+"  /  BEST OF "+std::to_string(g.settings.legacy?1:g.settings.best_of()),1.5f);
        for(unsigned i=0;i<2;++i) {
            const auto p=Game::players[i];const float bx=i?752:48,by=116;
            const bool loser=match->winner>=0&&unsigned(match->winner)!=i;
            const bool cpu_side=i&&g.settings.opponent;
            colour(cpu_side?170:(i?125:245),cpu_side?170:(i?199:154),cpu_side?170:(i?245:139));text(bx,22,character_name(m.read_byte(p+0x82)),1.8f);
            const auto&warning=attacks->players[i];const unsigned incoming=warning.pending;
            const bool bright=incoming&&(warning.flash||warning.severity>=2)&&((attacks->ticks/6)%2);
            if(g.settings.legacy){
            colour(bright?153:29,bright?46:54,bright?34:56);rect(bx-3,44,151,62);rect(bx+157,44,86,62);
            colour(incoming?255:133,incoming?(warning.severity>=2?116:218):163,incoming?101:157);rect(bx-3,44,151,62,false);rect(bx+157,44,86,62,false);
            text(bx+8,49,"INCOMING",1.1f);text(bx+9,68,std::to_string(incoming),3);
            text(bx+169,49,"ROWS",1);text(bx+171,68,std::to_string(warning.rows),2.3f);
            for(unsigned n=0;n<warning.remainder;++n){colour(255,211,111);rect(bx+165+n*12,99,8,4);}
            if(warning.overflow){colour(255,116,82);text(bx+84,84,"DANGER",1);}
            }
            colour(159,189,176);rect(bx-5,by-5,250,530);colour(6,14,22);rect(bx,by,240,520);
            if(g.settings.legacy&&incoming){colour(255,bright?104:173,bright?75:102);rect(bx-5,by-5,250,530,false);}
            const SDL_Rect board_clip{int(bx),int(by),240,520};SDL_SetRenderClipRect(renderer,&board_clip);
            const auto field=m.read_long(p+0x164);
            for(unsigned y=0;y<13;++y)for(unsigned x=0;x<6;++x) {
                colour(21,37,43);rect(bx+x*40,by+y*40,40,40,false);
                if(!g.settings.legacy)continue;
                if(loss_row_hidden(m,p,y))continue;
                auto value=m.read_word(field+y*16+x*2+2);const auto&cell=breaks->cells[i][y*6+x];
                if(!cell.marked)gem(bx+x*40,by+y*40,value);
                shatter(bx+x*40,by+y*40,cell);
            }

            if(g.settings.legacy&&m.read_byte(p+0x17e)==0&&m.read_byte(Game::players[1-i]+0x17e)!=20&&!match->waiting) {
                const auto cell=g.pair_address(p),second=g.secondary(p,cell);
                const float slide=float(Game::signed_word(m.read_word(p+0x242)))*2.5f;
                for(unsigned k=0;k<2;++k){int delta=int((k?second:cell)-field),row=delta/16,col=(delta%16-2)/2;if(row>=-1&&row<13&&col>=0&&col<6)gem(bx+col*40,by+row*40+slide,m.read_word(p+0x152+k*2),true);}
            }
            if(loser){colour(25,30,39,175);rect(bx,by,240,520);}
            SDL_SetRenderClipRect(renderer,nullptr);
            if(g.settings.legacy&&warning.countered){colour(17,45,39,230);rect(bx+28,by+24,188,26);colour(151,244,181);text(bx+38,by+31,"COUNTERED",1.7f);}
            if(g.settings.legacy&&loser){colour(238,145,138);text(bx+52,by+230,"LOSE",3);}
            colour(cpu_side?170:(i?125:245),cpu_side?170:(i?199:154),cpu_side?170:(i?245:139));text(bx,662,std::string(side_name(i,g.settings.opponent))+"  WINS "+std::to_string(match->wins[i]),1.8f);
            colour(218,230,218);
            if(i&&g.settings.opponent==2){
                text(bx,698,"CLASSIC CPU",1.2f);
                text(bx,718,"Difficulty: "+std::string(difficulty_name(g.settings.difficulty)),1.05f);
                text(bx,738,"Solo level: "+std::string(course_name(g.settings.course)),1.05f);
                text(bx,758,"Game speed: "+std::string(speed_name(g.settings.speed)),1.05f);
                text(bx,778,"Damage: "+std::string(damage_name(g.settings.damage)),1.05f);
            }else if(i&&g.settings.opponent==1){
                text(bx,698,"TESTING MACRO",1.2f);
                text(bx,718,"Game speed: "+std::string(speed_name(g.settings.speed)),1.05f);
                text(bx,738,"Player speed: "+std::to_string(g.settings.handicap?g.settings.handicaps[1]+1:3),1.05f);
                text(bx,758,"Damage: "+std::string(damage_name(g.settings.damage)),1.05f);
            }else{
                const bool solo=g.settings.opponent!=0;
                text(bx,696,solo?"Move: A/D or arrows":i?"Move: Left / Right":"Move: A / D",1.05f);
                text(bx,714,solo?"Drop: S or Down":i?"Drop: Down":"Drop: S",1.05f);
                text(bx,732,solo?"Rotate: C/V or J/K":i?"Rotate: J / K":"Rotate: C / V",1.05f);
                text(bx,750,solo?"Taunt: Enter / keypad Enter":i?"Taunt: keypad Enter":"Taunt: Enter",1.0f);
                if(controllers[i])text(bx,770,"Pad: move / face turn; left taunt",1.0f);
            }
            if(g.settings.legacy){colour(245,215,140);for(unsigned win=0;win<match->wins[i];++win)text(bx+168+win*23,666,"V",1.5f);}
            const float nx=i?625:365;colour(cpu_side?170:(i?130:255),cpu_side?170:(i?205:151),cpu_side?170:(i?255:149));text(nx-14,112,"NEXT",1.8f);
            colour(4,13,20);rect(nx-5,144,50,88);if(g.settings.legacy){gem(nx,148,m.read_word(p+0x158));gem(nx,188,m.read_word(p+0x156));}
            colour(242,229,173);text(nx-25,246,character_name(m.read_byte(p+0x82)),1.15f);
            const auto source_actor=Game::globals+0xb00+i*0x100;
            if(g.settings.legacy&&characters&&(g.settings.legacy||m.read_byte(source_actor+1))){const auto&pose=characters->poses[i];if(auto*t=art->figure(renderer,m,pose.character,pose.frame,g.settings.legacy?-1:int(i))){SDL_FRect target{(i?618.0f:422.0f)-192,228,384,384};
                if(!g.settings.legacy){const auto normal=0x14902u+(pose.character*2+i)*4;target.x+=2*Game::signed_word(std::uint16_t(m.read_word(source_actor+0x10)-m.opcode_word(normal)));target.y+=2*Game::signed_word(std::uint16_t(m.opcode_word(normal+2)-m.read_word(source_actor+0x14)));}
                const SDL_Rect clip{316,270,408,310};SDL_SetRenderClipRect(renderer,&clip);SDL_RenderTextureRotated(renderer,t,nullptr,&target,0,nullptr,i?SDL_FLIP_HORIZONTAL:SDL_FLIP_NONE);SDL_SetRenderClipRect(renderer,nullptr);}}
            colour(cpu_side?155:(i?102:244),cpu_side?155:(i?173:143),cpu_side?155:(i?210:146));rect(334+i*205,600,190,63);colour(13,32,32);text(344+i*205,609,"SCORE",1.2f);text(344+i*205,631,std::to_string(m.read_long(p+0xaa)),1.8f);
            const unsigned chain=m.read_byte(p+0x19c);if(g.settings.legacy&&chain>1){colour(249,223,125);text(bx+38,by+40,std::to_string(chain)+" CHAIN",2);}
        }
        colour(192,219,206);text(342,682,"F10  CHARACTERS / OPTIONS",1.25f);text(342,711,g.source_arcade?"SPACE pause":"R rematch   SPACE pause",1.25f);text(342,740,"ESC quit",1.25f);text(342,771,"Seed: "+std::to_string(match_seed),1.1f);
        if(!g.settings.legacy&&error.empty()){original_text(m);source_packets(g);}
        if(g.settings.legacy&&match->intro&&!paused&&error.empty()) {
            colour(9,24,30,239);rect(325,294,390,90);colour(255,222,155);
            text(363,317,match->intro_elapsed<72?"ROUND "+std::to_string(match->round):"FIGHT!",3);
        }
        if((g.settings.legacy&&match->waiting)||(match->finished&&match->post_complete)||paused||!error.empty()) {
            colour(9,24,30,235);rect(325,294,390,90);colour(255,222,155);
            const std::string title=!error.empty()?"POSITION ERROR":paused?"PAUSED":match->finished?std::string(side_name(unsigned(match->winner),g.settings.opponent))+" WINS MATCH":std::string(side_name(unsigned(match->winner),g.settings.opponent))+" WINS ROUND";
            text(345,310,title,2);colour(216,232,214);text(345,350,match->finished?"R rematch / F10 options":match->waiting?"Round complete":"SPACE resume",1.35f);
        }
    }
    void capture(const std::string&path) {
        auto*surface=SDL_RenderReadPixels(renderer,nullptr);if(!surface)throw std::runtime_error(SDL_GetError());
        const bool ok=SDL_SaveBMP(surface,path.c_str());SDL_DestroySurface(surface);if(!ok)throw std::runtime_error(SDL_GetError());
    }
};
#include "menu.hpp"
int main(int argc,char**argv)try {
    auto o=options(argc,argv);std::filesystem::create_directories(o.user_dir);Game game(o.root,o.seed,o.config);if(o.exercise)exercise(game);
    SoloMacro macro;BreakVisuals visuals;CharacterVisuals actors;Match match(game.settings.legacy);SetupMenu menu;AttackHud attacks;DangerMusic danger_music;MatchFeedback feedback;
    NativeMatchRuntime source_runtime(game,match);
    source_runtime.cabinet.operator_store.path=o.user_dir+"/arcade-operator-counters.bin";
    std::unique_ptr<NativeAudio> audio;
    if(!o.mute&&(!o.headless||!o.audio_wav.empty()))audio=std::make_unique<NativeAudio>(o.root,!o.audio_wav.empty(),o.music);
    if(game.settings.legacy)game.effects.sink=[&](SoundRequest r){if(audio&&(o.music||r.number>0x10))audio->enqueue(r);};
    source_runtime.device_ready=[&](){return !audio||audio->source_command_ready();};
    source_runtime.deliver_sound=[&](SourceSoundCommand r){if(audio&&(o.music||r.number>0x10))audio->enqueue({r.number,r.pan,r.extra});};
    source_runtime.set_stereo=[&](std::uint8_t value){if(audio)audio->source_stereo(value);};
    source_runtime.ending_complete=[&](){return (!audio||!o.music||audio->music_finished())&&(!audio||(feedback.voice_sent&&audio->effects_finished()))&&game.memory.read_word(Game::globals+0x57a0)==game.memory.read_word(Game::globals+0x57a2);};
    source_runtime.winner_event=[&](){if(!source_runtime.arcade.active){actors.finish_round(game,unsigned(match.winner));puzzle_fighter::source_post_match_jingle(Game::globals,game.memory);}};
    source_runtime.new_round_event=[&](){visuals.observe(game);if(o.exercise)exercise(game);};
    const auto reset_audio=[&](){if(!game.settings.legacy)game.memory.write_byte(Game::globals+0xa6,!o.music);if(audio)audio->reset(game.settings.legacy?1:0);};
    if(!game.settings.legacy)source_runtime.restart();
    const auto begin_arcade=[&](){source_runtime.begin_arcade();game.memory.write_byte(Game::globals+0xa6,!o.music);};
    reset_audio();if(o.arcade)begin_arcade();else{actors.observe(game);menu.animate(game,o.config);attacks.observe(game);}
    const auto tick=[&](std::array<std::uint16_t,2> input){
        if(!game.settings.legacy){
            if(!source_runtime.arcade.active&&game.settings.opponent==1&&match.playing())input[1]=macro.input(game);
            source_runtime.tick(input);if(!source_runtime.arcade.active)visuals.observe(game);
        }else{
            game.begin_host_frame();if(game.settings.opponent==1&&match.playing())input[1]=macro.input(game);game.stage_inputs(input);
            if(match.intro)match.advance_intro(game);else if(match.playing()){if(match.observe(game)){actors.finish_round(game,unsigned(match.winner));if(audio)audio->music_cue(0x10);}}else if(match.advance(game,(!audio||!o.music||audio->music_finished())&&(!audio||(feedback.voice_sent&&audio->effects_finished())))){visuals.observe(game);if(o.exercise)exercise(game);}
            if(!match.post_complete){game.tick(input,false,true);}
            visuals.observe(game);if(audio&&match.playing())danger_music.observe(game,*audio);game.presentation_tasks();
        }
        if(!source_runtime.arcade.active){actors.observe(game);attacks.observe(game);}
        if(audio){if(!source_runtime.arcade.active)feedback.observe(game,actors,match.waiting,match.winner,*audio);audio->frame();}
    };
    const auto finish_audio=[&](){if(audio){audio->save(o.audio_wav);std::cerr<<"Native audio: "<<audio->requests<<" commands, "<<audio->nonzero_samples<<" nonzero samples, peak "<<audio->peak<<'\n';}};
    if(o.headless) {for(unsigned i=0;i<o.frames;++i)tick(o.demo?demonstration(game):std::array<std::uint16_t,2>{});report(game,o,macro,visuals,match,feedback,&source_runtime);finish_audio();return 0;}
    View view(o.root);view.arcade=&source_runtime.arcade;view.original_layout=o.original_layout;view.match_seed=o.seed;view.breaks=&visuals;view.characters=&actors;view.match=&match;view.attacks=&attacks;view.solo=o.solo;Gamepads pads;
    if(audio) {
        try{audio->open_device();view.audio_playing=true;}
        catch(const std::exception&e){std::cerr<<"Audio device unavailable: "<<e.what()<<'\n';}
    }
    bool running=true,paused=false,setup=o.setup;std::string error;unsigned frames=0;double last=SDL_GetTicksNS()/1e9,accum=0;
    SDL_StartTextInput(view.window);
    const auto menu_audio=[&](){if(audio)for(const auto&r:menu.sounds)audio->enqueue(r);menu.sounds.clear();};
    if(setup&&audio)audio->music_cue(0x0e);
    const auto restart=[&](bool from_menu){
        if(from_menu){o.config.legacy=false;o.config.validate();if(o.save_options)o.config.save(o.user_dir);}
        o.seed=o.config.new_match_seed();view.match_seed=o.seed;game.settings=o.config;game.seed=o.seed;game.reset();match=Match(game.settings.legacy);if(!game.settings.legacy)source_runtime.restart();macro=SoloMacro{};visuals=BreakVisuals{};actors=CharacterVisuals{};danger_music=DangerMusic{};feedback=MatchFeedback{};
        if(o.exercise)exercise(game);
        visuals.observe(game);actors.observe(game);attacks=AttackHud{};attacks.observe(game);reset_audio();menu_audio();if(audio)audio->pause(false);
        setup=false;error.clear();paused=false;
    };
    const auto enter_setup=[&](){setup=true;o.config=game.settings;o.config.legacy=false;menu=SetupMenu{};menu.animate(game,o.config);if(audio){audio->pause(false);audio->music_cue(0x0e);}};
    const auto enter_arcade=[&](){
        game.settings.random_seed=o.config.random_seed;game.settings.chosen_seed=o.config.chosen_seed;
        reset_audio();begin_arcade();match=Match{};macro=SoloMacro{};visuals=BreakVisuals{};actors=CharacterVisuals{};attacks=AttackHud{};danger_music=DangerMusic{};feedback=MatchFeedback{};
        setup=false;error.clear();paused=false;view.original_layout=true;if(audio)audio->pause(false);
    };
    while(running) {
        // Route a scheduled restart through the actual window key handler.
        if(o.restart_at&&frames>=o.restart_at) {
            SDL_Event restart{};restart.type=SDL_EVENT_KEY_DOWN;
            restart.key.scancode=SDL_SCANCODE_R;SDL_PushEvent(&restart);o.restart_at=0;
        }
        if(o.start_at&&frames>=o.start_at){SDL_Event start{};start.type=SDL_EVENT_KEY_DOWN;start.key.scancode=SDL_SCANCODE_RETURN;SDL_PushEvent(&start);o.start_at=0;}
        while(!o.menu_keys.empty()&&frames>=o.menu_keys.front().first){SDL_Event key{};key.type=SDL_EVENT_KEY_DOWN;key.key.scancode=o.menu_keys.front().second;SDL_PushEvent(&key);o.menu_keys.erase(o.menu_keys.begin());}
        SDL_Event ev;
        while(SDL_PollEvent(&ev)) {
            const int removed=pads.event(ev);
            if(removed>=0&&!setup&&match.playing()&&(removed==0||game.settings.opponent==0)){paused=true;if(audio)audio->pause(true);}
            if(ev.type==SDL_EVENT_QUIT)running=false;
            if(setup&&ev.type==SDL_EVENT_TEXT_INPUT)menu.text(ev.text.text,o.config);
            const auto code=ev.type==SDL_EVENT_KEY_DOWN?ev.key.scancode:SDL_SCANCODE_UNKNOWN;
            const bool menu_direction=setup&&(code==SDL_SCANCODE_W||code==SDL_SCANCODE_A||code==SDL_SCANCODE_S||code==SDL_SCANCODE_D||code==SDL_SCANCODE_UP||code==SDL_SCANCODE_LEFT||code==SDL_SCANCODE_DOWN||code==SDL_SCANCODE_RIGHT);
            if(ev.type==SDL_EVENT_KEY_DOWN&&(!ev.key.repeat||menu_direction)) {
                if(ev.key.scancode==SDL_SCANCODE_ESCAPE)running=false;
                else if(ev.key.scancode==SDL_SCANCODE_F10)enter_setup();
                else if(ev.key.scancode==SDL_SCANCODE_F8){if(setup||!source_runtime.arcade.active)enter_arcade();}
                else if(!setup&&source_runtime.arcade.active&&ev.key.scancode==SDL_SCANCODE_5)source_runtime.insert_coin();
                else if(!setup&&source_runtime.arcade.active&&ev.key.scancode==SDL_SCANCODE_6)source_runtime.insert_coin(1);
                else if(ev.key.scancode==SDL_SCANCODE_F9)view.original_layout=!view.original_layout;
                else if(setup){if(menu.keyboard(ev.key.scancode,o.config))restart(true);}
                else if(ev.key.scancode==SDL_SCANCODE_SPACE){paused=!paused;if(audio)audio->pause(paused);}
                else if(ev.key.scancode==SDL_SCANCODE_R&&!source_runtime.arcade.active)restart(false);
            }
            if(setup&&ev.type==SDL_EVENT_MOUSE_BUTTON_DOWN){float x=0,y=0;SDL_RenderCoordinatesFromWindow(view.renderer,ev.button.x,ev.button.y,&x,&y);if(menu.click(x,y,ev.button.button==SDL_BUTTON_RIGHT,o.config))restart(true);}
        }
        const double now=SDL_GetTicksNS()/1e9;accum+=std::min(now-last,0.2);last=now;
        if(o.demo&&o.frames)accum=1.0/60;
        while(accum>=1.0/60) {
            accum-=1.0/60;
            const auto controller=pads.sample(SDL_GetTicks());
            if(controller.options&&!setup)enter_setup();
            if(setup) {
                bool start=controller.start;
                for(const auto key:controller.menu){start=menu.key(key,o.config)||start;if(start)break;}
                if(start)restart(true);
            }else if(!source_runtime.arcade.active&&controller.start) {
                if(match.finished)restart(false);else {paused=!paused;if(audio)audio->pause(paused);}
            }else if(!source_runtime.arcade.active&&match.finished&&(controller.confirm[0]||controller.confirm[1]))restart(false);
            if(!setup&&!paused&&error.empty()) {
                auto*k=SDL_GetKeyboardState(nullptr);
                auto input=keyboard_gameplay(k,source_runtime.arcade.active?0:game.settings.opponent);
                if(source_runtime.arcade.active){if(k[SDL_SCANCODE_W])input[0]|=8;if(k[SDL_SCANCODE_UP])input[1]|=8;}
                for(unsigned i=0;i<2;++i)input[i]|=controller.gameplay[i];
                try{tick(o.demo?demonstration(game):input);}catch(const std::exception&e){error=e.what();std::cerr<<"tick "<<game.ticks<<": "<<error<<'\n';}
            }else if(setup){menu_audio();menu.animate(game,o.config);if(audio)audio->frame();}
            ++frames;if(o.frames&&frames>=o.frames){running=false;break;}
        }
        view.controllers={pads.connected(0),pads.connected(1)};menu.controllers=view.controllers;
        if(setup)menu.draw(view,game,o.config);else view.draw(game,paused,error);
        if(view.visible_break&&!o.capture_break.empty()){view.capture(o.capture_break);o.capture_break.clear();}
        if(!setup&&!o.capture_attack.empty()&&(attacks.players[0].pending>=12||attacks.players[1].pending>=12)){view.capture(o.capture_attack);o.capture_attack.clear();}
        if(!running&&!o.capture.empty())view.capture(o.capture);
        SDL_RenderPresent(view.renderer);SDL_Delay(1);
    }
    report(game,o,macro,visuals,match,feedback,&source_runtime);finish_audio();audio.reset();return error.empty()?0:1;
}catch(const std::exception&e){std::cerr<<"Prototype: "<<e.what()<<'\n';return 1;}
