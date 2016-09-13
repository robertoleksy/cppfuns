
// Copyrighted (C) 2016 by developers of this project, as gift-ware (as in lib allegro 4)


#include <allegro.h>
#include <vector>

using namespace std;


struct c_ball {
	float m_x,m_y;
	float m_dx,m_dy;

	c_ball(float x, float y, float dx, float dy)
		: m_x(x), m_y(y), m_dx(dx), m_dy(dy)
	{ }
};

int main(void)
{


	//	nie_ma_takiej();

   BITMAP *buffer;
   BITMAP *page1, *page2;
   BITMAP *active_page;

   if (allegro_init() != 0)
      return 1;
   install_timer();
   install_keyboard();

   if (set_gfx_mode(GFX_AUTODETECT_WINDOWED, 1024, 768, 0, 2000) != 0) {
      if (set_gfx_mode(GFX_AUTODETECT, 320, 200, 0, 0) != 0) {
	 if (set_gfx_mode(GFX_SAFE, 320, 200, 0, 0) != 0) {
	    set_gfx_mode(GFX_TEXT, 0, 0, 0, 0);
	    allegro_message("Unable to set any graphic mode\n%s\n", allegro_error);
	    return 1;
         }
      }
   }
   set_palette(desktop_palette);

   /* allocate the memory buffer */
   buffer = create_bitmap(SCREEN_W, SCREEN_H);

   /* first with a double buffer... */
   clear_keybuf();

   destroy_bitmap(buffer);

   /* now create two video memory bitmaps for the page flipping */
   page1 = create_video_bitmap(SCREEN_W, SCREEN_H);
   page2 = create_video_bitmap(SCREEN_W, SCREEN_H);

   if ((!page1) || (!page2)) {
      set_gfx_mode(GFX_TEXT, 0, 0, 0, 0);
      allegro_message("Unable to create two video memory pages\n");
      return 1;
   }

   active_page = page2;

   /* do the animation using page flips... */
   clear_keybuf();
   bool done=0;

	vector<c_ball> tab;
	tab.push_back( c_ball( 50, 300, 0.4, 0  ) );
	tab.push_back( c_ball( 250, 400, 0.2, 0  ) );

   while (!done) {
      clear_to_color(active_page, makecol(255, 255, 255));
      textprintf_ex(active_page, font, 0, 0, makecol(0, 0, 0), -1,
		    "Page flipping (%s)", gfx_driver->name);

      //circlefill(active_page, c, SCREEN_H/2, 32, makecol(0, 0, 0));

      for(c_ball & obj : tab) {
      	obj.m_x += obj.m_dx;
      	obj.m_y += obj.m_dy;

   	  	obj.m_dy += 0.06;


				float ky = 700;
				if (obj.m_y > ky) {
					obj.m_y = ky;
					obj.m_dy *= -0.5;
				}
			}

      for(c_ball & obj : tab) {
	      circlefill(active_page, obj.m_x, obj.m_y, 32, makecol(0, 0, 0));
			}

      show_video_bitmap(active_page);
      if (active_page == page1) active_page = page2; else active_page = page1;

      rest(25);

      if (keypressed()) break;
   }

   destroy_bitmap(page1);
   destroy_bitmap(page2);

   return 0;
}
END_OF_MAIN()


