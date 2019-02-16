// WL_DRAW.C

#include "WL_DEF.H"
#include <DOS.H>
#pragma hdrstop

//#define DEBUGWALLS

/*
=============================================================================

                                                 LOCAL CONSTANTS

=============================================================================
*/

// the door is the last picture before the sprites
#define DOORWALL        (PMSpriteStart-20)

#define ACTORSIZE       0x4000

/*
=============================================================================

                                                 GLOBAL VARIABLES

=============================================================================
*/

// Global definitions 
#ifndef   FOCALLENGTH 
#define   FOCALLENGTH            0x5700L               // MUST match the FOCALLENGTH definition in WL_MAIN.C 
#endif 
#define   FOCALTHRESHOLD         FOCALLENGTH            // Greater lengths won't change the view position 

long playx,playy;

#ifdef DEBUGWALLS
unsigned screenloc[3]= {0,0,0};
#else
unsigned screenloc[3]= {PAGE1START,PAGE2START,PAGE3START};
#endif
unsigned freelatch = FREESTART;

long    lasttimecount;
long    frameon;
boolean fpscounter;

int         fps_frames=0, fps_time=0, fps=0;

int     messagetime=0;
int     signmessagetime=0;
int     lyricstime=0;

int wallheight[MAXVIEWWIDTH],min_wallheight;

//
// math tables
//
short pixelangle[MAXVIEWWIDTH];
long finetangent[FINEANGLES/4];
fixed sintable[ANGLES+ANGLES/4];
fixed *costable = sintable+(ANGLES/4);

//
// refresh variables
//
fixed   viewx,viewy;                    // the focal point
short viewangle;
fixed   viewsin,viewcos;

void    TransformActor (objtype *ob);
void    BuildTables (void);
void    ClearScreen (void);
int     CalcRotate (objtype *ob);
void    DrawScaleds (void);
void    CalcTics (void);
void    FixOfs (void);
void    ThreeDRefresh (void);



//
// wall optimization variables
//
int     lastside;               // true for vertical
long    lastintercept;
int     lasttilehit;
int     lasttexture;

//
// ray tracing variables
//
short focaltx,focalty,viewtx,viewty;
longword xpartialup,xpartialdown,ypartialup,ypartialdown;

short midangle,angle;

word tilehit;
int pixx;

short xtile,ytile;
short xtilestep,ytilestep;
long xintercept,yintercept;
word xstep,ystep; // long
word xspot,yspot;
int texdelta;

word horizwall[MAXWALLTILES],vertwall[MAXWALLTILES];

extern fixed FixedByFrac (fixed a, fixed b);
extern fixed FixedMul(fixed a, fixed b);


/*
============================================================================

                           3 - D  DEFINITIONS

============================================================================
*/

/*
========================
=
= TransformActor
=
= Takes paramaters:
=   gx,gy               : globalx/globaly of point
=
= globals:
=   viewx,viewy         : point of view
=   viewcos,viewsin     : sin/cos of viewangle
=   scale               : conversion from global value to screen value
=
= sets:
=   screenx,transx,transy,screenheight: projected edge location and size
=
========================
*/


//
// transform actor
//
void TransformActor (objtype *ob)
{
        fixed gx,gy,gxt,gyt,nx,ny;

//
// translate point to view centered coordinates
//
        gx = ob->x-viewx;
        gy = ob->y-viewy;

//
// calculate newx
//
        gxt = FixedMul(gx,viewcos);
        gyt = FixedMul(gy,viewsin);
        nx = gxt-gyt-ACTORSIZE;         // fudge the shape forward a bit, because
                                                                // the midpoint could put parts of the shape
                                                                // into an adjacent wall

//
// calculate newy
//
        gxt = FixedMul(gx,viewsin);
        gyt = FixedMul(gy,viewcos);
        ny = gyt+gxt;

//
// calculate perspective ratio
//
        ob->transx = nx;
        ob->transy = ny;

        if (nx<mindist)                 // too close, don't overflow the divide
        {
          ob->viewheight = 0;
          return;
        }

        ob->viewx = (word)(centerx + ny*scale/nx);      // DEBUG: use assembly divide

//
// calculate height (heightnumerator/(nx>>8))
//
        ob->viewheight = (word)(heightnumerator/(nx>>8));
}

//==========================================================================

/*
========================
=
= TransformTile
=
= Takes paramaters:
=   tx,ty               : tile the object is centered in
=
= globals:
=   viewx,viewy         : point of view
=   viewcos,viewsin     : sin/cos of viewangle
=   scale               : conversion from global value to screen value
=
= sets:
=   screenx,transx,transy,screenheight: projected edge location and size
=
= Returns true if the tile is withing getting distance
=
========================
*/

boolean TransformTile (int tx, int ty, short *dispx, short *dispheight)
{
        fixed gx,gy,gxt,gyt,nx,ny;

//
// translate point to view centered coordinates
//
        gx = ((long)tx<<TILESHIFT)+0x8000-viewx;
        gy = ((long)ty<<TILESHIFT)+0x8000-viewy;

//
// calculate newx
//
        gxt = FixedMul(gx,viewcos);
        gyt = FixedMul(gy,viewsin);
        nx = gxt-gyt-0x2000;            // 0x2000 is size of object

//
// calculate newy
//
        gxt = FixedMul(gx,viewsin);
        gyt = FixedMul(gy,viewcos);
        ny = gyt+gxt;


//
// calculate height / perspective ratio
//
        if (nx<mindist)                 // too close, don't overflow the divide
                *dispheight = 0;
        else
        {
                *dispx = (short)(centerx + ny*scale/nx);        // DEBUG: use assembly divide
                *dispheight = (short)(heightnumerator/(nx>>8));
        }

//
// see if it should be grabbed
//
        if (nx<TILEGLOBAL && ny>-TILEGLOBAL/2 && ny<TILEGLOBAL/2)
                return true;
        else
                return false;
}

//==========================================================================

/*
====================
=
= CalcHeight
=
= Calculates the height of xintercept,yintercept from viewx,viewy
=
====================
*/

int CalcHeight();
#pragma aux CalcHeight = \
        "mov    eax,dword ptr xintercept" \
        "sub    eax,dword ptr viewx" \
        "imul   dword ptr viewcos" \
        "shrd   eax,edx,16" \
        "mov    edi,eax" \
        "mov    eax,dword ptr yintercept" \
        "sub    eax,dword ptr viewy" \
        "imul   dword ptr viewsin" \
        "shrd   eax,edx,16" \
        "sub    edi,eax" \
        "cmp    edi,1024" \
        "jge    dontclip" \
        "mov    edi,1024" \
        "dontclip:" \
        "xor    edx,edx" \
        "sar    edi,9" \
        "mov    eax,dword ptr heightnumerator" \
        "sar    eax,1" \
        "div    edi" \
        value [eax] \
        modify exact [eax edx edi]

//==========================================================================

/*
===================
=
= ScalePost
=
===================
*/

byte *postsource;
int postx;
int postwidth;

// usage: masks[width-1][xpos&3]
byte masks[4][4]={{1,2,4,8},{3,6,12,0},{7,14,0,0},{15,0,0,0}};

void ScalePost()
{
   int ywcount,yoffs,yw,yd,yendoffs;
   byte col;

   VGAMAPMASK(masks[postwidth-1][postx&3]);
   ywcount=yd=wallheight[postx]>>3;

   if(yd<=0) yd=100;
   yoffs=(viewheight/2-ywcount)*80;
   if(yoffs<0) yoffs=0;
   yoffs+=postx>>2;
   yendoffs=viewheight/2+ywcount-1;
   yw=63;
   while(yendoffs>=viewheight)
   {
      ywcount-=32;
      while(ywcount<=0)
      {
         ywcount+=yd;
         yw--;
      }
      yendoffs--;
   }
   if(yw<0) return;
   col=((byte *)postsource)[yw];
   yendoffs=yendoffs*80+(postx>>2);
   while(yoffs<=yendoffs)
   {
      vbuf[yendoffs]=col;
      ywcount-=32;
      if(ywcount<=0)
      {
         do
         {
            ywcount+=yd;
            yw--;
         }
         while(ywcount<=0);
         if(yw<0) break;
         col=postsource[yw];
      }
      yendoffs-=80;
   }
}


/*
====================
=
= HitVertWall
=
= tilehit bit 7 is 0, because it's not a door tile
= if bit 6 is 1 and the adjacent tile is a door tile, use door side pic
=
====================
*/

void HitVertWall (void)
{
        int wallpic;
        int texture;

        texture = ((yintercept+texdelta)>>4)&0xfc0;
        if (xtilestep == -1)
        {
                texture = 0xfc0-texture;
                xintercept += TILEGLOBAL;
        }

        if(lastside==1 && lastintercept==xtile && lasttilehit==tilehit && !(lasttilehit & 0x40))
        {
                if((pixx&3) && texture == lasttexture)
                {
                        postwidth++;
                        wallheight[pixx] = wallheight[pixx-1];
                        return;
                }
                ScalePost();
                wallheight[pixx] = CalcHeight();
                postsource+=texture-lasttexture;
                postwidth=1;
                postx=pixx;
                lasttexture=texture;
                return;
        }

        if(lastside!=-1) ScalePost();

        lastside=1;
        lastintercept=xtile;
        lasttilehit=tilehit;
        lasttexture=texture;
        wallheight[pixx] = CalcHeight();
        postx = pixx;
        postwidth = 1;

        if (tilehit & 0x40)
        {                                                               // check for adjacent doors
         ytile = (short)(yintercept>>TILESHIFT);
         if ( tilemap[xtile-xtilestep][ytile]&0x80 )
          { 
            switch(doorobjlist[tilemap[xtile-xtilestep][ytile]&0x7f].lock) 
            {                                                                                        
               case dr_normal:
			   case dr_lock1:
               case dr_lock2:	
               case dr_lock3:
               case dr_lock4: 		
                  wallpic = DOORWALL+15;
                  break;                    
               case dr_elevator:	
                  wallpic = DOORWALL-88;
                  break;  
               case dr_glass:	
                  wallpic = DOORWALL-42;
                  break;                                  
               }   	  		  		     	     
           }
           else
                 wallpic = vertwall[tilehit & ~0x40];
        }       	
        else
                wallpic = vertwall[tilehit];

        postsource = Pages+(wallpic<<12)+texture;
}


/*
====================
=
= HitHorizWall
=
= tilehit bit 7 is 0, because it's not a door tile
= if bit 6 is 1 and the adjacent tile is a door tile, use door side pic
=
====================
*/

void HitHorizWall (void)
{
        int wallpic;
        int texture;

        texture = ((xintercept+texdelta)>>4)&0xfc0;
        if (ytilestep == -1)
                yintercept += TILEGLOBAL;
        else
                texture = 0xfc0-texture;

        if(lastside==0 && lastintercept==ytile && lasttilehit==tilehit && !(lasttilehit & 0x40))
        {
                if((pixx&3) && texture == lasttexture)
                {
                        postwidth++;
                        wallheight[pixx] = wallheight[pixx-1];
                        return;
                }
                ScalePost();
                wallheight[pixx] = CalcHeight();
                postsource+=texture-lasttexture;
                postwidth=1;
                postx=pixx;
                lasttexture=texture;
                return;
        }

        if(lastside!=-1) ScalePost();

        lastside=0;
        lastintercept=ytile;
        lasttilehit=tilehit;
        lasttexture=texture;
        wallheight[pixx] = CalcHeight();
        postx = pixx;
        postwidth = 1;

        if (tilehit & 0x40)
        {                                                               // check for adjacent doors
           xtile = (short)(xintercept>>TILESHIFT);
           if ( tilemap[xtile][ytile-ytilestep]&0x80)
            { 
            switch(doorobjlist[tilemap[xtile][ytile-ytilestep]&0x7f].lock) 
            {                                                                                          
               case dr_normal:
			   case dr_lock1:
               case dr_lock2:
               case dr_lock3:
               case dr_lock4: 			
                  wallpic = DOORWALL+14;
                  break;                   
               case dr_elevator:	
                  wallpic = DOORWALL-87;
                  break;  
               case dr_glass:	
                  wallpic = DOORWALL-41;
                  break;                            
               }   	  		  		     	     
           }
           else
                 wallpic = horizwall[tilehit & ~0x40];
        }       	
        else
                wallpic = horizwall[tilehit];

        postsource = Pages+(wallpic<<12)+texture;
}

//==========================================================================

/*
====================
=
= HitHorizDoor
=
====================
*/

void HitHorizDoor (void)
{
        int doorpage;
        int doornum;
        int texture;

        doornum = tilehit&0x7f;
        texture = ((xintercept-doorposition[doornum])>>4)&0xfc0;

        if(lasttilehit==tilehit)
        {
                if((pixx&3) && texture == lasttexture)
                {
                        postwidth++;
                        wallheight[pixx] = wallheight[pixx-1];
                        return;
                }
                ScalePost();
                wallheight[pixx] = CalcHeight();
                postsource+=texture-lasttexture;
                postwidth=1;
                postx=pixx;
                lasttexture=texture;
                return;
        }

        if(lastside!=-1) ScalePost();

        lastside=2;
        lasttilehit=tilehit;
        lasttexture=texture;
        wallheight[pixx] = CalcHeight();
        postx = pixx;
        postwidth = 1;

        switch(doorobjlist[doornum].lock)
        {
                case dr_glass:
                        doorpage = DOORWALL-44;
                        break;                                             
                case dr_normal:
                        doorpage = DOORWALL+12;
                        break;
                case dr_lock1:
                        doorpage = DOORWALL+18;
                        break;
                case dr_lock2:
                        doorpage = DOORWALL-86;
                        break;
                case dr_lock3:
                        doorpage = DOORWALL-54;
                        break;
                case dr_lock4:
                        doorpage = DOORWALL-52;
                        break;
                case dr_elevator:
                        doorpage = DOORWALL+16;
                        break;                    
        }

        postsource = Pages+(doorpage<<12)+texture;
}

//==========================================================================

/*
====================
=
= HitVertDoor
=
====================
*/

void HitVertDoor (void)
{
        int doorpage;
        int doornum;
        int texture;

        doornum = tilehit&0x7f;
        texture = ((yintercept-doorposition[doornum])>>4)&0xfc0;

        if(lasttilehit==tilehit)
        {
                if((pixx&3) && texture == lasttexture)
                {
                        postwidth++;
                        wallheight[pixx] = wallheight[pixx-1];
                        return;
                }
                ScalePost();
                wallheight[pixx] = CalcHeight();
                postsource+=texture-lasttexture;
                postwidth=1;
                postx=pixx;
                lasttexture=texture;
                return;
        }

        if(lastside!=-1) ScalePost();

        lastside=2;
        lasttilehit=tilehit;
        lasttexture=texture;
        wallheight[pixx] = CalcHeight();
        postx = pixx;
        postwidth = 1;

        switch(doorobjlist[doornum].lock)
        {
                case dr_glass:
                        doorpage = DOORWALL-43;
                        break;                                               
               case dr_normal:
                        doorpage = DOORWALL+13;
                        break;
                case dr_lock1:
                        doorpage = DOORWALL+19;
                        break;
                case dr_lock2:
                        doorpage = DOORWALL-85;
                        break;
                case dr_lock3:
                        doorpage = DOORWALL-53;
                        break;
                case dr_lock4:
                        doorpage = DOORWALL-51;
                        break;
                case dr_elevator:
                        doorpage = DOORWALL+17;
                        break;                                                
        }

        postsource = Pages+(doorpage<<12)+texture;
}

//==========================================================================


/*
====================
=
= HitHorizPWall
=
= A pushable wall in action has been hit
=
====================
*/

void HitHorizPWall (void)
{
        int wallpic;
        int texture,offset;

        texture = (xintercept>>4)&0xfc0;
        offset = pwallpos<<10;
        if (ytilestep == -1)
                yintercept += TILEGLOBAL-offset;
        else
        {
                texture = 0xfc0-texture;
                yintercept += offset;
        }
        
        if(lasttilehit==tilehit && lastside==0)
        {
                if((pixx&3) && texture == lasttexture)
                {
                        postwidth++;
                        wallheight[pixx] = wallheight[pixx-1];
                        return;
                }
                ScalePost();
                wallheight[pixx] = CalcHeight();
                postsource+=texture-lasttexture;
                postwidth=1;
                postx=pixx;
                lasttexture=texture;
                return;
        }

        if(lastside!=-1) ScalePost();

        lastside=0;
        lasttilehit=tilehit;
        lasttexture=texture;
        wallheight[pixx] = CalcHeight();
        postx = pixx;
        postwidth = 1;

        wallpic = horizwall[pwalltile&63];

        postsource = Pages+(wallpic<<12)+texture;
}

/*
====================
=
= HitVertPWall
=
= A pushable wall in action has been hit
=
====================
*/

void HitVertPWall (void)
{
        int wallpic;
        int texture,offset;

        texture = (yintercept>>4)&0xfc0;
        offset = pwallpos<<10;
        if (xtilestep == -1)
        {
                xintercept += TILEGLOBAL-offset;
                texture = 0xfc0-texture;
        }
        else
                xintercept += offset;
                
        if(lasttilehit==tilehit && lastside==1)
        {
                if((pixx&3) && texture == lasttexture)
                {
                        postwidth++;
                        wallheight[pixx] = wallheight[pixx-1];
                        return;
                }
                ScalePost();
                wallheight[pixx] = CalcHeight();
                postsource+=texture-lasttexture;
                postwidth=1;
                postx=pixx;
                lasttexture=texture;
                return;
        }

        if(lastside!=-1) ScalePost();

        lastside=1;
        lasttilehit=tilehit;
        lasttexture=texture;
        wallheight[pixx] = CalcHeight();
        postx = pixx;
        postwidth = 1;

        wallpic = vertwall[pwalltile&63];

        postsource = Pages+(wallpic<<12)+texture;
}

#define HitHorizBorder HitHorizWall
#define HitVertBorder HitVertWall

//==========================================================================

#define USE_TEX(page) (0x0000|(page)) 

/* 
** use USE_TEX(n) to use floor/ceiling textures for given level 
** in should be 1..255 !!! 
** it is MAP-ID (from FloEdit). First texture goes to floor, second to ceiling 
*/ 

// ------------------------- * Textured flats * ------------------------- 
/* 
** Draw Textured Floor/Ceiling 
*/ 

/* 
** Draw Textured Floor/Ceiling (DarkOne & Adam Biser)
*/ 

void DrawFlats()
{
  int x, y, y0, halfheight;
  unsigned top_offset0, bot_offset0, top_offset, bot_offset;
  unsigned top_add, bot_add;
  byte p, color;
  byte far *src_top, far *src_bot; 
  fixed dist;            // distance to row projection
  fixed tex_step;        // global step per one screen pixel
  fixed gu, gv, du, dv; // global texture coordinates
  int u, v;              // local texture coordinates
  int flatx,flaty;
  unsigned curfloortex, lastflattex; 
  unsigned curceiltex, lastflattex2;

// ------ * prepare * --------

  halfheight=viewheight>>1;
  y0=min_wallheight>>3;      // starting y value
  if(y0>halfheight) return;  // view obscued by walls
  if(y0==0) y0=1;           // don't let division by zero
  top_offset0=80*(halfheight-y0-1);  // and will decrease by 80 each row
  bot_offset0=80*(halfheight+y0);  // and will increase by 80 each row
  fixed zoomoffset = focallength/0x5700;

  lastflattex=-1; 
  lastflattex2=-1;

// draw horizontal lines

  for(p=0; p<4; p++)
  {
    _asm{
        mov eax,0x0102
        mov cl,[p]
        shl ah,cl
        mov edx,0x3c4
        out dx,ax
    }

    for(y=y0, top_offset=top_offset0, bot_offset=bot_offset0; y<halfheight; y++, top_offset-=80, bot_offset+=80)
    {
      dist=((heightnumerator/(y+1))<<5); 
      gu= viewx+FixedMul(dist, viewcos);
      gv=-viewy+FixedMul(dist, viewsin);
      tex_step=(dist<<8)/viewwidth/175;
      if (zoomoffset>1) tex_step/=(fixed)(zoomoffset*0.645);
      du= FixedMul(tex_step, viewsin);
      dv=-FixedMul(tex_step, viewcos);
      gu-=((viewwidth>>1)-p)*du;
      gv-=((viewwidth>>1)-p)*dv; // starting point (leftmost)
      du<<=2; // 4pix step
      dv<<=2;
      for(x=p, top_add=top_offset, bot_add=bot_offset; x<viewwidth; x+=4, top_add++, bot_add++)
      {
        if(wallheight[x]>>3<=y)
        {
               flatx = (gu>>TILESHIFT)&63;
               flaty = (-(gv>>TILESHIFT)-1)&63;
               // read the high byte in plane 3
               curfloortex=(MAPSPOT(flatx,flaty,2)&0xff); 
               if (curfloortex==96) // Whatever your wall ID is 
                   curfloortex+= (frameon>>4)&5; 
               curceiltex=(MAPSPOT(flatx,flaty,2)>>8);
               if (curceiltex==96) // Whatever your wall ID is 
                   curceiltex+= (frameon>>4)&5; 
               if (curfloortex != lastflattex)
               {   
                  lastflattex=curfloortex;
                  src_bot=Pages+(curfloortex<<12);
               }
               if (curceiltex != lastflattex2)
               {   
                  lastflattex2=curceiltex;
                  src_top=Pages+(curceiltex<<12);
               }

          u=(gu>>10)&63; v=(gv>>10)&63;

          if (curceiltex)
          {   
          color=*(src_top+((63-u)<<6)+(63-v));
          
// draw top pixel using <color>

        _asm {
          mov edi,[vbuf]
          add edi,[top_add]
          mov al,[color]
          mov es:[edi],al
        } 
}                

          color=*(src_bot+(u<<6)+(63-v));

// draw bottom pixel using <color>

        _asm {
          mov edi,[vbuf]
          add edi,[bot_add]
          mov al,[color]
          mov es:[edi],al
        }
        }
        gu+=du; gv+=dv;
      }
    }
  }
}

#define USE_TEX(page) (0x0000|(page))

unsigned vgaCeiling[]= 
{ 
#ifndef SPEAR 
// 1      2      3      4      5      6      7      8      9      B 
// alte erste Zeile 0x9e9e,0x1d1d,0xdddd,0x1d1d,0x1e1e,0xd7d7,0xdede,0x9e9e,0x1d1d,0x1c1c, 
 USE_TEX(1),USE_TEX(2),USE_TEX(3),USE_TEX(4),USE_TEX(5),USE_TEX(6),USE_TEX(7),USE_TEX(8),USE_TEX(9),USE_TEX(10),
 USE_TEX(11),USE_TEX(12),USE_TEX(13),USE_TEX(14),USE_TEX(15),USE_TEX(16),USE_TEX(17),USE_TEX(18),USE_TEX(19),USE_TEX(20),
 USE_TEX(21),USE_TEX(22),USE_TEX(23),USE_TEX(24),USE_TEX(25),USE_TEX(26),USE_TEX(27),USE_TEX(28),USE_TEX(29),USE_TEX(30)
#else 
 0x6f6f,0x4f4f,0x1d1d,0xdede,0xdfdf,0x2e2e,0x7f7f,0x9e9e,0xaeae,0x7f7f, 
 0x1d1d,0xdede,0xdfdf,0xdede,0xdfdf,0xdede,0xe1e1,0xdcdc,0x2e2e,0x1d1d,0xdcdc 
#endif 
}; 

/*
=====================
=
= VGAClearScreen
=
=====================
*/

void VGAClearScreen (unsigned ceiling)
{
        //unsigned ceiling=vgaCeiling[gamestate.episode*30+mapon];
        ceiling|=ceiling<<16;

        _asm {
                cli
                mov     edx,SC_INDEX
                mov     eax,SC_MAPMASK+15*256
                out     dx,ax
                sti
                mov     edx,80
                mov     ebx,[viewwidth]
                shr     ebx,2
                sub     edx,ebx

                shr     ebx,2
                mov     bh,byte ptr [viewheight]
                shr     bh,1

                mov     edi,[vbuf]
                mov     eax,[ceiling]
                xor     ecx,ecx

toploop:
                mov     cl,bl
                rep     stosd
                add     edi,edx
                dec     bh
                jnz     toploop

                mov     bh,byte ptr [viewheight]
                shr     bh,1
                mov     eax,0x19191919

bottomloop:
                mov     cl,bl
                rep     stosd
                add     edi,edx
                dec     bh
                jnz     bottomloop
        }
}

//==========================================================================

/*
=====================
=
= CalcRotate
=
=====================
*/

int     CalcRotate (objtype *ob)
{
        int     angle,viewangle;

        // this isn't exactly correct, as it should vary by a trig value,
        // but it is close enough with only eight rotations

        viewangle = player->angle + (centerx - ob->viewx)/8;

        if (ob->obclass == rocketobj || ob->obclass == hrocketobj)
                angle =  (viewangle-180)- ob->angle;
        else
                angle =  (viewangle-180)- dirangle[ob->dir];

        angle+=ANGLES/16;
        while (angle>=ANGLES)
                angle-=ANGLES;
        while (angle<0)
                angle+=ANGLES;

        if (ob->state->rotate == 2)             // 2 rotation pain frame
                return 0;               // pain with shooting frame bugfix

        return angle/(ANGLES/8);
}


void ScaleShape (int xcenter, int shapenum, unsigned height)
{
        t_compshape   *shape;
        unsigned scale,pixheight;
        unsigned starty,endy;
        word *cmdptr;
        word *cline;
        word *line;
        byte *vmem;
        int actx,i,upperedge;
        short newstart;
        int scrstarty,screndy,j,lpix,rpix,pixcnt,ycnt;
        byte col;
#ifdef SHADE_COUNT
        byte shade;
        byte *curshades;
#endif

        shape=(t_compshape *)(Pages+((PMSpriteStart+shapenum)<<12));

        scale=height>>3;                 // low three bits are fractional
        if(!scale/* || scale>maxscale*/) return;   // too close or far away

#ifdef SHADE_COUNT
        switch(shapenum)
        {
                case 5:
                case 6:
                case 16:
                case 437:
                        shade=0;   // let lamps "shine" in the dark
                        break;
                default:
                        shade=(scale<<2)/((maxscaleshl2>>8)+1+LSHADE_flag);
                        if(shade>32) shade=32;
                        else if(shade<1) shade=1;
                        shade=32-shade;
                        break;
        }
        curshades=shadetable[shade];
#endif

        pixheight=scale*2;
        actx=xcenter-scale;
        upperedge=viewheight/2-scale;

        cmdptr=(word *) shape->dataofs;

        for(i=shape->leftpix,pixcnt=i*pixheight,rpix=(pixcnt>>6)+actx;i<=shape->rightpix;i++,cmdptr++)
        {
                lpix=rpix;
                if(lpix>=viewwidth) break;
                pixcnt+=pixheight;
                rpix=(pixcnt>>6)+actx;
                if(lpix!=rpix && rpix>0)
                {
                        if(lpix<0) lpix=0;
                        if(rpix>viewwidth) rpix=viewwidth,i=shape->rightpix+1;
                        cline=(word *)((byte *)shape + *cmdptr);
                        while(lpix<rpix)
                        {
                                if(wallheight[lpix]<=height)
                                {
                                        VGAMAPMASK(1<<(lpix&3));
                                        line=cline;
                                        while(*line)
                                        {
                                                starty=(*(line+2))>>1;
                                                endy=(*line)>>1;
                                                newstart=(*(line+1));
                                                j=starty;
                                                ycnt=j*pixheight;
                                                screndy=(ycnt>>6)+upperedge;
                                                if(screndy<0) vmem=vbuf+(lpix>>2);
                                                else vmem=vbuf+screndy*80+(lpix>>2);
                                                for(;j<endy;j++)
                                                {
                                                        scrstarty=screndy;
                                                        ycnt+=pixheight;
                                                        screndy=(ycnt>>6)+upperedge;
                                                        if(scrstarty!=screndy && screndy>0)
                                                        {
#ifdef SHADE_COUNT
                                                                col=curshades[((byte _seg *)shape)[newstart+j]];
#else
                                                                col=((byte *)shape)[newstart+j];
#endif
                                                                if(scrstarty<0) scrstarty=0;
                                                                if(screndy>viewheight) screndy=viewheight,j=endy;

                                                                while(scrstarty<screndy)
                                                                {
                                                                        *vmem=col;
                                                                        vmem+=80;
                                                                        scrstarty++;
                                                                }
                                                        }
                                                }
                                                line+=3;
                                        }
                                }
                                lpix++;
                        }
                }
        }
}

void SimpleScaleShape (int xcenter, int shapenum, unsigned height)
{
        t_compshape   *shape;
        unsigned scale,pixheight;
        unsigned starty,endy;
        word *cmdptr;
        word *cline;
        word *line;
        int actx,i,upperedge;
        short newstart;
        int scrstarty,screndy,j,lpix,rpix,pixcnt,ycnt;
        byte mask,col;
        byte *vmem;

        shape=(t_compshape *)(Pages+((PMSpriteStart+shapenum)<<12));

        scale=height>>1;                 // low three bits are fractional
        if(!scale/* || scale>maxscale*/) return;   // too close or far away

        pixheight=scale*2;
        actx=xcenter-scale;
        upperedge=viewheight/2-scale;

        cmdptr=shape->dataofs;

        for(i=shape->leftpix,pixcnt=i*pixheight,rpix=(pixcnt>>6)+actx;i<=shape->rightpix;i++,cmdptr++)
        {
                lpix=rpix;
                if(lpix>=viewwidth) break;
                pixcnt+=pixheight;
                rpix=(pixcnt>>6)+actx;
                if(lpix!=rpix && rpix>0)
                {
                        if(lpix<0) lpix=0;
                        if(rpix>viewwidth) rpix=viewwidth,i=shape->rightpix+1;
                        cline=(word *)((byte *)shape + *cmdptr);
                        while(lpix<rpix)
                        {
                                mask=1<<(lpix&3);
                                VGAMAPMASK(mask);
                                line=cline;
                                while(*line)
                                {
                                        starty=(*(line+2))>>1;
                                        endy=(*line)>>1;
                                        newstart=(*(line+1));
                                        j=starty;
                                        ycnt=j*pixheight;
                                        screndy=(ycnt>>6)+upperedge;
                                        if(screndy<0) vmem=vbuf+(lpix>>2);
                                        else vmem=vbuf+screndy*80+(lpix>>2);
                                        for(;j<endy;j++)
                                        {
                                                scrstarty=screndy;
                                                ycnt+=pixheight;
                                                screndy=(ycnt>>6)+upperedge;
                                                if(scrstarty!=screndy && screndy>0)
                                                {
                                                        col=((byte *)shape)[newstart+j];
                                                        if(scrstarty<0) scrstarty=0;
                                                        if(screndy>viewheight) screndy=viewheight,j=endy;

                                                        while(scrstarty<screndy)
                                                        {
                                                                *vmem=col;
                                                                vmem+=80;
                                                                scrstarty++;
                                                        }
                                                }
                                        }
                                        line+=3;
                                }
                                lpix++;
                        }
                }
        }
}

/*
===================
=
= Scale3DShaper
= by Ripper
=
===================
*/

void Scale3DShaper (int x1,int x2,int shapenum,unsigned height1,unsigned height2, 
                fixed ny1,fixed ny2,fixed nx1,fixed nx2) 
{ 
        t_compshape *shape; 
        unsigned scale1,scale2,starty,endy; 
        word *cmdptr; 
        word *line; 
        byte *vmem; 
        int dx=x2-x1,len,i,j,newstart,ycnt,pixheight,screndy,upperedge,scrstarty; 
        fixed height,dheight; 
        int xpos[65]; 
        int slinex; 
        fixed dxx=(ny2-ny1)<<8,dzz=(nx2-nx1)<<8; 
        fixed dxa=0,dza=0; 
        byte mask,col; 
#ifdef SHADE_COUNT 
        byte shade; 
        byte *curshades; 
#endif 

        if(!dx) return; 

        height=(((fixed)height1)<<12)+2048; 
        dheight=(((fixed)height2-(fixed)height1)<<12)/(fixed)dx; 

        // Get length/address of pixeldata 
        shape =(t_compshape *)(Pages+((PMSpriteStart+shapenum)<<12)); 

        scale1 = height1>>3; // low three bits are fractional 
        scale2 = height2>>3; // low three bits are fractional 
        if (!scale1 && !scale2 /*|| scale1>maxscale && scale2>maxscale*/) 
                return; // too close or far away                 

        len=shape->rightpix-shape->leftpix+1; 
        if(!len) return; 
        
        ny1+=dxx>>9; 
        nx1+=dzz>>9; 

        dxa=-(dxx>>1),dza=-(dzz>>1); 
        dxx>>=6,dzz>>=6; 
        
        dxa+=shape->leftpix*dxx,dza+=shape->leftpix*dzz; 

        xpos[0]=(int)((ny1+(dxa>>8))*scale/(nx1+(dza>>8))+centerx); 

        for(i=1;i<=len;i++) 
        { 
                dxa+=dxx,dza+=dzz; 
                xpos[i]=(int)((ny1+(dxa>>8))*scale/(nx1+(dza>>8))+centerx); 
                if(xpos[i-1]>viewwidth) break; 
        } 
        len=i-1; 

        //*(((unsigned *)&line)+1)=(unsigned)shape; // seg of shape 
        cmdptr = (word *) &shape->dataofs[0]; 
        //*(((unsigned *)&vmem)+1)=0xa000; 

        i=0; 
        if(x2>viewwidth) x2=viewwidth; 

        for(i=0;i<len;i++) 
        { 
                for(slinex=xpos[i];slinex<xpos[i+1] && slinex<x2;slinex++) 
                { 
                        height+=dheight; 
                        if(slinex<0) continue;                     

                        scale1=(unsigned)(height>>15); 

                        if(wallheight[slinex]<(height>>12) && scale1 /*&& scale1<=maxscale*/) 
                        { 
#ifdef SHADE_COUNT 
                                shade=(scale1<<2)/((maxscaleshl2>>8)+1+LSHADE_flag); 
                                if(shade>32) shade=32; 
                                else if(shade<1) shade=1; 
                                shade=32-shade; 
                                curshades=shadetable[shade]; 
#endif 

                                pixheight=scale1*2; 
                                upperedge=viewheight/2-scale1; 

                                mask=1<<(slinex&3); 
                                VGAMAPMASK(mask); 
                                line=(word *)((byte *)shape + cmdptr[i]); 
                                
                                while(*line) 
                                { 
                                        starty=(*(line+2))>>1; 
                                        endy=(*line)>>1; 
                                        newstart=(*(line+1)); 
                                        j=starty; 
                                        ycnt=j*pixheight; 
                                        screndy=(ycnt>>6)+upperedge; 
                                        if(screndy<0) vmem=vbuf+(slinex>>2); 
                                        else vmem=vbuf+screndy*80+(slinex>>2); 
                                        for(;j<endy;j++) 
                                        { 
                                                scrstarty=screndy; 
                                                ycnt+=pixheight; 
                                                screndy=(ycnt>>6)+upperedge; 
                                                if(scrstarty!=screndy && screndy>0) 
                                                { 
#ifdef SHADE_COUNT 
                                                        col=curshades[((byte *)shape)[newstart+j]]; 
#else 
                                                        col=((byte *)shape)[newstart+j]; 
#endif 
                                                        if(scrstarty<0) scrstarty=0; 
                                                        if(screndy>viewheight) screndy=viewheight,j=endy; 
                                                        
                                                        while(scrstarty<screndy) 
                                                        { 
                                                                *vmem=col; 
                                                                vmem+=80; 
                                                                scrstarty++; 
                                                        } 
                                                } 
                                        } 
                                        line+=3; 
                                } 
                        } 
                } 
        } 
} 

extern long playx,playy;

/*
=========================
=
= Scale3DShape
= by Ripper
=
=========================
*/

void Scale3DShape(statobj_t *ob) 
{ 
   fixed nx1,nx2,ny1,ny2; 
        int viewx1,viewx2; 
   long height1, height2; 
   fixed diradd; 

// 
// the following values for "diradd" aren't optimized yet 
// if you have problems with sprites being visible through wall edges 
// where they shouldn't, you can try to adjust these values and 
// the -1024 and +2048 in both "if" blocks 
// 

   if(ob->flags & FL_DIRFORWARD) diradd=0x7ff0+0x8000; 
   else if(ob->flags & FL_DIRBACKWARD) diradd=-0x7ff0+0x8000; 
   else diradd=0x8000; 

   if(ob->flags & FL_DIREAST) 
   { 
      fixed gx1,gx2,gy,gxt1,gxt2,gyt; 
// 
// translate point to view centered coordinates 
// 
      gx1 = (((long)ob->tilex) << TILESHIFT)+0x8000-playx-0x8000L-1024; 
      gx2 = gx1+0x10000L+2048; 
      gy = (((long)ob->tiley) << TILESHIFT)+diradd-playy; 

// 
// calculate newx 
// 
      gxt1 = FixedMul(gx1,viewcos); 
      gxt2 = FixedMul(gx2,viewcos); 
      gyt = FixedMul(gy,viewsin); 
      nx1 = gxt1-gyt-0x2000; 
      nx2 = gxt2-gyt-0x2000; 

// 
// calculate newy 
// 
      gxt1 = FixedMul(gx1,viewsin); 
      gxt2 = FixedMul(gx2,viewsin); 
      gyt = FixedMul(gy,viewcos); 
      ny1 = gyt+gxt1; 
      ny2 = gyt+gxt2; 
      } 
      else 
      { 
      fixed gy1,gy2,gx,gyt1,gyt2,gxt; 
// 
// translate point to view centered coordinates 
// 
      gy1 = (((long)ob->tiley) << TILESHIFT)+0x8000-playy-0x8000L-1024; 
      gy2 = gy1+0x10000L+2048; 
      gx = (((long)ob->tilex) << TILESHIFT)+diradd-playx; 

// 
// calculate newx 
// 
      gxt = FixedMul(gx,viewcos); 
      gyt1 = FixedMul(gy1,viewsin); 
      gyt2 = FixedMul(gy2,viewsin); 
      nx1 = gxt-gyt1-0x2000; 
      nx2 = gxt-gyt2-0x2000; 

// 
// calculate newy 
// 
      gxt = FixedMul(gx,viewsin); 
      gyt1 = FixedMul(gy1,viewcos); 
      gyt2 = FixedMul(gy2,viewcos); 
      ny1 = gyt1+gxt; 
      ny2 = gyt2+gxt; 
   } 

// 
// calculate perspective ratio 
// 
   if(nx1>=0 && nx1<=1792) nx1=1792; 
   if(nx1<0 && nx1>=-1792) nx1=-1792; 
   if(nx2>=0 && nx2<=1792) nx2=1792; 
   if(nx2<0 && nx2>=-1792) nx2=-1792; 

   viewx1=(int)(centerx+ny1*scale/nx1); 
   viewx2=(int)(centerx+ny2*scale/nx2); 

// 
// calculate height (heightnumerator/(nx>>8)) (heightnumerator=0x36800) 
// 
    height1 = (short)(heightnumerator/(nx1>>8)); 
    height2 = (short)(heightnumerator/(nx2>>8));  

    height1 = (short)(heightnumerator/(nx1>>8)); 
    height2 = (short)(heightnumerator/(nx2>>8)); 

    if(viewx2 < viewx1) 
    { 
     Scale3DShaper(viewx2,viewx1,ob->shapenum,(unsigned)height2, 
        (unsigned)height1,ny2,ny1,nx2,nx1); 
    } 
    else 
    { 
    Scale3DShaper(viewx1,viewx2,ob->shapenum,(unsigned)height1, 
       (unsigned)height2,ny1,ny2,nx1,nx2); 
    } 
} 

/*
=====================
=
= DrawScaleds
=
= Draws all objects that are visable
=
=====================
*/

#define MAXVISABLE      50

typedef struct
{
        short   viewx,
                viewheight,
                shapenum;
        statobj_t *transsprite;  
} visobj_t;

visobj_t        vislist[MAXVISABLE];
visobj_t *visptr,*visstep,*farthest;
void            Scale3DShape(statobj_t *ob);

void DrawScaleds (void)
{
        int             i,least,numvisable,height;
        byte            *tilespot,*visspot;
        unsigned spotloc;

        statobj_t       *statptr;
        objtype         *obj;

        visptr = &vislist[0];

//
// place static objects
//
        for (statptr = &statobjlist[0] ; statptr !=laststatobj ; statptr++)
        {
                if ((visptr->shapenum = statptr->shapenum) == -1)
                        continue;                                               // object has been deleted

                if (!*statptr->visspot)
                        continue;                                               // not visable
                        
                switch (statptr->shapenum)
      	    	{
   	      		case SPR_AUQARIUM1:			
			    	visptr->shapenum+=(short)((frameon>>4)%4); // slow animation with 4 frames
                 	break;   
      		    case SPR_LIGHT1:	
		    	case SPR_LAMP1:		
			    	visptr->shapenum+=(short)(frameon>>3)%2; // fast animation with 2 frames
                 	break;      
      	    	}                    

                if (TransformTile (statptr->tilex,statptr->tiley
                        ,&visptr->viewx,&visptr->viewheight) && statptr->flags & FL_BONUS)
                {
                        GetBonus (statptr);
                        continue;
                }

                if (!visptr->viewheight)
                        continue;                                               // to close to the object
                if (statptr->flags & (FL_DIRSOUTH | FL_DIREAST))
		           visptr->transsprite=statptr;
	        	else
		           visptr->transsprite=NULL;  
                if (visptr < &vislist[MAXVISABLE-1])    // don't let it overflow
                        visptr++;
        }

//
// place active objects
//
        for (obj = player->next;obj;obj=obj->next)
        {
                if ((visptr->shapenum = obj->state->shapenum)==0)
                        continue;                                               // no shape

                spotloc = (obj->tilex<<mapshift)+obj->tiley;   // optimize: keep in struct?
                visspot = &spotvis[0][0]+spotloc;
                tilespot = &tilemap[0][0]+spotloc;

                //
                // could be in any of the nine surrounding tiles
                //
                if (*visspot
                || ( *(visspot-1) && !*(tilespot-1) )
                || ( *(visspot+1) && !*(tilespot+1) )
                || ( *(visspot-65) && !*(tilespot-65) )
                || ( *(visspot-64) && !*(tilespot-64) )
                || ( *(visspot-63) && !*(tilespot-63) )
                || ( *(visspot+65) && !*(tilespot+65) )
                || ( *(visspot+64) && !*(tilespot+64) )
                || ( *(visspot+63) && !*(tilespot+63) ) )
                {
                        obj->active = ac_yes;
                        TransformActor (obj);
                        if (!obj->viewheight)
                                continue;                                               // too close or far away

                        visptr->viewx = obj->viewx;
                        visptr->viewheight = obj->viewheight;
                        if (visptr->shapenum == -1)
                                visptr->shapenum = obj->temp1;  // special shape

                        if (obj->state->rotate)
                                visptr->shapenum += CalcRotate (obj);

                        if (visptr < &vislist[MAXVISABLE-1])    // don't let it overflow
		             	{
			              	visptr->transsprite=NULL;
			            	visptr++;
		             	}
                        obj->flags |= FL_VISABLE;
                }
                else
                        obj->flags &= ~FL_VISABLE;
        }

//
// draw from back to front
//
        numvisable = visptr-&vislist[0];

        if (!numvisable)
                return;                                                                 // no visable objects

        for (i = 0; i<numvisable; i++)
        {
                least = 32000;
                for (visstep=&vislist[0] ; visstep<visptr ; visstep++)
                {
                        height = visstep->viewheight;
                        if (height < least)
                        {
                                least = height;
                                farthest = visstep;
                        }
                }
                //
                // draw farthest
                //
                if(farthest->transsprite)
		        Scale3DShape(farthest->transsprite);
	            else
                ScaleShape(farthest->viewx,farthest->shapenum,farthest->viewheight);

                farthest->viewheight = 32000;
        }

}

//==========================================================================

/*
==============
=
= DrawParallax
=
= by Ripper
=
==============
*/

#define TEXTURESHIFT    6
#define TEXTURESIZE     (1<<TEXTURESHIFT)
#define TEXTUREMASK     (TEXTURESIZE*(TEXTURESIZE-1))

#define NUMPARALLAXTEX  16

void DrawParallax(int startpage) 
{ 
   word xtex,nextxtex,offs; 
   byte *skytex; 
   byte *destbuf=(byte *)(vbuf); // + 0xa0000000L 
   int i,x=0,x2,curtex,nextx=0,texoffs,t,tend; 
   byte mask; 
   int midangle=player->angle*(FINEANGLES/ANGLES); 
   int skyheight=viewheight>>1; 
   startpage += NUMPARALLAXTEX - 1; 

   t=pixelangle[0]+midangle; 
   while(t<0) t+=FINEANGLES; 
   while(t>=FINEANGLES) t-=FINEANGLES; 
   xtex=(word)((((long)t) * NUMPARALLAXTEX * TEXTURESIZE)/FINEANGLES); 

   do 
   { 
      curtex=xtex>>TEXTURESHIFT; 
      skytex=Pages+((startpage-curtex)<<12); 
      nextx=0x1fff; 
      for(i=0;i<4;i++) 
      { 
         mask=1<<((x+i)&3); 
         VGAMAPMASK(mask); 
         for(x2=x+i;x2<viewwidth;x2+=4) 
         { 
            t=pixelangle[x2]+midangle; 
            while(t<0) t+=FINEANGLES; 
            while(t>=FINEANGLES) t-=FINEANGLES; 
            xtex=(word)((((long)t) * NUMPARALLAXTEX * TEXTURESIZE)/FINEANGLES); 

            t=xtex>>TEXTURESHIFT; 
            if(t!=curtex) 
            { 
               if(x2<nextx) nextx=x2,nextxtex=xtex; 
               break; 
            } 

            texoffs= TEXTUREMASK -((xtex & (TEXTURESIZE - 1) ) << TEXTURESHIFT); 
            tend=skyheight-(wallheight[x2]>>3); 
            if(tend<=0) continue; 
            for(t=0,offs=x2>>2;t<tend;t++,offs+=80) 
               destbuf[offs]=skytex[texoffs+((t*TEXTURESIZE)/skyheight)]; 
         } 
         if(x2>=viewwidth+3) nextx=viewwidth; 

      } 
      x=nextx; 
      xtex=nextxtex; 
   } 
   while(x<viewwidth); 
}

/*
==============
=
= DrawPlayerWeapon
=
= Draw the player's hands
=
==============
*/

int     weaponscale[NUMWEAPONS-1] = {SPR_KNIFEREADY,SPR_PISTOLREADY
        ,SPR_MACHINEGUNREADY,SPR_CHAINREADY,SPR_SHOTGUNREADY,SPR_DSHOTGUNREADY};

void DrawPlayerWeapon (void)
{
        int     shapenum;
        int     realviewwidth, realviewheight;

#ifndef SPEAR
        if (gamestate.victoryflag)
        {
                if (player->state == &s_deathcam && (TimeCount&32) )
                        SimpleScaleShape(viewwidth/2,SPR_DEATHCAM,viewheight+1);
                return;
        }
#endif

        if (gamestate.car == true) 
        { 
         if (gamestate.left == true) 
         { 
          SimpleScaleShape(viewwidth/2,SPR_CAR3,viewheight+1); 
          return; 
          } 
          else if (gamestate.right == true) 
          { 
          SimpleScaleShape(viewwidth/2,SPR_CAR1,viewheight+1); 
          return; 
         } 
         SimpleScaleShape(viewwidth/2,SPR_CAR2,viewheight+1); 
         return; 
        } 
        
        if (gamestate.zoom == 0)
        {
                SimpleScaleShape(viewwidth/2,SPR_ZOOM,viewheight+1);
                VWB_Bar(0,0,80,152,0);
                VWB_Bar(224,0,80,152,0);
                return;       
        }        

        if (gamestate.weapon != -1)
        {

            realviewwidth = viewwidth/2;
            realviewheight = viewheight+1;

            if (bobmode)
            {
                realviewwidth = realviewwidth-10+gamestate.bobber2;
                realviewheight = realviewheight+gamestate.bobber+gamestate.weaponchange;
            }

            if (gamestate.weapon == wp_dpistol)
            {
                SimpleScaleShape(realviewwidth,SPR_LDPISTOLREADY+gamestate.weaponframe,realviewheight);
                SimpleScaleShape(realviewwidth,SPR_RDPISTOLREADY+gamestate.weaponframe,realviewheight);
            }
            else
            {

                if (gamestate.weapon == wp_assault)
                    shapenum = SPR_ASSAULTREADY;
                else
                    shapenum = weaponscale[gamestate.weapon];

                shapenum += gamestate.weaponframe;

                SimpleScaleShape(realviewwidth,shapenum,realviewheight);
            }
        }
      
        if (demorecord || demoplayback)
                SimpleScaleShape(viewwidth/2,SPR_DEMO,viewheight+1);
}


//==========================================================================


/*
=====================
=
= CalcTics
=
=====================
*/

void CalcTics (void)
{
        long    newtime;

//
// calculate tics since last refresh for adaptive timing
//
        if (lasttimecount > TimeCount)
                TimeCount = lasttimecount;              // if the game was paused a LONG time

        do
        {
                newtime = TimeCount;
                tics = newtime-lasttimecount;
        } while (!tics);                        // make sure at least one tic passes

        lasttimecount = newtime;

        if (tics>MAXTICS)
        {
                TimeCount -= (tics-MAXTICS);
                tics = MAXTICS;
        }
}


//==========================================================================


/*
========================
=
= FixOfs
=
========================
*/

void    FixOfs (void)
{
        VW_ScreenToScreen (vdisp,vbuf,viewwidth/8,viewheight);
}


//==========================================================================

//#define DEBUGRAYTRACER

#ifdef DEBUGRAYTRACER
#define LOGF if(dolog) fprintf
#define MARKPIX(y,col) VGAMAPMASK(1<<(pixx&3)); \
	vbuf[(pixx>>2)+(y)*80]=(col);
#else
#define LOGF 0 &&
#define MARKPIX(y,col)
#endif

void AsmRefresh()
{
    long xstep,ystep;
    longword xpartial,ypartial;

#ifdef DEBUGRAYTRACER
	 static int logpressed=0;
	 int dolog=0;
    FILE *log=NULL;
#endif

    for(pixx=0;pixx<viewwidth;pixx++)
    {
        short angl=midangle+pixelangle[pixx];
        if(angl<0) angl+=FINEANGLES;
        if(angl>=3600) angl-=FINEANGLES;
        if(angl<900)
        {
            xtilestep=1;
            ytilestep=-1;
            xstep=finetangent[900-1-angl];
            ystep=-finetangent[angl];
            xpartial=xpartialup;
            ypartial=ypartialdown;
        }
        else if(angl<1800)
        {
            xtilestep=-1;
            ytilestep=-1;
            xstep=-finetangent[angl-900];
            ystep=-finetangent[1800-1-angl];
            xpartial=xpartialdown;
            ypartial=ypartialdown;
        }
        else if(angl<2700)
        {
            xtilestep=-1;
            ytilestep=1;
            xstep=-finetangent[2700-1-angl];
            ystep=finetangent[angl-1800];
            xpartial=xpartialdown;
            ypartial=ypartialup;
        }
        else if(angl<3600)
        {
            xtilestep=1;
            ytilestep=1;
            xstep=finetangent[angl-2700];
            ystep=finetangent[3600-1-angl];
            xpartial=xpartialup;
            ypartial=ypartialup;
        }
        yintercept=FixedMul(ystep,xpartial)+viewy;
        xtile=focaltx+xtilestep;
        xspot=(xtile<<mapshift)+*((word *)&yintercept+1);
        xintercept=FixedMul(xstep,ypartial)+viewx;
        ytile=focalty+ytilestep;
        yspot=(*((word *)&xintercept+1)<<mapshift)+ytile;
		  texdelta=0;

        if(xintercept<0) xintercept=0;
        if(xintercept>mapwidth*65536-1) xintercept=mapwidth*65536-1;
        if(yintercept<0) yintercept=0;
        if(yintercept>mapheight*65536-1) yintercept=mapheight*65536-1;

#ifdef DEBUGRAYTRACER
		  if(pixx==93)
		  {
			  VGAMAPMASK(1<<(pixx&3));
			  vbuf[(pixx>>2)+80]=14;
			  if(logpressed)
			  {
				  if(!Keyboard[sc_L]) logpressed=0;
			  }
			  else
			  {
				  if(Keyboard[sc_L])
				  {
					  logpressed=1;
					  dolog=1;
					  log=fopen("draw93.txt","wt");
					  if(!log) return;
					  fprintf(log,"player->x=%.8X  player->y=%.8X  player->angle=%i  pixx=%i\nxintercept=%.8X  xtile=%.4X  xtilestep=%i  xstep=%.8X\nyintercept=%.8X  ytile=%.4X  ytilestep=%i  ystep=%.8X\n",player->x,player->y,player->angle,pixx,xintercept,xtile,xtilestep,xstep,yintercept,ytile,ytilestep,ystep);
				  }
			  }
		  }
#endif

        do
        {
            if(ytilestep==-1 && *((short *)&yintercept+1)<=ytile) goto horizentry;
            if(ytilestep==1 && *((short *)&yintercept+1)>=ytile) goto horizentry;
vertentry:
            if((longword)yintercept>mapheight*65536-1 || (word)xtile>=mapwidth)
            {
                if(xtile<0) xintercept=0;
                if(xtile>=mapwidth) xintercept=mapwidth<<TILESHIFT;
                if(yintercept<0) yintercept=0;
                if(yintercept>=(mapheight<<TILESHIFT)) yintercept=mapheight<<TILESHIFT;
                yspot=0xffff;
                HitHorizBorder();
                break;
            }
            if(xspot>mapspotend) break;
            tilehit=*((byte *)tilemap+xspot);
            if(tilehit)
            {
                if(tilehit&0x80)
                {
                    long yintbuf=yintercept+(ystep>>1);
                    if(*((word *)&yintbuf+1)!=*((word *)&yintercept+1))
                        goto passvert;
                    if((word)yintbuf<doorposition[tilehit&0x7f])
                        goto passvert;
                    yintercept=yintbuf;
                    xintercept=(xtile<<TILESHIFT)|0x8000;
                    HitVertDoor();
                }
                else
                {
                    if(tilehit==64)
                    {
							  	if(pwalldir==di_west || pwalldir==di_east)
								{
	                        long yintbuf;
									int pwallposnorm;
									int pwallposinv;
									if(pwalldir==di_west)
									{
										pwallposnorm = 63-pwallpos;
										pwallposinv = pwallpos;
									}
									else
									{
										pwallposnorm = pwallpos;
										pwallposinv = 63-pwallpos;
									}
									if(pwalldir == di_east && xtile==pwallx && *((word *)&yintercept+1)==pwally
										|| pwalldir == di_west && !(xtile==pwallx && *((word *)&yintercept+1)==pwally))
									{
										yintbuf=yintercept+((ystep*pwallposnorm)>>6);
	   	                     if(*((word *)&yintbuf+1)!=*((word *)&yintercept+1))
   	   	                     goto passvert;
											
									   MARKPIX(4,2);

         		               xintercept=(xtile<<TILESHIFT)+TILEGLOBAL-(pwallposinv<<10);
      	      	            yintercept=yintbuf;
										tilehit=pwalltile;
                                                                           // LOGF(log,"Pushwall hit 1: HitVertWall:\n  xintercept=%.8X  xtile=%.4X\n  yintercept=%.8X  ytile=%.4X\n  pwallpos=%i\n",xintercept,xtile,yintercept,ytile,pwallpos);
	               	         HitVertWall();
									}
									else
									{
										yintbuf=yintercept+((ystep*pwallposinv)>>6);
	   	                     if(*((word *)&yintbuf+1)!=*((word *)&yintercept+1))
   	   	                     goto passvert;
										
									   MARKPIX(4,1);

         		               xintercept=(xtile<<TILESHIFT)-(pwallposinv<<10);
      	      	            yintercept=yintbuf;
										tilehit=pwalltile;
                                                                           // LOGF(log,"Pushwall hit 2: HitVertWall:\n  xintercept=%.8X  xtile=%.4X\n  yintercept=%.8X  ytile=%.4X\n  pwallpos=%i\n",xintercept,xtile,yintercept,ytile,pwallpos);
	               	         HitVertWall();
									}
								}
								else
								{
									int pwallposi = pwallpos;
									if(pwalldir==di_north) pwallposi = 63-pwallpos;
									if(pwalldir==di_south && (word)yintercept<(pwallposi<<10)
											|| pwalldir==di_north && (word)yintercept>(pwallposi<<10))
									{
										if(*((word *)&yintercept+1)==pwally && xtile==pwallx)
										{												
										   if(pwalldir==di_south && (long)((word)yintercept)+ystep<(pwallposi<<10)
													|| pwalldir==di_north && (long)((word)yintercept)+ystep>(pwallposi<<10))
											   goto passvert;
												
										   MARKPIX(5,15);

                                                                              // LOGF(log,"Pushwall hit 3: HitHorizWall old values:\n  xintercept=%.8X  xtile=%.4X\n  yintercept=%.8X  ytile=%.4X\n  pwallpos=%.4X",xintercept,xtile,yintercept,ytile,pwallpos);
											if(pwalldir==di_south)
											   yintercept=(yintercept&0xffff0000)+(pwallposi<<10);
											else
											   yintercept=(yintercept&0xffff0000)-TILEGLOBAL+(pwallposi<<10);
     		      	               xintercept=xintercept-((xstep*(63-pwallpos))>>6);
											tilehit=pwalltile;
                                                                              // LOGF(log,"Pushwall hit 3: HitHorizWall:\n  xintercept=%.8X  xtile=%.4X\n  yintercept=%.8X  ytile=%.4X\n  pwallpos=%.4X\n",xintercept,xtile,yintercept,ytile,pwallpos);
	               	            HitHorizWall();
										}
										else
										{
										   MARKPIX(3,11);
										
									      texdelta = -(pwallposi<<10);
											xintercept=xtile<<TILESHIFT;
											tilehit=pwalltile;
                                                                              // LOGF(log,"Pushwall hit 4: HitVertWall:\n  xintercept=%.8X  xtile=%.4X\n  yintercept=%.8X  ytile=%.4X\n  pwallpos=%.4X\n",xintercept,xtile,yintercept,ytile,pwallpos);
											HitVertWall();
										}
									}
									else
									{
										if(*((word *)&yintercept+1)==pwally && xtile==pwallx)
										{
										   MARKPIX(3,12);
										
									      texdelta = -(pwallposi<<10);
											xintercept=xtile<<TILESHIFT;
											tilehit=pwalltile;
                                                                                   // LOGF(log,"Pushwall hit 5: HitVertWall:\n  xintercept=%.8X  xtile=%.4X\n  yintercept=%.8X  ytile=%.4X\n  pwallpos=%.4X\n",xintercept,xtile,yintercept,ytile,pwallpos);
											HitVertWall();
										}
										else
										{
										   if(pwalldir==di_south && (long)((word)yintercept)+ystep>(pwallposi<<10)
													|| pwalldir==di_north && (long)((word)yintercept)+ystep<(pwallposi<<10))
											   goto passvert;
											
										   MARKPIX(3,3);

											if(pwalldir==di_south)
											   yintercept=(yintercept&0xffff0000)-((63-pwallpos)<<10);
											else
											   yintercept=(yintercept&0xffff0000)+((63-pwallpos)<<10);
     		      	               xintercept=xintercept-((xstep*pwallpos)>>6);
											tilehit=pwalltile;
                                                                                   // LOGF(log,"Pushwall hit 6: HitHorizWall:\n  xintercept=%.8X  xtile=%.4X\n  yintercept=%.8X  ytile=%.4X\n  pwallpos=%.4X\n",xintercept,xtile,yintercept,ytile,pwallpos);
	               	            HitHorizWall();
										}
									}
								}
                    }
                    else
                    {
                        xintercept=xtile<<TILESHIFT;
                                                           // LOGF(log,"HitVertWall:\n  xintercept=%.8X  xtile=%.4X\n  yintercept=%.8X  ytile=%.4X\n",xintercept,xtile,yintercept,ytile);
                        HitVertWall();
                    }
                }
                break;
            }
passvert:
            *((byte *)spotvis+xspot)=1;
            xtile+=xtilestep;
            yintercept+=ystep;
            xspot=(xtile<<mapshift)+*((word *)&yintercept+1);
                           // LOGF(log,"passvert:\n  xintercept=%.8X  xtile=%.4X\n  yintercept=%.8X  ytile=%.4X\n",xintercept,xtile,yintercept,ytile);
        }
        while(1);
#ifdef DEBUGRAYTRACER
		  if(dolog)
		  {
			  fclose(log);
			  dolog=0;
			  log=NULL;
		  }
#endif
        continue;
        do
        {
            if(xtilestep==-1 && *((short *)&xintercept+1)<=xtile) goto vertentry;
            if(xtilestep==1 && *((short *)&xintercept+1)>=xtile) goto vertentry;
horizentry:
            if((longword)xintercept>mapwidth*65536-1 || (word)ytile>=mapheight)
            {
                if(ytile<0) yintercept=0;
                if(ytile>=mapheight) yintercept=mapheight<<TILESHIFT;
                if(xintercept<0) xintercept=0;
                if(xintercept>=(mapwidth<<TILESHIFT)) xintercept=mapwidth<<TILESHIFT;
                xspot=0xffff;
                HitVertBorder();
                break;
            }
            if(yspot>mapspotend) break;
            tilehit=*((byte *)tilemap+yspot);
            if(tilehit)
            {
                if(tilehit&0x80)
                {
                    long xintbuf=xintercept+(xstep>>1);
                    if(*((word *)&xintbuf+1)!=*((word *)&xintercept+1))
                        goto passhoriz;
                    if((word)xintbuf<doorposition[tilehit&0x7f])
                        goto passhoriz;
                    xintercept=xintbuf;
                    yintercept=(ytile<<TILESHIFT)+0x8000;
                    HitHorizDoor();
                }
                else
                {
                    if(tilehit==64)
                    {
							   if(pwalldir==di_north || pwalldir==di_south)
								{
	                        long xintbuf;
									int pwallposnorm;
									int pwallposinv;
									if(pwalldir==di_north)
									{
										pwallposnorm = 63-pwallpos;
										pwallposinv = pwallpos;
									}
									else
									{
										pwallposnorm = pwallpos;
										pwallposinv = 63-pwallpos;
									}
									if(pwalldir == di_south && ytile==pwally && *((word *)&xintercept+1)==pwallx
										|| pwalldir == di_north && !(ytile==pwally && *((word *)&xintercept+1)==pwallx))
									{
										xintbuf=xintercept+((xstep*pwallposnorm)>>6);
	   	                     if(*((word *)&xintbuf+1)!=*((word *)&xintercept+1))
   	   	                     goto passhoriz;
											
									   MARKPIX(4,2);

         		               yintercept=(ytile<<TILESHIFT)+TILEGLOBAL-(pwallposinv<<10);
      	      	            xintercept=xintbuf;
										tilehit=pwalltile;
                                                                           // LOGF(log,"Pushwall hit 7: HitHorizWall:\n  xintercept=%.8X  xtile=%.4X\n  yintercept=%.8X  ytile=%.4X\n  pwallpos=%i\n",xintercept,xtile,yintercept,ytile,pwallpos);
	               	         HitHorizWall();
									}
									else
									{
										xintbuf=xintercept+((xstep*pwallposinv)>>6);
	   	                     if(*((word *)&xintbuf+1)!=*((word *)&xintercept+1))
   	   	                     goto passhoriz;
										
									   MARKPIX(4,1);

         		               yintercept=(ytile<<TILESHIFT)-(pwallposinv<<10);
      	      	            xintercept=xintbuf;
										tilehit=pwalltile;
                                                                           // LOGF(log,"Pushwall hit 8: HitHorizWall:\n  xintercept=%.8X  xtile=%.4X\n  yintercept=%.8X  ytile=%.4X\n  pwallpos=%i\n",xintercept,xtile,yintercept,ytile,pwallpos);
	               	         HitHorizWall();
									}
								}
								else
								{
									int pwallposi = pwallpos;
									if(pwalldir==di_west) pwallposi = 63-pwallpos;
									if(pwalldir==di_east && (word)xintercept<(pwallposi<<10)
											|| pwalldir==di_west && (word)xintercept>(pwallposi<<10))
									{
										if(*((word *)&xintercept+1)==pwallx && ytile==pwally)
										{												
										   if(pwalldir==di_east && (long)((word)xintercept)+xstep<(pwallposi<<10)
													|| pwalldir==di_west && (long)((word)xintercept)+xstep>(pwallposi<<10))
											   goto passhoriz;
												
										   MARKPIX(3,15);

                                                                              // LOGF(log,"Pushwall hit 9: HitVertWall old values:\n  xintercept=%.8X  xtile=%.4X\n  yintercept=%.8X  ytile=%.4X\n  pwallpos=%.4X",xintercept,xtile,yintercept,ytile,pwallpos);

											if(pwalldir==di_east)
											   xintercept=(xintercept&0xffff0000)+(pwallposi<<10);
											else
											   xintercept=(xintercept&0xffff0000)-TILEGLOBAL+(pwallposi<<10);
     		      	               yintercept=yintercept-((ystep*(63-pwallpos))>>6);
											tilehit=pwalltile;
                                                                              // LOGF(log,"Pushwall hit 9: HitVertWall:\n  xintercept=%.8X  xtile=%.4X\n  yintercept=%.8X  ytile=%.4X\n  pwallpos=%.4X\n",xintercept,xtile,yintercept,ytile,pwallpos);
	               	            HitVertWall();
										}
										else
										{
										   MARKPIX(3,11);
											
									      texdelta = -(pwallposi<<10);
											yintercept=ytile<<TILESHIFT;
											tilehit=pwalltile;
                                                                                   // LOGF(log,"Pushwall hit 10: HitHorizWall:\n  xintercept=%.8X  xtile=%.4X\n  yintercept=%.8X  ytile=%.4X\n  pwallpos=%.4X\n",xintercept,xtile,yintercept,ytile,pwallpos);
											HitHorizWall();
										}
									}
									else
									{
										if(*((word *)&xintercept+1)==pwallx && ytile==pwally)
										{
										   MARKPIX(3,12);
										
									      texdelta = -(pwallposi<<10);
											yintercept=ytile<<TILESHIFT;
											tilehit=pwalltile;
                                                                                   // LOGF(log,"Pushwall hit 11: HitHorizWall:\n  xintercept=%.8X  xtile=%.4X\n  yintercept=%.8X  ytile=%.4X\n  pwallpos=%.4X\n",xintercept,xtile,yintercept,ytile,pwallpos);
											HitHorizWall();
										}
										else
										{
										   if(pwalldir==di_east && (long)((word)xintercept)+xstep>(pwallposi<<10)
													|| pwalldir==di_west && (long)((word)xintercept)+xstep<(pwallposi<<10))
											   goto passhoriz;
												
										   MARKPIX(3,3);

											if(pwalldir==di_east)
											   xintercept=(xintercept&0xffff0000)-((63-pwallpos)<<10);
											else
											   xintercept=(xintercept&0xffff0000)+((63-pwallpos)<<10);
     		      	               yintercept=yintercept-((ystep*pwallpos)>>6);
											tilehit=pwalltile;
                                                                                   // LOGF(log,"Pushwall hit 12: HitVertWall:\n  xintercept=%.8X  xtile=%.4X\n  yintercept=%.8X  ytile=%.4X\n  pwallpos=%.4X\n",xintercept,xtile,yintercept,ytile,pwallpos);
	               	            HitVertWall();
										}
									}
								}
                    }
                    else
                    {
                        yintercept=ytile<<TILESHIFT;
                                                           // LOGF(log,"HitHorizWall:\n  xintercept=%.8X  xtile=%.4X\n  yintercept=%.8X  ytile=%.4X\n",xintercept,xtile,yintercept,ytile);
                        HitHorizWall();
                    }
                }
                break;
            }
passhoriz:
            *((byte *)spotvis+yspot)=1;
            ytile+=ytilestep;
            xintercept+=xstep;
            yspot=(*((word *)&xintercept+1)<<mapshift)+ytile;
                           // LOGF(log,"passhoriz:\n  xintercept=%.8X  xtile=%.4X\n  yintercept=%.8X  ytile=%.4X\n",xintercept,xtile,yintercept,ytile);
        }
        while(1);
#ifdef DEBUGRAYTRACER
		  if(dolog)
		  {
			  fclose(log);
			  dolog=0;
			  log=NULL;
		  }
#endif
    }
}

/*
====================
=
= WallRefresh
=
====================
*/

void WallRefresh (void)
{
//
// set up variables for this view
//
        viewangle = player->angle;
        midangle = viewangle*(FINEANGLES/ANGLES);
        viewsin = sintable[viewangle];
        viewcos = costable[viewangle];

        if ( focallength < FOCALTHRESHOLD ) {
          viewx = player->x - FixedMul( focallength, viewcos );
          viewy = player->y + FixedMul( focallength, viewsin );
        } else {
          viewx = player->x - FixedMul( FOCALTHRESHOLD, viewcos );
          viewy = player->y + FixedMul( FOCALTHRESHOLD, viewsin );
        }

        focaltx = (short)(viewx>>TILESHIFT);
        focalty = (short)(viewy>>TILESHIFT);

        viewtx = (short)(player->x >> TILESHIFT);
        viewty = (short)(player->y >> TILESHIFT);

        xpartialdown = viewx&(TILEGLOBAL-1);
        xpartialup = TILEGLOBAL-xpartialdown;
        ypartialdown = viewy&(TILEGLOBAL-1);
        ypartialup = TILEGLOBAL-ypartialdown;

        lastside = -1;                  // the first pixel is on a new wall
        AsmRefresh ();
        ScalePost ();                   // no more optimization on last post
}

//==========================================================================

/*
========================
=
= ThreeDRefresh
=
========================
*/

void    ThreeDRefresh (void)
{
    unsigned ceiling,parallax=1; 
    boolean flats; 
            
//
// clear out the traced array
//
        memset(spotvis,0,maparea);
        spotvis[player->tilex][player->tiley] = 1;       // Detect all sprites over player fix

        vbuf+=screenofs;

//
// follow the walls from there to the right, drawwing as we go
//
    ceiling=vgaCeiling[gamestate.episode*30+mapon]; 
/*    
    if((ceiling>>8)==(ceiling&0xFF))
    { 
          VGAClearScreen(ceiling); 
          flats=false; 
    } 
    else
*/       
    { 
          flats=true; 
          ceiling=((ceiling&0xFF)-1)<<1; 
    } 

    WallRefresh(); 
    
    if(parallax) 
          DrawParallax(MAPSPOT(0,63,0));    

    if(flats) 
          DrawFlats(); // draw textured floor/ceiling 

        WallRefresh ();

//
// draw all the scaled images
//
        playy=(player->y+FixedMul(0x7300,viewsin)); 
        playx=(player->x-FixedMul(0x7300,viewcos)); 
        
        DrawScaleds();                  // draw scaled stuff
        DrawPlayerWeapon ();    // draw player's hands

        if (messagetime > 0)
          DrawMessage ();
          
        if (signmessagetime > 0)
          DrawSignMessage ();  
          
        if (lyricstime > 0)
          DrawLyrics ();            
          
        if (gamestate.activetimers)  
          DisplayTimer();
          
//
// show screen and time last cycle
//

        if (fizzlein)
        {
                FizzleFade((long)vbuf-0xa0000,(long)vdisp-0xa0000+screenofs,viewwidth,viewheight,20,false);
                fizzlein = false;

                lasttimecount = TimeCount = 0;          // don't make a big tic count
        }
        
        vbuf-=screenofs;
        vdisp=vbuf;

        _asm {
                cli
                mov     edx,0x3d4
                mov     al,0x0c
                out     dx,al
                inc     edx
                mov     al,byte ptr vdisp+1
                out     dx,al
                sti
        }

        vbuf+=SCREENSIZE;

        if((long)vbuf > 0xa0000+PAGE3START)
                vbuf=(byte *) 0xa0000;
        frameon++;        

#ifndef REMDEBUG
        if (fpscounter)
        {
                fps_frames++;
                fps_time+=tics;

                if(fps_time>35)
                {
                        fps_time-=35;
                        fps=fps_frames<<1;
                        fps_frames=0;
                }
                fontnumber = 0;
                SETFONTCOLOR(7,127);
                PrintX=8; PrintY=190;
                VWB_Bar(2,189,50,10,bordercol);
                US_PrintSigned(fps);
                US_Print(" fps");
        }
#endif

                
}


/*
===================================
=
= GetMessage (gets ingame messages)
=
===================================
*/

void GetMessage (char *lastmessage)
{
        messagetime = 150; // time for message to display

        strcpy(gamestate.message, lastmessage);
}

/*
========================================
=
= DrawMessage (displays ingame messages)
=
========================================
*/

void DrawMessage (void)
{
	if(messagetime <= 0) { strcpy (gamestate.message,""); return; }
    messagetime-=tics;

    fontnumber = 0;

// message's shadow 
    SETFONTCOLOR(0x0,0x1); // set the color
    PrintX=7; PrintY=1; // position the message 
    US_Print(gamestate.message); // print the message 

// the actual message     
    SETFONTCOLOR(0x45,0x0); // set the color
    PrintX=8; PrintY=2; // position the message
    US_Print(gamestate.message); // print the message
    DrawAllPlayBorderSides ();
}

/*
===================================
=
= GetSignMessage (gets ingame sign messages)
=
===================================
*/


void    GetSignMessage (char *lastsignmessage, unsigned lastsignpicnum) //, soundnames sound)
{
        signmessagetime = 450; // time for sign message to display

        strcpy(gamestate.signmessage, lastsignmessage);
        gamestate.signpicnum = lastsignpicnum;

//        SD_PlaySound (sound); 
}

/*
========================================
=
= DrawSignMessage (displays ingame messages)
=
========================================
*/

void DrawSignMessage (void)
{
	if(signmessagetime <= 0) { strcpy (gamestate.signmessage,""); return; }
    signmessagetime-=tics;

    fontnumber = 0;

    LatchDrawPic (1,15,gamestate.signpicnum);

// message's shadow 
    SETFONTCOLOR(0x0,0x1); // set the color
    PrintX=39; PrintY=19; // position the sign message 
    US_Print(gamestate.signmessage); // print the sign message 

// the actual message     
    SETFONTCOLOR(0x45,0x0); // set the color
    PrintX=40; PrintY=20; // position the sign message
    US_Print(gamestate.signmessage); // print the sign message
    DrawAllPlayBorderSides ();
}

/*
===================================
=
= GetLyrics (gets lyrcs)
=
===================================
*/

void GetLyrics (char *lastlyrics)
{
        lyricstime = 450; // time for message to display

        strcpy(gamestate.lyrics, lastlyrics);
}

/*
========================================
=
= DrawLyrics (displays ingame messages)
=
========================================
*/

void DrawLyrics (void)
{
	if(lyricstime <= 0) { strcpy (gamestate.lyrics,""); return; }
    lyricstime-=tics;

    fontnumber = 0;

// message's shadow 
    SETFONTCOLOR(0x0,0x1); // set the color
    PrintX=39; PrintY=19; // position the sign message 
    US_Print(gamestate.lyrics); // print the sign message 

// the actual message     
    SETFONTCOLOR(0x45,0x0); // set the color
    PrintX=40; PrintY=20; // position the sign message
    US_Print(gamestate.lyrics); // print the sign message
    DrawAllPlayBorderSides ();
}


/*
========================
=
= GetTimer
=
= gets ingame messages
=
========================
*/

void GetTimer (int seconds, int eventtype)
{
    if (gamestate.activetimers > MAXTIMERS) return;

    // Find if a timer for the event exists
    for (int t = 0; t < gamestate.activetimers; t++) {
        if (gamestate.timers[t].type == eventtype) {
            gamestate.timers[t].tics += seconds * 70;
            return;
        }
    }

    // Timer doesn't already exist for event
    gamestate.timers[gamestate.activetimers].tics = seconds * 70;       // increment the correct timer
    gamestate.timers[gamestate.activetimers].type = eventtype;

    switch (eventtype) { // set the color based on event type
        case T_GOD:
            gamestate.timers[gamestate.activetimers].color = GOD_TMR_CLR;
            break;
    }
    gamestate.activetimers++;                   // increase the number of active timers
}

/*
==================
=
= DisplayTimer 
=
==================
*/

int lastsec = -1;

void DisplayTimer (void)
{
   for (int x = 0; x < gamestate.activetimers; x++) {
       // Calculate timer values (tics -> min/sec)
       if (gamestate.timers[x].tics <= 0) { // a timer is done
           gamestate.activetimers--;

            for (int t = x; t < gamestate.activetimers; t++) { // shift the relatively new timers up
                gamestate.timers[t].tics = gamestate.timers[t+1].tics;
                gamestate.timers[t].color = gamestate.timers[t+1].color;
                gamestate.timers[t].type = gamestate.timers[t+1].type;
            }
       }

       if (gamestate.timers[x].tics <= 0) return;

       int sec = gamestate.timers[x].tics / 70;
       int min = sec / 60;
       int yOffset = (x << 3) + x;
       sec %= 60;
       
       // Draw timer's shadow
       fontnumber = 0;
       SETFONTCOLOR(0x0,0x1); // set the color{
       SETPRINTXY(timerPrintX+267,timerPrintY+138 + yOffset + 1);
       US_PrintUnsignedTimer(min); US_Print(":"); US_PrintUnsignedTimer(sec); // print message

       // Draw timer
       SETFONTCOLOR(gamestate.timers[x].color,0xf); // set the color
       SETPRINTXY(timerPrintX+268,timerPrintY+140 + yOffset);
       US_PrintUnsignedTimer(min); US_Print(":"); US_PrintUnsignedTimer(sec);  // print message

       if (gamestate.timers[x].tics > 0)
            gamestate.timers[x].tics -= tics;
   }
} 


//===========================================================================

