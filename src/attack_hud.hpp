// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game.hpp"
namespace puzzle_fighter::prototype {
// Read-only host presentation. Severity follows USA C50C thresholds; this is
// not a replacement for that recovered routine or its actor/palette effects.
struct AttackHud {
    struct Warning {unsigned pending=0,rows=0,remainder=0,severity=0,flash=0,countered=0,received=0;bool overflow=false;};
    std::array<Warning,2> players{};unsigned round=0,ticks=0;
    void observe(Game&g) {
        if(round!=g.restarts){players={};round=g.restarts;}
        ++ticks;auto&m=g.memory;
        for(unsigned i=0;i<2;++i) {
            auto&w=players[i];const auto player=Game::players[i];const unsigned pending=m.read_byte(Game::players[1-i]+0x19e);
            if(w.flash)--w.flash;
            if(w.countered)--w.countered;
            if(pending>w.pending)w.flash=60;
            if(pending<w.pending&&g.received[i]==w.received)w.countered=45;
            w.received=g.received[i];w.pending=pending;w.rows=pending/6;w.remainder=pending%6;
            w.severity=pending>=30?4:pending>=24?3:pending>=18?2:pending>=12?1:0;
            w.overflow=false;
            if(pending>=30) {
                const auto field=m.read_long(player+0x164);unsigned empty=0;
                while(empty<14&&!m.read_word(field+8+empty*16))++empty;
                w.overflow=13-empty+pending/6>=13;
            }
        }
    }
};
} // namespace puzzle_fighter::prototype
