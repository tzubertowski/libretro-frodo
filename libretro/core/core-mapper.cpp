#include "libretro.h"
#include "libretro-core.h"
#include "retroscreen.h"
//CORE VAR
#ifdef _WIN32
char slash = '\\';
#else
char slash = '/';
#endif
extern const char *retro_save_directory;
extern const char *retro_system_directory;
extern const char *retro_content_directory;
char RETRO_DIR[512];

//TIME
#ifdef __CELLOS_LV2__
#include "sys/sys_time.h"
#include "sys/timer.h"
#define usleep  sys_timer_usleep
#else
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#endif

extern void Screen_SetFullUpdate(int scr);

long frame=0;
unsigned long  Ktime=0 , LastFPSTime=0;

//VIDEO
#ifdef  RENDER16B
	uint16_t Retro_Screen[1024*1024];
#else
	unsigned int Retro_Screen[1024*1024];
#endif 

//SOUND
short signed int SNDBUF[1024*2];
int snd_sampler = 44100 / 50;

//PATH
char RPATH[512];

//EMU FLAGS
int NPAGE=-1, KCOL=1, BKGCOLOR=0;
int SHOWKEY=-1;

int MAXPAS=6,SHIFTON=-1,MOUSE_EMULATED=-1,MOUSEMODE=-1,PAS=4;
int SND; //SOUND ON/OFF
static int firstps=0;
int pauseg=0; //enter_gui
int touch=-1; // gui mouse btn
//JOY
int al[2][2];//left analog1
int ar[2][2];//right analog1
unsigned char MXjoy[2]; // joy
int NUMjoy=1;

//MOUSE
extern int pushi;  // gui mouse btn
int gmx,gmy; //gui mouse

//KEYBOARD
char Key_Sate[512];
char Key_Sate2[512];

static int mbt[16]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

//STATS GUI
int BOXDEC= 32+2;
int STAT_BASEY;

/*static*/ retro_input_state_t input_state_cb;
static retro_input_poll_t input_poll_cb;

void retro_set_input_state(retro_input_state_t cb)
{
   input_state_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
   input_poll_cb = cb;
}

long GetTicks(void)
{ // in MSec
#ifndef _ANDROID_

#ifdef __CELLOS_LV2__

   //#warning "GetTick PS3\n"

   unsigned long	ticks_micro;
   uint64_t secs;
   uint64_t nsecs;

   sys_time_get_current_time(&secs, &nsecs);
   ticks_micro =  secs * 1000000UL + (nsecs / 1000);

   return ticks_micro/1000;
#else
   struct timeval tv;
   gettimeofday (&tv, NULL);
   return (tv.tv_sec*1000000 + tv.tv_usec)/1000;
#endif

#else

   struct timespec now;
   clock_gettime(CLOCK_MONOTONIC, &now);
   return (now.tv_sec*1000000 + now.tv_nsec/1000)/1000;
#endif

} 

//NO SURE FIND BETTER WAY TO COME BACK IN MAIN THREAD IN HATARI GUI
void gui_poll_events(void)
{
   Ktime = GetTicks();

   if(Ktime - LastFPSTime >= 1000/50)
   {
      frame++; 
      LastFPSTime = Ktime;		
      co_switch(mainThread);
   }
}

extern bool Dialog_DoProperty(void);

void enter_gui(void)
{
  //save_bkg();
	Dialog_DoProperty();
	pauseg=0;
}

void pause_select(void)
{
   if(pauseg==1 && firstps==0)
   {
      firstps=1;
      enter_gui();
      firstps=0;
   }
}


void texture_uninit(void)
{

}

void texture_init(void)
{
   memset(Retro_Screen, 0, sizeof(Retro_Screen));

   gmx=(retrow/2)-1;
   gmy=(retroh/2)-1;
}

void retro_key_down(unsigned char retrok)
{

}

void retro_key_up(unsigned char retrok)
{

}

int bitstart=0;
int pushi=0; //mouse button
int keydown=0,keyup=0;
int KBMOD=-1;

#include "main.h"
#include "C64.h"
#include "Display.h"
#include "Prefs.h"
#include "SAM.h"
extern C64 *TheC64;

void Process_key/*(void)*/(uint8 *key_matrix, uint8 *rev_matrix, uint8 *joystick)
{
   int i;

   keydown=0;keyup=0;

   for(i=0;i<320;i++)
   {
      	Key_Sate[i]=input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0,i) ? 0x80: 0;
      
        if(Key_Sate[i]  && Key_Sate2[i]==0)
        {

			if(i==RETROK_RALT){
				KBMOD=-KBMOD;
				printf("Modifier pressed %d \n",KBMOD); 
		        Key_Sate2[i]=1;
				continue;
			}
			
			/*the_app->*/TheC64->TheDisplay->Keymap_KeyDown(i,key_matrix,rev_matrix,joystick);

            //retro_key_down( i );
            		Key_Sate2[i]=1;
			bitstart=1;//
			keydown++;
        }
        else if ( !Key_Sate[i] && Key_Sate2[i]==1 )
        {

			if(i==RETROK_RALT){
				//KBMOD=-KBMOD;
				//printf("Modifier pressed %d \n",KBMOD); 
        		Key_Sate2[i]=0;
				continue;
			}

			
			/*the_app->*/TheC64->TheDisplay->Keymap_KeyUp(i,key_matrix,rev_matrix,joystick);

            //retro_key_up( i );
            		Key_Sate2[i]=0;
			bitstart=0;
			keyup++;

        }
      
   }



}

int Retro_PollEvent/*(void)*/(uint8 *key_matrix, uint8 *rev_matrix, uint8 *joystick)
{
	//   RETRO        B    Y    SLT  STA  UP   DWN  LEFT RGT  A    X    L    R    L2   R2   L3   R3
    //   INDEX        0    1    2    3    4    5    6    7    8    9    10   11   12   13   14   15
   int SAVPAS=PAS;	
   int i;

   input_poll_cb();

   int mouse_l;
   int mouse_r;
   int16_t mouse_x,mouse_y;
   mouse_x=mouse_y=0;

   if(SHOWKEY==-1)Process_key(key_matrix,rev_matrix,joystick);

   i=10;//show vkbd toggle
   if ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i) && mbt[i]==0 )
      mbt[i]=1;
   else if ( mbt[i]==1 && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i) )
   {
      mbt[i]=0;
      SHOWKEY=-SHOWKEY;
      Screen_SetFullUpdate(0);  
   }

   //mouse/joy toggle
   if ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, 2) && mbt[2]==0 )
      mbt[2]=1;
   else if ( mbt[2]==1 && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, 2) ){
      mbt[2]=0;
      MOUSE_EMULATED=-MOUSE_EMULATED;
   }


   if(MOUSE_EMULATED==1){

      //TODO FIX THIS :(
#if defined(__CELLOS_LV2__) 
      //Slow Joypad Mouse Emulation for PS3
      static int pair=-1;
      pair=-pair;
      if(pair==1)return;
      PAS=1;
#elif defined(GEKKO) 
      PAS=1;
#endif

      if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT))mouse_x += PAS;
      if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT))mouse_x -= PAS;
      if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN))mouse_y += PAS;
      if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP))mouse_y -= PAS;
      mouse_l=input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A);
      mouse_r=input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B);

      PAS=SAVPAS;
   }
   else {

      mouse_x = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
      mouse_y = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y);
      mouse_l    = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT);
      mouse_r    = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT);
   }

   static int mmbL=0,mmbR=0;

   if(mmbL==0 && mouse_l){

      mmbL=1;		
      pushi=1;

   }
   else if(mmbL==1 && !mouse_l) {

      mmbL=0;
      pushi=0;
   }

   if(mmbR==0 && mouse_r){
      mmbR=1;		
   }
   else if(mmbR==1 && !mouse_r) {
      mmbR=0;
   }

   gmx+=mouse_x;
   gmy+=mouse_y;
   if(gmx<0)gmx=0;
   if(gmx>retrow-1)gmx=retrow-1;
   if(gmy<0)gmy=0;
   if(gmy>retroh-1)gmy=retroh-1;



return 1;

}


int input_gui(void)
{

/*
	unsigned char ginputstate =0;

	input_poll_cb();
	
	
     if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT))
         ginputstate|= 0x8;
      if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT))
         ginputstate|= 0x4;
      if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN))
         ginputstate|= 0x2;
      if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP))
         ginputstate|= 0x1;
      if(input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A))
         ginputstate|= 0x10;
	if(input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B))
		ginputstate|=0x20;


	return ginputstate;
*/
#if 1
   int SAVPAS=PAS;	

   input_poll_cb();

   int mouse_l;
   int mouse_r;
   int16_t mouse_x,mouse_y;
   mouse_x=mouse_y=0;

   //mouse/joy toggle
   if ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, 2) && mbt[2]==0 )
      mbt[2]=1;
   else if ( mbt[2]==1 && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, 2) )
   {
      mbt[2]=0;
      MOUSEMODE=-MOUSEMODE;
   }

   if(MOUSEMODE==1)
   {
      //TODO FIX THIS :(
#if defined(__CELLOS_LV2__) 
      //Slow Joypad Mouse Emulation for PS3
      static int pair=-1;
      pair=-pair;
      if(pair==1)
         return;
      PAS=1;
#elif defined(GEKKO) 
      PAS=1;
#endif

      if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT))
         mouse_x += PAS;
      if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT))
         mouse_x -= PAS;
      if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN))
         mouse_y += PAS;
      if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP))
         mouse_y -= PAS;
      mouse_l=input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A);
      mouse_r=input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B);

      PAS=SAVPAS;
   }
   else
   {
      mouse_x = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
      mouse_y = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y);
      mouse_l    = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT);
      mouse_r    = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT);
   }

   static int mmbL = 0, mmbR = 0;

   if(mmbL==0 && mouse_l)
   {
      mmbL=1;		
      touch=1;
   }
   else if(mmbL==1 && !mouse_l)
   {
      mmbL=0;
      touch=-1;
   }

   if(mmbR==0 && mouse_r)
      mmbR=1;		
   else if(mmbR==1 && !mouse_r)
      mmbR=0;

   gmx+=mouse_x;
   gmy+=mouse_y;
   if (gmx<0)
      gmx=0;
   if (gmx>retrow-1)
      gmx=retrow-1;
   if (gmy<0)
      gmy=0;
   if (gmy>retroh-1)
      gmy=retroh-1;
#endif

}

int browser_input(void)
{
   int ret=0;	
   static int dskflag[7]={0,0,0,0,0,0,0};// UP -1 DW 1 A 2 B 3 LFT -10 RGT 10 X 4	

  // input_poll_cb();	

   if ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP) && dskflag[0]==0 )
   {
      dskflag[0]=1;
      //ret= -1; 
   }
   else if (dskflag[0]==1/* && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP)*/ )
   {
      dskflag[0]=0;
      ret= -1; 
   }
/*
   else
      dskflag[0]=0;
*/

   if ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN) && dskflag[1]==0 )
   {
      dskflag[1]=1;
     // ret= 1;
   } 
   else if (dskflag[1]==1 /*&& ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN)*/ )
   {
      dskflag[1]=0;
      ret= 1; 
   }
/*
   else
      dskflag[1]=0;
*/

   if ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT) && dskflag[4]==0 )
      dskflag[4]=1;
   else if (dskflag[4]==1 && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT) )
   {
      dskflag[4]=0;
      ret= -10; 
   }

   if ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT) && dskflag[5]==0 )
      dskflag[5]=1;
   else if (dskflag[5]==1 && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT) )
   {
      dskflag[5]=0;
      ret= 10; 
   }

   if ( ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A) || \
            input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_LCTRL) ) && dskflag[2]==0 )
      dskflag[2]=1;

   else if (dskflag[2]==1 && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A)\
         && !input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_LCTRL) )
   {
      dskflag[2]=0;
      ret= 2;
   }

   if ( ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B) || \
            input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_LALT) ) && dskflag[3]==0 )
      dskflag[3]=1;
   else if (dskflag[3]==1 && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B) &&\
         !input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_LALT) )
   {
      dskflag[3]=0;
      ret= 3;
   }

   if ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X) && dskflag[6]==0 )
      dskflag[6]=1;
   else if (dskflag[6]==1 && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X) )
   {
      dskflag[6]=0;
      ret= 4; 
   }

   return ret;
}
