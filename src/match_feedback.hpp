// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "presentation.hpp"
namespace puzzle_fighter::prototype {
struct MatchFeedback {
    unsigned round=0,voices=0;bool voice_sent=false;
    template<class Audio>void observe(Game&g,const CharacterVisuals&actors,bool waiting,int winner,Audio&audio){
        if(round!=g.restarts){round=g.restarts;voice_sent=false;}
        (void)actors;(void)audio;
        if(!waiting||winner<0||voice_sent||g.memory.read_word(Game::globals+0x10)!=6)return;
        // Original WonSpeak chooses USA2B7E8 (9A) or2B802 (9B) by
        // the winner's computer flag, after task3 publishes showing4.
        // The source round controller already emitted the result command.
        voice_sent=true;++voices;
    }
};
} // namespace puzzle_fighter::prototype
