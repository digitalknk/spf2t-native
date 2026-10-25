// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <array>
#include <cstdint>
#include <fstream>
#include <string>
#include <stdexcept>
#include <random>
namespace puzzle_fighter::prototype {
inline const char*side_name(unsigned side,unsigned opponent){return side?(opponent?"CPU":"P2"):"P1";}
inline const char*difficulty_name(unsigned n){constexpr const char*names[]={"Easiest","Very easy","Easy","Medium","Medium hard","Hard","Very hard","Hardest"};return names[n];}
inline const char*speed_name(unsigned n){constexpr const char*names[]={"Slow","Normal","Fast","Fastest"};return names[n];}
inline const char*course_name(unsigned n){constexpr const char*names[]={"Easy","Normal","Hard"};return names[n];}
inline const char*damage_name(unsigned n){constexpr const char*names[]={"Low","Normal","High","Maximum"};return names[n];}
struct GameSettings {
    // The old setup is retained explicitly for saved regression scenarios.
    bool legacy=true,handicap=true;
    bool random_seed=true;
    unsigned chosen_seed=0x920d;
    // Hard is the native standalone default course, using the original rules;
    // unlike the first Normal arcade stage it exercises full CPU steering.
    unsigned opponent=0,difficulty=3,course=2,speed=1,damage=1,rounds_cpu=1,rounds_human=3;
    std::array<unsigned,2> characters{0,1},handicaps{2,2};
    unsigned best_of()const{return opponent?rounds_cpu:rounds_human;}
    std::uint16_t new_match_seed()const{return random_seed?std::uint16_t(std::random_device{}()):std::uint16_t(chosen_seed);}
    void validate()const {
        if(opponent>2||difficulty>7||course>2||speed>3||damage>3||chosen_seed>65535||characters[0]>10||characters[1]>10||handicaps[0]>4||handicaps[1]>4||
           (rounds_cpu!=1&&rounds_cpu!=3&&rounds_cpu!=5)||(rounds_human!=1&&rounds_human!=3&&rounds_human!=5))
            throw std::runtime_error("Unsupported original game setting");
    }
    void load(const std::string&root) {
        std::ifstream f(root+"/player-options.ini");std::string key;unsigned value;
        while(f>>key>>value) {
            if(key=="p1"&&value<11)characters[0]=value;else if(key=="p2"&&value<11)characters[1]=value;
            else if(key=="difficulty"&&value<8)difficulty=value;else if(key=="speed"&&value<4)speed=value;
            else if(key=="course"&&value<3)course=value;
            else if(key=="damage"&&value<4)damage=value;
            else if(key=="seed_random")random_seed=value!=0;
            else if(key=="seed_value"&&value<=65535)chosen_seed=value;
            else if(key=="handicap")handicap=value!=0;else if(key=="h1"&&value<5)handicaps[0]=value;
            else if(key=="h2"&&value<5)handicaps[1]=value;
            else if(key=="rounds1"&&(value==1||value==3||value==5))rounds_cpu=value;
            else if(key=="rounds2"&&(value==1||value==3||value==5))rounds_human=value;
        }
    }
    void save(const std::string&root)const {
        std::ofstream f(root+"/player-options.ini");if(!f)throw std::runtime_error("Cannot save project-local game options");
        f<<"p1 "<<characters[0]<<"\np2 "<<characters[1]<<"\ndifficulty "<<difficulty<<"\ncourse "<<course<<"\nspeed "<<speed
         <<"\nhandicap "<<handicap<<"\nh1 "<<handicaps[0]<<"\nh2 "<<handicaps[1]<<"\nrounds1 "<<rounds_cpu<<"\nrounds2 "<<rounds_human
         <<"\nseed_random "<<random_seed<<"\nseed_value "<<chosen_seed<<"\ndamage "<<damage<<'\n';
    }
};
} // namespace puzzle_fighter::prototype
