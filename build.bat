set PATH=PATH;e:\z88dk\;e:\z88dk\bin;e:\z88dk\lib\config\
REM zcc +zx -vn -m -startup=31 -clib=sdcc_iy main.c sprite.asm -o main -create-app
zcc +zx -vn -m -startup=31 -clib=sdcc_iy main.c spaceship1.asm spaceship2.asm -o main -create-app -lm

pause

call main.tap