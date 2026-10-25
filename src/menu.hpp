// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "keyboard.hpp"
namespace puzzle_fighter::prototype {
// Native PC-port UI. Player cursors/previews never write gameplay RAM or RNG.
struct SetupMenu {
    static constexpr unsigned columns=4,character_rows=3,settings_count=11,start_focus=12;
    static constexpr float settings_y=416,row_height=24,left_arrow=584,right_arrow=770;
    struct Cursor {unsigned focus=0;bool ready=false;}; // 0 roster, 1..11 settings, 12 Start
    std::array<Cursor,2> cursors{};
    std::array<bool,2> controllers{};
    std::array<bool,2> choosing_speed{};
    std::vector<SoundRequest> sounds;
    std::string seed_text;bool editing_seed=false;
    bool edit_opponent=false;
    CharacterVisuals previews;unsigned ticks=0;
    static const char*difficulty(unsigned n){return difficulty_name(n);}
    static const char*speed(unsigned n){return speed_name(n);}
    static const char*course(unsigned n){return course_name(n);}
    static bool enabled(unsigned row,const GameSettings&c){return (row==2||row==3)?c.opponent==2:(row>=5&&row<=7)?c.opponent!=2:row==10?!c.random_seed:true;}
    void sound(unsigned player,bool selected=false){sounds.push_back({std::uint16_t((selected?0xe4:0xe2)+player),0x1000,0});}
    static unsigned cycle(unsigned value,int direction,unsigned count){return unsigned((int(value)+direction+int(count))%int(count));}
    bool both_ready()const{return cursors[0].ready&&cursors[1].ready;}
    bool confirm(unsigned player){choosing_speed[player]=false;sound(player,true);cursors[player]={start_focus,true};return both_ready();}
    bool choose(unsigned player,GameSettings&c){
        if(!cursors[player].focus&&c.opponent!=2&&c.handicap&&!choosing_speed[player]){choosing_speed[player]=true;sound(player,true);return false;}
        return confirm(player);
    }
    void cancel(unsigned player){cursors[player].ready=false;}
    void vertical(unsigned player,int direction,GameSettings&c) {
        editing_seed=false;
        auto&cursor=cursors[player];auto&ch=c.characters[player];cursor.ready=false;
        if(cursor.focus==0) {
            const int row=int(ch/columns)+direction;const unsigned col=ch%columns;
            if(row<0)cursor.focus=start_focus;
            else if(row>=int(character_rows))cursor.focus=1;
            else ch=std::min(unsigned(row)*columns+col,10u);
        }else if(cursor.focus==start_focus) {
            cursor.focus=direction<0?settings_count:0;
            if(!cursor.focus)ch%=columns;
        }else if(cursor.focus==1&&direction<0) {
            cursor.focus=0;ch=std::min(8u+ch%columns,10u);
        }else cursor.focus=unsigned(int(cursor.focus)+direction);
    }
    void horizontal(unsigned player,int direction,GameSettings&c) {
        editing_seed=false;
        if(choosing_speed[player]){c.handicaps[player]=cycle(c.handicaps[player],direction,5);return;}
        const auto row=cursors[player].focus;
        if(!row){auto&ch=c.characters[player];const unsigned first=ch/columns*columns;ch=first+cycle(ch-first,direction,std::min(columns,11-first));return;}
        if(!enabled(row,c))return;
        if(row==1){c.opponent=cycle(c.opponent,direction,3);choosing_speed={};edit_opponent=false;}
        else if(row==2)c.course=cycle(c.course,direction,3);
        else if(row==3)c.difficulty=cycle(c.difficulty,direction,8);
        else if(row==4)c.speed=cycle(c.speed,direction,4);
        else if(row==5)c.handicap=!c.handicap;
        else if(row==6||row==7){if(c.handicap)c.handicaps[row-6]=cycle(c.handicaps[row-6],direction,5);}
        else if(row==8){auto&rounds=c.opponent?c.rounds_cpu:c.rounds_human;rounds=cycle((rounds-1)/2,direction,3)*2+1;}
        else if(row==9)c.random_seed=!c.random_seed;
        else if(row==10)c.chosen_seed=cycle(c.chosen_seed,direction,65536);
        else if(row==11)c.damage=cycle(c.damage,direction,4);
    }
    void text(const char*input,GameSettings&c){
        if(c.random_seed||(cursors[0].focus!=10&&cursors[1].focus!=10))return;
        if(!editing_seed){seed_text.clear();editing_seed=true;}
        for(const char*p=input;*p;++p)if(*p>='0'&&*p<='9'){
            const auto candidate=seed_text+*p;
            if(candidate.size()<=5&&std::stoul(candidate)<=65535){seed_text=candidate;c.chosen_seed=unsigned(std::stoul(seed_text));}
        }
    }
    bool key(SDL_Scancode code,GameSettings&c) {
        if(code==SDL_SCANCODE_RETURN||code==SDL_SCANCODE_KP_ENTER)return true;
        if(code==SDL_SCANCODE_BACKSPACE&&!c.random_seed&&(cursors[0].focus==10||cursors[1].focus==10)){
            if(!editing_seed)seed_text=std::to_string(c.chosen_seed);
            editing_seed=true;if(!seed_text.empty())seed_text.pop_back();c.chosen_seed=seed_text.empty()?0:unsigned(std::stoul(seed_text));return false;
        }
        unsigned player;
        switch(code) {
            case SDL_SCANCODE_W:case SDL_SCANCODE_A:case SDL_SCANCODE_S:case SDL_SCANCODE_D:case SDL_SCANCODE_C:case SDL_SCANCODE_V:case SDL_SCANCODE_Q:case SDL_SCANCODE_E:player=0;break;
            case SDL_SCANCODE_UP:case SDL_SCANCODE_LEFT:case SDL_SCANCODE_DOWN:case SDL_SCANCODE_RIGHT:case SDL_SCANCODE_J:case SDL_SCANCODE_K:player=1;break;
            default:return false;
        }
        if(code==SDL_SCANCODE_C||code==SDL_SCANCODE_Q||code==SDL_SCANCODE_J)return choose(player,c);
        if(code==SDL_SCANCODE_V||code==SDL_SCANCODE_E||code==SDL_SCANCODE_K){choosing_speed[player]=false;cancel(player);sound(player);return false;}
        if(choosing_speed[player]){if(code==SDL_SCANCODE_A||code==SDL_SCANCODE_LEFT||code==SDL_SCANCODE_D||code==SDL_SCANCODE_RIGHT){horizontal(player,(code==SDL_SCANCODE_A||code==SDL_SCANCODE_LEFT)?-1:1,c);sound(player);}return false;}
        if(!enabled(cursors[player].focus,c)&&(code==SDL_SCANCODE_A||code==SDL_SCANCODE_LEFT||code==SDL_SCANCODE_D||code==SDL_SCANCODE_RIGHT))return false;
        if(code==SDL_SCANCODE_W||code==SDL_SCANCODE_UP)vertical(player,-1,c);
        else if(code==SDL_SCANCODE_S||code==SDL_SCANCODE_DOWN)vertical(player,1,c);
        else horizontal(player,(code==SDL_SCANCODE_A||code==SDL_SCANCODE_LEFT)?-1:1,c);
        sound(player);return false;
    }
    bool keyboard(SDL_Scancode code,GameSettings&c){
        if(code==SDL_SCANCODE_TAB&&c.opponent){edit_opponent=!edit_opponent;sound(edit_opponent?1:0);return false;}
        return key(keyboard_menu_key(code,c.opponent,edit_opponent),c);
    }
    bool click(float x,float y,bool right,GameSettings&c) {
        const unsigned player=right?1:0;
        if(choosing_speed[player]){
            const float px=player?802:34;
            if(x>=px&&x<px+204&&y>=420&&y<488){if(y>=458)return confirm(player);horizontal(player,x<px+102?-1:1,c);sound(player);}return false;
        }
        if(x>=304&&x<736&&y>=126&&y<354) {
            const unsigned col=unsigned(x-304)/108,row=unsigned(y-126)/76,index=row*columns+col;
            if(index<11){c.characters[player]=index;cursors[player]={0,false};sound(player);}return false;
        }
        if(x>=238&&x<802&&y>=settings_y&&y<settings_y+settings_count*row_height) {
            cursors[player]={1+unsigned((y-settings_y)/row_height),false};editing_seed=false;
            if(x>=left_arrow&&x<left_arrow+24){horizontal(player,-1,c);sound(player);}
            else if(x>=right_arrow&&x<right_arrow+24){horizontal(player,1,c);sound(player);}
            return false;
        }
        if(x>=304&&x<736&&y>=704&&y<752)return confirm(right?1:(x>=520?1:0));
        return false;
    }
    void animate(Game&g,const GameSettings&c) {
        auto&m=g.memory;m.accesses=0;++ticks;
        for(unsigned i=0;i<2;++i){auto&p=previews.poses[i];if(p.character!=c.characters[i])previews.start(m,p,c.characters[i],0);else previews.step(m,p);}
    }
    template<class V>static void player_colour(V&v,unsigned i,unsigned opponent){if(i&&opponent)v.colour(170,170,170);else v.colour(i?109:255,i?196:155,i?250:129);}
    template<class V>void marker(V&v,unsigned player,float x,float y,unsigned opponent)const {
        player_colour(v,player,opponent);v.rect(x,y,player&&opponent?36:29,20);v.colour(10,25,28);v.text(x+3,y+5,side_name(player,opponent),1.25f);
    }
    template<class V>void draw(V&v,Game&g,const GameSettings&c) {
        g.memory.accesses=0;v.game=&g;v.colour(12,27,30);SDL_RenderClear(v.renderer);
        v.colour(245,226,154);v.text(304,22,"PUZZLE FIGHTER",2.7f);
        v.colour(177,216,203);v.text(304,70,"CHOOSE FIGHTERS / MATCH OPTIONS   F8 ARCADE",1.2f);
        for(unsigned i=0;i<2;++i) {
            const float x=i?762:34;player_colour(v,i,c.opponent);v.text(x,94,std::string(side_name(i,c.opponent))+"  "+character_name(c.characters[i]),1.5f);
            const auto&p=previews.poses[i];
            if(p.frame)if(auto*t=v.art->figure(v.renderer,g.memory,p.character,p.frame)) {
                SDL_FRect target{x-2,76,244,244};SDL_RenderTextureRotated(v.renderer,t,nullptr,&target,0,nullptr,i?SDL_FLIP_HORIZONTAL:SDL_FLIP_NONE);
            }
            player_colour(v,i,c.opponent);v.text(x+38,292,"ATTACK PATTERN",1.1f);
            for(unsigned row=0;row<4;++row)for(unsigned col=0;col<6;++col)
                v.gem(x+49+col*24,312+row*24,attack_preview_gem(g.memory,c.characters[i],row,col),false,24);
            if(choosing_speed[i]){
                const float px=i?802:34;v.colour(27,55,58);v.rect(px,420,204,78);player_colour(v,i,c.opponent);
                v.text(px+10,428,"CHOOSE YOUR SPEED",1.25f);
                v.text(px+10,449,"<     "+std::to_string(c.handicaps[i]+1)+"     >",1.6f);
                v.text(px+10,479,controllers[i]?"Bottom button: ready":"Confirm: ready",1.1f);
            }
        }
        for(unsigned ch=0;ch<11;++ch) {
            const float x=304+(ch%columns)*108,y=126+(ch/columns)*76;
            v.colour(27,55,58);v.rect(x,y,102,70);
            for(unsigned i=0;i<2;++i)if(c.characters[i]==ch){player_colour(v,i,c.opponent);v.rect(x+float(i)*2,y+float(i)*2,102-float(i)*4,70-float(i)*4,false);if(!cursors[i].focus)marker(v,i,x+2+i*(c.opponent?60:67),y+2,c.opponent);}
            if(auto*t=v.art->portrait(v.renderer,g.memory,ch)){SDL_FRect target{x+33,y+15,36,36};SDL_RenderTexture(v.renderer,t,nullptr,&target);}
            else {CharacterVisuals::Pose pose;previews.start(g.memory,pose,ch,0);if(auto*t=v.art->figure(v.renderer,g.memory,ch,pose.frame)){SDL_FRect source{48,64,96,96},target{x+33,y+15,36,36};SDL_RenderTexture(v.renderer,t,&source,&target);}}
            v.colour(229,238,221);v.text(x+5,y+55,character_name(ch),1.05f);
        }
        if(c.opponent!=2&&((cursors[0].focus>=5&&cursors[0].focus<=7)||(cursors[1].focus>=5&&cursors[1].focus<=7))){v.colour(175,204,190);v.text(304,390,"Also adjusts attack balance",1.15f);}
        constexpr const char*opponents[]={"Two human players","Testing macro","Classic CPU"};
        const std::array<std::string,11> labels={"Opponent","Solo level","CPU difficulty","Game speed","Player speeds","P1 speed",c.opponent?"CPU speed":"P2 speed","Match format","Seed mode","Seed value","Damage"};
        const std::array<std::string,11> values={opponents[c.opponent],course(c.course),difficulty(c.difficulty),speed(c.speed),c.handicap?"On":"Off",std::to_string(c.handicaps[0]+1),std::to_string(c.handicaps[1]+1),"Best of "+std::to_string(c.best_of()),c.random_seed?"Random":"Choose",std::to_string(c.chosen_seed),damage_name(c.damage)};
        for(unsigned row=0;row<settings_count;++row) {
            const float y=settings_y+row*row_height;
            if(cursors[0].focus==row+1||cursors[1].focus==row+1){v.colour(30,62,59);v.rect(238,y,564,row_height-1);}
            for(unsigned i=0;i<2;++i)if(cursors[i].focus==row+1)marker(v,i,246+i*34,y+2,c.opponent);
            const bool active=enabled(row+1,c)&&(!(row==5||row==6)||c.handicap);
            v.colour(active?177:100,active?201:121,active?188:116);v.text(326,y+8,labels[row],1.1f);v.colour(active?246:100,active?229:121,active?178:116);
            if(active){v.text(left_arrow+4,y+8,"<",1.1f);v.text(612,y+8,values[row],1.1f);v.text(right_arrow+4,y+8,">",1.1f);}
            else v.text(612,y+8,row==9?values[row]:"--",1.1f);
        }
        for(unsigned i=0;i<2;++i) {
            const float x=304+i*216;
            if(cursors[i].ready)v.colour(64,153,104);else v.colour(53,75,71);
            v.rect(x,704,212,48);
            if(cursors[i].focus==start_focus){player_colour(v,i,c.opponent);v.rect(x,704,212,48,false);}
            player_colour(v,i,c.opponent);v.text(x+14,711,side_name(i,c.opponent),1.2f);v.colour(239,233,202);v.text(x+14,732,cursors[i].ready?"READY":"START MATCH",1.35f);
        }
        v.colour(179,210,196);
        v.text(28,760,c.opponent?"Move: WASD or arrows  Confirm: C/J  Cancel: V/K":"P1: WASD move / C confirm / V cancel",1.0f);
        v.text(558,760,c.opponent?(edit_opponent?"Editing CPU fighter / Tab: return to P1":"Tab: edit CPU fighter"):"P2: Arrows move / J confirm / K cancel",1.0f);
        v.colour(148,179,167);v.text(28,782,!c.random_seed&&(cursors[0].focus==10||cursors[1].focus==10)?"Type seed 0-65535 / Backspace edit / arrows adjust":(c.opponent?"Enter: start / Mouse: left P1, right CPU":"Enter: start / Mouse: left P1, right P2"),1.0f);
        if(controllers[0]||controllers[1])v.text(558,782,"Pad: stick move / bottom confirm / right cancel",1.0f);
    }
};
} // namespace puzzle_fighter::prototype
