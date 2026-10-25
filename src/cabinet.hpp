// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "arcade.hpp"
#include "operator_store.hpp"
#include <puzzle_fighter/source_cabinet_scenes.hpp>
#include <puzzle_fighter/source_idle_start.hpp>
#include <puzzle_fighter/source_attract_gameplay.hpp>
#include <puzzle_fighter/source_title_scene.hpp>
namespace puzzle_fighter::prototype {
// Finite source owners replace the known boot/idle/title jobs, with the source
// IRQ clear/fade continuations. This is not a generic coroutine/CPU scheduler.
struct NativeCabinetSession {
 static constexpr auto g=Game::globals;Game&game;NativeArcadeSession&arcade;Memory&m;MatchScheduler&scheduler;
 SourceBoot<Memory>boot;SourceIdleStart<Memory,NativeCabinetSession>start;SourceIdleNotices<Memory,NativeCabinetSession>notices;
 SourceAttractGameplay<Memory,NativeCabinetSession>attract;SourceTitleScene<Memory,NativeCabinetSession>title;
 SourceScoreboard<Memory,NativeCabinetSession>scores;
 enum class Owner {inactive,boot,idle,paid_title,arcade};Owner owner=Owner::inactive;
 enum class Wait {none,boot_fade_out,boot_initial_clear,boot_fade_in,boot_scene_clear,idle_initial_irq,idle_scene_clear,start_clear,paid_initial_irq};Wait wait=Wait::none;
 enum class IdleResume {none,tutorial,score};IdleResume idle_resume=IdleResume::none;
 NativeOperatorStore operator_store;bool credit_refresh_pending=false;unsigned credit_refreshes=0;
 void run_credit_display_job(){if(credit_refresh_pending){credit_refresh_pending=false;draw_source_credit_totals(g,m);++credit_refreshes;}}
 unsigned cold_starts=0,soft_returns=0,idle_cycles=0,accepted_starts=0,seed_injections=0;
 bool retired=false,title_child_active=false;
 NativeCabinetSession(Game&game_,NativeArcadeSession&a,MatchScheduler&s):game(game_),arcade(a),m(game.memory),scheduler(s),boot{m,g},start{m,*this,g},notices{m,*this,g},attract{m,*this,g},title{m,*this,g},scores{m,*this,g}{}
 bool owns_frame()const{return owner!=Owner::inactive&&owner!=Owner::arcade;}
 void stop(){owner=Owner::inactive;wait=Wait::none;credit_refresh_pending=false;}
 void palette_frame(){arcade.finish_auxiliary_jobs();scheduler.irq_ready=false;}
 void finish(){++scheduler.completed_main_cycles;arcade.sync_labels();palette_frame();}
 void clear(Wait next){arcade.clear.start(g,m,arcade);wait=next;}
 void cold(){
  owner=Owner::boot;wait=Wait::none;arcade.active=true;arcade.terminal=false;arcade.return_to_idle=false;arcade.player_job=false;arcade.entry_pending=false;arcade.wait=NativeArcadeSession::Wait::none;arcade.fade={};game.source_arcade=true;game.settings.legacy=false;game.sprite_frame={};game.board_objects.clear();game.pair_objects.clear();game.unrecovered_task_kinds.clear();m.accesses=0;
  // Adapter supplies the successful blank board-test result, absent EEPROM,
  // inactive statistics/card and no service input. All game-owned defaults
  // below are original producers, not a captured startup state.
  m.video.fill(0); // Native power-on backing memory, independent of quick scenes.
  clear_source_board_ram(m);initialize_source_empty_tile_pages(m);initialize_source_board_mirrors(g,m);initialize_source_sound_queue(g,m);initialize_source_base_palettes(m);initialize_source_tile_work_areas(m);queue_sound_request(g,m,{0xff00,0,0});initialize_source_leaderboard(g,m);m.write_word(g+0x90,2);m.write_byte(g+0x86,0);initialize_source_operator_defaults(g,m);operator_store.cold(g,m);apply_source_operator_settings(g,m);
  game.settings.difficulty=m.read_byte(g+0xa0);game.settings.damage=m.read_byte(g+0xa1);game.settings.speed=m.read_byte(g+0xde);game.settings.rounds_cpu=m.read_byte(g+0xa2)*2-1;game.settings.rounds_human=m.read_byte(g+0xa3)*2-1;boot.pending=decltype(boot)::Resume::none;start.pending=decltype(start)::Pending::none;attract.awaiting_clear=false;attract.howto.reset();scores.clearing=false;retired=false;title_child_active=false;++cold_starts;
  arcade.fade.request(g,m);wait=Wait::boot_fade_out;palette_frame();
 }
 void idle_entry(){owner=Owner::idle;initialize_source_idle_state(g,m);wait=Wait::idle_initial_irq;start.pending=decltype(start)::Pending::none;arcade.player_job=false;arcade.return_to_idle=false;}
 void return_from_arcade(){if(!arcade.return_to_idle)throw std::runtime_error("Missing source soft-return handoff");++soft_returns;idle_entry();}
 void random_tail(){const auto r=global_random(m.read_word(g+0xae));m.write_word(g+0xae,r.seed);}
 void idle_tail(){match_output_bits(g,m);const auto result=start.idle_credit_gate();if(result==SourceIdleStartResult::clear_screen){clear(Wait::start_clear);palette_frame();return;}if(result!=SourceIdleStartResult::complete)throw std::runtime_error("Idle source tail state");random_tail();finish();}
 void paid_frame(){m.write_byte(g+0x81,m.read_byte(g+0x81)+1);m.write_byte(g+0xcd,0x1f);const auto result=start.paid_title_tick();if(result==SourceIdleStartResult::clear_screen){clear(Wait::start_clear);palette_frame();return;}if(result!=SourceIdleStartResult::complete)throw std::runtime_error("Paid title source state");match_output_bits(g,m);start.paid_title_prompts();random_tail();finish();}
 void boot_frame(){const auto phase=m.read_word(g);const auto result=boot.run();if(!phase)game.seed=m.read_word(g+0xae);if(result==SourceBootResult::wait_object_clear){clear(Wait::boot_scene_clear);palette_frame();return;}if(result!=SourceBootResult::complete)throw std::runtime_error("Boot source state");match_output_bits(g,m);finish();}
 void idle_frame(){
  m.write_byte(g+0x81,m.read_byte(g+0x81)+1);m.write_byte(g+0xcd,0x1f);const auto scene=m.read_word(g);
  if(scene==20)title.intro();else if(scene==40)title.still();else if(scene==24){if(attract.tutorial()==SourceAttractResult::clear_screen){idle_resume=IdleResume::tutorial;clear(Wait::idle_scene_clear);palette_frame();return;}}
  else if(scene==32)attract.cpu_demo();else if(scene==28||scene==44){const auto result=scores.run();if(result==ScoreResult::unsupported)throw std::runtime_error("Idle score scene state");if(result==ScoreResult::wait_object_clear){idle_resume=IdleResume::score;clear(Wait::idle_scene_clear);palette_frame();return;}}
  else{const auto result=notices.run();if(result==IdleNoticeResult::unsupported)throw std::runtime_error("Idle source scene "+std::to_string(scene));if(scene==48)++idle_cycles;if(result==IdleNoticeResult::wait_object_clear){idle_resume=IdleResume::none;clear(Wait::idle_scene_clear);palette_frame();return;}}
  idle_tail();
 }
 void frame(){
  if(wait!=Wait::none){const auto resumed=wait;
   if(resumed==Wait::boot_fade_out||resumed==Wait::boot_fade_in){if(!arcade.fade.resume_after_irq(g,m)){palette_frame();return;}wait=Wait::none;if(resumed==Wait::boot_fade_out){clear(Wait::boot_initial_clear);palette_frame();return;}boot_frame();return;}
   if(resumed==Wait::idle_initial_irq){wait=Wait::none;idle_frame();return;}
   if(resumed==Wait::paid_initial_irq){wait=Wait::none;start.resume_after_irq();paid_frame();return;}
   if(!arcade.clear.resume_after_irq(g,m,arcade)){palette_frame();return;}wait=Wait::none;
   if(resumed==Wait::boot_initial_clear){arcade.fade.request_in(g,m);wait=Wait::boot_fade_in;palette_frame();return;}
   if(resumed==Wait::boot_scene_clear){const auto result=boot.resume_after_clear();if(result==SourceBootResult::idle){idle_entry();palette_frame();return;}if(result!=SourceBootResult::complete)throw std::runtime_error("Boot continuation state");match_output_bits(g,m);finish();return;}
   if(resumed==Wait::idle_scene_clear){if(idle_resume==IdleResume::tutorial)attract.resume_tutorial_after_clear();else if(idle_resume==IdleResume::score&&scores.resume_after_clear()!=ScoreResult::complete)throw std::runtime_error("Idle score continuation");idle_resume=IdleResume::none;idle_tail();return;}
   if(resumed==Wait::start_clear){retired=false;if(start.resume_after_clear()!=SourceIdleStartResult::retired||!retired)throw std::runtime_error("Start source retirement state");palette_frame();return;}
   throw std::runtime_error("Cabinet wait state");
  }
  if(owner==Owner::boot)boot_frame();else if(owner==Owner::idle)idle_frame();else if(owner==Owner::paid_title)paid_frame();else throw std::runtime_error("Cabinet owner state");
 }
 std::uint32_t allocate_first(){return arcade.allocate_first();}std::uint32_t allocate_third(){return arcade.allocate_third();}void actor_step(std::uint32_t a){arcade.actor_step(a);}
 void action(ScoreAction a){arcade.action(a);}
 void action(IdleNoticeAction a,unsigned x=0,unsigned y=0){switch(a){
  case IdleNoticeAction::palette:source_idle_palettes(g,m);break;case IdleNoticeAction::stop_sound:queue_sound_request(g,m,{0xff00,0,0});break;case IdleNoticeAction::reset_pools:clear_scene_records(g,m,true);break;
  case IdleNoticeAction::fade_in:arcade.fade.request_in_background(g,m);break;case IdleNoticeAction::fade_out:arcade.fade.request_out_background(g,m);break;
  case IdleNoticeAction::wide_picture:arcade.selection.draw_picture_wide(x,y);break;case IdleNoticeAction::large_picture:draw_source_large_picture(m,x,y);break;
  case IdleNoticeAction::logo_sound:arcade.cue(0x9f);break;
  case IdleNoticeAction::notice_song:if(!m.read_byte(g+0xa6)){queue_sound_request(g,m,{0xff05,0,0});queue_sound_request(g,m,{30,0,0},true);queue_sound_request(g,m,{31,0x11000,0x80},true);}break;
 }}
 void action(SourceIdleStartAction a,unsigned x=0){switch(a){
  case SourceIdleStartAction::credit_cue:arcade.credit_cue();break;
  case SourceIdleStartAction::title_setup:title.prompt();break;case SourceIdleStartAction::title_picture:title.picture();break;
  case SourceIdleStartAction::show_credit_totals:draw_source_credit_totals(g,m);break;case SourceIdleStartAction::prompt:draw_source_notice(m,x);break;
  case SourceIdleStartAction::first_pool:game.source_first_pool();break;case SourceIdleStartAction::objects:game.source_packet_output();break;
  case SourceIdleStartAction::persist_operator_counter:operator_store.persist(g,m);break;
  case SourceIdleStartAction::stop_title_child:if(title_child_active)throw std::runtime_error("Unbound active title child");break;
  case SourceIdleStartAction::start_paid_title:owner=Owner::paid_title;if(start.paid_title_begin()!=SourceIdleStartResult::wait_irq)throw std::runtime_error("Paid title entry state");wait=Wait::paid_initial_irq;break;
  case SourceIdleStartAction::start_credit_job:arcade.active=true;arcade.player_job=true;break;
  case SourceIdleStartAction::start_arcade_job:if(!game.settings.random_seed){m.write_word(g+0xae,game.settings.chosen_seed);++seed_injections;}game.seed=m.read_word(g+0xae);++accepted_starts;arcade.adopt_source_start();owner=Owner::arcade;break;
  case SourceIdleStartAction::retire_current_job:retired=true;if(owner!=Owner::paid_title&&owner!=Owner::arcade)throw std::runtime_error("Missing replacement source owner");break;
 }}
 void action(SourceAttractAction a,unsigned x=0,unsigned y=0){switch(a){
  case SourceAttractAction::idle_credit_marker:allocate_source_idle_credit_marker(g,m);break;case SourceAttractAction::initialize_players:initialize_source_attract_players(g,m,game);break;
  case SourceAttractAction::cpu_start_skill:ClassicCpu<Memory,Effects>(m,game.effects,g,g+0x300).start_skill();break;case SourceAttractAction::cpu_match_level:ClassicCpu<Memory,Effects>(m,game.effects,g,g+0x300).level_for_match();break;
  case SourceAttractAction::round_music:start_source_round_music(g,m);break;case SourceAttractAction::stop_music:queue_sound_request(g,m,{0xff05,0,0});break;case SourceAttractAction::reset_music:queue_sound_request(g,m,{0xff01,0,0});break;case SourceAttractAction::tutorial_music:arcade.music(13);break;
  case SourceAttractAction::scene_palettes:source_attract_scene_palettes(g,m);break;case SourceAttractAction::tutorial_palettes:source_idle_palettes(g,m);break;
  case SourceAttractAction::demo_text:initialize_stage_actors(g,m);break;case SourceAttractAction::stage_setup:initialize_source_scene_picture(g,m);break;case SourceAttractAction::round_labels:source_round_labels(g,m);break;case SourceAttractAction::fade_in:arcade.fade.request_in_background(g,m);break;
  case SourceAttractAction::text_page:arcade.selection.draw_picture(x,y);break;case SourceAttractAction::background_page:arcade.selection.draw_picture_wide(x,y);break;
  case SourceAttractAction::turns:game.tick({},false,true);break;case SourceAttractAction::stopped_turns:game.stopped_work();break;case SourceAttractAction::reactions:game.source_reactions();break;case SourceAttractAction::characters:game.source_characters();break;
  case SourceAttractAction::first_pool:game.source_first_pool();break;case SourceAttractAction::second_pool:game.source_second_pool();break;case SourceAttractAction::third_pool:game.source_third_pool();break;case SourceAttractAction::music_control:source_music_tempo(g,m);break;case SourceAttractAction::stage_tick:stage_frame(g,m);break;case SourceAttractAction::gem_pictures:build_gem_pictures(g,m);break;case SourceAttractAction::objects:game.source_packet_output();break;
 }}
 void action(SourceTitleAction a,unsigned x=0,unsigned y=0){switch(a){
  case SourceTitleAction::prompt:draw_source_notice(m,x);break;case SourceTitleAction::title_palettes:source_title_palettes(g,m);break;
  case SourceTitleAction::palette_background:source_title_palette_bank(g,m,SourceTitlePaletteBank::objects);break;case SourceTitleAction::palette_foreground:source_title_palette_bank(g,m,SourceTitlePaletteBank::scroll1);break;case SourceTitleAction::palette_logo:source_title_palette_bank(g,m,SourceTitlePaletteBank::scroll3);break;case SourceTitleAction::palette_characters:source_title_palette_bank(g,m,SourceTitlePaletteBank::scroll2);break;
  case SourceTitleAction::fill_title_map:fill_source_large_tile_page(m,0,29,x);break;case SourceTitleAction::background_page:arcade.selection.draw_picture_wide(x,y);break;case SourceTitleAction::intro_music:arcade.music(12);break;case SourceTitleAction::title_cue:arcade.cue(0xa8+(m.read_word(g+0x90)!=0));break;case SourceTitleAction::stop_music:queue_sound_request(g,m,{0xff05,0,0});break;case SourceTitleAction::reset_music:queue_sound_request(g,m,{0xff01,0,0});break;
  case SourceTitleAction::idle_credit_marker:allocate_source_idle_credit_marker(g,m);break;case SourceTitleAction::fade_in:arcade.fade.request_in_background(g,m);break;case SourceTitleAction::stage_setup:initialize_source_scene_picture(g,m);break;case SourceTitleAction::stage_tick:stage_frame(g,m);break;case SourceTitleAction::demo_text:initialize_stage_actors(g,m);break;case SourceTitleAction::round_labels:source_round_labels(g,m);break;
  case SourceTitleAction::arcade_palettes:initialize_arcade_entry_palettes(g,m);break;case SourceTitleAction::initialize_players:initialize_source_attract_players(g,m,game);break;case SourceTitleAction::cpu_start_skill:ClassicCpu<Memory,Effects>(m,game.effects,g,g+0x300).start_skill();break;case SourceTitleAction::cpu_match_level:ClassicCpu<Memory,Effects>(m,game.effects,g,g+0x300).level_for_match();break;
  case SourceTitleAction::text_clear:clear_source_text_plane(m);break;case SourceTitleAction::turns:game.tick({},false,true);break;case SourceTitleAction::reactions:game.source_reactions();break;case SourceTitleAction::characters:game.source_characters();break;case SourceTitleAction::first_pool:game.source_first_pool();break;case SourceTitleAction::second_pool:game.source_second_pool();break;case SourceTitleAction::third_pool:game.source_third_pool();break;case SourceTitleAction::gem_pictures:build_gem_pictures(g,m);break;case SourceTitleAction::objects:game.source_packet_output();break;
 }}
};
} // namespace puzzle_fighter::prototype
