// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game.hpp"
#include "match.hpp"
#include <puzzle_fighter/arcade_entry.hpp>
#include <puzzle_fighter/arcade_match_flow.hpp>
#include <puzzle_fighter/arcade_match_intro.hpp>
#include <puzzle_fighter/arcade_name_entry.hpp>
#include <puzzle_fighter/arcade_player_picture.hpp>
#include <puzzle_fighter/source_selection.hpp>
#include <puzzle_fighter/scene_overlays.hpp>
#include <puzzle_fighter/scene_palette_fade.hpp>
#include <puzzle_fighter/match_scheduler.hpp>
#include <puzzle_fighter/source_ending.hpp>
#include <puzzle_fighter/source_easy_ending.hpp>
#include <puzzle_fighter/source_scoreboard.hpp>
#include <puzzle_fighter/source_cabinet_scenes.hpp>
namespace puzzle_fighter::prototype {
// Source scene owners with typed continuations. Host choices are initial seed,
// operator values, virtual credits and the terminal ending/attract handoff.
struct NativeArcadeSession {
 static constexpr auto g=Game::globals;
 Game&game;Match&match;MatchScheduler&scheduler;Memory&m;
 ArcadeEntry<Memory,NativeArcadeSession> entry;
 SourceSelection<Memory,NativeArcadeSession> selection;
 ArcadeOrderContext retained_order{};
 ArcadeMatchIntro<Memory,NativeArcadeSession> introduction;
 ArcadeMatchFlow<Memory,NativeArcadeSession> flow;
 SourceEnding<Memory,NativeArcadeSession> ending;
 SourceEasyEnding<Memory,NativeArcadeSession> easy_ending;
 SourceScoreboard<Memory,NativeArcadeSession> scoreboard;
 MatchObjectClear clear;SourceScenePaletteFade fade;std::function<void()> credit_display_job;
 void finish_auxiliary_jobs(){if(credit_display_job)credit_display_job();fade.after_main(g,m);}
 enum class Wait {none,entry_clear,selection_clear,flow_clear,palette,ending_clear,easy_clear,score_clear,return_clear};Wait wait=Wait::none;
 bool active=false,terminal=false,player_job=true,return_to_idle=false,entry_pending=false;ArcadeScene destination=ArcadeScene::other;
 unsigned completed_matches=0,selection_visits=0,continue_visits=0,join_events=0;
 explicit NativeArcadeSession(Game&game_,Match&match_,MatchScheduler&s):game(game_),match(match_),scheduler(s),m(game.memory),entry{m,*this,g},selection{m,*this,g},introduction{m,*this,g},flow{m,*this,g},ending{m,*this,g},easy_ending{m,*this,g},scoreboard{m,*this,g}{}
 void stop(){active=false;game.source_arcade=false;wait=Wait::none;fade={};terminal=false;return_to_idle=false;}
 void begin(unsigned players=1){
  active=true;terminal=false;player_job=true;game.source_arcade=true;wait=Wait::none;fade={};completed_matches=selection_visits=continue_visits=join_events=0;flow.continuation=decltype(flow)::Continuation::none;entry.awaiting_object_clear=false;entry.order={};retained_order={};
  m.accesses=0;m.write_word(g,0);m.write_word(g+4,0);m.write_word(g+8,0);m.write_byte(g+0x83,0);m.write_byte(g+0x86,0);m.write_byte(g+0xa8,std::uint8_t(players));m.write_byte(g+0x98,1);m.write_byte(g+0xa4,1);m.write_byte(g+0xa5,0);m.write_byte(g+0x9c,0);m.write_byte(g+0x9d,0);m.write_byte(g+0x9e,0);m.write_byte(g+0x76,3);m.write_byte(g+0xce,0);m.write_byte(g+0x4781,0);m.write_byte(g+0x4782,0);m.write_word(g+0xae,game.seed);m.write_word(g+0x90,2);m.write_word(g+0x92,2);
  // SourceA03C owns all player/task/scene initialization after this host entry.
  if(entry.begin()!=ArcadeEntryResult::clear_screen)throw std::runtime_error("Original arcade entry rejected");
  clear.start(g,m,*this);wait=Wait::entry_clear;match=Match{};
 }
 // Source2988 creates credit job80 before mainA0 in the same supervisor pass.
 //9FB8 then waits one IRQ before the first main prologue/A03C.
 void adopt_source_start(){active=true;terminal=false;return_to_idle=false;player_job=true;game.source_arcade=true;wait=Wait::none;fade={};entry.awaiting_object_clear=false;entry.order={};retained_order={};flow.continuation=decltype(flow)::Continuation::none;ending.pending=decltype(ending)::Resume::none;easy_ending.clearing=false;scoreboard.clearing=false;match=Match{};before_main();initialize_source_arcade_job_before_irq(g,m);entry_pending=true;}
 void credit(){auto value=m.read_byte(g+0x76);if(value<0x99)m.write_byte(g+0x76,std::uint8_t(value+((value&15)==9?7:1)));}
 void object_buffer_write(std::uint32_t,std::uint32_t){game.sprite_frame={};game.board_objects.clear();game.pair_objects.clear();}
 std::uint32_t allocate_first(){return allocate_scene_first(g,m);}std::uint32_t allocate_third(){return allocate_scene_third(g,m);}
 void actor_act(std::uint32_t a,unsigned index){const auto at=0x143f6+m.read_byte(a+0x82)*4;const auto table=(std::uint32_t(m.opcode_word(at))<<16)|m.opcode_word(at+2);game.task_helpers().init(a,index,table);}
 void actor_step(std::uint32_t a){game.task_helpers().step(a);}
 bool overlay(unsigned kind,std::uint32_t caller){return request_scene_overlay(g,m,kind,caller);}
 void player_join_accounting(){if(m.read_byte(g+0x86))throw std::runtime_error("Active original cabinet statistics unbound");}
 void cue(std::uint32_t number,std::uint32_t pan=0x1000){queue_sound_request(g,m,{number,pan,0},true);}
 void music(unsigned song){if(m.read_byte(g+0xa6))return;queue_sound_request(g,m,{0xff05,0,0});queue_sound_request(g,m,{song,0,0},true);}
 void credit_cue(){queue_sound_request(g,m,{0xff05,0,0});queue_sound_request(g,m,{0x90,0x1000,0});}
 void character_act_with_events(std::uint32_t actor,unsigned index){auto h=game.task_helpers();CharacterController<Memory,decltype(h)>c{m,h,g};c.actor=actor;c.animation(index);}
 void actor_init(std::uint32_t actor,unsigned index,std::uint32_t table){game.task_helpers().init(actor,index,table);}
 void action(EndingAction a,unsigned x=0,unsigned y=0){switch(a){
  case EndingAction::reset_pools:clear_scene_records(g,m,false);break;
  case EndingAction::reset_first_pool:initialize_task_pool(g,m);break;
  case EndingAction::first_pool:game.source_first_pool();break;case EndingAction::second_pool:game.source_second_pool();break;case EndingAction::objects:game.source_packet_output();break;
  case EndingAction::blank_scroll2:for(unsigned n=0;n<0x4000;n+=4)m.write_long(0x904000+n,0x58200000);break;
  case EndingAction::ending_song:music(0x17);break;case EndingAction::stop_and_voice:credit_cue();break;
  case EndingAction::stage_picture:initialize_source_scene_picture(g,m);break;case EndingAction::stage_tick:stage_frame(g,m);break;case EndingAction::stage_actors:initialize_stage_actors(g,m);break;
  case EndingAction::all_palettes:introduction.intro_palettes();break;
  case EndingAction::interlude_scene:if(!tick_stage_interlude_scene(g,m))throw std::runtime_error("Ending interlude state");break;
  case EndingAction::palette_actors:allocate_source_ending_palette_actors(g,m,g+0x2900);break;
  case EndingAction::caption:draw_source_notice(m,x);break;case EndingAction::closing_mark:draw_source_closing_mark(m,x,y);break;
  case EndingAction::packed_sound:queue_sound_request(g,m,{(x&0xffff0000u)|0xff07u,(x&255u)<<24,0});break;
  case EndingAction::easy_ending:throw std::runtime_error("Short ending must use its source owner");
 }}
 void action(EasyEndingAction a,unsigned x=0,unsigned y=0){switch(a){
  case EasyEndingAction::reset_pools:clear_scene_records(g,m,false);break;
  case EasyEndingAction::wide_picture:selection.draw_picture_wide(x,y);break;case EasyEndingAction::large_picture:draw_source_large_picture(m,x,y);break;
  case EasyEndingAction::all_palettes:introduction.intro_palettes();break;case EasyEndingAction::song:music(0x11);break;
  //97DC precedes the first pool allocation: preceding149DA leaves its scratch
  // owner atg3300, both hidden players skip14B66, and+81 is source-cleared.
  case EasyEndingAction::voice:if(!m.read_byte(g+0xbc))cue(0x25f,0xc00);break;
  case EasyEndingAction::first_pool:game.source_first_pool();break;case EasyEndingAction::third_pool:game.source_third_pool();break;case EasyEndingAction::objects:game.source_packet_output();break;
  case EasyEndingAction::large_map_palette:introduction.stage_map_palette(true);break;
 }}
 void action(ScoreAction a){switch(a){
  case ScoreAction::reset_pools:clear_scene_records(g,m,true);break;case ScoreAction::reset_pools_retained:clear_scene_records(g,m,false);break;
  case ScoreAction::score_song:music(0x15);break;case ScoreAction::score_reveal_song:music(0x16);break;
  case ScoreAction::stage_picture:initialize_source_scene_picture(g,m);break;case ScoreAction::stage_tick:stage_frame(g,m);break;case ScoreAction::score_palettes:source_scene_object_palettes(g,m,true);break;
  case ScoreAction::gem_coordinates:allocate_source_idle_credit_marker(g,m);break;case ScoreAction::stage_actors:initialize_stage_actors(g,m);break;
  case ScoreAction::first_pool:game.source_first_pool();break;case ScoreAction::second_pool:game.source_second_pool();break;case ScoreAction::third_pool:game.source_third_pool();break;case ScoreAction::objects:game.source_packet_output();break;
  case ScoreAction::fade_in:fade.request_in_background(g,m);break;case ScoreAction::fade_out:fade.request_out_background(g,m);break;
 }}
 void action(SelectionAction a,std::uint32_t=0,std::uint32_t=0){switch(a){
  case SelectionAction::reset_pools:clear_scene_records(g,m,false);break;
  case SelectionAction::scene_actor_setup:initialize_selection_scene(g,m,*this);break;
  case SelectionAction::first_pool:game.source_first_pool();break;case SelectionAction::third_pool:game.source_third_pool();break;case SelectionAction::objects:game.source_packet_output();break;
  case SelectionAction::end_fade:finish_scene_fade_screen(g,m);break;
  case SelectionAction::character_accounting:if(m.read_byte(g+0x86)&&m.read_byte(g+0xbf))throw std::runtime_error("Active selection statistics unbound");break;
 }}
 void action(MatchIntroAction a){switch(a){
  case MatchIntroAction::initialize_players:initialize_source_players(g,m,game);++game.restarts;break;
  case MatchIntroAction::cpu_match_level:ClassicCpu<Memory,Effects>(m,game.effects,g,Game::players[1]).level_for_match();break;
  case MatchIntroAction::stage_setup:initialize_source_scene_picture(g,m);break;
  case MatchIntroAction::round_labels:source_round_labels(g,m);break;case MatchIntroAction::course_label:source_course_label(g,m);break;
  case MatchIntroAction::announcements_and_stage_actors:allocate_round_announcements(g,m);initialize_stage_actors(g,m);break;
  case MatchIntroAction::first_pool:game.source_first_pool();break;case MatchIntroAction::second_pool:game.source_second_pool();break;
  case MatchIntroAction::stage_tick:stage_frame(g,m);break;case MatchIntroAction::objects:game.source_packet_output();break;
 }}
 void action(ArcadeAction a,std::uint32_t x=0,std::uint32_t y=0,std::uint32_t z=0){switch(a){
  case ArcadeAction::reset_pools:clear_scene_records(g,m,false);break;case ArcadeAction::reset_third_pool:initialize_attack_display_pool(g,m);break;
  case ArcadeAction::first_pool:game.source_first_pool();break;case ArcadeAction::second_pool:game.source_second_pool();break;case ArcadeAction::third_pool:game.source_third_pool();break;case ArcadeAction::stage_tick:stage_frame(g,m);break;case ArcadeAction::objects:game.source_packet_output();retained_order=arcade_order_after_object_composition(g,m);break;
  case ArcadeAction::show_totals:introduction.show_totals();break;case ArcadeAction::clear_totals:introduction.clear_totals();break;case ArcadeAction::loser_colours:introduction.loser_colours(y);break;case ArcadeAction::quote_init:initialize_source_quote_extra(g,m);break;
  case ArcadeAction::award_message:case ArcadeAction::award_accounting:throw std::runtime_error("Active award hardware boundary");
  case ArcadeAction::match_end_song:source_post_match_jingle(g,m);break;case ArcadeAction::character_act:character_act_with_events(y,x);break;
  case ArcadeAction::stage_setup:initialize_source_scene_picture(g,m);break;case ArcadeAction::gem_coordinates:initialize_stage_actors(g,m);break;
  case ArcadeAction::scene_palettes:introduction.common_palette();break;case ArcadeAction::scene_prepare:if(!initialize_stage_interlude_palettes(g,m))throw std::runtime_error("Interlude palette state");break;
  case ArcadeAction::scene_static:introduction.stage_object_palette();break;case ArcadeAction::scene_aux_palette:introduction.stage_map_palette(false);break;case ArcadeAction::scene_common_palette:introduction.stage_map_palette(true);break;
  case ArcadeAction::scene_tick:if(!tick_stage_interlude_scene(g,m))throw std::runtime_error("Interlude scene state");break;case ArcadeAction::scene_caption:draw_stage_interlude_caption(x,m);break;
  case ArcadeAction::interlude_song:music(0x11);break;case ArcadeAction::sound_command:queue_sound_request(g,m,{x,0,0});break;
  case ArcadeAction::show_characters:selection.ShowCharacters();break;case ArcadeAction::selection_palette:selection.selection_palettes();break;case ArcadeAction::bring_back_characters:selection.BringBackAfterMatch();break;
  case ArcadeAction::draw_picture:selection.draw_picture(x,y);break;case ArcadeAction::draw_stage_number:selection.draw_stage_number(x,y);break;case ArcadeAction::draw_ladder:selection.DrawLadder(0x800);break;
  case ArcadeAction::stage_spin:selection.SpinStage();break;case ArcadeAction::stage_opponent:selection.ShowStageOpponent(0x800);break;case ArcadeAction::ladder_spin:selection.SpinLadder();break;case ArcadeAction::last_opponent:selection.ShowAfterLadder();break;
  case ArcadeAction::selection_start_sound:cue(0xe4);break;
  case ArcadeAction::selection_done_sound:{const auto actor=selection.active_character,index=unsigned(m.read_byte(actor+0x82))+(m.read_word(g+0x90)?16u:0u);cue(std::uint16_t(m.opcode_word(0x2b928+index*2)+(m.read_byte(actor+0x81)?16:0)));break;}
  case ArcadeAction::last_opponent_sound:if(!m.read_byte(g+0xbc))cue(0x262);break;case ArcadeAction::scene_actor_setup:initialize_selection_scene(g,m,*this);break;
  case ArcadeAction::continue_palette:introduction.player_palettes();break;case ArcadeAction::continue_sound:cue(0x9d);break;case ArcadeAction::continue_song:music(0x12);break;
  case ArcadeAction::copy_palette:introduction.copy_palette(x,y,z+1);break;case ArcadeAction::gameover_song:music(0x14);break;case ArcadeAction::gameover_palette:introduction.gameover_palette();break;case ArcadeAction::gameover_voice:cue(0x9c);break;
  case ArcadeAction::kill_job:if(x!=0x80)throw std::runtime_error("Unbound source job kill");player_job=false;break;case ArcadeAction::begin_palette_fade:fade.request(g,m);break;
  case ArcadeAction::give_ladder:if(m.read_byte(g+0x86)&&m.read_byte(g+0xbf))throw std::runtime_error("Active ladder statistics unbound");break;
  case ArcadeAction::join_credit_used:if(m.read_byte(g+0x9c))throw std::runtime_error("Freeplay EEPROM service outside virtual-credit profile");break;
  case ArcadeAction::join_player_reset:for(unsigned offset:{4u,0xd0u,0xd4u})m.write_long(y+offset,0);for(unsigned offset=0x100;offset<0x400;offset+=4)m.write_long(y+offset,0);++join_events;break;
  case ArcadeAction::player_join_accounting:player_join_accounting();break;case ArcadeAction::join_clear_fields:ClassicCpu<Memory,Effects>(m,game.effects,g,y).skill_after_continue();break;case ArcadeAction::join_cue:music(0x13);break;
  case ArcadeAction::player_prompt:draw_arcade_player_prompt(y,g,m,x);break;case ArcadeAction::player_prompt_clear:draw_arcade_player_prompt(y,g,m,0);break;
  case ArcadeAction::player_gameover_prompt:if(!tick_arcade_name_entry(y,g,m))throw std::runtime_error("Name-entry state");break;case ArcadeAction::player_countdown:draw_arcade_continue_digits(y,g,m);break;
  case ArcadeAction::end_fade_screen:finish_scene_fade_screen(g,m);break;
 }}
 void sync_labels(){for(unsigned side=0;side<2;++side)game.settings.characters[side]=m.read_byte(Game::players[side]+0x82);game.settings.course=m.read_byte(g-0x7374);game.settings.opponent=((m.read_byte(g+0xac)|m.read_byte(g+0x8d))&3)==3?0:2;}
 void before_main(){if(active&&!terminal&&player_job&&!ArcadePlayerFlow<Memory,NativeArcadeSession>{m,*this,g,0}.run())throw std::runtime_error("Original player-job state");}
 bool normal_match()const{return active&&!terminal&&wait==Wait::none&&m.ram[0x8004]==0&&m.ram[0x8005]==4&&m.ram[0x8008]==0&&m.ram[0x8009]==4;}
 void source_finish(){match_output_bits(g,m);if(m.read_byte(g+0x86))throw std::runtime_error("Active main accounting unbound");++scheduler.completed_main_cycles;scheduler.irq_ready=false;finish_auxiliary_jobs();}
 void after_normal(){sync_labels();finish_auxiliary_jobs();}
 void match_over(){flow.match_over();++completed_matches;}
 void source_result(ArcadeFlowResult result){if(result==ArcadeFlowResult::unsupported)throw std::runtime_error("Unsupported source arcade scene state");if(result==ArcadeFlowResult::wait_palette_fade)wait=Wait::palette;else if(result==ArcadeFlowResult::clear_screen){clear.start(g,m,*this);wait=Wait::flow_clear;}}
 void frame(){
  if(wait!=Wait::none){
   if(wait==Wait::palette){if(!fade.resume_after_irq(g,m)){finish_auxiliary_jobs();return;}wait=Wait::none;source_result(flow.resume_after_palette_fade());}
   else{if(!clear.resume_after_irq(g,m,*this)){finish_auxiliary_jobs();return;}const auto resumed=wait;wait=Wait::none;if(resumed==Wait::entry_clear){if(entry.resume_after_object_clear()!=ArcadeEntryResult::complete)throw std::runtime_error("Entry resume state");}else if(resumed==Wait::selection_clear){if(selection.resume_after_object_clear()!=SelectionResult::complete)throw std::runtime_error("Selection resume state");}else if(resumed==Wait::ending_clear){if(ending.resume_after_clear()!=EndingResult::complete)throw std::runtime_error("Ending clear resume");}else if(resumed==Wait::easy_clear){if(easy_ending.resume_after_clear()!=EasyEndingResult::complete)throw std::runtime_error("Easy ending clear resume");}else if(resumed==Wait::score_clear){if(scoreboard.resume_after_clear()!=ScoreResult::complete)throw std::runtime_error("Score clear resume");}else if(resumed==Wait::return_clear){m.write_byte(g+0xb0,0);return_to_idle=true;player_job=false;finish_auxiliary_jobs();return;}else source_result(flow.resume_after_object_clear());}
   if(wait==Wait::none)source_finish();else finish_auxiliary_jobs();return;
  }
  main_frame_begin(g,m);copy_player_inputs(g,m);const bool join_cleanup=m.read_byte(g+0x2900)&&m.read_byte(g+0x2902)==2&&m.read_byte(g+0x2904)>=4;auto h=game.task_helpers();if(!tick_scene_overlay(g,m,h))throw std::runtime_error("Arcade overlay state");
  if(join_cleanup)retained_order=arcade_order_after_join_overlay_cleanup(m);
  if(entry_pending){entry_pending=false;if(entry.begin()!=ArcadeEntryResult::clear_screen)throw std::runtime_error("Source paid entry state");clear.start(g,m,*this);wait=Wait::entry_clear;}
  else if(m.read_word(g+4)==0){if((retained_order.origin==ArcadeOrderContext::Origin::unavailable?entry.prepare_selection():entry.prepare_selection(retained_order))!=ArcadeEntryResult::complete)throw std::runtime_error("Selection preparation context");}
  else switch(arcade_scene(g,m)){
   case ArcadeScene::selection:{const auto result=selection.run();if(result==SelectionResult::unsupported)throw std::runtime_error("Original selection state");if(result==SelectionResult::wait_object_clear){clear.start(g,m,*this);wait=Wait::selection_clear;++selection_visits;}break;}
   case ArcadeScene::match_setup:if(!introduction.start())throw std::runtime_error("Original match setup rejected");match=Match{};break;
   case ArcadeScene::match_intro:if(!introduction.tick())throw std::runtime_error("Original versus introduction state");break;
   case ArcadeScene::post_match:case ArcadeScene::stage_interlude:case ArcadeScene::ladder:case ArcadeScene::continue_screen:case ArcadeScene::game_over:{const auto scene=arcade_scene(g,m);if(scene==ArcadeScene::continue_screen&&!m.read_word(g+8))++continue_visits;source_result(flow.run(scene));break;}
   case ArcadeScene::other:{if(m.read_word(g+4)!=14)throw std::runtime_error("Unsupported original arcade destination");const auto result=scoreboard.run();if(result==ScoreResult::unsupported)throw std::runtime_error("Scoreboard scene state");if(result==ScoreResult::wait_object_clear){clear.start(g,m,*this);wait=Wait::score_clear;}break;}
   case ArcadeScene::ending:{
    if(m.read_byte(g+0xbf)||!m.read_byte(g-0x7374)){const auto result=easy_ending.run();if(result==EasyEndingResult::unsupported)throw std::runtime_error("Short ending state");if(result==EasyEndingResult::wait_object_clear){clear.start(g,m,*this);wait=Wait::easy_clear;}}
    else{const auto result=ending.run();if(result==EndingResult::unsupported)throw std::runtime_error("Source ending state");if(result==EndingResult::wait_object_clear){clear.start(g,m,*this);wait=Wait::ending_clear;}}break;
   }
   case ArcadeScene::title_reset:for(unsigned o=0;o<24;o+=4)m.write_long(g+o,0);m.write_byte(g+0x4781,0);m.write_byte(g+0x4782,0);fade={};clear.start(g,m,*this);wait=Wait::return_clear;break;
   case ArcadeScene::playing:throw std::runtime_error("Arcade match must use original match scheduler");
  }
  sync_labels();
  if(wait==Wait::none)source_finish();else finish_auxiliary_jobs();
 }
};
} // namespace puzzle_fighter::prototype
