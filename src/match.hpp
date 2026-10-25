// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game.hpp"
#include <puzzle_fighter/after_round.hpp>
#include <puzzle_fighter/last_opponent_intro.hpp>
namespace puzzle_fighter::prototype {
// Native match-flow glue. Original operator round choices determine the target;
// presentation delay/round bootstrap remain an explicitly provisional adapter.
struct Match {
    std::array<unsigned,2> wins{};
    unsigned round=1,completed_rounds=0,delay=0;int winner=-1;
    unsigned intro_elapsed=0,end_elapsed=0,tail=0;
    std::array<unsigned,5> intermissions{};
    bool waiting=false,finished=false,intro=true,intro_started=false,post_complete=false;
    explicit Match(bool legacy=false):intro(!legacy){}
    bool playing()const{return !intro&&!waiting&&!finished;}
    enum class IntroEvent {none,round_voice,fight_voice,play};
    IntroEvent advance_intro() {
        if(!intro)return IntroEvent::none;
        ++intro_elapsed;
        if(intro_elapsed==12)return IntroEvent::round_voice;
        if(intro_elapsed==84)return IntroEvent::fight_voice;
        if(intro_elapsed>=144){intro=false;return IntroEvent::play;}
        return IntroEvent::none;
    }
    IntroEvent advance_intro(Game&g){
        if(g.settings.legacy)return advance_intro();
        if(!intro)return IntroEvent::none;
        ++intro_elapsed;auto&m=g.memory;constexpr auto base=Game::globals;
        switch(m.read_word(base+0x10)){
        case 0:{begin_round_state(base,m);g.task_helpers().sound(0x0100ff08,0,0);source_match_win_labels(base,m);return IntroEvent::none;}
        case 2:
            if(m.read_word(base+0x13e)==2){if(!tick_last_opponent_intro(base,m))throw std::runtime_error("Original last-opponent intro state");return IntroEvent::none;}
            if(m.read_word(base+0x13e))throw std::runtime_error("Source special interlude requires scheduler continuation");
            m.write_word(base+0x10,4);m.write_byte(base+0x10f,0);m.write_byte(base+0x8f,0);m.write_byte(base+0xbd,0);
            if(!m.read_word(base+0x102)&&m.read_byte(base+0x581c)){
                const auto stack=announcement_detail::address(m.read_word(base+0x580a));const auto actor=announcement_detail::address(m.read_word(stack));m.write_byte(base+0x581c,m.read_byte(base+0x581c)-1);m.write_word(base+0x580a,std::uint16_t(stack+2));
                if(std::uint16_t(stack+2)){m.write_byte(actor,m.read_byte(actor)+1);m.write_word(actor+2,0x308);m.write_byte(actor+0xa,m.read_byte(base+0x121)+0x2f);}
            }return IntroEvent::none;
        case 4:
            if(m.read_word(base+0xb04)==0x208||m.read_word(base+0xc04)==0x208)return IntroEvent::none;
            m.write_word(base+0x10,6);m.write_byte(base+0x12c,1);intro_started=true;return IntroEvent::none;
        case 6:if(m.read_byte(base+0x12c))return IntroEvent::none;break;
        default:throw std::runtime_error("Unsupported source intro phase");}
        m.write_word(base+0xc,m.read_word(base+0xc)+2);m.write_long(base+0x10,0);m.write_byte(base+0x10e,0);intro=false;
        if(!m.read_byte(base+0x156)){m.write_byte(base+0x156,0xff);start_source_round_music(base,m);}
        return IntroEvent::play;
    }
    bool observe(Game&g) {
        auto&m=g.memory;if(g.settings.legacy||waiting||finished)return false;
        // LossStep must finish its row flags before it writes the original
        // winner bit. Observing step20 early froze that routine completely.
        const auto bit=m.read_byte(Game::globals+0x10c);
        if(bit==m.read_byte(Game::players[0]+0x180))winner=0;
        else if(bit==m.read_byte(Game::players[1]+0x180))winner=1;
        else return false;
        ++wins[unsigned(winner)];++completed_rounds;
        waiting=true;delay=240;end_elapsed=tail=0;finished=wins[unsigned(winner)]>g.settings.best_of()/2;
        m.write_word(Game::globals+0x13a,std::uint16_t(Game::players[unsigned(winner)]));
        m.write_word(Game::globals+0x13c,std::uint16_t(Game::players[1-unsigned(winner)]));
        const auto p=Game::players[unsigned(winner)];const auto won=m.read_byte(p+0xa3);
        m.write_byte(p+0xa4+won,4);m.write_byte(p+0xa3,std::uint8_t(won+1));
        if(g.source_arcade)finished=m.read_byte(p+0xa3)==m.read_byte(Game::globals+0x121);
        if(m.read_byte(p+0xa3)==m.read_byte(Game::globals+0x121)){
            m.write_byte(Game::globals+0x105,0xff);g.task_helpers().sound(0x0100ff07,0,0);
            if(m.read_byte(Game::globals+0xac)==3){
                const auto count=m.read_byte(p+0xa2);m.write_byte(p+0xa2,count==0x99?0x99:std::uint8_t(count+((count&15)==9?7:1)));
            }else if(!m.read_byte(p+0x9e)){
                const auto count=m.read_byte(Game::globals+0x14f);if(count<99)m.write_byte(Game::globals+0x14f,count+1);
                if(m.read_word(Game::globals+0x112)==m.read_word(Game::globals+0x114)){m.write_byte(Game::globals+0x14c,0xff);m.write_byte(Game::globals+0xbd,0xff);}
            }
        }
        m.write_word(Game::globals+0xc,m.read_word(Game::globals+0xc)+2);m.write_long(Game::globals+0x10,0);
        m.write_byte(Game::globals+0x8f,1);m.write_byte(Game::globals+0x107,1);
        // USA A8D2 calls this at every completed round, despite the old host
        // method's name. Restricting it to match victory lost original draws.
        if(g.settings.opponent==2)ClassicCpu<Memory,Effects>(m,g.effects,Game::globals,Game::players[1]).skill_after_match();
        return true;
    }
    bool advance(Game&g,bool ending_complete=true) {
        if(!waiting)return false;
        ++end_elapsed;
        if(!g.settings.legacy){
            if(post_complete)return false;
            auto h=g.task_helpers();auto&m=g.memory;
            if(m.read_word(Game::globals+0xc)!=6){
                if(m.read_word(Game::globals+0x10)<6)round_won_announcement_phase(Game::globals,m,h);
                else finish_round_announcements(Game::globals,m,h);
                return false;
            }
            bool pressed=false;const auto who=m.read_byte(Game::globals+0xac)==3?m.read_byte(Game::globals+0x10c):m.read_byte(Game::globals+0xac);
            for(unsigned side=0;side<2;++side)if(who&(1u<<side))pressed=pressed||bool((m.read_word(Game::players[side]+0x9a)&~m.read_word(Game::players[side]+0x9c))&0x7700);
            if(!after_round_phase(Game::globals,m,pressed,ending_complete))return false;
            if(finished){post_complete=true;return false;}
        }else{if(finished)return false;if(delay)--delay;
        tail=ending_complete?tail+1:0;
        if(delay||tail<18)return false;}
        auto&m=g.memory;const std::array<std::uint32_t,2> scores={m.read_long(Game::players[0]+0xaa),m.read_long(Game::players[1]+0xaa)};
        g.seed=m.read_word(Game::globals+0xae);g.reset(true);
        for(unsigned i=0;i<2;++i)m.write_long(Game::players[i]+0xaa,scores[i]);
        intermissions[std::min(round-1,4u)]=end_elapsed;
        ++round;waiting=false;winner=-1;intro=true;intro_started=false;intro_elapsed=0;return true;
    }
};
} // namespace puzzle_fighter::prototype
