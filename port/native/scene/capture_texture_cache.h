// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "scene_bridge.h"
#include <cstring>
#include <list>
#include <unordered_map>
namespace silent_scope::scene {
struct TextureSource {
 uint32_t width{},height{},lod{},format{};
 std::vector<uint8_t> bytes;std::vector<uint32_t> palette;
 bool operator==(const TextureSource&)const=default;
};
inline uint64_t texture_source_hash(const TextureSource& source){
 uint64_t hash=1469598103934665603ULL;
 auto mix=[&](uint64_t v){hash^=v;hash*=1099511628211ULL;};
 for(auto v:{source.width,source.height,source.lod,source.format})mix(v);
 auto data=[&](const void* memory,size_t size){
  auto p=static_cast<const uint8_t*>(memory);
  while(size>=8){uint64_t word;std::memcpy(&word,p,8);mix(word);p+=8;size-=8;}
  while(size--){mix(*p++);}
 };
 mix(source.bytes.size());data(source.bytes.data(),source.bytes.size());
 mix(source.palette.size());data(source.palette.data(),source.palette.size()*4);return hash;
}
// Called only by the emulation thread. Hash collisions require exact source equality.
// Entries own source bytes and immutable decoded images; eviction cannot alter readers.
class CapturedTextureCache {
 struct Entry {uint64_t hash;TextureSource source;std::shared_ptr<const TextureImage> image;size_t bytes;};
 using Entries=std::list<Entry>;Entries entries_;
 std::unordered_multimap<uint64_t,Entries::iterator> index_;
 size_t budget_,resident_{};TextureSource scratch_;
 uint64_t(*hash_)(const TextureSource&);
public:
 struct Stats {uint64_t lookups{},hits{},decodes{},source_bytes{},decoded_bytes{};};Stats stats;
 explicit CapturedTextureCache(size_t budget=64*1024*1024,uint64_t(*hash)(const TextureSource&)=texture_source_hash):budget_(budget),hash_(hash){}
 TextureSource& scratch(){return scratch_;}
 size_t resident_bytes()const{return resident_;}
 void clear(){index_.clear();entries_.clear();resident_=0;stats={};}
 template<class Decode>std::shared_ptr<const TextureImage> get(const TextureSource& source,Decode decode){
  ++stats.lookups;stats.source_bytes+=source.bytes.size()+source.palette.size()*4;
  auto hash=hash_(source);auto range=index_.equal_range(hash);
  for(auto i=range.first;i!=range.second;++i)if(i->second->source==source){
   ++stats.hits;entries_.splice(entries_.begin(),entries_,i->second);return i->second->image;
  }
  auto image=decode();++stats.decodes;stats.decoded_bytes+=image->pixels.size()*4;
  size_t bytes=sizeof(Entry)+source.bytes.size()+source.palette.size()*4+image->pixels.size()*4;
  if(bytes>budget_)return image;
  while(resident_+bytes>budget_&&!entries_.empty()){
   auto old=std::prev(entries_.end());auto group=index_.equal_range(old->hash);
   for(auto i=group.first;i!=group.second;++i)if(i->second==old){index_.erase(i);break;}
   resident_-=old->bytes;entries_.erase(old);
  }
  entries_.push_front({hash,source,image,bytes});index_.emplace(hash,entries_.begin());resident_+=bytes;return image;
 }
};
inline CapturedTextureCache& captured_texture_cache(){static CapturedTextureCache cache;return cache;}
}

