/**
 * @file revived.cpp
 *
 * Revived features
 */
#include "all.h"
#include "../3rdParty/Storm/Source/storm.h"

#include <sstream>

DEVILUTION_BEGIN_NAMESPACE

char pcursxp;

std::vector<FloatingText> floating_text_queue;

bool weapon_switch_icons_loaded = false;
BYTE *weapon_switch_icons;

BYTE sgbIsScrolling;
DWORD sgdwLastWalk;
BOOL sgbIsWalking;
static BYTE sgbCommand;
static WORD sgwParam1;

inline void colour_pixel(int x, int y, int color) {
  char* WorkingSurface = (char*)gpBuffer;
  WorkingSurface[y*BUFFER_WIDTH+x] = color;
}

inline void fast_draw_horiz_line(int x, int y, int width, BYTE col) {
  memset(&gpBuffer[SCREENXY(x, y)], col, width);
}

inline void fast_draw_vert_line(int x, int y, int height, BYTE col) {
  BYTE *p = &gpBuffer[SCREENXY(x, y)];
  for (int j = 0; j < height; j++) {
    *p = col;
    p += BUFFER_WIDTH;
  }
}

inline void fill_rect(int x, int y, int width, int height, BYTE col) {
  for (int j = 0; j < height; j++) {
    fast_draw_horiz_line(x, y + j, width, col);
  }
}

inline void fill_square(int x, int y, int size, BYTE col) {
  fill_rect(x, y, size, size, col);
}

void track_process_continuous_attacks() {
  if (!sgbIsWalking)
    return;
  
  if (cursmx < 0 || cursmx >= MAXDUNX - 1 || cursmy < 0 || cursmy >= MAXDUNY - 1)
    return;
  
  if (sgbCommand == CMD_WALKXY && plr[myplr]._pVar8 <= 6 && plr[myplr]._pmode != PM_STAND)
    return;
  
  if (cursmx != plr[myplr]._ptargx || cursmy != plr[myplr]._ptargy) {
    DWORD tick = SDL_GetTicks();
    if ((int)(tick - sgdwLastWalk) >= tick_delay * 6) {
      sgdwLastWalk = tick;
      switch (sgbCommand) {
        case CMD_WALKXY:
          NetSendCmdLoc(TRUE, CMD_WALKXY, cursmx, cursmy);
          if (!sgbIsScrolling)
            sgbIsScrolling = 1;
          break;
        case CMD_SATTACKXY:
        case CMD_RATTACKXY:
          NetSendCmdLoc(TRUE, sgbCommand, cursmx, cursmy);
          break;
        case CMD_ATTACKID:
        case CMD_RATTACKID:
          if (pcursmonst != sgwParam1)
            return;
          NetSendCmdParam1(TRUE, sgbCommand, pcursmonst);
          break;
      }
    }
  }
}

void track_lmb_loc(BYTE bCmd, BYTE x, BYTE y) {
  NetSendCmdLoc(TRUE, bCmd, x, y);
  sgbCommand = bCmd;
}

void track_lmb_param1(BYTE bCmd, WORD wParam1) {
  NetSendCmdParam1(TRUE, bCmd, wParam1);
  sgbCommand = bCmd;
  sgwParam1 = wParam1;
}

void play_lvlup_sound() {
  PlaySFX(IS_QUESTDN);
}

int xp_share(int exp, int totalplrs) {
  return exp;
}

bool shouldnt_drop_items() {
  //return gbMaxPlayers > 1 && plr[pnum].plrlevel == 16;
  return true;
}

const char* get_monster_type_text(const MonsterData& monsterData) {
  switch (monsterData.mMonstClass) {
    case MC_ANIMAL:
      return "Animal";
    case MC_DEMON:
      return "Demon";
    case MC_UNDEAD:
      return "Undead";
    default:
      return "Unknown";
  }
}

void escape_closes_windows() {
  if (helpflag || invflag || chrflag || sbookflag || spselflag) {
    helpflag = FALSE;
    invflag = FALSE;
    chrflag = FALSE;
    sbookflag = FALSE;
    spselflag = FALSE;
  } else {
    track_repeat_walk(FALSE);
    gamemenu_on();
  }
}

void check_if_projectile_hit_barrel(int *oi, int mx, int my) { 
  if (dObject[mx][my] <= 0) {
      *oi = -1 - dObject[mx][my];
  } else {
    *oi = dObject[mx][my] - 1;
  }
  
  if (object[*oi]._otype >= OBJ_BARREL && object[*oi]._otype <= OBJ_BARRELEX && object[*oi]._oBreak == 1) {
    BreakBarrel(myplr, *oi, 100, 0, 1);
  }
}

void pack_player_weapon_switch(PkPlayerStruct *pPack, PlayerStruct* pPlayer) {
  PackItem(&pPack->alternateWeapons[0], &pPlayer->alternateWeapons[0]);
  PackItem(&pPack->alternateWeapons[1], &pPlayer->alternateWeapons[1]);
  pPack->currentWeaponSet = pPlayer->currentWeaponSet;
}

void unpack_player_weapon_switch(PkPlayerStruct *pPack, PlayerStruct* pPlayer) {
  pPlayer->currentWeaponSet = pPack->currentWeaponSet;
  UnPackItem(&pPack->alternateWeapons[0], &pPlayer->alternateWeapons[0]);
  UnPackItem(&pPack->alternateWeapons[1], &pPlayer->alternateWeapons[1]);
}

void create_weapon_switch(int pnum) {
  memset(&plr[pnum].alternateWeapons[0],0, sizeof(ItemStruct));
  memset(&plr[pnum].alternateWeapons[1], 0, sizeof(ItemStruct));
  plr[pnum].alternateWeapons[0]._itype = -1;
  plr[pnum].alternateWeapons[1]._itype = -1;
  plr[pnum].currentWeaponSet = 0;
}

void switch_weapons() {
  if (plr[myplr]._pmode != PM_ATTACK && plr[myplr]._pmode != PM_RATTACK && 
      plr[myplr]._pmode != PM_BLOCK && plr[myplr]._pmode != PM_SPELL && 
      plr[myplr]._pmode != PM_DEATH && plr[myplr]._pmode != PM_WALK && 
      plr[myplr]._pmode != PM_WALK2 && plr[myplr]._pmode != PM_WALK3) {
    ItemStruct temp = plr[myplr].InvBody[INVLOC_HAND_LEFT];
    ItemStruct temp2 = plr[myplr].InvBody[INVLOC_HAND_RIGHT];

    plr[myplr].InvBody[INVLOC_HAND_LEFT] = plr[myplr].alternateWeapons[0];
    plr[myplr].InvBody[INVLOC_HAND_RIGHT] = plr[myplr].alternateWeapons[1];

    plr[myplr].alternateWeapons[0] = temp;
    plr[myplr].alternateWeapons[1] = temp2;
    
    plr[myplr].currentWeaponSet = (plr[myplr].currentWeaponSet == 0 ? 1 : 0);
    CalcPlrInv(myplr, 1);
    PlaySFX(IS_TITLEMOV);
  }
}

void auto_pickup_gold(int pnum) {
  PlayerStruct& player = plr[pnum];
  if (currlevel == ENTRY_MAIN) return;
  
  for (int orient = 0; orient < 9; ++orient) {
    int row = player._px + pathxdir[orient];
    int col = player._py + pathydir[orient];

    int itemIndex = dItem[row][col] - 1;
    if (itemIndex > -1) {
      ItemStruct* it = &(item[itemIndex]);
      if (it->_itype == ITYPE_GOLD) {
        draw_floating_gold(it->_ivalue, row, col);
        NetSendCmdGItem(1u, CMD_REQUESTAGITEM, pnum, pnum, itemIndex);
        item[itemIndex]._iRequest = 1;
        CalcPlrInv(myplr, 1);
        PlaySFX(68);
      }
    }
  }
}

bool is_rogue_detect_trap(int trapFlag) {
  if (plr[myplr]._pClass == UI_ROGUE) {
    if (trapFlag) {
      return true;
    }
  }
  return false;
}

void set_player_max_life_and_mana() {
  if (plr[myplr]._pHitPoints != plr[myplr]._pMaxHP || plr[myplr]._pMana != plr[myplr]._pMaxMana)
      PlaySFX(IS_CAST8);
  drawhpflag = TRUE;
  plr[myplr]._pHitPoints = plr[myplr]._pMaxHP;
  plr[myplr]._pHPBase = plr[myplr]._pMaxHPBase;
  
  plr[myplr]._pMana = plr[myplr]._pMaxMana;
  plr[myplr]._pManaBase = plr[myplr]._pMaxManaBase;
}

void highlight_items_on_map() {
  // items on ground name highlighting (Qndel)
  class itemLabel {
  public:
    int itemID;
    int x;
    int y;
    int width;
    int height;
    int magicLevel;
    std::string text;
    itemLabel(int x, int y, int width, int height, int itemID, int q2, std::string text):
      itemID(itemID), x(x), y(y), width(width), height(height), magicLevel(q2), text(text) {}
  };
  
  char textOnGround[256];
  std::vector<itemLabel> q;
  
  for (int i = 0; i < numitems; i++) {
    ItemStruct& item_local = item[itemactive[i]];
    if (item_local._itype == ITYPE_GOLD) {
      sprintf(textOnGround, "%i gold", item_local._ivalue);
    } else {
      sprintf(textOnGround, "%s", item_local._iIdentified ? item_local._iIName : item_local._iName);
    }
    
    int walkStandX = ScrollInfo._sxoff;// +plr[myplr]._pyoff;
    int walkStandY = ScrollInfo._syoff;// +plr[myplr]._pxoff;
    if (plr[myplr]._pmode == PM_WALK2 && ScrollInfo._sdir == 4) {
      walkStandX += 32;
      walkStandY += 16;
    } else if (plr[myplr]._pmode == PM_WALK2 && ScrollInfo._sdir == 5) {
      walkStandY += 32;
    } else if(plr[myplr]._pmode == PM_WALK2 && ScrollInfo._sdir == 6) {
      walkStandX += -32;
      walkStandY += 16;
    }
    
    int row = item_local._ix - plr[myplr]._px;
    int col = item_local._iy - plr[myplr]._py;
    int x = (row - col) * TILE_WIDTH  / 2 + (200 * (walkStandX) / 100 >> 1);
    int y = (row + col) * TILE_HEIGHT / 2 + (200 * (walkStandY) / 100 >> 1);
    
    // add to drawing queue
    const int labelWidth = CalculateTextWidth(textOnGround);
    const int &id = itemactive[i];
    const int &mag = item_local._iMagical;
    const std::string &text = std::string(textOnGround);
    q.push_back(itemLabel(x - labelWidth / 2, y, labelWidth, 13, id, mag, text));
  }
  
  const int borderX = 5;
  for (unsigned int item1 = 0; item1 < q.size(); ++item1) {
    std::map<int, bool> backtrace;
    bool canShow = FALSE;
    while (!canShow) {
      canShow = TRUE;
      for (unsigned int item2 = 0; item2 < item1; ++item2) {
        if (abs(q[item2].y - q[item1].y) >= q[item1].height + 2) {
          continue;
        }
        if (q[item2].x >= q[item1].x && q[item2].x - q[item1].x < q[item1].width + borderX) {
          canShow = FALSE;
          int newpos = q[item2].x - q[item1].width - borderX;
          if (backtrace.find(newpos) == backtrace.end()) {
            q[item1].x = newpos;
            backtrace[newpos] = TRUE;
          } else {
            newpos = q[item2].x + q[item2].width + borderX;
            q[item1].x = newpos;
            backtrace[newpos] = TRUE;
          }
        } else if (q[item2].x < q[item1].x && q[item1].x - q[item2].x < q[item2].width + borderX) {
          canShow = FALSE;
          int newpos = q[item2].x + q[item2].width + borderX;
          if (backtrace.find(newpos) == backtrace.end()) {
            q[item1].x = newpos;
            backtrace[newpos] = TRUE;
          } else {
            newpos = q[item2].x - q[item1].width - borderX;
            q[item1].x = newpos;
            backtrace[newpos] = TRUE;
          }
        }
      }
    }
  }
  
  for (unsigned int i = 0; i < q.size(); ++i) {
    itemLabel &t = q[i];
    
    int sx = t.x + (SCREEN_WIDTH) / 2;
    int sy = t.y + (PANEL_TOP) / 2  - 16;
    
    if (sx < 0 || sx > SCREEN_WIDTH || sy < 0 || sy > SCREEN_HEIGHT) {
      continue;
    }
    
    if ((chrflag || questlog) && sx < SPANEL_WIDTH && sy < SPANEL_HEIGHT) {
      continue;
    }
    
    if ((invflag || sbookflag) && sx + t.width > SCREEN_WIDTH - SPANEL_WIDTH  && sy < SPANEL_HEIGHT) {
      continue;
    }
    
    int bgcolor = 0;
    // highlight label if item is under cursor:
    if (pcursitem == t.itemID) {
      bgcolor = 134;
    }
    
    char color = COL_WHITE;
    if (t.magicLevel == ITEM_QUALITY_MAGIC) {
      color = COL_BLUE;
    } else if (t.magicLevel == ITEM_QUALITY_UNIQUE) {
      color = COL_GOLD;
    }
    
    DrawTransparentRectangle(sx - 1, t.width + 2, sy - t.height + 1, t.height, bgcolor);
    PrintGameStr(sx, sy, (char*)(t.text.c_str()), color);
  }
}

void draw_trap_if_rogue(int bv, int sx, int sy) {
  if (is_rogue_detect_trap(object[bv]._oTrapFlag) == true) {
    int offset = 2;
    for (int k = 1; k <= offset; ++k) {
      for (int i = -1; i <= 1; ++i) {
        for (int j = -1; j <= 1; ++j) {
          CelBlitOutline(140, sx+i*k, sy+j*k, object[bv]._oAnimData, object[bv]._oAnimFrame, object[bv]._oAnimWidth);
        }
      }
    }
  }
}

void draw_xp_bar() {
  int barColor = PAL8_YELLOW+4;
  int backgroundColor = 0;
  
  PlayerStruct *player = &plr[myplr];
  int charLevel; 
  unsigned int curXp; 
  unsigned int prevXp;
  unsigned int prevXpDelta;
  unsigned int prevXpDelta_1; 
  int visibleBar = 0;
  
  charLevel = player->_pLevel;
  
  for (int k = 0; k < XPBAR_SEGMENTS; ++k) {
    int offset = (XPBAR_SEGMENT_SIZE)*k + XPBAR_SEGMENT_SPACING*k;
    fill_rect(XPBAR_X+offset, XPBAR_Y, XPBAR_SEGMENT_SIZE, XPBAR_HEIGHT, backgroundColor);
  }
  
  curXp = ExpLvlsTbl[charLevel];
  if (charLevel != 50) {
    prevXp = ExpLvlsTbl[charLevel - 1];
    prevXpDelta = curXp - prevXp;
    prevXpDelta_1 = player->_pExperience - prevXp;
    visibleBar = XPBAR_WIDTH*(unsigned __int64)prevXpDelta_1 / prevXpDelta;
    for (int k = 0; k < XPBAR_SEGMENTS; ++k) {
      int offset = (XPBAR_SEGMENT_SIZE)*k + XPBAR_SEGMENT_SPACING*k;
      int x = XPBAR_X+offset;
      
      if (visibleBar > offset) {
        if (visibleBar > offset+XPBAR_SEGMENT_SIZE) {
          fill_rect(x, XPBAR_Y, XPBAR_SEGMENT_SIZE, XPBAR_HEIGHT, barColor);
        } else {
          fill_rect(x, XPBAR_Y, XPBAR_SEGMENT_SIZE-(offset+XPBAR_SEGMENT_SIZE-visibleBar), XPBAR_HEIGHT, barColor);
        }
      }
    }
  }
  
  if (pcursxp != -1) {
    int screen_height = SCREEN_HEIGHT+SCREEN_Y;
    
    char xp_text[256];
    
    sprintf(xp_text, "xp: %i / %i", player->_pExperience, curXp);
    int x = SCREEN_WIDTH*0.5 - CalculateTextWidth(xp_text)*0.5;
    int y = screen_height-SCREEN_Y-16;
    PrintGameStr(x, y, (char*)(std::string(xp_text).c_str()), 255);
  }
}

void draw_monster_health_bar(int monsterID) {
  static const int yPos = 10;
  static const int height = 25;
  static const int width = 250;
  static const int xCenter = (SCREEN_WIDTH) / 2 + 1; // centered between orbs
  static const int xPos = xCenter - width / 2;
  static const int xOffset = 0;
  static const int yOffset = 0; // was 1
  static const int borderWidth = 2;
  static const int borderColors[] = { 242/*undead*/, 232/*demon*/, 182/*beast*/ };
  static const int filledColor = 142; // optimum balance in bright red between dark and light
  static const bool fillCorners = TRUE;
  static const int square = 10;
  static const char* immuText = "IMMU: ";
  static const char* resText = "RES: ";
  static const char* vulnText = ":VULN";
  static const int resSize = 3;
  static const int resistColors[] = { 148, 140, 129 };
  static const unsigned short immunes[] = { 0x8, 0x10, 0x20 };
  static const unsigned short resists[] = { 0x1, 0x2, 0x4 };
  static const bool cheat = FALSE;

  if (!monster[monsterID].MData) {
    return;
  }

  MonsterStruct* mon = &monster[monsterID];
  BOOL specialMonster = !!mon->_uniqtype;
  int currentLife = mon->_mhitpoints;
  int maxLife = mon->_mmaxhp;
  if (currentLife > maxLife) {
    maxLife = currentLife;
  }
  int borderColor =  borderColors[(int)mon->MData->mMonstClass];
  float FilledPercent = (float)currentLife / (float)maxLife;
  unsigned short mres = mon->mMagicRes;
  bool showHPNumbers = cheat || monstkills[mon->MType->mtype] >= 30;
  bool showDamageModifiers = cheat || monstkills[mon->MType->mtype] >= 15;

  if (showDamageModifiers) {
    int resOffset = 0 + CalculateTextWidth(resText);
    for (int k = 0; k < resSize; ++k) {
      if (!(mres & resists[k])) {
        continue;
      }
      DrawSolidRectangle(xPos + resOffset, square, yPos + height + yOffset + borderWidth + 2, square, resistColors[k]);
      resOffset += 12;
    }

    int vulOffset = width - square - CalculateTextWidth(vulnText) - 4;
    for (int k = 0; k < resSize; ++k) {
      if (mres & resists[k] || mres & immunes[k]) {
        continue;
      }
      DrawSolidRectangle(xPos + vulOffset, square, yPos + height + yOffset + borderWidth + 2, square, resistColors[k]);
      vulOffset -= 12;
    }
  }

  DrawTransparentRectangle(xPos, (int) ceil(FilledPercent * width), yPos, height, filledColor);

  static const int cornerMod = fillCorners ? borderWidth : 0;
  static const int x0 = xPos - xOffset;
  static const int x1 = xPos + xOffset + width;
  static const int dx = width + 2*xOffset + cornerMod;
  static const int y0 = yPos - yOffset;
  static const int y1 = yPos + yOffset + height;
  static const int dy = height + 2*yOffset + cornerMod;
  DrawSolidRectangle(x0 - cornerMod, dx, y0 - borderWidth, borderWidth, borderColor);
  DrawSolidRectangle(x0            , dx, y1,               borderWidth, borderColor);
  DrawSolidRectangle(x0 - borderWidth, borderWidth, y0,             dy, borderColor);
  DrawSolidRectangle(x1,               borderWidth, y0 - cornerMod, dy, borderColor);

  bool drawImmu = FALSE;
  if (showDamageModifiers) {
    int immuOffset = 0 + CalculateTextWidth(immuText) - 5;
    for (int k = 0; k < resSize; ++k) {
      if (!(mres & immunes[k])) {
        continue;
      }
      drawImmu = TRUE;
      DrawSolidRectangle(xPos + immuOffset, square, yPos + height + yOffset + borderWidth + 2 - square - 5, square, resistColors[k]);
      immuOffset += 12;
    }
  }

  std::stringstream name;
  name << mon->mName;
  if (mon->leader > 0) {
    name << " (minion)";
  }
  int namecolor = COL_WHITE;
  if (specialMonster) {
    namecolor = COL_GOLD;
  }

  if (!showHPNumbers) {
    PrintGameStr(xCenter - CalculateTextWidth((char*)name.str().c_str()) / 2, yPos + 17, (char*)name.str().c_str(), namecolor);
  } else {
    PrintGameStr(xCenter - CalculateTextWidth((char*)name.str().c_str()) / 2, yPos + 10, (char*)name.str().c_str(), namecolor);
    std::stringstream life;
    life << (currentLife >> 6);
    life << "/";
    life << (maxLife >> 6);
    PrintGameStr(xCenter - CalculateTextWidth((char*)life.str().c_str()) / 2, yPos + height - 2, (char*)life.str().c_str(), COL_WHITE);
  }
  static const int ytxt = yPos + height + borderWidth + 12;
  if (showDamageModifiers) {
    PrintGameStr(xCenter - width / 2 + 2, ytxt, resText, COL_GOLD);
  }

  std::stringstream kills;
  kills << "Kills: " << monstkills[mon->MType->mtype];
  PrintGameStr(xCenter - CalculateTextWidth("kills") / 2 - 30, ytxt, (char*)(kills.str().c_str()), COL_WHITE);

  if (drawImmu) {
    PrintGameStr(xCenter - width / 2 + 2, yPos + height - 1, immuText, COL_GOLD);
  }
  if (showDamageModifiers) {
    PrintGameStr(xCenter + width / 2 - CalculateTextWidth(vulnText), ytxt, vulnText, COL_RED);
  }
}

void draw_floating_text_above_player() {
  float DurationOfTextInSeconds = 2;
  int PercentOfTheScreenToTravel = 12;
  int percentToMerge = 80;
  int ScreenWidth = BUFFER_WIDTH;
  int ScreenHeight = BUFFER_HEIGHT;
  int MaxFPS = 20;

  std::vector<int> indexes;
  if (floating_text_queue.size() > 1 && indexes.size() == 0) {
    indexes.push_back(floating_text_queue.size() - 1);
  }
  for (;;) {
    for (uint j = 0; j < indexes.size(); ++j) {
      FloatingText lastElem = floating_text_queue[indexes[j]];
      if (lastElem.callerID != -1) {
        for (int i = 0; i < (int)floating_text_queue.size() - 2; ++i) {
          if (i == indexes[j]) {
            continue;
          }
          if (lastElem.description == floating_text_queue[i].description && lastElem.callerID == floating_text_queue[i].callerID) {
            if (abs(lastElem.iterations - floating_text_queue[i].iterations) < (MaxFPS*DurationOfTextInSeconds*percentToMerge / 100) && floating_text_queue[i].iterations < (int)((float)MaxFPS*DurationOfTextInSeconds)) {
              indexes.push_back(i);
              floating_text_queue[i].value += lastElem.value;
              floating_text_queue[indexes[j]].iterations = 9999999;
            }
          }
        }
      }
      indexes.erase(indexes.begin() + j);
    }
    if (indexes.size() == 0) {
      break;
    }
  }
  
  for (int i = floating_text_queue.size() - 1; i >= 0; --i) {
    std::string text = floating_text_queue[i].text;
    int val = floating_text_queue[i].value;
    int iterations = floating_text_queue[i].iterations;
    int color = floating_text_queue[i].color;
    if (iterations < (int)((float)MaxFPS*DurationOfTextInSeconds)) {
      int x, y;

      int walkStandX = ScrollInfo._sxoff;// +plr[myplr]._pyoff;
      int walkStandY = ScrollInfo._syoff;// +plr[myplr]._pxoff;

      if (plr[myplr]._pmode == PM_WALK2 && ScrollInfo._sdir == 4) {
        walkStandX += 32;
        walkStandY += 16;
      } else if (plr[myplr]._pmode == PM_WALK2 && ScrollInfo._sdir == 5) {
        walkStandY += 32;
      } else if (plr[myplr]._pmode == PM_WALK2 && ScrollInfo._sdir == 6) {
        walkStandX += -32;
        walkStandY += 16;
      }
      
      if (floating_text_queue[i].showOnCenter == true) {
        x = (ScreenWidth - CalculateTextWidth((char*)text.c_str())) / 2;
        y = int((float)ScreenHeight / 2.3);
      } else {
        x = floating_text_queue[i].posX;
        y = floating_text_queue[i].posY;
        int row = floating_text_queue[i].posX - plr[myplr]._px;
        int col = floating_text_queue[i].posY - plr[myplr]._py;
        x = 32 * (row - col) + (200 * (walkStandX) / 100 >> 1) - CalculateTextWidth((char*)text.c_str()) / 2;
        y = 16 * (row + col) + (200 * (walkStandY) / 100 >> 1)  - 16;

        int drawXOffset = 0;
        if (invflag || sbookflag)
          drawXOffset -= 160;
        if (chrflag || questlog)
          drawXOffset += 160;
        x = x + ScreenWidth*0.45 + drawXOffset;
        y = y + 180;
      }
      
      //double PI = 3.14159265;
      double DistanceToTravel = ScreenHeight * PercentOfTheScreenToTravel / 100;
      int dest_x = x;
      int dest_y = y + int(DistanceToTravel);
      double progress = iterations / (MaxFPS * DurationOfTextInSeconds);
      int diff_x = dest_x - x;
      int diff_y = dest_y - y;
      
      int drawx = x - int(progress * diff_x);
      int drawy = y - int(progress * diff_y);
      if (drawx > 0 && drawy < ScreenWidth && drawy > 0 && drawy < ScreenHeight) {
        char bfr[256];
        
        sprintf(bfr, text.c_str(), val);
        PrintGameStr(drawx, drawy, bfr, color);
      }
      floating_text_queue[i].IncreaseIterations();
    }
    else {
      floating_text_queue.erase(floating_text_queue.begin() + i);
    }
  }
}

void draw_floating_gold(int goldGain, int row, int col) {
  floating_text_queue.push_back(FloatingText("+ %i Gold", COL_GOLD, row, col, false, -1, "gainGOLD", goldGain));
}

void draw_weapon_switch_icons() {
  if (weapon_switch_icons_loaded == false) {
    weapon_switch_icons_loaded = true;
    weapon_switch_icons = LoadFileInMem("CtrlPan\\P8But2.CEL", 0);
  }

  CelDraw(RIGHT_PANEL_X + 17*1.8, 233, weapon_switch_icons, panbtn[7] + 3+(plr[myplr].currentWeaponSet==0), 33);
  CelDraw(RIGHT_PANEL_X + 17*1.8, 233, weapon_switch_icons, panbtn[7] + 5+(plr[myplr].currentWeaponSet != 0), 33);
  int offset = 231;
  CelDraw(RIGHT_PANEL_X + 17*1.8+offset, 233, weapon_switch_icons, panbtn[7] + 3+(plr[myplr].currentWeaponSet == 0), 33);
  CelDraw(RIGHT_PANEL_X + 17*1.8+offset, 233, weapon_switch_icons, panbtn[7] + 5+(plr[myplr].currentWeaponSet != 0), 33);
}

DEVILUTION_END_NAMESPACE
