// SPDX-License-Identifier: GPL-3.0-only
// Native storage endpoint for source1E6F4, not an EEPROM bus/CPU emulator.
#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
namespace puzzle_fighter::prototype {
struct NativeOperatorStore {
 std::filesystem::path path;std::array<std::uint8_t,128>image{};bool initialized=false;unsigned transactions=0;
 template<class M> void cold(std::uint32_t g,M&m){
  std::array<std::uint8_t,128>factory{},candidate{};for(unsigned i=0;i<128;++i)factory[i]=m.read_byte(g-0x7070 + i);
  bool found=initialized;candidate=image;
  if(!path.empty()){std::ifstream f(path,std::ios::binary);found=bool(f);if(found){f.read(reinterpret_cast<char*>(candidate.data()),candidate.size());found=f.gcount()==128&&f.peek()==std::char_traits<char>::eof();}}
  // This endpoint preserves counters for the supported factory profile only.
  // Operator UI/validation remains separate; foreign settings never activate
  // an unimplemented hardware profile via a storage file.
  if(!found||!std::equal(factory.begin(),factory.begin()+96,candidate.begin()))candidate=factory;
  image=candidate;initialized=true;for(unsigned i=0;i<128;++i)m.write_byte(g-0x7070 + i,image[i]);
 }
 template<class M> void persist(std::uint32_t g,M&m){
  const auto count=m.read_word(g-0x6f6c),offset=m.read_word(g-0x6f68);
  if(!initialized||count!=1||(offset!=0x60&&offset!=0x64&&offset!=0x68))throw std::runtime_error("Unbound original operator storage transaction");
  // Successful1E6F4 wrapper contract. The transport writes two16-bit words,
  // reads those words into its source readback cache, then verifies them.
  m.write_byte(g-0x6f64,10);m.write_long(g-0x6f70,g-0x7070);m.write_word(g-0x6f6a,count);m.write_word(g-0x6f66,offset);m.write_byte(g+0x87,1);
  auto next=image;for(unsigned i=0;i<4;++i)next[offset+i]=m.read_byte(g-0x7070 + offset+i);
  if(!path.empty()){
   if(!path.parent_path().empty()){std::filesystem::create_directories(path.parent_path());}auto temporary=path;temporary+=".tmp";
   std::ofstream f(temporary,std::ios::binary|std::ios::trunc);f.write(reinterpret_cast<const char*>(next.data()),next.size());f.close();if(!f)throw std::runtime_error("Cannot save original operator counters");std::filesystem::rename(temporary,path);
  }
  image=next;++transactions;m.write_byte(g+0x87,0);m.write_long(g-0x6f70,g-0x6ff0);m.write_word(g-0x6f6a,count);m.write_word(g-0x6f66,offset);
  for(unsigned i=0;i<4;++i)m.write_byte(g-0x6ff0 + offset+i,image[offset+i]);
 }
};
} // namespace puzzle_fighter::prototype
