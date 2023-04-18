// SPACE WAR for Sinclair ZX Spectrum 48K, Adrian Pilkington 2023

// some of this code started from the excellent sprite code example from here: 
//https://github.com/z88dk/z88dk/blob/master/doc/ZXSpectrumZSDCCnewlib_SP1_02_SimpleMaskedSprite.md

// zcc +zx -vn -m -startup=31 -clib=sdcc_iy main.c sprite.asm -o main -create-app

// really useful link to how to handle sprites:
// https://www.z88dk.org/wiki/doku.php?id=libnew:examples:sp1_ex1
// link to how to draw lines etc:
// https://github.com/z88dk/z88dk/blob/master/doc/ZXSpectrumZSDCCnewlib_03_SimpleGraphics.md
//https://github.com/z88dk/z88dk/blob/master/doc/ZXSpectrumZSDCCnewlib_SP1_04_BiggerSprites.md

#pragma output REGISTER_SP = 0xD000

//#define DEBUG_NO_MOVING_ENEMY

#include <arch/zx.h>
#include <arch/zx/sp1.h>
#include <intrinsic.h>
#include <z80.h>
#include <im2.h>
#include <string.h>
#include <input.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <sound.h>

IM2_DEFINE_ISR(isr) {}
#define TABLE_HIGH_BYTE        ((unsigned int)0xD0)
#define JUMP_POINT_HIGH_BYTE   ((unsigned int)0xD1)
#define UI_256                 ((unsigned int)256)
#define TABLE_ADDR             ((void*)(TABLE_HIGH_BYTE*UI_256))
#define JUMP_POINT             ((unsigned char*)( (unsigned int)(JUMP_POINT_HIGH_BYTE*UI_256) + JUMP_POINT_HIGH_BYTE ))

#define MAX_SPEED_SACESHIP_1 6
#define FIRE_CUTTOUT 1
#define MIN_ENEMY 1
#define MAX_ENEMY 20
#define BONUS_Y_POS 30
#define SCORE_WHEN_BONUS_HIT 1000
#define BONUS_COUNTDOWN_INITIAL 150
#define LENGTH_OF_LAZER 50
#define MAX_SHOTS_AT_ANY_TIME  8

extern unsigned char spaceship1_masked[];
extern unsigned char spaceship2_masked[];
extern unsigned char bonusship_masked[];

struct sp1_Rect full_screen = {
  0,
  0,
  32,
  24
};

typedef struct shotStruct{
    unsigned char shotLen;    
    unsigned char XBotCoord;
    unsigned char YBotCoord;
    unsigned char PrevYBotCoord;
    unsigned char shotLiveCountDown;
    unsigned char clearLast;
} t_ShotStruct;

struct sp1_pss pss = 
{
    &
    full_screen, // print confined to this rectangle
    SP1_PSSFLAG_INVALIDATE | SP1_PSSFLAG_XWRAP | SP1_PSSFLAG_YINC | SP1_PSSFLAG_YWRAP,
    0, // current x
    0, // current y
    0, // attribute mask - overwrite underlying colour
    INK_BLACK | PAPER_WHITE,
    0, // sp1_update* must be consistent with x,y
    0 // visit function (set to zero)
};


void printDebug(int debug) 
{
  char buffer[8];
  sprintf(buffer, "%06d", debug);
  sp1_SetPrintPos(&pss, 1, 30);
  sp1_PrintString(&pss, buffer);
}

void printDebug2(int debug1, int debug2) 
{
  char buffer[8];
  sprintf(buffer, "X=%03d", debug1);
  sp1_SetPrintPos(&pss, 3, 1);
  sp1_PrintString(&pss, buffer);
  sprintf(buffer, "Y=%03d", debug2);  
  sp1_SetPrintPos(&pss, 3, 10);
  sp1_PrintString(&pss, buffer);  
}

void printDebug3(int debug1, int debug2) 
{
  char buffer[8];
  sprintf(buffer, "X=%03d", debug1);
  sp1_SetPrintPos(&pss, 4, 1);
  sp1_PrintString(&pss, buffer);
  sprintf(buffer, "Y=%03d", debug2);  
  sp1_SetPrintPos(&pss, 4, 10);
  sp1_PrintString(&pss, buffer);  
}


void plot(unsigned char x, unsigned char y) {
  * zx_pxy2saddr(x, y) |= zx_px2bitmask(x);
}

void clearplot(unsigned char x, unsigned char y) {
  * zx_pxy2saddr(x, y) &= 0;
}

void plotShotAndMoveUp(t_ShotStruct * s)
{
  unsigned char i;
  unsigned char shotLen;
 
  if ((s->shotLiveCountDown > 0) && (s->YBotCoord > 5))
  {
    s->shotLiveCountDown = s->shotLiveCountDown - 1;

    shotLen = s->shotLen;
  
    for (i = 0; i < shotLen; i++) 
    {
      plot(s->XBotCoord, (s->YBotCoord)+i);
    }
      
    if (s->clearLast == 1)
    {
      for (i = 0; i < shotLen; i++) 
      {
        clearplot(s->XBotCoord, (s->PrevYBotCoord)+i);      
      }
    } 
    
    if (s->YBotCoord >= 30)
    {
      s->PrevYBotCoord = s->YBotCoord;
      s->YBotCoord = (s->YBotCoord) - 10;
    } else if (s->YBotCoord >= 10)
    {
      s->PrevYBotCoord = s->YBotCoord;
      s->YBotCoord = (s->YBotCoord) - 10;
    }
  }
  else
  {
    s->shotLiveCountDown = 0;
    // on last time just clear it!
    for (i = 0; i < shotLen; i++) 
    {
      clearplot(s->XBotCoord, s->YBotCoord+i);      
      clearplot(s->XBotCoord, s->PrevYBotCoord+i);            
    }
  }  
}


void line(unsigned char x0, unsigned char y0, unsigned char x1, unsigned char y1) {
  unsigned char dx = abs(x1 - x0);
  unsigned char dy = abs(y1 - y0);
  signed char sx = x0 < x1 ? 1 : -1;
  signed char sy = y0 < y1 ? 1 : -1;
  int err = (dx > dy ? dx : -dy) / 2;
  int e2;

  while (1) {
    plot(x0, y0);
    if (x0 == x1 && y0 == y1) break;

    e2 = err;
    if (e2 > -dx) {
      err -= dy;
      x0 += sx;
    }
    if (e2 < dy) {
      err += dx;
      y0 += sy;
    }
  }
}

void vertline(unsigned char x0, unsigned char y0, unsigned char y1) {
  unsigned char i;
  for (i = y1; i < y0; i++) {
    plot(x0, i);
  }
}

void clearVertline(unsigned char x0, unsigned char y0, unsigned char y1) {
  unsigned char i;
  for (i = y1; i < y0; i++) {
    clearplot(x0, i);
  }
}

void printScores(struct sp1_pss * pss, int score, int highScore, unsigned char lives, unsigned char level) {
  char buffer[8];
  sp1_SetPrintPos(pss, 0, 0);
  sp1_PrintString(pss, "1UP Score--SPACE-WAR--High Score");
  sp1_SetPrintPos(pss, 1, 0);
  sp1_PrintString(pss, "        Lives     Lev.          ");

  sprintf(buffer, "%06d", score);
  sp1_SetPrintPos(pss, 1, 0);
  sp1_PrintString(pss, buffer);
  sprintf(buffer, "%06d", highScore);
  sp1_SetPrintPos(pss, 1, 26);
  sp1_PrintString(pss, buffer);
  sp1_SetPrintPos(pss, 1, 14);
  sprintf(buffer, "%02d", lives);
  sp1_PrintString(pss, buffer);
  sp1_SetPrintPos(pss, 1, 23);
  sprintf(buffer, "%d", level);
  sp1_PrintString(pss, buffer);
}

unsigned char colour;
unsigned char cmask;

void colourSpr(unsigned int count, struct sp1_cs * c) {
  c -> attr_mask = cmask;
  c -> attr = colour;
}

int main() {
  unsigned char fire;
  t_ShotStruct shot[MAX_SHOTS_AT_ANY_TIME];
  unsigned char shotCounter;
  unsigned char enemyCount;
  short currentEnemyCount;
  int score;
  int highscore;
  unsigned char dead;
  unsigned char lives;
  unsigned char level;

  unsigned char countdownTillFireAvailable;
  unsigned char overWriteLine;
  unsigned char c;

  struct sp1_ss * spaceship1;
  struct sp1_ss * bonusship;
  struct sp1_ss * enemyArray[MAX_ENEMY];

  short sp1XPosInc;
  short sp1YPosInc;
  short sp2XPosInc[MAX_ENEMY];
  short sp2YPosInc[MAX_ENEMY];

  unsigned char sp1YPos;
  unsigned char sp1XPos;
  unsigned char sp1XPosPrev;
  unsigned char sp1YPosPrev;
  unsigned char sp2XPos[MAX_ENEMY];
  unsigned char sp2YPos[MAX_ENEMY];
  unsigned char enemyEnabled[MAX_ENEMY];
  unsigned char bonusEnabled;
  unsigned char bonusshipPosX;
  int bonusCountdown;

  memset(TABLE_ADDR, JUMP_POINT_HIGH_BYTE, 257);
  z80_bpoke(JUMP_POINT, 195);
  z80_wpoke(JUMP_POINT + 1, (unsigned int) isr);
  im2_init(TABLE_ADDR);
  intrinsic_ei();

  zx_border(INK_YELLOW);

  highscore = 0;
  while (1) // main game loop
  {
    sp1_Initialize(SP1_IFLAG_MAKE_ROTTBL | SP1_IFLAG_OVERWRITE_TILES | SP1_IFLAG_OVERWRITE_DFILE,
      INK_YELLOW | PAPER_BLACK, ' ');
    sp1_Invalidate( & full_screen);

    bonusEnabled = 0;    
    bonusCountdown = BONUS_COUNTDOWN_INITIAL;
    shotCounter = 0;

    for (enemyCount = 0; enemyCount < MAX_ENEMY; enemyCount++) {
      if (enemyArray[enemyCount] != NULL) sp1_DeleteSpr(enemyArray[enemyCount]);
      enemyArray[enemyCount] = sp1_CreateSpr(SP1_DRAW_MASK2LB, SP1_TYPE_2BYTE, 2, (int) spaceship2_masked, 0);
      sp1_AddColSpr(enemyArray[enemyCount], SP1_DRAW_MASK2RB, SP1_TYPE_2BYTE, 0, 0);
      colour = (INK_RED | PAPER_BLACK);
      cmask = SP1_AMASK_INK & SP1_AMASK_PAPER;
      sp1_IterateSprChar(enemyArray[enemyCount], colourSpr);
#ifdef DEBUG_NO_MOVING_ENEMY            
      sp2XPosInc[enemyCount] = 0;
      sp2YPosInc[enemyCount] = 0;
#else      
      sp2XPosInc[enemyCount] = 4;
      sp2YPosInc[enemyCount] = 4;      
#endif      
      sp2XPos[enemyCount] = 0;
      sp2YPos[enemyCount] = 0;
      enemyEnabled[enemyCount] = 0;
    }
#ifdef DEBUG_NO_MOVING_ENEMY      
    sp2XPosInc[0] = 0;
    sp2YPosInc[0] = 0;   
#else    
    sp2XPosInc[0] = 4;
    sp2YPosInc[0] = 4;       
#endif    
    sp2XPos[0] = (unsigned char)(rand() * 100);
    sp2YPos[0] = (unsigned char)(rand() * 100);
    enemyEnabled[0] = 1;
#ifdef DEBUG_NO_MOVING_ENEMY
    sp2XPosInc[1] = 0;
    sp2YPosInc[1] = 0;
#else
    sp2XPosInc[1] = 4;
    sp2YPosInc[1] = 4;
#endif    
    sp2XPos[1] = (unsigned char)(rand() * 100);
    sp2YPos[1] = (unsigned char)(rand() * 100);
    enemyEnabled[1] = 1;
    spaceship1 = sp1_CreateSpr(SP1_DRAW_MASK2LB, SP1_TYPE_2BYTE, 2, (int) spaceship1_masked, 0);
    sp1_AddColSpr(spaceship1, SP1_DRAW_MASK2RB, SP1_TYPE_2BYTE, 0, 0);
    colour = (INK_CYAN | PAPER_BLACK);
    cmask = SP1_AMASK_INK & SP1_AMASK_PAPER;
    sp1_IterateSprChar(spaceship1, colourSpr);

    bonusship = sp1_CreateSpr(SP1_DRAW_MASK2LB, SP1_TYPE_2BYTE, 2, (int) bonusship_masked, 0);
    sp1_AddColSpr(bonusship, SP1_DRAW_MASK2RB, SP1_TYPE_2BYTE, 0, 0);
    colour = (INK_GREEN | PAPER_BLACK);
    cmask = SP1_AMASK_INK & SP1_AMASK_PAPER;
    sp1_IterateSprChar(bonusship, colourSpr);
    bonusshipPosX = 0; // make it off screen until bonus

    currentEnemyCount = 2;

    level = 1; // just start at level 1 for now

    // initialise everything else  
    sp1XPos = 10;
    sp1YPos = 160;

    sp1XPosInc = 1;
    sp1YPosInc = 1;
    countdownTillFireAvailable = FIRE_CUTTOUT;

    score = 0;
    dead = 0; /// dead = 1 not dead = 0;
    lives = 3;

    while (dead == 0) {
      printScores( & pss, score, highscore, lives, level);

      c = in_inkey();

      switch (c) {
      case 'o':
        sp1XPosInc -= 1;
        break;
      case 'p':
        sp1XPosInc += 1;
        break;
      case 'q':
        sp1YPosInc -= 1;
        break;
      case 'a':
        sp1YPosInc += 1;
        break;
      case ' ':
        fire = 1;
        break;
      default:
        break;
      }
      sp1XPos = sp1XPosInc + sp1XPos;
      sp1YPos = sp1YPosInc + sp1YPos;

      sp1_MoveSprPix(spaceship1, & full_screen, 0, sp1XPos, sp1YPos);

      if (bonusEnabled > 1) {
        sp1_MoveSprPix(bonusship, & full_screen, 0, bonusshipPosX, BONUS_Y_POS);
        bonusshipPosX += 1;
        bonusEnabled--;
      }
      if (bonusEnabled == 0) {
        bonusshipPosX = 0;
        bonusEnabled = 0;
        sp1_MoveSprPix(bonusship, & full_screen, 0, 255, 192);
      }

      for (enemyCount = 0; enemyCount < MAX_ENEMY; enemyCount++) {
        if (enemyEnabled[enemyCount] == 1) {
#ifdef DEBUG_NO_MOVING_ENEMY
#else          
          sp2XPos[enemyCount] = sp2XPosInc[enemyCount] + sp2XPos[enemyCount];
          sp2YPos[enemyCount] = sp2YPosInc[enemyCount] + sp2YPos[enemyCount];
#endif
          sp1_MoveSprPix(enemyArray[enemyCount], & full_screen, 0, sp2XPos[enemyCount], sp2YPos[enemyCount]);

          if (sp2XPos[enemyCount] <= -sp2XPosInc[enemyCount]) {
            sp2XPosInc[enemyCount] = 4;
            sp2XPos[enemyCount] = 0;
          }
          if (sp2XPos[enemyCount] >= 248 - sp2XPosInc[enemyCount]) {
            sp2XPosInc[enemyCount] = -4;
            sp2XPos[enemyCount] = 248;
          }
          if (sp2YPos[enemyCount] <= -sp2YPosInc[enemyCount] + 16) {
            sp2YPosInc[enemyCount] = 4;
            sp2YPos[enemyCount] = 16;
          }
          if (sp2YPos[enemyCount] >= 184 - sp2YPosInc[enemyCount]) {
            sp2YPosInc[enemyCount] = -4;
            sp2YPos[enemyCount] = 184;
          }
        }
      }
      intrinsic_halt();
      sp1_UpdateNow();

      // limit the xy position to screen area and "bounce" off edges
      if (sp1XPos <= -sp1XPosInc) {
        sp1XPosInc = 1;
        sp1XPos = 0;
      }
      if (sp1XPos >= 248 - sp1XPosInc) {
        sp1XPosInc = -1;
        sp1XPos = 248;
      }
      if (sp1YPos <= -sp1YPosInc + 16) {
        sp1YPosInc = 1;
        sp1YPos = 16;
      }
      if (sp1YPos >= 184 - sp1YPosInc) {
        sp1YPosInc = -1;
        sp1YPos = 184;
      }
      if (sp1XPosInc <= -MAX_SPEED_SACESHIP_1) sp1XPosInc = -MAX_SPEED_SACESHIP_1;
      if (sp1XPosInc >= MAX_SPEED_SACESHIP_1) sp1XPosInc = MAX_SPEED_SACESHIP_1;
      if (sp1YPosInc <= -MAX_SPEED_SACESHIP_1) sp1YPosInc = -MAX_SPEED_SACESHIP_1;
      if (sp1YPosInc >= MAX_SPEED_SACESHIP_1) sp1YPosInc = MAX_SPEED_SACESHIP_1;

      bonusCountdown--;
      if (bonusCountdown <= 0) {
        bonusEnabled = 255;
        bonusCountdown = BONUS_COUNTDOWN_INITIAL;
      }
      // check if we've hit anything, including the bonus's
      if (fire == 1)
      {       
        if (shot[shotCounter].shotLiveCountDown == 0) // only start next shot if old one done
        {
          shot[shotCounter].XBotCoord = sp1XPos+3;
          shot[shotCounter].YBotCoord = sp1YPos-5;
          shot[shotCounter].shotLen = 5;
          shot[shotCounter].shotLiveCountDown = 20;  
          shot[shotCounter].clearLast = 0;  // this is how many game loops need to pass before calling plotShotAndMoveUp
          bit_beep(5, 2000);
        }
        shotCounter++;
        if (shotCounter >= MAX_SHOTS_AT_ANY_TIME) shotCounter=0;
               

        unsigned char scoreIncrease = 0;
        for (unsigned char st = 0; st < MAX_SHOTS_AT_ANY_TIME; st++)
        {
          if (bonusEnabled > 0) 
          {
            if ((shot[st].XBotCoord-5 <= bonusshipPosX) && (shot[st].XBotCoord+5 >= bonusshipPosX) &&
                (shot[st].YBotCoord-30 <= BONUS_Y_POS) && (shot[st].YBotCoord >= BONUS_Y_POS))                
            {
              score += SCORE_WHEN_BONUS_HIT;
              bonusshipPosX = 0;
              bit_beep(100, 250);
              bit_beep(100, 500);
              bit_beep(100, 1000);
              bonusshipPosX = 0;
              bonusEnabled = 0;
              sp1_MoveSprPix(bonusship, & full_screen, 0, 255, 192);
            }
          }
          for (enemyCount = 0; enemyCount < MAX_ENEMY; enemyCount++) 
          {
            if (enemyEnabled[enemyCount] == 1) 
            {
              if ((shot[st].XBotCoord-5 <= sp2XPos[enemyCount]) && (shot[st].XBotCoord+5 >= sp2XPos[enemyCount]) &&
                  (shot[st].YBotCoord-30 <= sp2YPos[enemyCount]) && (shot[st].YBotCoord >= sp2YPos[enemyCount]))
              {
                scoreIncrease = 1;
                currentEnemyCount--;
#ifdef DEBUG_NO_MOVING_ENEMY
                sp2XPosInc[enemyCount] = 0;
                sp2YPosInc[enemyCount] = 0;
#else
                sp2XPosInc[enemyCount] = 4;
                sp2YPosInc[enemyCount] = 4;
#endif                
                sp2XPos[enemyCount] = 0;
                sp2YPos[enemyCount] = 0;
                enemyEnabled[enemyCount] = 0;
              }
            }
          }
        }
        //printDebug3(sp2XPos[1], sp2YPos[1]);
        
        if (scoreIncrease) {
          score += 100;
          bit_beep(5, 1000);
          // new life every 10000 
          if (score % 10000 == 0) {
            lives++;
          }
          // level up
          if (score % 500 == 0) {
            level++;
          }

          if (currentEnemyCount <= 0) currentEnemyCount = 0;

          for (enemyCount = 0; enemyCount < MAX_ENEMY; enemyCount++) {

            if ((enemyEnabled[enemyCount] == 0) && (currentEnemyCount < level + 1)) {
              enemyEnabled[enemyCount] = 1;
              sp2XPosInc[enemyCount] = -sp2XPosInc[0];
              sp2YPosInc[enemyCount] = -sp2YPosInc[0];
              sp2XPos[enemyCount] = (unsigned char)(rand() * 100);
              sp2YPos[enemyCount] = (unsigned char)(rand() * 100);

              sp1_MoveSprPix(enemyArray[enemyCount], & full_screen, 0, sp2XPos[enemyCount], sp2YPos[enemyCount]);

              currentEnemyCount++;
              //printDebug(&pss, currentEnemyCount);
            }
          }
          scoreIncrease = 0;
        }

        sp1XPosPrev = sp1XPos;
        sp1YPosPrev = sp1YPos;
        overWriteLine = 1;
        countdownTillFireAvailable = FIRE_CUTTOUT;
      }
     
         
      
      unsigned char st;
      for (st = 0; st < MAX_SHOTS_AT_ANY_TIME; st++)
      {
        plotShotAndMoveUp(&(shot[st]));
        shot[st].clearLast = 1;
      }
      printScores( & pss, score, highscore, lives, level);
      
      fire = 0;

      // check collision, this doesn't scale well to multiple enemys		
      for (enemyCount = 0; enemyCount < level; enemyCount++) {
        if ((sp1XPos + 7 >= sp2XPos[enemyCount]) && (sp1XPos <= sp2XPos[enemyCount] + 7) &&
          (sp1YPos + 7 >= sp2YPos[enemyCount]) && (sp1YPos <= sp2YPos[enemyCount] + 7)) {
          lives -= 1;
          // reset ships positions
          sp1XPos = 10;
          sp1YPos = 160;

          bit_beep(50, 700);
          bit_beep(50, 500);

          if (lives <= 0) {
            dead = 1;
            if (score > highscore) {
              highscore = score;
            }

            //sp1_SetPrintPos( & pss, 10, 10);
            //sp1_PrintString( & pss, "Game Over");

            for (short x = 0; x < 4; x++) {
              bit_beep(100, 500);
              bit_beep(100, 250);
            }
          }
          break;
        }
      }
    }
  }
}