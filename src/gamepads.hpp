// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <SDL3/SDL.h>
#include <array>
#include <cstdint>
#include <vector>
namespace puzzle_fighter::prototype {
// Host input only. Uses SDL's mapped gamepad API; no gameplay memory access.
class Gamepads {
    struct Slot {
        SDL_Gamepad*pad=nullptr;SDL_JoystickID id=0;
        std::array<bool,4> direction{};
        std::array<std::uint64_t,4> repeat{};
        bool confirm=false,cancel=false,start=false,back=false;
    };
    std::array<Slot,2> slots{};
    bool initialized=false;
    void discover() {
        int count=0;auto*ids=SDL_GetGamepads(&count);
        for(int n=0;n<count;++n) {
            bool known=false;for(const auto&s:slots)known|=s.id==ids[n];if(known)continue;
            for(unsigned i=0;i<2;++i)if(!slots[i].pad) {
                auto*pad=SDL_OpenGamepad(ids[n]);if(!pad)break;
                slots[i].pad=pad;slots[i].id=ids[n];SDL_SetGamepadPlayerIndex(pad,int(i));break;
            }
        }
        SDL_free(ids);
    }
public:
    struct Frame {
        std::array<std::uint16_t,2> gameplay{};
        std::array<bool,2> confirm{};
        std::vector<SDL_Scancode> menu;
        bool start=false,options=false;
    };
    Gamepads(){initialized=SDL_InitSubSystem(SDL_INIT_GAMEPAD);if(initialized)discover();}
    ~Gamepads(){for(auto&s:slots)if(s.pad)SDL_CloseGamepad(s.pad);if(initialized)SDL_QuitSubSystem(SDL_INIT_GAMEPAD);}
    Gamepads(const Gamepads&)=delete;
    Gamepads&operator=(const Gamepads&)=delete;
    bool connected(unsigned player)const{return slots[player].pad!=nullptr;}
    SDL_JoystickID instance(unsigned player)const{return slots[player].id;}
    // Return the disconnected slot, so the host can pause a human's game.
    int event(const SDL_Event&e) {
        int removed=-1;
        if(e.type==SDL_EVENT_GAMEPAD_REMOVED)for(unsigned i=0;i<2;++i)if(slots[i].id==e.gdevice.which){SDL_CloseGamepad(slots[i].pad);slots[i]=Slot{};removed=int(i);}
        if(initialized&&(e.type==SDL_EVENT_GAMEPAD_ADDED||e.type==SDL_EVENT_GAMEPAD_REMOVED))discover();
        return removed;
    }
    Frame sample(std::uint64_t now) {
        Frame out;
        constexpr std::array<std::array<SDL_Scancode,4>,2> keys={{{SDL_SCANCODE_W,SDL_SCANCODE_S,SDL_SCANCODE_A,SDL_SCANCODE_D},{SDL_SCANCODE_UP,SDL_SCANCODE_DOWN,SDL_SCANCODE_LEFT,SDL_SCANCODE_RIGHT}}};
        for(unsigned i=0;i<2;++i) {
            auto&s=slots[i];if(!s.pad)continue;
            const auto button=[&](SDL_GamepadButton b){return SDL_GetGamepadButton(s.pad,b);};
            const int x=SDL_GetGamepadAxis(s.pad,SDL_GAMEPAD_AXIS_LEFTX),y=SDL_GetGamepadAxis(s.pad,SDL_GAMEPAD_AXIS_LEFTY);
            std::array<bool,4> dirs={button(SDL_GAMEPAD_BUTTON_DPAD_UP)||y<-12000,button(SDL_GAMEPAD_BUTTON_DPAD_DOWN)||y>12000,button(SDL_GAMEPAD_BUTTON_DPAD_LEFT)||x<-12000,button(SDL_GAMEPAD_BUTTON_DPAD_RIGHT)||x>12000};
            const auto raw_dirs=dirs; // Gameplay retains the original simultaneous bits.
            if(dirs[0]&&dirs[1])dirs[0]=dirs[1]=false;
            if(dirs[2]&&dirs[3])dirs[2]=dirs[3]=false;
            const bool confirm=button(SDL_GAMEPAD_BUTTON_SOUTH),cancel=button(SDL_GAMEPAD_BUTTON_EAST);
            out.gameplay[i]=std::uint16_t((raw_dirs[3]?1:0)|(raw_dirs[2]?2:0)|(raw_dirs[1]?4:0)|(raw_dirs[0]?8:0)|(confirm?0x100:0)|(cancel?0x200:0)|(button(SDL_GAMEPAD_BUTTON_WEST)?0x8000:0));
            for(unsigned d=0;d<4;++d) {
                if(dirs[d]&&(!s.direction[d]||now>=s.repeat[d])){out.menu.push_back(keys[i][d]);s.repeat[d]=now+(s.direction[d]?90:300);}
                s.direction[d]=dirs[d];
            }
            if(confirm&&!s.confirm){out.confirm[i]=true;out.menu.push_back(i?SDL_SCANCODE_J:SDL_SCANCODE_C);}
            if(cancel&&!s.cancel)out.menu.push_back(i?SDL_SCANCODE_K:SDL_SCANCODE_V);
            const bool start=button(SDL_GAMEPAD_BUTTON_START),back=button(SDL_GAMEPAD_BUTTON_BACK);
            if(start)out.gameplay[i]|=0x8000;
            out.start|=start&&!s.start;out.options|=back&&!s.back;
            s.confirm=confirm;s.cancel=cancel;s.start=start;s.back=back;
        }
        return out;
    }
};
} // namespace puzzle_fighter::prototype
