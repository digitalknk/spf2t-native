// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <SDL3/SDL.h>
#include <array>
#include <cstdint>
namespace puzzle_fighter::prototype {
// Keyboard aliases only. Physical gamepad slots remain independent.
inline SDL_Scancode keyboard_menu_key(SDL_Scancode code,unsigned opponent,bool edit_opponent=false){
    if(!opponent)return code;
    if(edit_opponent){
        switch(code){
            case SDL_SCANCODE_W:return SDL_SCANCODE_UP;
            case SDL_SCANCODE_S:return SDL_SCANCODE_DOWN;
            case SDL_SCANCODE_A:return SDL_SCANCODE_LEFT;
            case SDL_SCANCODE_D:return SDL_SCANCODE_RIGHT;
            case SDL_SCANCODE_C:case SDL_SCANCODE_Q:return SDL_SCANCODE_J;
            case SDL_SCANCODE_V:case SDL_SCANCODE_E:return SDL_SCANCODE_K;
            default:return code;
        }
    }
    switch(code){
        case SDL_SCANCODE_UP:return SDL_SCANCODE_W;
        case SDL_SCANCODE_DOWN:return SDL_SCANCODE_S;
        case SDL_SCANCODE_LEFT:return SDL_SCANCODE_A;
        case SDL_SCANCODE_RIGHT:return SDL_SCANCODE_D;
        case SDL_SCANCODE_J:return SDL_SCANCODE_C;
        case SDL_SCANCODE_K:return SDL_SCANCODE_V;
        default:return code;
    }
}
inline std::array<std::uint16_t,2> keyboard_gameplay(const bool*keys,unsigned opponent){
    // Q/E remain compatibility aliases for saved scripts; C/V are defaults.
    std::array<std::uint16_t,2> out{
        std::uint16_t((keys[SDL_SCANCODE_D]?1:0)|(keys[SDL_SCANCODE_A]?2:0)|(keys[SDL_SCANCODE_S]?4:0)|((keys[SDL_SCANCODE_C]||keys[SDL_SCANCODE_Q])?0x100:0)|((keys[SDL_SCANCODE_V]||keys[SDL_SCANCODE_E])?0x200:0)|(keys[SDL_SCANCODE_RETURN]?0x8000:0)),
        std::uint16_t((keys[SDL_SCANCODE_RIGHT]?1:0)|(keys[SDL_SCANCODE_LEFT]?2:0)|(keys[SDL_SCANCODE_DOWN]?4:0)|(keys[SDL_SCANCODE_J]?0x100:0)|(keys[SDL_SCANCODE_K]?0x200:0)|(keys[SDL_SCANCODE_KP_ENTER]?0x8000:0))};
    if(opponent){out[0]|=out[1];out[1]=0;}
    return out;
}
} // namespace puzzle_fighter::prototype
