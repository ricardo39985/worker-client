#pragma once
#include "ow/adaptive_hardware.hpp"
#include <optional>
#include <set>
#include <tuple>

namespace ow {
// Reporting evidence only. Never mutates the scheduler or its active reservations.
// Caller serializes access with the same lock as the current profile set.
class CapabilityReport {
 using Key=std::tuple<std::string,adaptive::Backend,adaptive::Codec>;
 bool assessed_{},video_{},image_{},app_video_{},app_image_{};
 std::vector<adaptive::Profile> profiles_;
 std::set<Key> failed_;
 static Key key(const adaptive::Profile& p){return {p.stable_id,p.backend,p.codec};}
 std::set<Key> visible() const {
  std::set<Key> out;for(const auto& p:profiles_)if(!failed_.contains(key(p)))out.insert(key(p));return out;
 }
 std::string update() const {auto tokens=encoders();return "ENCODERS UPDATE encoders="+(tokens.empty()?"none":tokens);}
public:
 void probed(bool video,bool image,bool app_video,bool app_image,std::vector<adaptive::Profile> profiles){
  assessed_=true;video_=video;image_=image;app_video_=app_video;app_image_=app_image;profiles_=std::move(profiles);failed_.clear();
 }
 bool video() const{return video_;}
 bool image() const{return image_;}
 bool app_image() const{return app_image_;}
 bool app_video() const{
  auto cpu=[&](adaptive::Codec codec){return std::any_of(profiles_.begin(),profiles_.end(),[&](const auto& p){return p.backend==adaptive::Backend::cpu&&p.codec==codec;});};
  return app_video_&&cpu(adaptive::Codec::h264)&&cpu(adaptive::Codec::hevc);
 }
 std::optional<bool> processing_ready() const{if(!assessed_)return {};return video_||image_||app_video()||app_image_;}
 const std::vector<adaptive::Profile>& profiles() const{return profiles_;}
 std::string encoders() const {
  std::vector<adaptive::Profile> current;
  for(const auto& p:profiles_)if(!failed_.contains(key(p)))current.push_back(p);
  // These CPU command probes are independent of adaptive video tuning.
  if(video_||image_||app_video_||app_image_){adaptive::Profile cpu{};cpu.backend=adaptive::Backend::cpu;current.push_back(cpu);}
  return adaptive::encoder_tokens(current);
 }
 std::string record() const {
  auto tokens=encoders();
  return "PROBE RESULT app_video="+std::to_string(app_video())+" app_image="+std::to_string(app_image_)+
   " video="+std::to_string(video_)+" image="+std::to_string(image_)+" encoders="+(tokens.empty()?"none":tokens);
 }
 std::optional<std::string> failed(const adaptive::Profile& profile,bool demoted=false){
  if(profile.backend==adaptive::Backend::cpu||!visible().contains(key(profile))||!failed_.insert(key(profile)).second)return {};
  return update()+" failed="+adaptive::backend_token(profile.backend)+"/"+
   (profile.codec==adaptive::Codec::hevc?"hevc":"h264")+(demoted?" demoted=1":"")+" fallback=cpu";
 }
 std::optional<std::string> changed(std::vector<adaptive::Profile> profiles){
  auto before=visible();profiles_=std::move(profiles);if(before==visible())return {};return update();
 }
 std::optional<std::string> disabled(){
  const auto before=encoders();probed(false,false,false,false,{});if(before.empty())return {};return update();
 }
};
}
