// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game.hpp"
namespace puzzle_fighter::prototype {
// Original row indices grow upward from the bottom. The first loss flag13
// is above the visible field; visible y0..12 corresponds to indices12..0.
inline bool loss_row_hidden(Memory&m,std::uint32_t player,unsigned visible_y){
    return m.read_byte(player+0x17e)==20&&m.read_byte(player+0x36c-visible_y)!=0;
}
// Host presentation only. Original marked-cell bit 4 drives a provisional
// shatter effect; observing it never clears a cell or changes the break timer.
struct BreakVisuals {
    struct Cell {bool marked=false,seen=false;std::uint16_t value=0;std::uint64_t started=0;};
    std::array<std::array<Cell,78>,2> cells{};
    unsigned round=0,bursts=0;
    void observe(Game&g) {
        if(round!=g.restarts){cells={};bursts=0;round=g.restarts;}
        auto&m=g.memory;
        for(unsigned i=0;i<2;++i) {
            const auto field=m.read_long(Game::players[i]+0x164);
            for(unsigned y=0;y<13;++y)for(unsigned x=0;x<6;++x) {
                auto&cell=cells[i][y*6+x];const auto value=m.read_word(field+y*16+x*2+2);
                const bool marked=value!=0&&value!=0xff&&(value&0x10u);
                if(marked&&!cell.marked){cell.value=value;cell.started=g.ticks;cell.seen=true;++bursts;}
                cell.marked=marked;
            }
        }
    }
};
// Deliberately simple testing macro, not recovered CPU AI. At each spawn it
// picks a low column, rotates tie preference, then steers and soft-drops using
// the same input bits as a human. No board edits, RNG draws or score changes.
struct SoloMacro {
    unsigned round=0,spawn=0,target=2;
    std::array<unsigned,6> targets{};
    std::uint16_t input(Game&g) {
        if(round!=g.restarts){round=g.restarts;spawn=0;target=2;targets={};}
        auto&m=g.memory;const auto p=Game::players[1];
        if(m.read_byte(p+0x17e)!=0)return 0;
        if(spawn!=g.spawns[1]) {
            spawn=g.spawns[1];const auto field=m.read_long(p+0x164);
            unsigned lowest=14;
            for(unsigned offset=0;offset<6;++offset) {
                const unsigned col=(offset+g.locks[1])%6;unsigned height=0;
                for(unsigned y=0;y<13;++y)if(m.read_word(field+y*16+col*2+2)){height=13-y;break;}
                if(height<lowest){lowest=height;target=col;}
            }
            ++targets[target];
        }
        const int col=int(m.read_word(p+0x10));
        return col<int(target)?1u:col>int(target)?2u:4u;
    }
};
inline const char*character_name(unsigned character) {
    constexpr const char*names[]={"MORRIGAN","CHUN-LI","RYU","KEN","HSIEN-KO","DONOVAN","FELICIA","SAKURA","DEVILOT","AKUMA","DAN"};
    return character<11?names[character]:"UNKNOWN";
}
// Read-only preview of the USA DATA tables used by the recovered FF3C selector.
inline std::uint16_t attack_pattern_word(Memory&m,unsigned character,unsigned row,unsigned column) {
    const auto colour=m.read_word(0x10188+character*0xc0+std::min(row,11u)*16+column*2);
    return std::uint16_t(m.read_word(0x10178+colour*2)+m.read_word(0x10160+character*2));
}
// Selection shows a compact board view: the first delivered row is at the
// bottom. Delivery-table length/order remain unchanged in the recovered game.
inline std::uint16_t attack_preview_gem(Memory&m,unsigned character,unsigned row,unsigned column) {
    const auto palette=(attack_pattern_word(m,character,3-std::min(row,3u),column)>>12)&3u;
    return std::uint16_t(palette?palette:4u); // ordinary gem, same original palette
}
struct CharacterVisuals {
    struct Pose {unsigned character=99,act=0,left=0,last_attack=0,last_received=0;std::uint32_t frame=0;bool busy=false,completed=false;};
    std::array<Pose,2> poses{};unsigned round=0;int result_winner=-1;
    void start(Memory&m,Pose&p,unsigned character,unsigned act) {
        const auto table=(std::uint32_t(m.opcode_word(0x143f6+character*4))<<16)|m.opcode_word(0x143f8+character*4);
        p.character=character;p.act=act;p.frame=table+std::uint32_t(Game::signed_word(m.read_word(table+act*2)));p.left=m.read_word(p.frame);p.busy=p.completed=false;
    }
    void step(Memory&m,Pose&p) {
        // Original actor controllers test the negative frame mark (+1A),
        // not an invented 40/50-frame hold or a terminal 4096-tick dwell.
        if(m.read_byte(p.frame+2)&0x80)p.completed=true;
        p.left=std::uint16_t(p.left-1);
        if(!p.left){const bool loop=m.read_byte(p.frame+2)&0x80;p.frame+=16;if(loop)p.frame=m.read_long(p.frame);p.left=m.read_word(p.frame);}
    }
    void observe(Game&g) {
        if(round!=g.restarts){poses={};round=g.restarts;result_winner=-1;}
        auto&m=g.memory;
        for(unsigned i=0;i<2;++i) {
            auto&p=poses[i];const auto player=Game::players[i];const unsigned ch=m.read_byte(player+0x82);
            if(!g.settings.legacy){const auto actor=Game::globals+0xb00+i*0x100;p.character=ch;p.frame=m.read_long(actor+0x1c);p.act=m.read_word(actor+4);p.completed=m.read_byte(actor+0x1a)&0x80;continue;}
            if(p.character!=ch)start(m,p,ch,0);
            if(result_winner>=0){step(m,p);continue;}
            if(g.hit_events[i]!=p.last_received){start(m,p,ch,0x18+std::min(g.hit_strength[i],5u));p.busy=true;}
            else if(g.attack_events[i]!=p.last_attack){start(m,p,ch,0x0a);p.busy=true;}
            else if(p.busy&&p.completed)p.busy=false;
            p.last_attack=g.attack_events[i];p.last_received=g.hit_events[i];
            if(!p.busy){const auto idle=std::min(unsigned(m.read_byte(player+0xd1)),2u);if(p.act!=idle)start(m,p,ch,idle);}
            step(m,p);
        }
    }
    void finish_round(Game&g,unsigned winner){
        if(round!=g.restarts){poses={};round=g.restarts;}
        if(result_winner>=0)return;
        result_winner=int(winner);
        if(!g.settings.legacy)return;
        // USA13EE6 winner act4, 13FA2 loser act5. Event timing remains
        // native match glue; sprites, frame durations and end marks are original.
        for(unsigned i=0;i<2;++i)start(g.memory,poses[i],g.settings.characters[i],i==winner?4:5);
    }
    bool result_ready()const{return result_winner>=0&&poses[0].completed&&poses[1].completed;}
};
// USA 02B302..02B342: original danger-driven music command, with the original
// 180-frame update gate and character-specific tempo words. Presentation-only.
struct DangerMusic {
    unsigned round=0,cooldown=0,danger=0,updates=0;
    template<class Audio>void observe(Game&g,Audio&audio){
        if(round!=g.restarts){round=g.restarts;cooldown=danger=0;}
        if(cooldown){--cooldown;return;}
        const auto next=std::max(g.memory.read_byte(Game::players[0]+0xd1),g.memory.read_byte(Game::players[1]+0xd1));
        if(next==danger)return;
        danger=next;cooldown=180;++updates;
        audio.tempo(g.memory.opcode_word(0x2b344+8*g.settings.characters[1]+2*std::min(danger,2u)));
    }
};
} // namespace puzzle_fighter::prototype
