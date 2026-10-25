// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <puzzle_fighter/pair_generation_memory.hpp>
#include <puzzle_fighter/piece_initialization.hpp>
#include <puzzle_fighter/fall_speed.hpp>
#include <puzzle_fighter/under_pair.hpp>
#include <puzzle_fighter/pair_placement.hpp>
#include <puzzle_fighter/push_down.hpp>
#include <puzzle_fighter/swap_pair.hpp>
#include <puzzle_fighter/turn_pair.hpp>
#include <puzzle_fighter/move_pair.hpp>
#include <puzzle_fighter/turn_picture.hpp>
#include <puzzle_fighter/settle_gems.hpp>
#include <puzzle_fighter/diamond_scan.hpp>
#include <puzzle_fighter/tidy_field.hpp>
#include <puzzle_fighter/counter_phase.hpp>
#include <puzzle_fighter/form_gems_dcd6_e0ee.hpp>
#include <puzzle_fighter/form_gems_dcca.hpp>
#include <puzzle_fighter/board_picture_codes.hpp>
#include <puzzle_fighter/find_breaks.hpp>
#include <puzzle_fighter/big_gem_bonus.hpp>
#include <puzzle_fighter/show_break.hpp>
#include <puzzle_fighter/join_gems_f060.hpp>
#include <puzzle_fighter/join_gems_eeb0.hpp>
#include <puzzle_fighter/join_gems_ecec.hpp>
#include <puzzle_fighter/counter_drop.hpp>
#include <puzzle_fighter/loose_drop.hpp>
#include <puzzle_fighter/loss_check.hpp>
#include <puzzle_fighter/loss_step.hpp>
#include <puzzle_fighter/frame_clocks.hpp>
#include <puzzle_fighter/task_pool.hpp>
#include <puzzle_fighter/danger_loss_actor.hpp>
#include <puzzle_fighter/actor_tasks.hpp>
#include <puzzle_fighter/round_announcements.hpp>
#include <puzzle_fighter/attack_display.hpp>
#include <puzzle_fighter/turn_frame.hpp>
#include <puzzle_fighter/input_sampling.hpp>
#include <puzzle_fighter/scene_reset.hpp>
#include <puzzle_fighter/player_initialization.hpp>
#include <puzzle_fighter/round_scene_picture.hpp>
#include <puzzle_fighter/sound_requests.hpp>
#include <puzzle_fighter/character_controller.hpp>
#include <puzzle_fighter/character_cosmetic_tasks.hpp>
#include <puzzle_fighter/character_special_tasks.hpp>
#include <puzzle_fighter/scene_actor_tasks.hpp>
#include <puzzle_fighter/source_howto_actors.hpp>
#include <puzzle_fighter/source_score_actors.hpp>
#include <puzzle_fighter/source_ending_actor_tasks.hpp>
#include <puzzle_fighter/source_idle_pictures.hpp>
#include <puzzle_fighter/source_title_actors.hpp>
#include <puzzle_fighter/selection_actor_tasks.hpp>
#include <puzzle_fighter/arcade_stage_interlude.hpp>
#include <puzzle_fighter/sprite_packets.hpp>
#include <puzzle_fighter/stage_background.hpp>
#include <puzzle_fighter/stage_actors.hpp>
#include <puzzle_fighter/stage_special_actors.hpp>
#include <puzzle_fighter/round_bonus.hpp>
#include <puzzle_fighter/gem_picture_builder.hpp>
#include <algorithm>
#include <array>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <set>
#include <string>
#include <vector>
#include "settings.hpp"
#include <puzzle_fighter/classic_cpu.hpp>

namespace puzzle_fighter::prototype {
// Native storage for recovered routines, not a CPU or instruction dispatcher.
struct Memory {
    std::array<std::uint8_t,65536> ram{};
    std::array<std::uint8_t,0x40000> video{};
    std::vector<std::uint8_t> data,opcodes;
    std::uint64_t accesses=0;
    static std::vector<std::uint8_t> load(const std::string&name) {
        std::ifstream f(name,std::ios::binary);
        if(!f)throw std::runtime_error("Missing original table image: "+name);
        std::vector<std::uint8_t> v((std::istreambuf_iterator<char>(f)),{});
        if(v.size()!=0x100000)throw std::runtime_error("Expected 1 MiB original table image: "+name);
        return v;
    }
    explicit Memory(const std::string&root):data(load(root+"/tables/main-program.bin")),opcodes(load(root+"/tables/main-opcodes.bin")){}
    void guard(){if(++accesses>2000000)throw std::runtime_error("Prototype tick exceeded memory-access budget; restart and report this position");}
    std::uint8_t read_byte(std::uint32_t a) {
        guard();a&=0xffffff;
        if(a<data.size())return data[a];
        if(a>=0xff0000)return ram[a&65535];
        if(a>=0x900000&&a<0x940000)return video[a-0x900000];
        throw std::runtime_error("Unmapped prototype read at "+std::to_string(a));
    }
    std::uint16_t read_word(std::uint32_t a){auto h=read_byte(a);return std::uint16_t((unsigned(h)<<8)|read_byte(a+1));}
    std::uint32_t read_long(std::uint32_t a){auto h=read_word(a);return (std::uint32_t(h)<<16)|read_word(a+2);}
    void write_byte(std::uint32_t a,std::uint8_t v) {
        guard();a&=0xffffff;
        if(a>=0xff0000){ram[a&65535]=v;return;}
        if(a>=0x900000&&a<0x940000){video[a-0x900000]=v;return;}
        throw std::runtime_error("Unmapped prototype write at "+std::to_string(a));
    }
    void write_word(std::uint32_t a,std::uint16_t v){write_byte(a,v>>8);write_byte(a+1,v&255);}
    void write_long(std::uint32_t a,std::uint32_t v){write_word(a,v>>16);write_word(a+2,v&65535);}
    std::uint16_t opcode_word(std::uint32_t a){guard();return std::uint16_t((unsigned(opcodes.at(a))<<8)|opcodes.at(a+1));}
    std::uint8_t opcode_byte(std::uint32_t a){guard();return opcodes.at(a);}
};
struct SoundRequest {std::uint32_t number;std::uint32_t pan=0x1000,extra=0;};
struct Effects {
    Memory*memory=nullptr;
    std::function<void(SoundRequest)> sink;
    std::array<bool,2> break_sent{};
    unsigned requests=0;
    void emit(std::uint32_t p,std::uint32_t table,int variant=0) {
        ++requests;if(!sink||!memory)return;
        auto&m=*memory;
        const auto command=std::uint16_t(m.opcode_word(table+2*(m.read_byte(p+0x81)&1))+variant);
        unsigned at=std::uint16_t((m.read_word(p+0x10)<<4)+16u+(m.read_byte(p+2)?256u:0u));
        at=std::min(at,0x17fu);
        const auto pan=m.opcodes.at(0x2b41a+at);
        sink({command,std::uint32_t(pan)<<8,0});
    }
    void move_sound(std::uint32_t p){emit(p,0x2ba70);}
    void turn_sound(std::uint32_t p){emit(p,0x2ba42);}
    void soft_drop_sound(std::uint32_t p){emit(p,0x2ba9e);}
    void landing_sound(std::uint32_t p){emit(p,0x2bacc);}
    void counter_sound(std::uint32_t p){emit(p,0x2bafa);}
    void break_sound(std::uint32_t p,BreakSoundKind kind){
        const auto which=kind==BreakSoundKind::power?1u:0u;
        if(break_sent[which])return;
        break_sent[which]=true;emit(p,which?0x2bb6e:0x2bb34);
    }
    void chain_palette(std::uint32_t actor){if(memory)turn_chain_palette(actor,*memory);}
    void chain_voice(std::uint32_t p,std::uint16_t index){
        ++requests;if(!sink||!memory)return;
        character_chain_voice(p,0xff8000u,index,*memory,[&](std::uint32_t command,std::uint32_t pan,std::uint32_t extra){sink({command,pan,extra});});
    }
    template<class M>void find_diamond(std::uint32_t p,M&m){diamond_scan(p,m);}
};
struct Game {
    static constexpr std::uint32_t globals=0xff8000;
    static constexpr std::array<std::uint32_t,2> players{0xff8300,0xff8700};
    Memory memory;Effects effects;
    GameSettings settings;
    std::uint16_t seed=0x920d,global_seed=0x920d;
    std::uint64_t ticks=0;
    std::array<unsigned,2> locks{},spawns{},clears{},formations{},received{},moves{},turns{};
    // Native presentation signals from recovered CounterDrop/AttackWarning.
    // These counters do not participate in gameplay or consume random draws.
    std::array<unsigned,2> attack_events{},hit_events{},hit_strength{};
    std::array<std::array<unsigned,21>,2> visits{};
    std::vector<GemPictureObject> board_objects,pair_objects;
    SpritePacketFrame sprite_frame;InputPollResult input_output;
    std::set<unsigned> unrecovered_task_kinds; // Diagnostic only; no fabricated task completion.
    std::array<std::uint16_t,2> previous{};
    unsigned restarts=0;bool source_arcade=false;
    explicit Game(const std::string&root,std::uint16_t initial,GameSettings selected={}):memory(root),settings(selected),seed(initial){settings.validate();reset();}
    static int signed_word(std::uint16_t v){return v<0x8000u?int(v):int(v)-65536;}
    void source_cpu_prefix(std::uint32_t p){ClassicCpu<Memory,Effects>(memory,effects,globals,p).initialize_first_pair();}
    void source_fall_speed(std::uint32_t p){fall_speed(p);}
    void source_initial_pacing(std::uint32_t p){ClassicCpu<Memory,Effects>(memory,effects,globals,p).initialize_pacing();}
    void source_generate_next(std::uint32_t p){make_next_pair_memory(p,globals,memory);}
    void source_second_player_tail(std::uint32_t p){ClassicCpu<Memory,Effects>(memory,effects,globals,p).finish_second_player_initialization();}
    void reset_source(bool keep_match){
        if(keep_match&&source_arcade){
            auto&m=memory;m.accesses=0;clear_scene_records(globals,m,false);m.write_word(globals+0x102,m.read_word(globals+0x102)+1);m.write_word(globals+0x20,0x92a0);m.write_word(globals+0x48,0x92a0);m.write_byte(globals+0x10f,1);
            initialize_source_scene_picture(globals,m);source_round_labels(globals,m);source_course_label(globals,m);initialize_source_players(globals,m,*this);allocate_round_announcements(globals,m);initialize_stage_actors(globals,m);start_source_round_music(globals,m);global_seed=m.read_word(globals+0xae);++restarts;return;
        }
        auto&m=memory;m.accesses=0;const auto round_ordinal=keep_match?std::uint16_t(m.read_word(globals+0x102)+1):0;
        clear_scene_records(globals,m,!keep_match);board_objects.clear();pair_objects.clear();sprite_frame={};unrecovered_task_kinds.clear();
        auto sink=std::move(effects.sink);effects={};effects.memory=&m;effects.sink=std::move(sink);ticks=0;locks={};spawns={};clears={};formations={};received={};moves={};turns={};attack_events={};hit_events={};hit_strength={};visits={};previous={};++restarts;
        // Fresh enhanced quick entry leaves the source attract profile. Source
        // retained arcade rounds take the separate keep_match path above.
        if(!keep_match)m.write_byte(globals+0x83,0);
        // Explicit native selection/operator/seed injection replaces the future
        // original boot/select UI; source reset and player logic follow it.
        m.write_word(globals,2);m.write_word(globals+4,4);m.write_word(globals+8,keep_match?4:2);
        m.write_byte(globals+0xa1,settings.damage);m.write_byte(globals+0xac,settings.opponent==2?1:3);m.write_byte(globals+0xbc,0);m.write_byte(globals+0x9a,1);
        m.write_byte(globals+0xa0,settings.difficulty);m.write_byte(globals+0xbe,settings.difficulty);m.write_byte(globals+0xde,settings.speed);m.write_byte(globals+0xdf,settings.speed);m.write_byte(globals+0xbb,settings.handicap);m.write_byte(0xff0c8c,settings.course);
        m.write_byte(globals+0xa2,(settings.rounds_cpu+1)/2);m.write_byte(globals+0xa3,(settings.rounds_human+1)/2);m.write_word(globals+0x114,8);m.write_word(globals+0x90,1);
        for(unsigned i=0;i<2;++i){const auto p=players[i],actor=globals+0xb00+i*0x100;m.write_byte(p,1);m.write_byte(p+2,i);m.write_byte(p+0x81,i);m.write_byte(p+0x82,settings.characters[i]);m.write_byte(p+0x9e,settings.opponent==2&&i==1);m.write_byte(p+0x9f,0);m.write_byte(p+0xa9,settings.handicap?settings.handicaps[i]:2);m.write_word(p+0x32,std::uint16_t(actor));m.write_word(p+0x34,std::uint16_t(players[1-i]));m.write_byte(actor,1);m.write_byte(actor+2,i);m.write_word(actor+0x32,std::uint16_t(p));}
        if(!keep_match){m.write_word(globals+0xae,seed);start_match_state(globals,m);if(settings.opponent==2)ClassicCpu<Memory,Effects>(m,effects,globals,players[1]).start_skill();}
        m.write_word(globals+0x102,round_ordinal);m.write_word(globals+0x100,settings.characters[1]*2);m.write_byte(globals+0x10f,keep_match?1:0);m.write_word(globals+0xc,0);m.write_long(globals+0x10,0);
        // Quick entry bypasses boot/selection2CEE4. Its source text clear is
        // required for the bonus transition into the off-screen text page.
        if(!keep_match)clear_source_text_plane(m);
        initialize_source_scene_picture(globals,m);source_round_labels(globals,m);source_course_label(globals,m);
        initialize_source_players(globals,m,*this);
        if(!keep_match&&settings.opponent==2)ClassicCpu<Memory,Effects>(m,effects,globals,players[1]).level_for_match();
        // Reuse original announcement/stage allocation after recovered scene setup.
        allocate_round_announcements(globals,m);initialize_stage_actors(globals,m);
        if(keep_match){m.write_word(globals+0x20,0x92a0);m.write_word(globals+0x48,0x92a0);start_source_round_music(globals,m);global_seed=m.read_word(globals+0xae);return;}
        initialize_stage_palettes(globals,m);
        for(unsigned i=0;i<2;++i)for(unsigned n=0;n<64;++n)m.write_word(0x90c180+i*0x80+n*2,std::uint16_t(m.read_word(0xa0000+settings.characters[i]*0x100+n*2)|0xf000));
        for(const auto pair:{std::array<unsigned,3>{0xa0b00,0x90c000,10},{0xaa620,0x90c140,1},{0xaa600,0x90c160,1},{0xa0c80,0x90c280,6}})for(unsigned n=0;n<pair[2]*16;++n)m.write_word(pair[1]+n*2,std::uint16_t(m.read_word(pair[0]+n*2)|0xf000));
        for(unsigned n=0;n<512;++n)m.write_word(0x90c400+n*2,std::uint16_t(m.read_word(0xa1ec0+n*2)|0xf000));
        global_seed=m.read_word(globals+0xae);
    }
    void reset(bool keep_match=false) {
        if(!settings.legacy){reset_source(keep_match);return;}
        const std::array<std::uint16_t,4> skill={memory.read_word(0xff0cc6),memory.read_word(0xff0cc8),memory.read_word(0xff0cca),memory.read_word(globals+0x12e)};
        const auto frame_phase=memory.read_word(globals+0x80);
        const auto round_ordinal=keep_match?std::uint16_t(memory.read_word(globals+0x102)+1u):0;
        std::array<std::array<std::uint8_t,256>,2> retained{};
        if(keep_match&&!settings.legacy)for(unsigned i=0;i<2;++i)
            std::copy_n(memory.ram.begin()+(players[i]&65535u),256,retained[i].begin());
        auto&m=memory;m.accesses=0;m.ram.fill(0);m.video.fill(0);
        if(keep_match&&!settings.legacy){
            // USA 2D992/2D9B0 retain player prefixes, clearing +04..07 and
            // +D0..D7. In particular actual +A8 and score +AA survive.
            for(unsigned i=0;i<2;++i){
                std::copy(retained[i].begin(),retained[i].end(),m.ram.begin()+(players[i]&65535u));
                m.write_long(players[i]+4,0);m.write_long(players[i]+0xd0,0);m.write_long(players[i]+0xd4,0);
            }
        }
        // Normal new-game and round setup retain BOTH independent clocks.
        // The initial all-zero RAM image remains a standalone host boundary.
        if(!settings.legacy)m.write_word(globals+0x80,frame_phase);
        auto sink=std::move(effects.sink);effects={};effects.memory=&m;effects.sink=std::move(sink);ticks=0;global_seed=seed;
        unrecovered_task_kinds.clear();locks={};spawns={};clears={};formations={};received={};moves={};turns={};attack_events={};hit_events={};hit_strength={};visits={};previous={};++restarts;
        // Explicit provisional human-versus initialization; no boot/save-state emulation.
        m.write_byte(globals+0xa1,settings.legacy?4:settings.damage);m.write_byte(globals+0xac,0);m.write_byte(globals+0xbc,0);
        const auto init=initialize_piece_streams(seed);global_seed=init.global_seed;
        m.write_word(globals+0xae,global_seed);
        if(!settings.legacy) {
            initialize_task_pool(globals,m);
            initialize_character_auxiliary_pool(globals,m);
            initialize_attack_display_pool(globals,m);
            m.write_word(globals+0x102,round_ordinal);
            m.write_byte(globals+0x121,std::uint8_t((settings.best_of()+1)/2));
            m.write_word(globals+0x100,std::uint16_t(settings.characters[1]*2));
            m.write_byte(globals+0xac,settings.opponent==2?1:3);
            m.write_byte(globals+0x133,1);m.write_byte(globals+0x10e,1);
            m.write_word(globals+0x90,1); // Standalone English locale; original bonus table selector.
            m.write_byte(globals+0xa0,settings.difficulty);m.write_byte(globals+0xbe,settings.difficulty);
            m.write_byte(globals+0xde,settings.speed);m.write_byte(globals+0xdf,settings.speed);
            m.write_byte(globals+0xbb,settings.handicap);m.write_byte(0xff0c8c,settings.course);
            m.write_word(globals+0x114,8);m.write_byte(globals+0xa2,(settings.rounds_cpu+1)/2);m.write_byte(globals+0xa3,(settings.rounds_human+1)/2);
            if(keep_match){m.write_word(0xff0cc6,skill[0]);m.write_word(0xff0cc8,skill[1]);m.write_word(0xff0cca,skill[2]);m.write_word(globals+0x12e,skill[3]);}
            else if(settings.opponent==2)ClassicCpu<Memory,Effects>(m,effects,globals,players[1]).start_skill();
        }
        for(unsigned i=0;i<2;++i) {
            const auto p=players[i],other=players[1-i],field=0xffab10u+i*0x200u;
            m.write_long(p+0x160,other);m.write_long(p+0x164,field);m.write_long(p+0x168,field+0x400);
            m.write_long(p+0x398,field);m.write_long(p+0x382,0x900160+i*0x1000);
            m.write_word(p+0x32,std::uint16_t(0x9000+i*0x200));m.write_word(p+0x34,std::uint16_t(0x9000+i*0x200));
            m.write_byte(p+2,i);m.write_byte(p+0x81,i);m.write_byte(p+0x82,i);m.write_byte(p+0x9f,0);
            m.write_byte(p+0xa9,2);m.write_byte(p+0x180,i+1);m.write_byte(p+0x328,1);
            if(!settings.legacy) {
                m.write_byte(p+0x82,settings.characters[i]);m.write_byte(p+0xa9,settings.handicap?settings.handicaps[i]:2);
                m.write_word(p+0x34,std::uint16_t(other));m.write_byte(p+0x9e,settings.opponent==2&&i==1);
                // USA 2B60/2B6E select B00/C00; 2C5C stores A4 here.
                // Keep character actors outside the original D00 task pool.
                m.write_word(p+0x32,std::uint16_t(globals+0xb00u+i*0x100u));
                const auto actor=globals+0xb00u+i*0x100u;
                m.write_byte(p,1);m.write_byte(p+1,1);m.write_byte(actor,1);m.write_byte(actor+1,1);m.write_byte(actor+2,i);m.write_word(actor+0x32,std::uint16_t(p));
            }
            m.write_word(p+0x144,init.player_seed);m.write_byte(p+0x380,init.order);
            m.write_word(p+0x156,init.next[0]);m.write_word(p+0x158,init.next[1]);
            for(unsigned j=0;j<4;++j)m.write_byte(p+0x350+j,init.colours[j]);
            m.write_word(p+0x1f6,m.read_word(0x124b8));
            for(int row=-2;row<=13;++row)for(int col=0;col<8;++col) {
                const auto a=field+std::uint32_t(row*16+col*2);
                m.write_word(a,col==0||col==7||row==13?0xff:0);m.write_word(a+0x400,0);
            }
            m.write_word(p+0x27c,1);m.write_word(p+0x388,0xfff0);
            if(!settings.legacy) {
                ClassicCpu<Memory,Effects> cpu(m,effects,globals,p);
                cpu.initialize_first_pair();
            }
            FallSpeedState s;s.handicap=m.read_byte(p+0xa9);s.mode=m.read_byte(globals+0xac);s.computer=m.read_byte(p+0x9e);s.setting=m.read_byte(globals+0xdf);auto f=set_fall_speed(s,m).state;
            m.write_byte(p+0x18a,f.delay);m.write_byte(p+0x15e,f.delay);
            if(!settings.legacy) {
                m.write_byte(p+0x1a1,f.ten_phase);m.write_word(p+0x1c8,f.pair_stage);m.write_word(p+0x184,f.twenty_phase);m.write_word(p+0x186,f.speed_stage);
                m.write_word(p+0x27c,f.slide_step);m.write_word(p+0x388,f.movement_limit);
            }
            if(!settings.legacy) {
                ClassicCpu<Memory,Effects> cpu(m,effects,globals,p);cpu.initialize_pacing();
                // Original initialization generation precedes the later
                // visible promotion; the prefix's temporary pair is not play.
                make_next_pair_memory(p,globals,m);
                if(i==1)cpu.finish_second_player_initialization();
            }
            if(settings.legacy)spawn(i,true);
        }
        // USA A36C/A372 assign the match level AFTER both initializers;
        // first pacing intentionally used the previously retained +A8.
        if(!settings.legacy){
            if(!keep_match&&settings.opponent==2)ClassicCpu<Memory,Effects>(m,effects,globals,players[1]).level_for_match();
            for(unsigned i=0;i<2;++i)spawn(i,true);
            global_seed=m.read_word(globals+0xae);
            allocate_round_announcements(globals,m);
            initialize_stage_actors(globals,m);
            allocate_attack_display(globals,m);
            // Original sprite palette bank for each selected fighter (14770).
            for(unsigned i=0;i<2;++i)for(unsigned n=0;n<64;++n)
                m.write_word(0x90c180+i*0x80+n*2,std::uint16_t(m.read_word(0xa0000+settings.characters[i]*0x100+n*2)|0xf000));
            // Original20E80/20F94 shared object palettes, including HUD1/8
            // and announcement bank11. Source copies force full brightness.
            for(const auto pair:{std::array<unsigned,3>{0xa0b00,0x90c000,10},
                                std::array<unsigned,3>{0xaa620,0x90c140,1},
                                std::array<unsigned,3>{0xaa600,0x90c160,1},
                                std::array<unsigned,3>{0xa0c80,0x90c280,6}})
                for(unsigned n=0;n<pair[2]*16;++n)m.write_word(pair[1]+n*2,std::uint16_t(m.read_word(pair[0]+n*2)|0xf000));
            for(unsigned n=0;n<512;++n)m.write_word(0x90c400+n*2,std::uint16_t(m.read_word(0xa1ec0+n*2)|0xf000));
            // The isolated finish bank needs the original full-brightness
            // scene domain. Its RGB is replaced by25B0E before first use.
            for(unsigned n=0;n<16;++n)m.write_word(0x90cfc0+n*2,0xf000);
            initialize_stage_backgrounds(globals,m);initialize_stage_palettes(globals,m);
        }
    }
    auto task_helpers(){
        auto emit=[this](std::uint32_t n,std::uint32_t pan,std::uint32_t extra){if(effects.sink)effects.sink({n,pan,extra});};
        return ActorTasks<Memory,decltype(emit)>{memory,emit,globals};
    }
    void source_reactions(){auto h=task_helpers();CharacterController<Memory,decltype(h)> c{memory,h,globals};c.reactions();}
    void source_characters(){auto h=task_helpers();CharacterController<Memory,decltype(h)> c{memory,h,globals};c.run();}
    void source_first_pool(){
        auto h=task_helpers();auto&m=memory;m.write_byte(globals+0x89,32);
        for(unsigned i=0;i<32;++i){const auto a=globals+0xd00+i*0x80;
            if(m.read_byte(a))switch(m.read_byte(a+2)){
                case 3:if(!continue_announcement_task(a,globals,m,h)&&!round_announcement_task(a,globals,m,h))throw std::runtime_error("Unsupported original announcement state");break;
                case 0x1e:h.large_finish(a);break;
                case 0x30:BonusTask<Memory,decltype(h)>{m,h,globals,a}.tick();break;
                default:if(!source_title_actor(a,globals,m,h)&&!source_ending_actor(a,globals,m,h)&&!source_howto_indicator(a,globals,m,h)&&!source_score_row(a,globals,m,h)&&!StageActors<Memory,decltype(h)>{m,h,globals,a}.tick()&&!stage_special_task(a,globals,m,h)&&!character_feedback_task(a,globals,m,h)&&!character_cosmetic_task(a,globals,m,h)&&!character_special_task(a,globals,m,h)&&!scene_actor_task(a,globals,m,h)&&!selection_actor_task(a,globals,m,h)&&!stage_interlude_actor(a,globals,m,h))unrecovered_task_kinds.insert(m.read_byte(a+2));break;
            }
            m.write_byte(globals+0x89,std::uint8_t(m.read_byte(globals+0x89)-1));
        }
    }
    void source_second_pool(){auto h=task_helpers();auto&m=memory;m.write_byte(globals+0x89,16);for(unsigned i=0;i<16;++i){const auto a=globals+0x1d00+i*64;if(m.read_byte(a)&&!character_attachment_task(a,globals,m,h)&&!character_special_attachment(a,globals,m,h)&&!stage_interlude_attachment(a,globals,m,h))unrecovered_task_kinds.insert(0x100+m.read_byte(a+2));m.write_byte(globals+0x89,m.read_byte(globals+0x89)-1);}}
    void source_third_pool(){auto h=task_helpers();auto&m=memory;m.write_byte(globals+0x89,4);for(unsigned i=0;i<4;++i){const auto a=globals+0x2100+i*0x200;if(m.read_byte(a)){bool handled=false;switch(m.read_byte(a+2)){case 0:handled=source_quote_task(a,globals,m);break;case 1:handled=attack_display_task(a,globals,m,h);break;case 2:handled=source_idle_credit_marker(a,globals,m);break;case 3:handled=source_top_score_text(a,globals,m);break;case 4:handled=selection_help_task(a,globals,m);break;default:break;}if(!handled)throw std::runtime_error("Unsupported original third-pool state");}m.write_byte(globals+0x89,m.read_byte(globals+0x89)-1);}}
    void source_packet_output(){auto frame=compose_sprite_packets(globals,memory);if(frame.updated){sprite_frame=std::move(frame);board_objects.clear();pair_objects.clear();for(unsigned n=0;n<sprite_frame.packets.size();++n){if(sprite_frame.origins[n].group==SpritePacketGroup::board)board_objects.push_back(sprite_frame.packets[n]);else if(sprite_frame.origins[n].group==SpritePacketGroup::pair)pair_objects.push_back(sprite_frame.packets[n]);}}}
    void presentation_tasks(){
        // Compatibility fixture adapter. Product execution uses MatchScheduler
        // callbacks so source pool/music/packet ordering is explicit.
        if(settings.legacy||memory.read_byte(globals+0x10f))return;
        source_reactions();source_characters();source_first_pool();source_second_pool();source_third_pool();source_music_tempo(globals,memory);stage_frame(globals,memory);
        if(memory.read_byte(globals+0x124))memory.write_byte(globals+0x124,memory.read_byte(globals+0x124)-1);
        build_gem_pictures(globals,memory);source_packet_output();
    }
    void stopped_work(){if(!settings.legacy){memory.accesses=0;stopped_turns(globals,memory);}}
    void spawn(unsigned i,bool first=false) {
        auto&m=memory;auto p=players[i];
        if(!first&&!settings.legacy&&settings.opponent==2&&i==1) {
            ClassicCpu<Memory,Effects> cpu(m,effects,globals,p);cpu.plan_pair();cpu.set_think_time();cpu.draw_slips();
        }
        advance_pair_memory(p,globals,m);++spawns[i];
        m.write_word(p+0x10,3);m.write_word(p+0x14,first?3:2);m.write_byte(p+0x170,0);
        m.write_byte(p+0x176,0);m.write_byte(p+0x328,1);m.write_byte(p+0x18e,1);
    }
    std::uint32_t pair_address(std::uint32_t p) {
        PairCellState s;s.a[0]=memory.read_long(p+0x164)+2;
        auto row=memory.read_word(p+0x14),col=memory.read_word(p+0x10);
        return pair_cell(s,row,col).a[0];
    }
    std::uint32_t secondary(std::uint32_t p,std::uint32_t cell) {
        const auto index=2u*memory.read_byte(p+0x170);
        auto a=memory.read_word(0x12528+index),b=memory.read_word(0x12530+index);
        return cell+std::uint32_t(signed_word(std::uint16_t(a+b)));
    }
    bool supported(std::uint32_t p) {
        PairCellState s;s.a[6]=p;s.sr=0x2700;s.supervisor_sp=0xff7d00;
        return (under_pair(s,memory).d[3]&255u)!=0;
    }
    void fall_speed(std::uint32_t p) {
        auto&m=memory;FallSpeedState s;
        s.ten_phase=m.read_byte(p+0x1a1);s.pair_stage=m.read_word(p+0x1c8);
        s.twenty_phase=m.read_word(p+0x184);s.speed_stage=m.read_word(p+0x186);
        s.handicap=m.read_byte(p+0xa9);s.elapsed=m.read_word(p+0x340);s.setting=m.read_byte(globals+0xdf);
        s.mode=m.read_byte(globals+0xac);s.computer=m.read_byte(p+0x9e);
        auto f=set_fall_speed(s,m).state;
        m.write_byte(p+0x1a1,f.ten_phase);m.write_word(p+0x1c8,f.pair_stage);m.write_word(p+0x184,f.twenty_phase);
        m.write_word(p+0x186,f.speed_stage);m.write_word(p+0x27c,f.slide_step);m.write_word(p+0x388,f.movement_limit);
        m.write_byte(p+0x18a,f.delay);m.write_byte(p+0x15e,f.fall_timer);
    }
    void lock(unsigned i) {
        auto&m=memory;auto p=players[i];place_pair(p,m);++locks[i];effects.landing_sound(p);
        if(m.read_word(p+0x140)==4)m.write_long(p+0xaa,m.read_long(p+0xaa)+10);
        m.write_word(p+0x320,1);m.write_word(p+0x314,0);m.write_byte(p+0x316,0);m.write_byte(p+0x1a3,0);
        m.write_word(p+0x1a6,0);m.write_word(p+0x242,0);m.write_byte(p+0x171,1);m.write_byte(p+0x174,1);
        m.write_byte(p+0x170,0);m.write_byte(p+0x1a8,0);m.write_byte(p+0x17e,1);fall_speed(p);
        m.write_word(p+0x14,0);m.write_word(p+0x10,3);turn_picture(p,m);
    }
    void steering(unsigned i) {
        auto&m=memory;const auto p=players[i];
        // Gameplay translation of the audited 1166C..11829 caller. CPU/AI/audio
        // runtime and original instruction cycles are intentionally absent.
        bool drop=false,run=true;
        if(m.read_byte(p+0x176)) {
            if(!supported(p)) {
                m.write_byte(p+0x176,0);m.write_byte(p+0x27e,1);m.write_word(p+0x188,0);m.write_word(p+0x34e,0);
                m.write_word(p+0x140,m.read_word(p+0x140)&0xfff0);drop=true;run=false;
            }else {
                m.write_byte(p+0x176,m.read_byte(p+0x176)-1);
                if(!m.read_byte(p+0x176)) {
                    if(supported(p)){lock(i);return;}
                    drop=true;run=false;
                }
            }
        }
        if(run) {
            m.write_byte(p+0x177,0);m.write_word(p+0x1a6,m.read_word(p+0x1a6)+1);m.write_byte(p+0x19a,0);
            if(!settings.legacy&&m.read_byte(p+0x9e)){ClassicCpu<Memory,Effects> cpu(m,effects,globals,p);cpu.steer();}
            push_down(p,m,effects);
            m.write_byte(p+0x15e,m.read_byte(p+0x15e)+m.read_byte(p+0x176)-1);
            if(m.read_byte(p+0x15e)&0x80) {
                if(m.read_byte(p+0x27e)){m.write_byte(p+0x15e,10);m.write_byte(p+0x27e,0);}
                else m.write_byte(p+0x15e,m.read_byte(p+0x18a));
                bool row_ready=true;
                if(m.read_word(p+0x242)) {
                    m.write_word(p+0x242,m.read_word(p+0x242)+m.read_word(p+0x27c));
                    const auto slide=m.read_word(p+0x242);
                    if(slide) {
                        row_ready=false;
                        if(!(slide&0x8000)){m.write_word(p+0x242,0xfffc);m.write_word(p+0x27c,4);}
                    }
                }
                if(row_ready&&!m.read_byte(p+0x176)) {
                    if(!supported(p))drop=true;
                    else {
                        m.write_byte(p+0x27e,1);m.write_byte(p+0x176,16);
                        m.write_byte(p+0x1a8,m.read_byte(p+0x1a8)+1);
                        if(m.read_byte(p+0x1a8)==8)m.write_byte(p+0x176,1);
                    }
                }
            }
        }
        if(drop) {
            m.write_word(p+0x14,m.read_word(p+0x14)+1);
            if(m.read_byte(p+0x27e)){m.write_word(p+0x242,0xfff8);m.write_byte(p+0x15e,10);}
            // USA117D8's MOVE.B #0A clears Z before117DE;117E0 writes
            // the movement limit even after the temporaryFFF8 assignment.
            m.write_word(p+0x242,m.read_word(p+0x388));
            m.write_byte(p+0x27e,0);
        }
        m.write_byte(p+0x397,0);
        if(m.read_byte(p+0x176)!=1) {
            swap_pair(p,m);
            auto right=turn_pair(p,true,m,effects);auto left=turn_pair(p,false,m,effects);turns[i]+=right.turned+left.turned;
            const auto moved=move_pair(p,m,effects);moves[i]+=moved.right+moved.left;
        }
        turn_picture(p,m);
    }
    void reset_turn(std::uint32_t p) {
        auto&m=memory;m.write_byte(globals+0x149,0);
        for(auto off:{0x252u,0x3a6u,0x261u,0x37cu,0x37au,0x396u})m.write_byte(p+off,0);
        for(auto off:{0x254u,0x256u,0x258u,0x25au,0x306u,0x308u})m.write_word(p+off,0);
        auto r=global_random(m.read_word(globals+0xae));global_seed=r.seed;m.write_word(globals+0xae,r.seed);m.write_byte(p+0x37b,r.value&7);
    }
    void end_turn(unsigned i) {
        auto&m=memory;const auto p=players[i];loose_drop(p,m);
        if(m.read_byte(p+0x17e)){tidy_field(p,m);return;}
        if(m.read_byte(p+0x318)) {
            m.write_word(p+0x32a,1);
            const auto raw=m.read_byte(p+0x37d);const int amount=raw<128?raw:int(raw)-256;
            const int kind=settings.legacy?(amount<6?0:amount<12?1:amount<18?2:amount<24?3:4)
                :(amount<=6?0:amount<=12?1:amount<=18?2:amount<=24?3:4);
            effects.emit(p,0x2bba0,kind);
        }
        for(int start:{0x12,2,-14}) {
            const auto field=m.read_long(p+0x164);
            for(unsigned col=0;col<6;++col){auto a=field+std::uint32_t(start)+col*2;if(m.read_word(a)&0x10){m.write_word(a,0);if(start!=0x12)m.write_word(a+0x400,0);}}
        }
        if(loss_check(p,m)){
            // USA C3D6..C3EE: LossCheck leaves D1.B=FF for this call.
            // Task creation is recovered; its animation runner stays separate.
            if(!settings.legacy&&m.read_byte(p+0x260))danger_loss_actor(p,globals,0xff,m,m);
            tidy_field(p,m);return;
        }
        if(settings.legacy){
            spawn(i);
            if(m.read_word(p+0x388)==0xfffe){
                m.write_word(p+0x14,1);
                if(m.read_word(m.read_long(p+0x164)+0x18)){m.write_word(p+0x14,2);m.write_word(p+0x242,0xfff0);}
            }
        }else{
            // USA C3E4..C444: position before planning; occupied-cell
            // slide applies at every speed. No generic spawn timer resets.
            m.write_word(p+0x10,3);m.write_word(p+0x14,2);
            if(m.read_word(p+0x388)==0xfffe)m.write_word(p+0x14,1);
            if(m.read_word(m.read_long(p+0x164)+0x18)){m.write_word(p+0x14,2);m.write_word(p+0x242,0xfff0);}
            if(m.read_byte(p+0x9e)){
                ClassicCpu<Memory,Effects> cpu(m,effects,globals,p);cpu.plan_pair();cpu.set_think_time();cpu.draw_slips();
            }
            advance_pair_memory(p,globals,m);++spawns[i];m.write_byte(p+0x18e,1);
        }
        if(m.read_word(p+0x140)&1)m.write_word(p+0x188,15);
        if(m.read_word(p+0x140)&2)m.write_word(p+0x34e,15);
        count_field(p,globals,m);
        for(auto off:{0x329u,0x371u,0x318u,0x322u})m.write_byte(p+off,0);
        m.write_word(p+0x20a,0);m.write_byte(p+0x260,0);m.write_word(p+0x1fc,0);m.write_byte(p+0x19c,0);
        for(auto off:{0x1feu,0x200u,0x202u,0x204u,0x206u,0x208u})m.write_word(p+off,0);
        for(auto off:{0x349u,0x370u,0x38cu})m.write_byte(p+off,0);
        m.write_byte(m.read_long(p+0x160)+0x1ee,0);tidy_field(p,m);
    }
    // Existing native 60Hz adapter: one virtual IRQ, then one normal-main
    // iteration. This does not establish special-interlude scheduler parity.
    void begin_host_frame() {
        if(settings.legacy)return;
        memory.accesses=0;
        interrupt_clock_increment(globals,memory);
        main_frame_begin(globals,memory);
    }
    void stage_inputs(std::array<std::uint16_t,2> input){
        if(settings.legacy)return;
        input_output=sample_input_ports(globals,memory,input_ports_from_words(input));copy_player_inputs(globals,memory);
    }
    void tick(std::array<std::uint16_t,2> input,bool round_play=true,bool inputs_staged=false) {
        auto&m=memory;m.accesses=0;effects.break_sent={};++ticks;
        if(!settings.legacy){
            if(round_play)m.write_word(0xff0cca,m.read_word(0xff0cca)+1);
            if(!inputs_staged)stage_inputs(input);
            const auto observed=turn_frame(globals,m,effects,*this);
            for(unsigned i=0;i<2;++i){const auto&o=observed.players[i];++visits[i][o.step];spawns[i]+=o.started;formations[i]+=o.formations;clears[i]+=o.removed;
                if(o.dropped){received[i]+=o.drop.placed;if(o.drop.warning.actor_updated)++attack_events[1-i];if(o.drop.placed){++hit_events[i];hit_strength[i]=std::min(5u,unsigned(m.read_byte(players[1-i]+0xd0))/2);}}
                previous[i]=m.read_word(players[i]+0x140);
            }
            global_seed=m.read_word(globals+0xae);return;
        }
        if(settings.legacy)m.write_word(globals+0x80,std::uint16_t(m.read_word(globals+0x80)+1));
        if(!settings.legacy&&round_play)m.write_word(0xff0cca,m.read_word(0xff0cca)+1);
        for(unsigned i=0;i<2;++i) {
            const auto p=players[i],op=players[1-i];
            if(settings.legacy||settings.opponent!=2||i!=1){m.write_word(p+0x142,previous[i]);m.write_word(p+0x140,input[i]);previous[i]=input[i];}
            m.write_byte(p+0x171,0);const auto step=m.read_byte(p+0x17e);if(step>20)throw std::runtime_error("Invalid prototype turn step");++visits[i][step];
            if(step)m.write_byte(p+0x266,1);
            auto advance=[&](){m.write_byte(p+0x17e,m.read_byte(p+0x17e)+1);};
            auto picture=[&](){board_picture_codes(p,m);tidy_field(p,m);};
            switch(step) {
                case 0:reset_turn(p);if(m.read_byte(op+0x17e)!=20)steering(i);break;
                case 1:case 3:case 10:m.write_byte(globals+0x149,1);settle_gems(p,m,effects);tidy_field(p,m);break;
                case 2:counter_phase(p,globals,m,effects);break;
                case 4:case 15:advance();formations[i]+=form_gems_dcaa(p,m).formations;picture();m.write_byte(p+0x266,1);break;
                case 5:advance();break;
                case 6:advance();formations[i]+=form_gems_dcb6(p,m).formations;picture();m.write_byte(p+0x266,1);break;
                case 7:advance();formations[i]+=form_gems_dcca(p,m).formed;picture();break;
                case 8:find_breaks(p,globals,m,effects);break;
                case 9:advance();if(m.read_word(p+0x25a))m.write_word(p+0x258,m.read_word(p+0x25a));big_gem_bonus(p,m);break;
                case 11:{auto result=show_break(p,globals,m,m);clears[i]+=result.score.removal.removed;break;}
                case 12:advance();formations[i]+=join_gems_f060(p,m).merges;picture();break;
                case 13:advance();formations[i]+=join_gems_eeb0(p,m).merges;picture();break;
                case 14:advance();formations[i]+=join_gems_ecec(p,m).merges;picture();break;
                case 16:advance();m.write_byte(op+0x244,m.read_byte(op+0x19e));break;
                case 17:advance();count_field(p,globals,m);break;
                case 18:{
                    advance();
                    if(m.read_byte(op+0x17e)!=20){
                        if(!m.read_byte(p+0x18d)){
                            const auto drop=counter_drop(p,globals,m);received[i]+=drop.placed;
                            if(drop.warning.actor_updated)++attack_events[1-i];
                            if(drop.placed){++hit_events[i];hit_strength[i]=std::min(5u,unsigned(m.read_byte(op+0xd0))/2);}
                        }
                        m.write_byte(p+0x18d,0);
                    }
                    break;
                }
                case 19:end_turn(i);break;
                case 20:loss_step(p,globals,m);break;
            }
            if(!settings.legacy)snapshot_attack_display(p,m);
            if(!settings.legacy&&settings.opponent==2&&i==1)previous[i]=m.read_word(p+0x140);
            if(m.read_byte(p+0x381)){m.write_byte(p+0x381,m.read_byte(p+0x381)-1);if(!m.read_byte(p+0x381))clear_banner(p,m);}
            if(m.read_word(p+0x32a)){m.write_word(p+0x32a,m.read_word(p+0x32a)+1);if(m.read_word(p+0x32a)>3)m.write_word(p+0x32a,0);}
            if(m.read_byte(op+0x17e)!=20) {
                m.write_word(p+0x342,m.read_word(p+0x342)+1);
                if(m.read_word(p+0x342)>=60){m.write_word(p+0x342,0);m.write_word(p+0x340,std::min<unsigned>(539,m.read_word(p+0x340)+1));}
            }
        }
    }
};
} // namespace puzzle_fighter::prototype
