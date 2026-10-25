// SPDX-License-Identifier: GPL-3.0-only
// Host coin switches: two IRQs closed then two open, consumed by source1550.
#pragma once
#include <puzzle_fighter/input_sampling.hpp>
#include <array>
#include <stdexcept>
namespace puzzle_fighter::prototype {
struct CoinSwitchInputs {
 struct Pulse {unsigned queued=0,remaining=0;};std::array<Pulse,3>switches{};
 void coin(unsigned side=0){if(side>1)throw std::runtime_error("Invalid coin chute");++switches[side].queued;}
 void service_credit(){++switches[2].queued;}
 void clear(){switches={};}
 void apply(InputPorts&ports){constexpr unsigned masks[]={0x1000,0x2000,0x0004};for(unsigned i=0;i<3;++i){auto&p=switches[i];if(!p.remaining&&p.queued){--p.queued;p.remaining=4;}if(p.remaining){if(p.remaining>=3)ports.system&=std::uint16_t(~masks[i]);--p.remaining;}}}
};
} // namespace puzzle_fighter::prototype
