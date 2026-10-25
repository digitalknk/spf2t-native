// SPDX-License-Identifier: GPL-3.0-only
// Native original-ROM sequencer + sample mixer. No Z80/DSP instruction execution.
#pragma once
#include "game.hpp"
#include <cps2recomp/sound/driver.h>
#include <cps2recomp/sound/qsound.h>
#include <SDL3/SDL.h>
#include <deque>
#include <memory>
#include <cmath>
namespace puzzle_fighter::prototype {
class NativeAudio {
    struct Write {std::uint8_t reg;std::uint16_t data;std::uint32_t gap;};
    std::vector<std::uint8_t> program,samples;
    std::array<std::uint8_t,4096> shared{};
    std::unique_ptr<cps2recomp::QSound> chip;
    std::unique_ptr<cps2recomp::SoundDriver> driver;
    std::deque<std::array<std::uint8_t,16>> commands;
    std::deque<Write> writes;
    std::uint64_t sample_count=0,clock=0,next_write=0,last_write=0;
    unsigned interrupt_phase=0,frame_phase=0,cue_frames=0;
    SDL_AudioStream*stream=nullptr;
    std::vector<std::int16_t> recording;
    bool record=false,music=true;
    static std::vector<std::uint8_t> load(const std::string&name) {
        std::ifstream f(name,std::ios::binary);if(!f)throw std::runtime_error("Missing prepared sound assets: "+name);
        return {(std::istreambuf_iterator<char>(f)),{}};
    }
    void render(unsigned frames,std::vector<std::int16_t>&out) {
        for(unsigned i=0;i<frames;++i) {
            clock=sample_count*8000000u/rate;
            interrupt_phase+=250;
            if(interrupt_phase>=rate) {
                interrupt_phase-=rate;
                if(shared[15]==255&&!commands.empty()){std::copy(commands.front().begin(),commands.front().end(),shared.begin());commands.pop_front();}
                driver->Interrupt();
            }
            if(!writes.empty()&&clock>=next_write) {
                const auto w=writes.front();writes.pop_front();chip->Write(0,w.data>>8);chip->Write(1,w.data&255);chip->Write(2,w.reg);
                last_write=clock;if(!writes.empty())next_write=last_write+writes.front().gap;
            }
            std::int16_t value[2];chip->Step(value);++sample_count;
            for(auto v:value){peak=std::max(peak,unsigned(std::abs(int(v))));nonzero_samples+=v!=0;out.push_back(v);}
        }
    }
public:
    static constexpr unsigned rate=cps2recomp::QSound::kSampleRate;
    unsigned peak=0;std::uint64_t nonzero_samples=0,requests=0;
    NativeAudio(const std::string&root,bool record_audio,bool play_music):program(load(root+"/assets/sound-program.bin")),samples(load(root+"/assets/samples.bin")),record(record_audio),music(play_music) {
        if(!cps2recomp::SoundDriver::Knows(program))throw std::runtime_error("Unsupported original sound driver; no interpreter fallback");
        chip=std::make_unique<cps2recomp::QSound>(samples.data(),samples.size());
        driver=std::make_unique<cps2recomp::SoundDriver>(program,shared.data(),[&](std::uint8_t reg,std::uint16_t data,std::uint32_t gap){if(writes.empty())next_write=std::max(clock,last_write)+gap;writes.push_back({reg,data,gap});});
        reset();
    }
    ~NativeAudio(){if(stream)SDL_DestroyAudioStream(stream);}
    void open_device() {
        if(!SDL_InitSubSystem(SDL_INIT_AUDIO))throw std::runtime_error(SDL_GetError());
        SDL_AudioSpec spec{SDL_AUDIO_S16,2,int(rate)};
        stream=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&spec,nullptr,nullptr);
        if(!stream)throw std::runtime_error(SDL_GetError());
        SDL_ResumeAudioStreamDevice(stream);
    }
    void reset(std::uint16_t song=1) {
        if(stream)SDL_ClearAudioStream(stream);
        commands.clear();writes.clear();shared.fill(0);sample_count=clock=next_write=last_write=0;interrupt_phase=frame_phase=0;
        chip->Reset();driver->Reset();shared[15]=255;shared[0xffd]=0x88;shared[0xffe]=1;shared[0xfff]=255;
        std::vector<std::int16_t> discard;render(rate/8,discard); // Native driver startup, not emulated boot.
        peak=0;nonzero_samples=0;requests=0;
        cue_frames=0;if(music&&song)enqueue({song,0x1000,0});
    }
    void music_cue(std::uint16_t song){cue_frames=0;if(music)enqueue({song,0x1000,0});}
    void tempo(std::uint16_t amount){
        if(!music)return;
        std::array<std::uint8_t,16> c{};c[0]=0xff;c[1]=8;c[3]=amount>>8;c[4]=amount&255;
        commands.push_back(c);++requests;
    }
    bool source_command_ready()const{return commands.empty()&&shared[15]==255;}
    void source_stereo(std::uint8_t value){shared[0xffe]=value;}
    std::uint16_t tempo_adjustment()const{return driver->TempoAdjustment();}
    bool music_finished()const{return !music||(cue_frames>30&&commands.empty()&&shared[15]==255&&!driver->MusicPlaying());}
    bool effects_finished()const{return commands.empty()&&shared[15]==255&&!driver->EffectsPlaying();}
    void enqueue(SoundRequest r) {
        std::array<std::uint8_t,16> c{};
        // Original USA2B14A..2B1A2 permutes the three queue longs onto shared bytes.
        c[0]=r.number>>8;c[1]=r.number&255;
        c[3]=r.number>>24;c[4]=(r.number>>16)&255;
        c[2]=r.pan>>24;c[6]=(r.pan>>16)&255;c[7]=(r.pan>>8)&255;c[8]=r.pan&255;
        c[9]=(r.extra>>8)&255;c[10]=r.extra&255;c[11]=r.extra>>24;c[12]=(r.extra>>16)&255;
        c[15]=0;commands.push_back(c);++requests;
    }
    void frame() {
        ++cue_frames;
        frame_phase+=rate;const unsigned count=frame_phase/60;frame_phase%=60;
        std::vector<std::int16_t> pcm;pcm.reserve(count*2);render(count,pcm);
        if(record)recording.insert(recording.end(),pcm.begin(),pcm.end());
        if(stream) {
            if(SDL_GetAudioStreamQueued(stream)>int(rate*4/3))SDL_ClearAudioStream(stream);
            if(!SDL_PutAudioStreamData(stream,pcm.data(),int(pcm.size()*2)))throw std::runtime_error(SDL_GetError());
        }
    }
    void pause(bool paused){if(stream){if(paused)SDL_PauseAudioStreamDevice(stream);else SDL_ResumeAudioStreamDevice(stream);}}
    void save(const std::string&path) {
        if(path.empty())return;
        std::ofstream f(path,std::ios::binary);if(!f)throw std::runtime_error("Cannot save WAV: "+path);
        auto put=[&](std::uint32_t v,unsigned n){for(unsigned i=0;i<n;++i)f.put(char(v>>(8*i)));};
        f.write("RIFF",4);put(36+recording.size()*2,4);f.write("WAVEfmt ",8);put(16,4);put(1,2);put(2,2);put(rate,4);put(rate*4,4);put(4,2);put(16,2);f.write("data",4);put(recording.size()*2,4);
        for(auto v:recording)put(std::uint16_t(v),2);
    }
};
} // namespace puzzle_fighter::prototype
