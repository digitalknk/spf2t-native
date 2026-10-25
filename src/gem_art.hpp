// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game.hpp"
#include <puzzle_fighter/large_finish_picture.hpp>
#include <puzzle_fighter/original_playfield_picture.hpp>
#include <SDL3/SDL.h>
#include <map>
#include <tuple>
namespace puzzle_fighter::prototype {
class GemArt {
    std::vector<std::uint8_t> graphics;
    std::map<std::uint64_t,SDL_Texture*> cache;
    std::map<unsigned,SDL_Texture*> portraits;
    std::map<std::tuple<unsigned,unsigned,std::uint64_t>,SDL_Texture*> glyphs;
    std::map<std::tuple<unsigned,std::uint32_t,std::uint64_t>,SDL_Texture*> figures;
    std::map<std::tuple<unsigned,unsigned,std::uint64_t>,SDL_Texture*> objects;
    std::map<std::pair<unsigned,std::uint64_t>,SDL_Texture*> finish_pictures;
    std::map<unsigned,std::vector<std::uint8_t>> finish_pens;
    SDL_Texture*stage_texture=nullptr;
    SDL_Texture*source_texture=nullptr;
public:
    explicit GemArt(const std::string&root) {
        std::ifstream f(root+"/assets/graphics.bin",std::ios::binary);
        if(!f)throw std::runtime_error("Missing original graphics; run scripts/build_prototype.py");
        graphics.assign((std::istreambuf_iterator<char>(f)),{});
        if(graphics.size()!=0x400000)throw std::runtime_error("Incorrect graphics asset size");
    }
    ~GemArt(){for(auto [key,t]:cache){(void)key;SDL_DestroyTexture(t);}for(auto [key,t]:portraits){(void)key;SDL_DestroyTexture(t);}for(auto [key,t]:figures){(void)key;SDL_DestroyTexture(t);}for(auto [key,t]:objects){(void)key;SDL_DestroyTexture(t);}for(auto [key,t]:finish_pictures){(void)key;SDL_DestroyTexture(t);}for(auto [key,t]:glyphs){(void)key;SDL_DestroyTexture(t);}if(stage_texture)SDL_DestroyTexture(stage_texture);if(source_texture)SDL_DestroyTexture(source_texture);}
    SDL_Texture*source_frame(SDL_Renderer*r,Game&game){
        struct Read{Memory&m;std::uint16_t read_word(std::uint32_t p){p&=0xffffff;if(p>=0xff0000)return std::uint16_t((unsigned(m.ram[p&65535])<<8)|m.ram[(p+1)&65535]);const auto a=p-0x900000;return std::uint16_t((unsigned(m.video.at(a))<<8)|m.video.at(a+1));}} read{game.memory};
        const auto frame=original_playfield_picture(Game::globals,read,graphics,game.sprite_frame);
        if(!source_texture){source_texture=SDL_CreateTexture(r,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STREAMING,384,224);if(!source_texture)throw std::runtime_error(SDL_GetError());SDL_SetTextureScaleMode(source_texture,SDL_SCALEMODE_NEAREST);}
        void*pixels=nullptr;int pitch=0;if(!SDL_LockTexture(source_texture,nullptr,&pixels,&pitch))throw std::runtime_error(SDL_GetError());
        for(unsigned y=0;y<224;++y)for(unsigned x=0;x<384;++x){const auto rgb=frame.colours[y*384+x];const auto bright=15+((rgb>>12)<<1);auto*p=static_cast<std::uint8_t*>(pixels)+y*pitch+x*4;p[0]=((rgb>>8)&15)*17*bright/45;p[1]=((rgb>>4)&15)*17*bright/45;p[2]=(rgb&15)*17*bright/45;p[3]=255;}
        SDL_UnlockTexture(source_texture);return source_texture;
    }
    SDL_Texture*stage(SDL_Renderer*r,Memory&m){
        // The bounded raster is a host GPU adapter. Read immutable frame
        // storage directly rather than charging each pixel to game-call guards.
        struct Read{Memory&m;std::uint16_t read_word(std::uint32_t p){p&=0xffffff;
            if(p>=0xff0000)return std::uint16_t((unsigned(m.ram[p&65535])<<8)|m.ram[(p+1)&65535]);
            const auto a=p-0x900000;return std::uint16_t((unsigned(m.video.at(a))<<8)|m.video.at(a+1));}} read{m};
        if(!stage_texture){stage_texture=SDL_CreateTexture(r,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STREAMING,384,224);if(!stage_texture)throw std::runtime_error(SDL_GetError());SDL_SetTextureScaleMode(stage_texture,SDL_SCALEMODE_NEAREST);}
        void*storage=nullptr;int pitch=0;if(!SDL_LockTexture(stage_texture,nullptr,&storage,&pitch))throw std::runtime_error(SDL_GetError());
        const auto control=m.read_word(Game::globals+0x32);
        for(unsigned y=0;y<224;++y)for(unsigned x=0;x<384;++x){unsigned rgb=read.read_word(0x90d7fe);
            for(unsigned slot=0;slot<4;++slot){const unsigned layer=(control>>(6+slot*2))&3;if(layer!=2&&layer!=3)continue;
                const auto sample=stage_picture_sample(layer-2,x,y,Game::globals,read);const auto at=sample.graphics_offset();unsigned pen=0;
                for(unsigned plane=0;plane<4;++plane)pen|=((graphics.at(at+plane)>>(7-sample.x%8))&1u)<<plane;
                if(pen!=15)rgb=read.read_word(0x90c000+sample.bank*32+pen*2);
            }
            const auto bright=15+((rgb>>12)<<1);auto*p=static_cast<std::uint8_t*>(storage)+y*pitch+x*4;p[0]=((rgb>>8)&15)*17*bright/45;p[1]=((rgb>>4)&15)*17*bright/45;p[2]=(rgb&15)*17*bright/45;p[3]=255;
        }
        SDL_UnlockTexture(stage_texture);return stage_texture;
    }
    std::uint64_t palette_key(Memory&m,unsigned bank){
        std::uint64_t hash=1469598103934665603ull;
        for(unsigned i=0;i<16;++i){hash^=m.read_word(0x90c000+bank*32+i*2);hash*=1099511628211ull;}return hash;
    }
    SDL_Texture*object(SDL_Renderer*r,Memory&m,unsigned tile,unsigned attr){
        const auto key=std::make_tuple(tile,attr,palette_key(m,attr&31));
        if(auto found=objects.find(key);found!=objects.end())return found->second;
        const unsigned across=((attr>>8)&15)+1,down=((attr>>12)&15)+1;
        auto*s=SDL_CreateSurface(across*16,down*16,SDL_PIXELFORMAT_RGBA32);if(!s)throw std::runtime_error(SDL_GetError());
        auto*bytes=static_cast<std::uint8_t*>(s->pixels);const bool fx=attr&0x20,fy=attr&0x40;
        for(unsigned ty=0;ty<down;++ty)for(unsigned tx=0;tx<across;++tx){
            const unsigned cx=fx?across-1-tx:tx,cy=fy?down-1-ty:ty;
            const unsigned code=(!fx&&!fy)?(tile&~15u)+((tile+cx)&15u)+16*cy:tile+cx+16*cy;
            for(unsigned y=0;y<16;++y)for(unsigned x=0;x<16;++x){
                const unsigned sx=fx?15-x:x,sy=fy?15-y:y,at=code*128+sy*8+(sx/8)*4;
                unsigned pen=15;if(at+3<graphics.size()){pen=0;for(unsigned plane=0;plane<4;++plane)pen|=((graphics[at+plane]>>(7-sx%8))&1u)<<plane;}
                const auto rgb=m.read_word(0x90c000+(attr&31)*32+pen*2);auto*p=bytes+(ty*16+y)*s->pitch+(tx*16+x)*4;
                const auto bright=15+((rgb>>12)<<1);
                p[0]=((rgb>>8)&15)*17*bright/45;p[1]=((rgb>>4)&15)*17*bright/45;p[2]=(rgb&15)*17*bright/45;p[3]=pen==15?0:255;
            }
        }
        auto*t=SDL_CreateTextureFromSurface(r,s);SDL_DestroySurface(s);if(!t)throw std::runtime_error(SDL_GetError());
        SDL_SetTextureScaleMode(t,SDL_SCALEMODE_NEAREST);SDL_SetTextureBlendMode(t,SDL_BLENDMODE_BLEND);objects[key]=t;return t;
    }
    SDL_Texture*glyph(SDL_Renderer*r,Memory&m,unsigned tile,unsigned attr){
        const unsigned bank=32+(attr&31);const auto key=std::make_tuple(tile,attr,palette_key(m,bank));
        if(auto found=glyphs.find(key);found!=glyphs.end())return found->second;
        auto*s=SDL_CreateSurface(8,8,SDL_PIXELFORMAT_RGBA32);if(!s)throw std::runtime_error(SDL_GetError());
        for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x){
            const unsigned sx=attr&0x20?7-x:x,sy=attr&0x40?7-y:y,at=(tile&0xffff)*64+sy*8;
            unsigned pen=15;if(at+3<graphics.size()){pen=0;for(unsigned plane=0;plane<4;++plane)pen|=((graphics[at+plane]>>(7-sx))&1u)<<plane;}
            const auto rgb=m.read_word(0x90c000+bank*32+pen*2);const auto bright=15+((rgb>>12)<<1);
            auto*p=static_cast<std::uint8_t*>(s->pixels)+y*s->pitch+x*4;p[0]=((rgb>>8)&15)*17*bright/45;p[1]=((rgb>>4)&15)*17*bright/45;p[2]=(rgb&15)*17*bright/45;p[3]=pen==15?0:255;
        }
        auto*t=SDL_CreateTextureFromSurface(r,s);SDL_DestroySurface(s);if(!t)throw std::runtime_error(SDL_GetError());
        SDL_SetTextureScaleMode(t,SDL_SCALEMODE_NEAREST);SDL_SetTextureBlendMode(t,SDL_BLENDMODE_BLEND);glyphs[key]=t;return t;
    }
    SDL_Texture*large_finish(SDL_Renderer*r,Memory&m,unsigned stage){
        const auto key=std::make_pair(stage,palette_key(m,126));
        if(auto f=finish_pictures.find(key);f!=finish_pictures.end())return f->second;
        auto&pens=finish_pens[stage];
        if(pens.empty()){pens.resize(384*224);for(unsigned y=0;y<224;++y)for(unsigned x=0;x<384;++x){
            const auto sample=large_finish_picture_sample(x,y,std::uint16_t(stage),m);if(sample.palette_bank()!=126)throw std::runtime_error("Unexpected original finish palette");
            const auto at=sample.graphics_offset();unsigned pen=0;for(unsigned plane=0;plane<4;++plane)pen|=((graphics.at(at+plane)>>(7-sample.x%8))&1u)<<plane;
            pens[y*384+x]=pen;
        }}
        auto*s=SDL_CreateSurface(384,224,SDL_PIXELFORMAT_RGBA32);if(!s)throw std::runtime_error(SDL_GetError());
        for(unsigned y=0;y<224;++y)for(unsigned x=0;x<384;++x){const auto pen=pens[y*384+x];const auto rgb=m.read_word(0x90cfc0+pen*2);const auto bright=15+((rgb>>12)<<1);auto*p=static_cast<std::uint8_t*>(s->pixels)+y*s->pitch+x*4;p[0]=((rgb>>8)&15)*17*bright/45;p[1]=((rgb>>4)&15)*17*bright/45;p[2]=(rgb&15)*17*bright/45;p[3]=pen==15?0:255;}
        auto*t=SDL_CreateTextureFromSurface(r,s);SDL_DestroySurface(s);if(!t)throw std::runtime_error(SDL_GetError());
        SDL_SetTextureScaleMode(t,SDL_SCALEMODE_NEAREST);SDL_SetTextureBlendMode(t,SDL_BLENDMODE_BLEND);finish_pictures[key]=t;return t;
    }
    SDL_Texture*figure(SDL_Renderer*r,Memory&m,unsigned character,std::uint32_t frame,int side=-1) {
        if(!frame)return nullptr;
        std::uint64_t colours=0;if(side>=0)for(unsigned n=0;n<4;++n)colours^=palette_key(m,12+unsigned(side)*4+n)+(colours<<6)+(colours>>2);
        const auto key=std::make_tuple(character,frame,colours);
        if(auto found=figures.find(key);found!=figures.end())return found->second;
        auto record=m.read_long(frame+4);if(!record||record>=m.data.size()-12)return nullptr;
        const unsigned format=m.read_word(record),budget=m.read_word(record+2);(void)budget;
        if(format!=0&&format!=2&&format!=4)return nullptr;
        unsigned count=m.read_word(record+(format==0?2:4))+(format==0?0:1);
        if(count>256)return nullptr;
        unsigned shared_attr=0;std::uint32_t coords=0;
        if(format==0){shared_attr=m.read_word(record+4);coords=m.read_long(0x2db84+m.read_word(record+6));record+=8;}
        else if(format==2){coords=m.read_long(0x2db84+m.read_word(record+6));record+=8;}
        else record+=6;
        auto*surface=SDL_CreateSurface(192,192,SDL_PIXELFORMAT_RGBA32);if(!surface)throw std::runtime_error(SDL_GetError());
        auto*bytes=static_cast<std::uint8_t*>(surface->pixels);std::fill(bytes,bytes+surface->pitch*192,0);
        for(unsigned part=0;part<count;++part) {
            unsigned tile,attr;int ox,oy;
            if(format==4){tile=m.read_word(record)+0x5800;attr=m.read_word(record+2);ox=Game::signed_word(m.read_word(record+4));oy=Game::signed_word(m.read_word(record+6));record+=8;}
            else {ox=Game::signed_word(m.read_word(coords));oy=Game::signed_word(m.read_word(coords+2));coords+=4;tile=m.read_word(record);attr=format==0?shared_attr:m.read_word(record+2);record+=format==0?2:4;}
            const unsigned across=((attr>>8)&15)+1,down=((attr>>12)&15)+1;
            const bool flipx=attr&0x20,flipy=attr&0x40;
            for(unsigned ty=0;ty<down;++ty)for(unsigned tx=0;tx<across;++tx) {
                unsigned cx=flipx?across-1-tx:tx,cy=flipy?down-1-ty:ty;
                const unsigned code=(!flipx&&!flipy)?(tile&~15u)+((tile+cx)&15u)+16*cy:tile+cx+16*cy;
                for(unsigned y=0;y<16;++y)for(unsigned x=0;x<16;++x) {
                    const int px=96+ox+int(tx*16+x),py=160+oy+int(ty*16+y);if(px<0||px>=192||py<0||py>=192)continue;
                    const unsigned sx=flipx?15-x:x,sy=flipy?15-y:y,at=code*128+sy*8+(sx/8)*4;
                    if(at+3>=graphics.size())continue;
                    unsigned pen=0;for(unsigned plane=0;plane<4;++plane)pen|=((graphics[at+plane]>>(7-sx%8))&1u)<<plane;
                    if(pen==15)continue;
                    auto rgb=m.read_word(side<0?0xa0000+character*256+(attr&3u)*32+pen*2:0x90c000+(12+unsigned(side)*4+(attr&3u))*32+pen*2);
                    auto*dst=bytes+py*surface->pitch+px*4;dst[0]=((rgb>>8)&15)*17;dst[1]=((rgb>>4)&15)*17;dst[2]=(rgb&15)*17;dst[3]=255;
                }
            }
        }
        auto*t=SDL_CreateTextureFromSurface(r,surface);SDL_DestroySurface(surface);if(!t)throw std::runtime_error(SDL_GetError());
        SDL_SetTextureScaleMode(t,SDL_SCALEMODE_NEAREST);SDL_SetTextureBlendMode(t,SDL_BLENDMODE_BLEND);figures[key]=t;return t;
    }
    SDL_Texture*portrait(SDL_Renderer*r,Memory&m,unsigned character) {
        if(character>=8)return nullptr;
        if(auto found=portraits.find(character);found!=portraits.end())return found->second;
        // USA DATA 8DC3C, third list; original 7D5E writes this tile block in
        // column order. These are original square portraits, not sprite poses.
        const auto table=0x8dc3cu+m.read_word(0x8dc40u);
        const auto record=m.read_long(table+character*16+4)+6;
        auto*surface=SDL_CreateSurface(48,48,SDL_PIXELFORMAT_RGBA32);if(!surface)throw std::runtime_error(SDL_GetError());
        auto*bytes=static_cast<std::uint8_t*>(surface->pixels);
        for(unsigned tx=0;tx<3;++tx)for(unsigned ty=0;ty<3;++ty) {
            const auto tile=m.read_word(record+(tx*3+ty)*4)+0x5800u;
            const auto attr=m.read_word(record+(tx*3+ty)*4+2);
            for(unsigned y=0;y<16;++y)for(unsigned x=0;x<16;++x) {
                const unsigned px=(attr&0x20)?15-x:x,py=(attr&0x40)?15-y:y;
                const auto at=tile*128+py*8+(px/8)*4;
                unsigned pen=0;for(unsigned plane=0;plane<4;++plane)pen|=((graphics.at(at+plane)>>(7-px%8))&1u)<<plane;
                // Original menu bank A52C0 (20D86's A4EC0 + 400).
                const auto rgb=m.read_word(0xa52c0u+(attr&31u)*32+pen*2);
                auto*dst=bytes+(ty*16+y)*surface->pitch+(tx*16+x)*4;
                dst[0]=((rgb>>8)&15)*17;dst[1]=((rgb>>4)&15)*17;dst[2]=(rgb&15)*17;dst[3]=pen==15?0:255;
            }
        }
        auto*t=SDL_CreateTextureFromSurface(r,surface);SDL_DestroySurface(surface);if(!t)throw std::runtime_error(SDL_GetError());
        SDL_SetTextureScaleMode(t,SDL_SCALEMODE_NEAREST);SDL_SetTextureBlendMode(t,SDL_BLENDMODE_BLEND);portraits[character]=t;return t;
    }
    SDL_Texture*texture(SDL_Renderer*r,Memory&m,std::uint16_t value,std::uint64_t frame) {
        const unsigned kind=value&15u;
        unsigned palette=kind&3u,tile=0x15a00;
        if(kind==6||kind==7) {
            const auto count=(value>>8)&15u;tile=count?0x15a50+std::min(count,9u):0x16776;palette=(value>>12)&3u;
        } else if(kind==5) {
            constexpr unsigned frames[]={0x1680c,0x1680d,0x1680e,0x1680f,0x1681c,0x1681d,0x1681e,0x1681f};
            tile=frames[(frame/2)%8];palette=16+(frame/4)%8;
        } else if(kind>=9&&kind<=12) {
            constexpr unsigned frames[]={0x1651f,0x1652f,0x1653f,0x1654f,0x16556,0x16557,0x1655f,0x1656f};tile=frames[(frame/3)%8];
        } else if(value&0x80u) {
            constexpr unsigned shapes[]={0x15a3c,0x15a3c,0x15a2c,0x15a30,0x15a38,0x15a28,0x15a20,0x15a40,0x15a3c,0x15a31,0x15a3c,0x15a18,0x15a21,0x15a44,0x15a1c,0x15a48};tile=shapes[value>>12];
        }
        const auto key=(std::uint64_t(tile)<<8)|palette;auto found=cache.find(key);if(found!=cache.end())return found->second;
        auto*surface=SDL_CreateSurface(16,16,SDL_PIXELFORMAT_RGBA32);if(!surface)throw std::runtime_error(SDL_GetError());
        auto*bytes=static_cast<std::uint8_t*>(surface->pixels);
        for(unsigned y=0;y<16;++y)for(unsigned x=0;x<16;++x) {
            const auto at=std::size_t(tile)*128+y*8-0x800000;
            unsigned pen=0;for(unsigned plane=0;plane<4;++plane)pen|=((graphics.at(at+(x/8)*4+plane)>>(7-x%8))&1u)<<plane;
            const auto pal=palette>=16?0xab8c0u+(palette-16u)*32:0xa0b00u+palette*32;
            const auto rgb=m.read_word(pal+pen*2);
            auto*dst=bytes+y*surface->pitch+x*4;dst[0]=((rgb>>8)&15)*17;dst[1]=((rgb>>4)&15)*17;dst[2]=(rgb&15)*17;dst[3]=pen==15?0:255;
        }
        auto*t=SDL_CreateTextureFromSurface(r,surface);SDL_DestroySurface(surface);if(!t)throw std::runtime_error(SDL_GetError());
        SDL_SetTextureScaleMode(t,SDL_SCALEMODE_NEAREST);SDL_SetTextureBlendMode(t,SDL_BLENDMODE_BLEND);cache[key]=t;return t;
    }
};
} // namespace puzzle_fighter::prototype
