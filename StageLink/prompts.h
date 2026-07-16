/*
  prompts.h — the decision-tree content.

  This is the file you'll come back to most. Add, remove, or reword
  items freely. Rules:
    1. Keep labels reasonably short (they're button text).
    2. Don't use the "|" character anywhere in a label — it's used
       internally to separate fields when messages are sent.

  colorId picks the accent color for a whole category:
    0 = shade A   1 = shade B   2 = shade C   3 = alert-red
  (What the shades look like depends on STAGELINK_THEME.)

  A category normally holds items. It can instead hold SUB-CATEGORIES —
  give it {name, colorId, nullptr, 0, SUBCAT_ARRAY, count, destRole} and
  each sub-category gets its own screen (see the DJ "Sound" menu below).
  One level of nesting only.

  destRole (last field) says where this category's messages go — a plain
  role (ROLE_DJ / ROLE_FOH / ROLE_STAGE_MGR / ROLE_EVENT_MGR, StageLink.ino)
  or one of two dynamic sentinels: DEST_HOSPITALITY (Event Manager if
  connected, else FOH — with a 60s ack-timeout escalation back to FOH) and
  DEST_REPLY_TO_SENDER (whichever unit sent the message currently being
  replied to). On a submenu parent, destRole is unused (nullptr items) —
  each child sub-category carries its own.

  Special item kinds (second field of PromptItem; omit for a normal
  message button):
    KIND_QUESTION — sends a question the other unit answers with
                    More / Less / Just Right buttons
    KIND_THUMBS   — sends a thumbs-up that briefly pops on the other
                    unit and auto-dismisses (no Seen step)
*/
#pragma once

enum ItemKind : uint8_t { KIND_NORMAL = 0, KIND_QUESTION = 1, KIND_THUMBS = 2 };

struct PromptItem {
  const char* label;   // shown on the button AND sent as the message text
  uint8_t kind;        // ItemKind; brace-initializers without it get KIND_NORMAL
};

struct PromptCategory {
  const char* name;
  uint8_t colorId;
  const PromptItem* items;         // leaf list (nullptr on a submenu parent)
  uint8_t itemCount;
  const PromptCategory* subcats;   // non-null = this entry opens a submenu
  uint8_t subcatCount;
  uint8_t destRole;                // where this category's messages go — see header comment
};

// Shared across DJ and Stage Manager: both units' Hospitality category
// routes the same way (DEST_HOSPITALITY) and reads naturally from either
// unit's vantage point, so it's one list instead of two copies.
const PromptItem HOSPITALITY_ITEMS[] = {
  {"Clean up on isle 5!"},
  {"It's too hot!"},
  {"It's too cold!"},
  {"Water please!"},
  {"Redbull me"},
  {"Adult Beverage Requested"},
  {"More Towels!"},
};

// Shared across DJ (under "Sound") and Stage Manager (under "Tech"): both
// route to FOH, and the same technical-issue wording reads naturally from
// either vantage point, so these are single lists instead of duplicated.
const PromptItem MONITOR_ITEMS[] = {
  {"More Volume"},
  {"More Bass"},
  {"Less Bass"},
  {"Monitors cut out"},
};

const PromptItem HOUSE_ITEMS[] = {
  {"Too quiet"},
  {"Too loud"},
  {"Too much bass"},
  {"Too much treble"},
  {"Sounds muddy"},
};

const PromptItem MIC_ITEMS[] = {
  {"Mic not working"},
  {"Mic too quiet"},
  {"Mic too loud"},
  {"Turn mic off"},
};

const PromptItem LIGHTS_ITEMS[] = {
  {"NO lights on me"},
  {"More lights on me"},
  {"I can't see through the fog!"},
  {"House is too dark!"},
  {"Aziz, more light!"},
};

#if UNIT_ROLE == ROLE_DJ
// ============== DJ -> FOH / Stage Manager / Event Manager ==============

const PromptCategory SOUND_SUBCATS[] = {
  {"Monitors",    0, MONITOR_ITEMS, 4, nullptr, 0, ROLE_FOH},
  {"House Sound", 1, HOUSE_ITEMS,   5, nullptr, 0, ROLE_FOH},
  {"Mic",         2, MIC_ITEMS,     4, nullptr, 0, ROLE_FOH},
};

const PromptItem SHOW_ITEMS[] = {
  {"5 min left"},
  {"10 min left"},
  {"Extending set"},
  {"Last song now"},
  {"Ready when you are"},
};

const PromptItem URGENT_ITEMS[] = {
  {"Come to the booth"},
  {"Taking a bio break"},
  {"Security to the stage"},
  {"Security to BACK stage"},
};

const PromptCategory CATEGORIES[] = {
  {"Sound",       0, nullptr,           0, SOUND_SUBCATS, 3, ROLE_FOH},
  {"Lights",      1, LIGHTS_ITEMS,      5, nullptr, 0, ROLE_FOH},
  {"Hospitality", 2, HOSPITALITY_ITEMS, 7, nullptr, 0, DEST_HOSPITALITY},
  {"Show",        0, SHOW_ITEMS,        5, nullptr, 0, ROLE_FOH},
  {"Urgent",      3, URGENT_ITEMS,      4, nullptr, 0, ROLE_FOH},
};

#elif UNIT_ROLE == ROLE_FOH
// ============== FOH -> DJ ==============

const PromptItem QUICK_ITEMS[] = {
  {"Got it"},
  {"On it"},
  {"Can't right now"},
  {"Give me a min"},
  {"How is it now?", KIND_QUESTION},
  {"Sounds good at FOH"},
  {"[thumbs-up]", KIND_THUMBS},   // label is the wire sentinel; drawn as a glyph
};

const PromptItem HEADSUP_ITEMS[] = {
  {"CO2 Incoming!"},
  {"Good to keep playing!"},
  {"I'm coming to the stage"},
};

const PromptItem FOH_SHOW_ITEMS[] = {
  {"15 min left"},
  {"5 min left"},
  {"Time to shut down"},
  {"Ready when you are!"},
  {"You can start anytime"},
};

const PromptCategory CATEGORIES[] = {
  {"Quick Comms", 0, QUICK_ITEMS,    7, nullptr, 0, ROLE_DJ},
  {"Heads Up",    1, HEADSUP_ITEMS,  3, nullptr, 0, ROLE_DJ},
  {"Show",        0, FOH_SHOW_ITEMS, 5, nullptr, 0, ROLE_DJ},
};

#elif UNIT_ROLE == ROLE_STAGE_MGR
// ============== Stage Manager -> DJ / FOH / Event Manager ==============
// Draft content — edit freely, same as every other role's list.

const PromptItem BOH_ITEMS[] = {
  {"5 min to stage"},
  {"Ready when you are"},
  {"Hold, not ready yet"},
  {"Talent is moving"},
};

const PromptCategory TECH_SUBCATS[] = {
  {"Lighting",    1, LIGHTS_ITEMS,  5, nullptr, 0, ROLE_FOH},
  {"House Sound", 0, HOUSE_ITEMS,   5, nullptr, 0, ROLE_FOH},
  {"Monitors",    2, MONITOR_ITEMS, 4, nullptr, 0, ROLE_FOH},
  {"Mic",         1, MIC_ITEMS,     4, nullptr, 0, ROLE_FOH},
};

const PromptCategory CATEGORIES[] = {
  {"Back of House", 0, BOH_ITEMS,         4, nullptr, 0, ROLE_DJ},
  {"Hospitality",   2, HOSPITALITY_ITEMS, 7, nullptr, 0, DEST_HOSPITALITY},
  {"Tech",          1, nullptr, 0, TECH_SUBCATS, 4, ROLE_FOH},
};

#elif UNIT_ROLE == ROLE_EVENT_MGR
// ============== Event Manager -> whoever it's replying to ==============
// Draft content — edit freely, same as every other role's list. Replies
// route back to whichever unit (DJ or Stage Manager) sent the hospitality
// request currently being answered — see DEST_REPLY_TO_SENDER.

const PromptItem EM_QUICK_ITEMS[] = {
  {"Got it"},
  {"On it"},
  {"Handled"},
  {"Give me a min"},
  {"[thumbs-up]", KIND_THUMBS},
};

const PromptCategory CATEGORIES[] = {
  {"Quick Comms", 0, EM_QUICK_ITEMS, 5, nullptr, 0, DEST_REPLY_TO_SENDER},
};

#endif

const uint8_t CATEGORY_COUNT = sizeof(CATEGORIES) / sizeof(CATEGORIES[0]);
