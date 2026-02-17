/*
 * Example software running in Logisim RISC-V Computer System model by Pavel Gladyshev
 * licensed under Creative Commons Attribution International license 4.0
 *
 * This example shows how graphics display can be used to dsisplay a dot bouncing
 * off the screen borders.
 *
 * the position of the ball can be changed by typing new x,y coordinates on the keyboard
 * and pressing Enter key. Coordinate values are in 0..63 range. The origin (0,0) is in
 * the top left corner of the vga display
 *
 */

#include "vga.h"

/*
 * Macro VGA_PLOT(X,Y,COLOUR) plots a dot with the specified colour at (X,Y)
 * All macro parameters in the expression are surrounded by brackets to avoid
 * unintended operator precedence problems
 */
#define VGA_PLOT(X, Y, COLOUR) ((VGA)[((Y) * (VGA_WIDTH)) + (X)] = (COLOUR))

/* Colour of the bouncing dot 0xRRGGBB */
#define DOT_COLOUR (0x0000FF)

void main()
{
   // initiall position of the dot
   int x = 12;
   int y = 7;

   // speed of dot movement along X and Y axes
   int dx = 1, dy = 1;

   // temporary variables for computing next x and y coords
   int new_x, new_y;

   VGA_PLOT(x, y, DOT_COLOUR);

   // forever: 1) calculate new dot position
   // 2) if new position is outside boundaries
   //    change directions, recalculate positions
   // 3) erase dot in the old position
   // 4) plot the dot in the new position
   for (;;)
   {   
      new_x = x + dx;
      new_y = y + dy;

      if ((new_x < 0) || (new_x >= VGA_WIDTH))
      {
         dx = -dx;       // bounce along X
         new_x = x + dx; // recalculate
      }

      if ((new_y < 0) || (new_y >= VGA_HEIGHT))
      {
         dy = -dy;       // bounce along y
         new_y = y + dy; // reclaculate
      }

      VGA_PLOT(new_x, new_y, DOT_COLOUR); // paint it in new coordinates
      VGA_PLOT(x, y, 0x000000);           // erase the dot by making it black

      x = new_x;
      y = new_y; // remember new coordinates

   }
}