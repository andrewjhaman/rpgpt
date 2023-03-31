#pragma once
#include "buff.h"
#include "HandmadeMath.h" // vector types in entity struct definition
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h> // atoi
#include "characters.gen.h"
 NPC_Skeleton,
 NPC_MOOSE,
} NpcKind;

#define Log(...) { printf("%s Log %d | ", __FILE__, __LINE__); printf(__VA_ARGS__); }

// REFACTORING:: also have to update in javascript!!!!!!!!
#define MAX_SENTENCE_LENGTH 400 // LOOOK AT AGBOVE COMMENT GBEFORE CHANGING
typedef BUFF(char, MAX_SENTENCE_LENGTH) Sentence;
#define SENTENCE_CONST(txt) {.data=txt, .cur_index=sizeof(txt)}
#define SENTENCE_CONST_CAST(txt) (Sentence)SENTENCE_CONST(txt)

#define REMEMBERED_PERCEPTIONS 24

typedef enum PerceptionType
{
 Invalid, // so that zero value in training structs means end of perception
 PlayerAction,
 PlayerDialog,
 NPCDialog, // includes an npc action in every npc dialog. So it's often nothing
 PlayerHeldItemChanged,
} PerceptionType;

typedef struct Perception
{
 PerceptionType type;

 union 
 {
  // player action
  Action player_action_type;

  // player dialog
  Sentence player_dialog;

  // npc dialog
  struct
  {
   Action npc_action_type;
   Sentence npc_dialog;
  };

  // player holding item. MUST precede any perceptions which come after the player is holding the item
  ItemKind holding;
 };
} Perception;

typedef enum PropKind
{
 TREE0,
 TREE1,
 TREE2,
} PropKind;

typedef struct EntityRef
{
 int index;
 int generation;
} EntityRef;

typedef enum CharacterState
{
 CHARACTER_WALKING,
 CHARACTER_IDLE,
 CHARACTER_ATTACK,
 CHARACTER_TALKING,
} CharacterState;

typedef struct Entity
{
 bool exists;
 bool destroy;
 int generation;

 // fields for all gs.entities
 Vec2 pos;
 Vec2 vel; // only used sometimes, like in old man and bullet
 float damage; // at 1.0, dead! zero initialized
 bool facing_left;
 double dead_time;
 bool dead;
 // multiple gs.entities have a sword swing
 BUFF(EntityRef, 8) done_damage_to_this_swing; // only do damage once, but hitbox stays around

 bool is_bullet;

 // props
 bool is_prop;
 PropKind prop_kind;

 // items
 bool is_item;
 bool held_by_player;
 ItemKind item_kind;

 // npcs
 bool is_npc;
 bool perceptions_dirty;
 BUFF(Perception, REMEMBERED_PERCEPTIONS) remembered_perceptions;
 double character_say_timer;
 int characters_said;
 NpcKind npc_kind;
 ItemKind last_seen_holding_kind;
#ifdef WEB
 int gen_request_id;
#endif
 bool aggressive;
 bool walking;
 double shotgun_timer;
 bool moved;
 Vec2 target_goto;
 // only for skeleton npc
 double swing_timer;

 // character
 bool is_character;
 EntityRef holding_item;
 Vec2 to_throw_direction;
 int boots_modifier;
 CharacterState state;
 EntityRef talking_to; // Maybe should be generational index, but I dunno. No death yet
 bool is_rolling; // can only roll in idle or walk states
 double time_not_rolling; // for cooldown for roll, so you can't just hold it and be invincible
 double roll_progress;
 double swing_progress;
} Entity;

bool npc_is_knight_sprite(Entity *it)
{
 return it->is_npc && ( it->npc_kind == NPC_Max || it->npc_kind == NPC_Hunter || it->npc_kind == NPC_John || it->npc_kind == NPC_Blocky);
}

typedef BUFF(char, MAX_SENTENCE_LENGTH*(REMEMBERED_PERCEPTIONS+4)) PromptBuff;
typedef BUFF(Action, 8) AvailableActions;

void fill_available_actions(Entity *it, AvailableActions *a)
{
 *a = (AvailableActions){0};
 BUFF_APPEND(a, ACT_none);
 if(it->npc_kind == NPC_GodRock)
 {
  BUFF_APPEND(a, ACT_heals_player);
 }
 else
 {
  BUFF_APPEND(a, ACT_fights_player);
  if(npc_is_knight_sprite(it))
  {
   BUFF_APPEND(a, ACT_strikes_air);
  }
  if(it->npc_kind == NPC_Blocky)
  {
   if(!it->moved)
   {
    BUFF_APPEND(a, ACT_allows_player_to_pass);
   }
  }
 }
}

void process_perception(Entity *it, Perception p)
{
 if(p.type != NPCDialog) it->perceptions_dirty = true;
 if(!BUFF_HAS_SPACE(&it->remembered_perceptions)) BUFF_REMOVE_FRONT(&it->remembered_perceptions);
 BUFF_APPEND(&it->remembered_perceptions, p);
 if(p.type == PlayerHeldItemChanged)
 {
  it->last_seen_holding_kind = p.holding;
 }
 else if(p.type == NPCDialog)
 {
  if(p.npc_action_type == ACT_allows_player_to_pass)
  {
   it->target_goto = AddV2(it->pos, V2(-50.0, 0.0));
   it->moved = true;
  }
 }
}

#define printf_buff(buff_ptr, ...) { int written = snprintf((buff_ptr)->data+(buff_ptr)->cur_index, ARRLEN((buff_ptr)->data) - (buff_ptr)->cur_index, __VA_ARGS__); assert(written >= 0); (buff_ptr)->cur_index += written; };

// returns if action index was valid
bool action_from_index(Entity *it, Action *out, int action_index)
{
 AvailableActions available = {0};
 fill_available_actions(it, &available);
 if(action_index < 0 || action_index >= available.cur_index)
 {
  return false;
 }
 else
 {
  *out = available.data[action_index];
  return true;
 }
}

// don't call on untrusted action, doesn't return error
int action_to_index(Entity *it, Action a)
{
 AvailableActions available = {0};
 fill_available_actions(it, &available);
 Action target_action = a;
 int index = -1;
 for(int i = 0; i < available.cur_index; i++)
 {
  if(available.data[i] == target_action)
  {
   index = i;
   break;
  }
 }
 assert(index != -1);
 return index;
}


void generate_prompt(Entity *it, PromptBuff *into)
{
 assert(it->is_npc);
 *into = (PromptBuff){0};

 // global prompt
 printf_buff(into, "%s", global_prompt);
 printf_buff(into, "%s", "\n");

 // npc description prompt
 assert(it->npc_kind < ARRLEN(prompt_table));
 printf_buff(into, "%s", prompt_table[it->npc_kind]);
 printf_buff(into, "%s", "\n");

 // item prompt
 if(it->last_seen_holding_kind != ITEM_nothing)
 {
  assert(it->last_seen_holding_kind < ARRLEN(item_prompt_table));
  printf_buff(into, "%s", item_prompt_table[it->last_seen_holding_kind]);
  printf_buff(into, "%s", "\n");
 }

 // available actions prompt
 AvailableActions available = {0};
 fill_available_actions(it, &available);
 printf_buff(into, "%s", "The NPC possible actions array, indexed by ACT_INDEX: [");
 BUFF_ITER(Action, &available)
 {
  printf_buff(into, "%s", action_strings[*it]);
  printf_buff(into, "%s", ", ");
 }
 printf_buff(into, "%s", "]\n");

 Entity *e = it;
 ItemKind last_holding = ITEM_nothing;
 BUFF_ITER(Perception, &e->remembered_perceptions)
 {
  if(it->type == PlayerAction)
  {
   assert(it->player_action_type < ARRLEN(action_strings));
   printf_buff(into, "Player: ACT %s \n", action_strings[it->player_action_type]);
  }
  else if(it->type == PlayerDialog)
  {
   printf_buff(into, "%s", "Player: \"");
   printf_buff(into, "%s", it->player_dialog.data);
   printf_buff(into, "%s", "\"\n");
  }
  else if(it->type == NPCDialog)
  {
   printf_buff(into, "%s: ACT %s \"%s\"\n", name_table[e->npc_kind], action_strings[it->npc_action_type], it->npc_dialog.data);
  }
  else if(it->type == PlayerHeldItemChanged)
  {
   if(last_holding != it->holding)
   {
    if(last_holding != ITEM_nothing)
    {
     printf_buff(into, "%s", item_discard_message_table[last_holding]);
     printf_buff(into, "%s", "\n");
    }
    if(it->holding != ITEM_nothing)
    {
     printf_buff(into, "%s", item_possess_message_table[it->holding]);
     printf_buff(into, "%s", "\n");
    }
    last_holding = it->holding;
   }
  }
  else
  {
   assert(false);
  }
 }

 printf_buff(into, "%s: ACT_INDEX", name_table[e->npc_kind]);
}

// returns if the response was well formatted
bool parse_ai_response(Entity *it, char *sentence_str, Perception *out)
{
 *out = (Perception){0};
 out->type = NPCDialog;
 
 size_t sentence_length = strlen(sentence_str);
 bool text_was_well_formatted = true;

 BUFF(char, 128) action_index_string = {0};
 int npc_sentence_beginning = 0;
 for(int i = 0; i < sentence_length; i++)
 {
  if(i == 0)
  {
   if(sentence_str[i] != ' ')
   {
    text_was_well_formatted = false;
    Log("Poorly formatted AI string, did not start with a ' ': `%s`\n", sentence_str);
    break;
   }
  }
  else
  {
   if(sentence_str[i] == ' ')
   {
    npc_sentence_beginning = i + 2;
    break;
   }
   else
   {
    BUFF_APPEND(&action_index_string, sentence_str[i]);
   }
  }
 }
 if(sentence_str[npc_sentence_beginning - 1] != '"' || npc_sentence_beginning == 0)
 {
  Log("Poorly formatted AI string, sentence beginning incorrect in AI string `%s` NPC sentence beginning %d ...\n", sentence_str, npc_sentence_beginning);
  text_was_well_formatted = false;
 }

 Action npc_action = 0;
 if(text_was_well_formatted)
 {
  int index_of_action = atoi(action_index_string.data);

  if(!action_from_index(it, &npc_action, index_of_action))
  {
   Log("AI output invalid action index %d action index string %s\n", index_of_action, action_index_string.data);
  }
 }

 Sentence what_npc_said = {0};
 bool found_end_quote = false;
 for(int i = npc_sentence_beginning; i < sentence_length; i++)
 {
  if(sentence_str[i] == '"')
  {
   found_end_quote = true;
   break;
  }
  else
  {
   BUFF_APPEND(&what_npc_said, sentence_str[i]);
  }
 }
 if(!found_end_quote)
 {
  Log("Poorly formatted AI string, couln't find matching end quote in string %s...\n", sentence_str);
  text_was_well_formatted = false;
 }

 if(text_was_well_formatted)
 {
  out->npc_action_type = npc_action;
  out->npc_dialog = what_npc_said;
 }

 return text_was_well_formatted;
}