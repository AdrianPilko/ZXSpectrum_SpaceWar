// SPACE WAR for Sinclair ZX Spectrum 48K, Adrian Pilkington 2023

// some of this code started from the excellent sprite code example from here: 
//https://github.com/z88dk/z88dk/blob/master/doc/ZXSpectrumZSDCCnewlib_SP1_02_SimpleMaskedSprite.md

// zcc +zx -vn -m -startup=31 -clib=sdcc_iy main.c sprite.asm -o main -create-app

// really useful link to how to handle sprites:
// https://www.z88dk.org/wiki/doku.php?id=libnew:examples:sp1_ex1
// link to how to draw lines etc:
// https://github.com/z88dk/z88dk/blob/master/doc/ZXSpectrumZSDCCnewlib_03_SimpleGraphics.md

#pragma output REGISTER_SP = 0xD000

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

#define MAX_SPEED_SACESHIP_1  6
#define FIRE_CUTTOUT 6

extern unsigned char spaceship1_masked[];
extern unsigned char spaceship2_masked[];

struct sp1_Rect full_screen = {0, 0, 32, 24};

void plot(unsigned char x, unsigned char y)
{
	*zx_pxy2saddr(x,y) |= zx_px2bitmask(x);
}

void clearplot(unsigned char x, unsigned char y)
{
	*zx_pxy2saddr(x,y) &= 0;
}

void line(unsigned char x0, unsigned char y0, unsigned char x1, unsigned char y1)
{	
	unsigned char dx  = abs(x1-x0);
	unsigned char dy  = abs(y1-y0);	
	signed   char sx  = x0<x1 ? 1 : -1;
	signed   char sy  = y0<y1 ? 1 : -1;
	int           err = (dx>dy ? dx : -dy)/2;
	int           e2;

	while (1)
	{
		plot(x0,y0);
		if (x0==x1 && y0==y1) break;

		e2 = err;
		if (e2 >-dx) { err -= dy; x0 += sx; }
		if (e2 < dy) { err += dx; y0 += sy; }
	}
}

void vertline(unsigned char x0, unsigned char y0, unsigned char y1)
{		
	unsigned char i;
	for (i = y1; i < y0; i++)
	{
		plot(x0,i);		
	}
}


void clearVertline(unsigned char x0, unsigned char y0, unsigned char y1)
{	
	unsigned char i;
	for (i = y1; i < y0; i++)
	{
		clearplot(x0,i);		
	}
}

void printScores(struct sp1_pss* pss, int score, int highScore, short lives)
{
	char buffer[7];
	sp1_SetPrintPos(pss, 0, 0);   
	sp1_PrintString(pss, "1UP Score--SPACE-WAR--High Score");
	sp1_SetPrintPos(pss, 1, 0);   
	sp1_PrintString(pss, "           Lives                ");
	
	
	sprintf(buffer, "%d", score);
	sp1_SetPrintPos(pss, 1, 3);   
	sp1_PrintString(pss, buffer);
	sprintf(buffer, "%d", highScore);
	sp1_SetPrintPos(pss, 1, 25);   	
	sp1_PrintString(pss, buffer);
	sp1_SetPrintPos(pss, 1, 17);   
	sprintf(buffer, "%d", lives);
	sp1_PrintString(pss, buffer);
}

unsigned char colour;
unsigned char cmask;

void colourSpr(unsigned int count, struct sp1_cs *c)
{
   c->attr_mask = cmask;
   c->attr = colour;
}


int main()
{
  int score;
  int highscore;
  int dead;  
  short lives;
  
  unsigned char countdownTillFireAvailable;
  unsigned char overWriteLine;
  unsigned char c;
  
  struct sp1_ss  *spaceship1;
  struct sp1_ss  *spaceship2;

  unsigned char sp1XPos;
  short sp1XPosInc;
  short sp1YPosInc;
  short sp2XPosInc;
  short sp2YPosInc;
  
  unsigned char sp2XPos;
  unsigned char sp1YPos;
  unsigned char sp2YPos;
  unsigned char sp1XPosPrev;
  unsigned char sp1YPosPrev;
  unsigned char fire;
  

  memset( TABLE_ADDR, JUMP_POINT_HIGH_BYTE, 257 );
  z80_bpoke( JUMP_POINT,   195 );
  z80_wpoke( JUMP_POINT+1, (unsigned int)isr );
  im2_init( TABLE_ADDR );
  intrinsic_ei();

  zx_border(INK_YELLOW);
  
  sp1_Initialize( SP1_IFLAG_MAKE_ROTTBL | SP1_IFLAG_OVERWRITE_TILES | SP1_IFLAG_OVERWRITE_DFILE,
                  INK_YELLOW | PAPER_BLACK,' ' );
				  
  sp1_Invalidate(&full_screen);
 
  spaceship1 = sp1_CreateSpr(SP1_DRAW_MASK2LB, SP1_TYPE_2BYTE, 2, (int)spaceship1_masked, 0);
  spaceship2 = sp1_CreateSpr(SP1_DRAW_MASK2LB, SP1_TYPE_2BYTE, 2, (int)spaceship2_masked, 0);

  sp1_AddColSpr(spaceship1, SP1_DRAW_MASK2RB, SP1_TYPE_2BYTE, 0, 0);
  colour = (INK_CYAN | PAPER_BLACK);   
  cmask = SP1_AMASK_INK & SP1_AMASK_PAPER; 
  sp1_IterateSprChar(spaceship1, colourSpr);
	  
  sp1_AddColSpr(spaceship2, SP1_DRAW_MASK2RB, SP1_TYPE_2BYTE, 0, 0);
  colour = (INK_RED | PAPER_BLACK);   
  cmask = SP1_AMASK_INK & SP1_AMASK_PAPER; 
  sp1_IterateSprChar(spaceship2, colourSpr);
  
	  
  struct sp1_pss pss = {
	   &full_screen,   // print confined to this rectangle
	   SP1_PSSFLAG_INVALIDATE | SP1_PSSFLAG_XWRAP | SP1_PSSFLAG_YINC | SP1_PSSFLAG_YWRAP,
	   0,    // current x
	   0,    // current y
	   0,    // attribute mask - overwrite underlying colour
	   INK_BLACK | PAPER_WHITE,
	   0,    // sp1_update* must be consistent with x,y
	   0    // visit function (set to zero)
  };
  highscore = 0;
  while (1)  // main game loop
  {
	sp1_Initialize( SP1_IFLAG_MAKE_ROTTBL | SP1_IFLAG_OVERWRITE_TILES | SP1_IFLAG_OVERWRITE_DFILE,
                  INK_YELLOW | PAPER_BLACK,' ' );
				  
    sp1_Invalidate(&full_screen);
 
    spaceship1 = sp1_CreateSpr(SP1_DRAW_MASK2LB, SP1_TYPE_2BYTE, 2, (int)spaceship1_masked, 0);
    spaceship2 = sp1_CreateSpr(SP1_DRAW_MASK2LB, SP1_TYPE_2BYTE, 2, (int)spaceship2_masked, 0);

    sp1_AddColSpr(spaceship1, SP1_DRAW_MASK2RB, SP1_TYPE_2BYTE, 0, 0);
    colour = (INK_CYAN | PAPER_BLACK);   
    cmask = SP1_AMASK_INK & SP1_AMASK_PAPER; 
    sp1_IterateSprChar(spaceship1, colourSpr);
	  
    sp1_AddColSpr(spaceship2, SP1_DRAW_MASK2RB, SP1_TYPE_2BYTE, 0, 0);
    colour = (INK_RED | PAPER_BLACK);   
    cmask = SP1_AMASK_INK & SP1_AMASK_PAPER; 
    sp1_IterateSprChar(spaceship2, colourSpr);
  
	// initialise everything else  
	sp1XPos=10;
	sp2XPos=100;
	sp1YPos=160;
	sp2YPos=20;
	sp1XPosInc = 1;
	sp1YPosInc = 1;
	sp2XPosInc = 4;
	sp2YPosInc = 4;	
	countdownTillFireAvailable = FIRE_CUTTOUT;
	
	score = 0;
	dead = 0;   /// dead = 1 not dead = 0;
	lives = 3;
	 
	printScores(&pss, score, highscore, lives);
	   
	while(dead == 0) 
	{
		printScores(&pss, score, highscore,lives);
		
		c = in_inkey();
		
		switch (c)
		{
			case 'o' : sp1XPosInc-=1; break;
			case 'p' : sp1XPosInc+=1; break;
			case 'q' : sp1YPosInc-=1; break;
			case 'a' : sp1YPosInc+=1; break;		
			case ' ' : fire=1; break;
			default:
			break;
		}
		sp1XPos = sp1XPosInc + sp1XPos;
		sp1YPos = sp1YPosInc + sp1YPos;
		sp2XPos = sp2XPosInc + sp2XPos;
		sp2YPos = sp2YPosInc + sp2YPos;
		
		sp1_MoveSprPix(spaceship1, &full_screen, 0, sp1XPos, sp1YPos);
		sp1_MoveSprPix(spaceship2, &full_screen, 0, sp2XPos, sp2YPos);
		intrinsic_halt();
		sp1_UpdateNow();
		
		// limit the xy position to screen area and "bounce" off edges
		if (sp1XPos<=-sp1XPosInc) { sp1XPosInc = 1; sp1XPos = 0; }
		if (sp1XPos>=248-sp1XPosInc) { sp1XPosInc = -1; sp1XPos = 248; }
		if (sp1YPos<=-sp1YPosInc+16) { sp1YPosInc = 1; sp1YPos = 16; }
		if (sp1YPos>=184-sp1YPosInc) { sp1YPosInc = -1; sp1YPos = 184; }
		
		if (sp2XPos<=-sp2XPosInc) { sp2XPosInc = 4; sp2XPos = 0; }
		if (sp2XPos>=248-sp1XPosInc) { sp2XPosInc = -4; sp2XPos = 248; }
		if (sp2YPos<=-sp2YPosInc+16) { sp2YPosInc = 4; sp2YPos = 16; }
		if (sp2YPos>=184-sp2YPosInc) { sp2YPosInc = -4; sp2YPos = 184; }
		
		if (sp1XPosInc<=-MAX_SPEED_SACESHIP_1) sp1XPosInc = -MAX_SPEED_SACESHIP_1;
		if (sp1XPosInc>=MAX_SPEED_SACESHIP_1) sp1XPosInc = MAX_SPEED_SACESHIP_1;
		if (sp1YPosInc<=-MAX_SPEED_SACESHIP_1) sp1YPosInc = -MAX_SPEED_SACESHIP_1;
		if (sp1YPosInc>=MAX_SPEED_SACESHIP_1) sp1YPosInc = MAX_SPEED_SACESHIP_1;
		
		if (overWriteLine == 1)
		{
			overWriteLine = 0;
			clearVertline(sp1XPosPrev, sp1YPosPrev, 16);
			clearVertline(sp1XPosPrev+7, sp1YPosPrev, 16);
		}
		
		if ((fire == 1) && (countdownTillFireAvailable-- == 0))
		{			
			vertline(sp1XPos, sp1YPos, 16);
			vertline(sp1XPos+7, sp1YPos, 16);
			
			bit_beep(10,1000);
			
			if ((sp1XPos+7 >= sp2XPos) && (sp1XPos <= sp2XPos+7))
			{
				score+=100;
				bit_beep(100,500);
			}	
			sp1XPosPrev = sp1XPos;
			sp1YPosPrev = sp1YPos;
			overWriteLine = 1;
			countdownTillFireAvailable = FIRE_CUTTOUT;
		}
		fire = 0;
		// check collision, this doesn't scale well to multiple enemys
		if ((sp1XPos+7 >= sp2XPos) && (sp1XPos <= sp2XPos+7) &&
			 (sp1YPos+7 >= sp2YPos) && (sp1YPos <= sp2YPos+7))
		{
			lives -= 1;
			// reset ships positions
			sp1XPos=10;
			sp2XPos=100;
			sp1YPos=160;
			sp2YPos=20;			
			
			bit_beep(200,700);				
			bit_beep(200,500);				
			

			if (lives<= 0) 
			{
				dead = 1;
				if (score > highscore) 
				{
					highscore = score;						
				}				
				
				sp1_SetPrintPos(&pss, 10, 10);   
				sp1_PrintString(&pss, "Game Over");
				
				
				for (short x = 0; x < 4; x++)
				{
					bit_beep(200,500);				
					bit_beep(200,250);				
				}
			}
		}	
	  }
  }
}