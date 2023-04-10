// SPACE WAR for Sinclair ZX Spectrum 48K, Adrian Pilkington 2023

// some of this code started from the excellent sprite code example from here: 
//https://github.com/z88dk/z88dk/blob/master/doc/ZXSpectrumZSDCCnewlib_SP1_02_SimpleMaskedSprite.md

// zcc +zx -vn -m -startup=31 -clib=sdcc_iy main.c sprite.asm -o main -create-app

#pragma output REGISTER_SP = 0xD000

#include <arch/zx.h>
#include <arch/zx/sp1.h>
#include <intrinsic.h>
#include <z80.h>
#include <im2.h>
#include <string.h>
#include <input.h>

IM2_DEFINE_ISR(isr) {}
#define TABLE_HIGH_BYTE        ((unsigned int)0xD0)
#define JUMP_POINT_HIGH_BYTE   ((unsigned int)0xD1)

#define UI_256                 ((unsigned int)256)
#define TABLE_ADDR             ((void*)(TABLE_HIGH_BYTE*UI_256))
#define JUMP_POINT             ((unsigned char*)( (unsigned int)(JUMP_POINT_HIGH_BYTE*UI_256) + JUMP_POINT_HIGH_BYTE ))

extern unsigned char spaceship1_masked[];
extern unsigned char spaceship2_masked[];

struct sp1_Rect full_screen = {0, 0, 32, 24};

int main()
{
  unsigned char c;
  
  struct sp1_ss  *spaceship1;
  struct sp1_ss  *spaceship2;

  unsigned char sp1XPos;
  unsigned char sp1XPosInc;
  unsigned char sp1YPosInc;
  
  unsigned char sp2XPos;
  unsigned char sp1YPos;
  unsigned char sp2YPos;
  unsigned char fire;
  

  memset( TABLE_ADDR, JUMP_POINT_HIGH_BYTE, 257 );
  z80_bpoke( JUMP_POINT,   195 );
  z80_wpoke( JUMP_POINT+1, (unsigned int)isr );
  im2_init( TABLE_ADDR );
  intrinsic_ei();

  zx_border(INK_YELLOW);

  sp1_Initialize( SP1_IFLAG_MAKE_ROTTBL | SP1_IFLAG_OVERWRITE_TILES | SP1_IFLAG_OVERWRITE_DFILE,
                  INK_YELLOW | PAPER_BLACK,
                  ' ' );
				  
  sp1_Invalidate(&full_screen);
 
  spaceship1 = sp1_CreateSpr(SP1_DRAW_MASK2LB, SP1_TYPE_2BYTE, 2, (int)spaceship1_masked, 0);
  spaceship2 = sp1_CreateSpr(SP1_DRAW_MASK2LB, SP1_TYPE_2BYTE, 2, (int)spaceship2_masked, 0);

  sp1_AddColSpr(spaceship1, SP1_DRAW_MASK2RB, SP1_TYPE_2BYTE, 0, 0);
  sp1_AddColSpr(spaceship2, SP1_DRAW_MASK2RB, SP1_TYPE_2BYTE, 0, 0);

  sp1XPos=10;
  sp2XPos=100;
  sp1YPos=160;
  sp2YPos=20;
  sp1XPosInc = 1;
  sp1YPosInc = 1;
   
  while(1) {
	c = in_inkey();
	
	switch (c)
	{
		case 'o' : sp1XPosInc=-1; break;
		case 'p' : sp1XPosInc=1; break;
		case 'q' : sp1YPosInc=-1; break;
		case 'a' : sp1YPosInc=1; break;		
		case ' ' : fire=1; break;
		default:
		break;
	}
	sp1XPos = sp1XPosInc + sp1XPos;
	sp1YPos = sp1YPosInc + sp1YPos;
	sp2XPos+=1;
	
    sp1_MoveSprPix(spaceship1, &full_screen, 0, sp1XPos, sp1YPos);
	sp1_MoveSprPix(spaceship2, &full_screen, 0, sp2XPos, sp2YPos);
    intrinsic_halt();
    sp1_UpdateNow();
	
	// limit the xy position to screen area and "bounce" off edges
	if (sp1XPos<=0) sp1XPosInc = 1;
	if (sp1XPos>=248) sp1XPosInc = -1;
	if (sp1YPos<=0) sp1YPosInc = 1;
	if (sp1YPos>=184) sp1YPosInc = -1;
	
	
	
	fire = 0;
  }
}