// WL_DEBUG.C

#include "WL_DEF.H"
#pragma hdrstop
#include <BIOS.H>

/*
=============================================================================

                                                 LOCAL CONSTANTS

=============================================================================
*/

#define VIEWTILEX       (viewwidth/16)
#define VIEWTILEY       (viewheight/16)

/*
=============================================================================

                                                 GLOBAL VARIABLES

=============================================================================
*/

#ifdef DEBUGKEYS

int DebugKeys (void);


// from WL_DRAW.C

void ScalePost();
void SimpleScaleShape (int xcenter, int shapenum, unsigned height);

/*
=============================================================================

                                                 LOCAL VARIABLES

=============================================================================
*/

int     maporgx;
int     maporgy;
enum {mapview,tilemapview,actoratview,visview}  viewtype;

void ViewMap (void);

//===========================================================================

#ifdef GIBTSNIMMER

/*
==================
=
= DebugMemory
=
==================
*/

void DebugMemory (void)
{
        int     i;
        char    scratch[80],str[10];
        long    mem;
        spritetype _seg *block;

        CenterWindow (16,7);

        US_CPrint ("Memoria hasznalata");
        US_CPrint ("------------");
        US_Print ("Teljes     :");
        US_PrintUnsigned (mminfo.mainmem/1024);
        US_Print ("k\nSzabad      :");
        US_PrintUnsigned (MM_UnusedMemory()/1024);
        US_Print ("k\nFelszabaditassal:");
        US_PrintUnsigned (MM_TotalFree()/1024);
        US_Print ("k\n");
        VW_UpdateScreen();
        IN_Ack ();
}

//===========================================================================

#endif

/*
==================
=
= CountObjects
=
==================
*/

void CountObjects (void)
{
        int     i,total,count,active,inactive,doors;
        objtype *obj;

        CenterWindow (17,7);
        active = inactive = count = doors = 0;

        US_Print ("Total statics :");
        total = laststatobj-&statobjlist[0];
        US_PrintUnsigned (total);

        char str[60];
        sprintf(str,"\nlaststatobj=%.8X",laststatobj);
        US_Print(str);

        US_Print ("\nHasznalatban levo statikusok:");
        for (i=0;i<total;i++)
                if (statobjlist[i].shapenum != -1)
                        count++;
                else
                        doors++;        //debug
        US_PrintUnsigned (count);

        US_Print ("\nAjtok        :");
        US_PrintUnsigned (doornum);

        for (obj=player->next;obj;obj=obj->next)
        {
                if (obj->active)
                        active++;
                else
                        inactive++;
        }

        US_Print ("\nTeljes szineszek  :");
        US_PrintUnsigned (active+inactive);

        US_Print ("\nAktiv szineszek :");
        US_PrintUnsigned (active);

        VW_UpdateScreen();
        IN_Ack ();
}


//===========================================================================

/*
===================
=
= PictureGrabber
=
===================
*/

void PictureGrabber (void)
{
                char *fname="WSHOT000.BMP";
                int file;
                byte buffer[320];
                byte *wolfpal=(byte *)(gamepal);
                int i,x;

                unsigned long col;
                byte bmpheads[54]={'B','M',0x8e,0x3f,0,0,0,0,0,0,0x36,4,0,0,
                    40,0,0,0,0x40,1,0,0,200,0,0,0,1,0,8,0,0,0,0,0,
                    0,0xfa,0,0,0xc4,0x0e,0,0,0xc4,0x0e,0,0,0,0,0,0,0,0,0,0};
                     
                for(i=0;i<1000;i++)
                {
                        fname[7]=i%10+'0';
                        fname[6]=(i/10)%10+'0';
                        fname[5]=i/100+'0';
                        file=open(fname,O_BINARY | O_RDONLY);
                        if(file==-1) break;
                        close(file);
                }

                // overwrites WSHOT999.BMP if all wshot files exist

                file=open(fname,O_CREAT| O_BINARY | O_WRONLY, S_IREAD | S_IWRITE | S_IFREG);
                if(file==-1)
                {
                        CenterWindow(22,3);
                        US_Print("Fajl letrehozasa nem sikerult:\n");
                        US_Print(fname);
                        VW_UpdateScreen();
                        IN_Ack();
                        return;
                }

                write(file,bmpheads,sizeof(bmpheads));
                for(i=0;i<256;i++)                              // 1024 bytes palette
                {
                        col=(((((unsigned long)wolfpal[i*3])<<10)+
                             (((unsigned long)wolfpal[i*3+1])<<2))<<8)+
                             (((unsigned long)wolfpal[i*3+2])<<2);
                        write(file,&col,4);
                }
                
                for(i=199;i>=0;i--)
                {                                               // 64000 bytes image data
                        for(x=0;x<320;x++)
                        {
                                byte mask=x&3, pix;
                                VGAREADMAP(mask);
                                pix=vbuf[i*80+(x>>2)];
                                buffer[x]=pix;
                        }
                        write(file,buffer,320);         // writes by line to make it 320 times faster
                }
                close(file);

                CenterWindow (18,2);
                US_PrintCentered ("Pillanatkepet keszitett");
                VW_UpdateScreen();
                IN_Ack();
}


//===========================================================================

/*
===================
=
= BasicOverhead
=
===================
*/

void BasicOverhead (void)
{
                int x, y, z, offx, offy;

                z = 128/MAPSIZE; // zoom scale
                offx = 320/2;
                offy = (160-MAPSIZE*z)/2;
                
        #ifdef MAPBORDER
                int temp = viewsize;
                NewViewSize(16);
                DrawAllPlayBorder();
        #endif

                // right side (raw)
                        
                for(x=0;x<MAPSIZE;x++)
                        for(y=0;y<MAPSIZE;y++)
                                VWB_Bar(x*z+offx, y*z+offy,z,z,(unsigned)actorat[x][y]);

                // left side (filtered)
                
                int tile, color;
                offx -= 128;
                
                for(x=0;x<MAPSIZE;x++)
                        for(y=0;y<MAPSIZE;y++)
                        {
                                tile = (unsigned)actorat[x][y];
                                if (tile > 255 && ((objtype *)tile)->flags&FL_SHOOTABLE) color = 72;  // enemy
                                else if (!tile || tile > 255)
                                {
                                        if (spotvis[x][y]) color = 111;  // visable
                                        else color = 0;  // nothing
                                }
                                else if (MAPSPOT(x,y,1) == PUSHABLETILE) color = 171;  // pushwall
                                else if (tile == 64) color = 158; // solid obj
                                else if (tile < 128) color = 154;  // walls
                                else if (tile < 256) color = 146;  // doors
                                
                                VWB_Bar(x*z+offx, y*z+offy,z,z,color);
                        }

                VWB_Bar(player->tilex*z+offx,player->tiley*z+offy,z,z,15); // player

                // resize the border to match

                VW_UpdateScreen();
                IN_Ack();
                
        #ifdef MAPBORDER
                NewViewSize(temp);
                DrawAllPlayBorder();
        #endif
}


//===========================================================================

/*
================
=
= ShapeTest
=
================
*/

void ShapeTest (void)
{
extern  word    NumDigi;
extern  word    *DigiList;
extern  int     postx;
extern  int     postwidth;
extern  byte    *postsource;
static  char    buf[10];

        boolean                 done;
        ScanCode                scan;
        int                             i,j,k,x;
        longword                l;
        byte *addr;
        soundnames sound;
//      PageListStruct  far *page;

        CenterWindow(20,16);
        VW_UpdateScreen();
        for (i = 0,done = false;!done;)
        {
                US_ClearWindow();
                sound = (soundnames) -1;

//              page = &PMPages[i];
                US_Print(" Oldal #");
                US_PrintUnsigned(i);
                if (i < PMSpriteStart)
                        US_Print(" (Fal)");
                else if (i < PMSoundStart)
                        US_Print(" (Cucc)");
                else if (i == ChunksInFile - 1)
                        US_Print(" (Hang Info)");
                else
                        US_Print(" (Hang)");

/*              US_Print("\n XMS: ");
                if (page->xmsPage != -1)
                        US_PrintUnsigned(page->xmsPage);
                else
                        US_Print("Nem");

                US_Print("\n Main: ");
                if (page->mainPage != -1)
                        US_PrintUnsigned(page->mainPage);
                else if (page->emsPage != -1)
                {
                        US_Print("EMS ");
                        US_PrintUnsigned(page->emsPage);
                }
                else
                        US_Print("Nem");

                US_Print("\n Last hit: ");
                US_PrintUnsigned(page->lastHit);*/

                US_Print("\n Cim: ");
//              addr = PM_GetPageAddress(i);
                addr = Pages+i*4096;
                sprintf(buf,"0x%08X",(long) addr);
                US_Print(buf);

                if (addr)
                {
                        if (i < PMSpriteStart)
                        {
                        //
                        // draw the wall
                        //
                                vbuf += 32*SCREENWIDTH;
                                postx = 128;
                                postwidth = 1;
                                postsource = addr;
                                for (x=0;x<64;x++,postx++,postsource+=64)
                                {
                                        wallheight[postx] = 256;
                                        ScalePost ();
                                }
                                vbuf -= 32*SCREENWIDTH;
                        }
                        else if (i < PMSoundStart)
                        {
                        //
                        // draw the sprite
                        //
                                vbuf += 32*SCREENWIDTH;
                                SimpleScaleShape (160, i-PMSpriteStart, 64);
                                vbuf -= 32*SCREENWIDTH;
                        }
                        else if (i == ChunksInFile - 1)
                        {
                                US_Print("\n\n Hangok szama: ");
                                US_PrintUnsigned(NumDigi);
                                for (l = j = k = 0;j < NumDigi;j++)
                                {
                                        l += DigiList[(j * 2) + 1];
                                        k += (DigiList[(j * 2) + 1] + (PMPageSize - 1)) / PMPageSize;
                                }
                                US_Print("\n Teljes bajtok: ");
                                US_PrintUnsigned(l);
                                US_Print("\n Teljes oldalak: ");
                                US_PrintUnsigned(k);
                        }
                        else
                        {
                                byte *dp = addr;
                                for (j = 0;j < NumDigi;j++)
                                {
                                        k = (DigiList[(j * 2) + 1] + (PMPageSize - 1)) / PMPageSize;
                                        if      ((i >= PMSoundStart + DigiList[j * 2])
                                                        &&      (i < PMSoundStart + DigiList[j * 2] + k))
                                                break;
                                }
                                if (j < NumDigi)
                                {
                                        sound = (soundnames) j;
                                        US_Print("\n Hang #");
                                        US_PrintUnsigned(j);
                                        US_Print("\n Szegmens #");
                                        US_PrintUnsigned(i - PMSoundStart - DigiList[j * 2]);
                                }
                                for (j = 0;j < PageLengths[i];j += 32)
                                {
                                        byte v = dp[j];
                                        int v2 = (unsigned)v;
                                        v2 -= 128;
                                        v2 /= 4;
                                        if (v2 < 0)
                                                VWB_Vlin(WindowY + WindowH - 32 + v2,
                                                                WindowY + WindowH - 32,
                                                                WindowX + 8 + (j / 32),BLACK);
                                        else
                                                VWB_Vlin(WindowY + WindowH - 32,
                                                                WindowY + WindowH - 32 + v2,
                                                                WindowX + 8 + (j / 32),BLACK);
                                }
                        }
                }

                VW_UpdateScreen();

                while ((scan = LastScan)==0)
                        SD_Poll();

                IN_ClearKey(scan);
                switch (scan)
                {
                case sc_LeftArrow:
                        if (i)
                                i--;
                        break;
                case sc_RightArrow:
                        if (++i >= ChunksInFile)
                                i--;
                        break;
                case sc_W:      // Walls
                        i = 0;
                        break;
                case sc_S:      // Sprites
                        i = PMSpriteStart;
                        break;
                case sc_D:      // Digitized
                        i = PMSoundStart;
                        break;
                case sc_I:      // Digitized info
                        i = ChunksInFile - 1;
                        break;
/*              case sc_L:      // Load all pages
                        for (j = 0;j < ChunksInFile;j++)
                                PM_GetPage(j);
                        break;*/
                case sc_P:
                        if (sound != -1)
                                SD_PlayDigitized(sound,8,8);
                        break;
                case sc_Escape:
                        done = true;
                        break;
/*              case sc_Enter:
                        PM_GetPage(i);
                        break;*/
                }
        }
        SD_StopDigitized();
}


//===========================================================================


/*
================
=
= DebugKeys
=
================
*/

int DebugKeys (void)
{
        boolean esc;
        int level;

/*      if (Keyboard[sc_A])
        {
                ammocheat^=1;
                CenterWindow (18,3);
                if (ammocheat)
                        US_PrintCentered ("Vegtelen tolteny BE");
                else
                        US_PrintCentered ("Vegtelen tolteny KI");
                VW_UpdateScreen();
                IN_Ack ();
                return 1;
        } */
        if (Keyboard[sc_B])             // B = border color
        {
                CenterWindow(20,3);
                PrintY+=6;
                US_Print(" Hatar szin (0-56): ");
                VW_UpdateScreen();
                esc = !US_LineInput (px,py,str,NULL,true,2,0);
                if (!esc)
                {
                        level = atoi (str);
                        if (level>=0 && level<=99)
                        {
                                if (level<30) level += 31;
                                else
                                {
                                        if (level > 56) level=31;
                                        else level -= 26;
                                }
                                        
                                bordercol=level*4+3;

                                if (bordercol == VIEWCOLOR)
                                        DrawAllStatusBorder(bordercol);
                                
                                DrawAllPlayBorder();
                                        
                                return 0;
                        }                            
                }
                return 1;
        }
        if (Keyboard[sc_C])             // C = count objects
        {
                CountObjects();
                return 1;
        }
        if (Keyboard[sc_D])              // D = Darkone's FPS counter
        {
                CenterWindow (22,2);
                if (fpscounter)
                  US_PrintCentered ("Darkone FPS szamlaloja KI");
                else
                  US_PrintCentered ("Darkone FPS szamlaloja BE");
                VW_UpdateScreen();
                IN_Ack();
                fpscounter ^= 1;
        /*      if (!fpscounter)
                        DrawPlayScreen(); */                
                return 1;
        }
        if (Keyboard[sc_E])             // E = quit level
        {
                if (tedlevel)
                        Quit (NULL);
                playstate = ex_completed;
//              gamestate.mapon++;
        }

        if (Keyboard[sc_F])             // F = facing spot
        {
                char str[60];
                CenterWindow (14,6);
                US_Print ("x:");     US_PrintUnsigned (player->x);
                US_Print (" (");     US_PrintUnsigned (player->x%65536);
                US_Print (")\ny:");  US_PrintUnsigned (player->y);
                US_Print (" (");     US_PrintUnsigned (player->y%65536);
                US_Print (")\nA:");  US_PrintUnsigned (player->angle);
                US_Print (" X:");    US_PrintUnsigned (player->tilex);
                US_Print (" Y:");    US_PrintUnsigned (player->tiley);
                US_Print ("\n1:");   US_PrintUnsigned (tilemap[player->tilex][player->tiley]);
                sprintf(str," 2:%.8X",(unsigned)actorat[player->tilex][player->tiley]); US_Print(str);
                US_Print ("\nf 1:"); US_PrintUnsigned (player->areanumber);
                US_Print (" 2:");    US_PrintUnsigned (MAPSPOT(player->tilex,player->tiley,1));
                US_Print (" 3:");
                if ((unsigned)actorat[player->tilex][player->tiley] < 256)
                        US_PrintUnsigned (spotvis[player->tilex][player->tiley]);
                else              
                        US_PrintUnsigned (actorat[player->tilex][player->tiley]->flags);
                VW_UpdateScreen();
                IN_Ack();
                return 1;
        }

        if (Keyboard[sc_G])             // G = god mode
        {
                CenterWindow (12,2);
                if (gamestate.godcheat == 0)
                        US_PrintCentered ("Isten mod BE");
                else if (gamestate.godcheat == 1)
                        US_PrintCentered ("Isten mod KI");

                VW_UpdateScreen();
                IN_Ack();
                if (gamestate.godcheat != 1)
                        gamestate.godcheat++;
                else
                        gamestate.godcheat = 0;                        
                return 1;
        }
        if (Keyboard[sc_H])             // H = hurt self
        {
                IN_ClearKeysDown ();
                TakeDamage (16,NULL);
        }
        else if (Keyboard[sc_I])                        // I = item cheat
        {
                CenterWindow (12,3);
                US_PrintCentered ("Ingyen targyak!");
                VW_UpdateScreen();
                GivePoints (100000);
                HealSelf2 (199);
                GiveExtraMan (200); 
                if (gamestate.weapon<wp_assault)
                        GiveWeapon (gamestate.weapon+1);
                gamestate.ammo += 300;
                if (gamestate.ammo > gamestate.maxammo)
                        gamestate.ammo = gamestate.maxammo;
                gamestate.shells += 200;
                if (gamestate.shells > gamestate.maxshells)
                        gamestate.shells = gamestate.maxshells;
                gamestate.clip += 125;
                if (gamestate.clip > gamestate.maxclip)
                        gamestate.clip = gamestate.maxclip;             
				DrawHealth ();		               
                DrawAmmo ();
                IN_Ack ();
                return 1;
        }
        else if (Keyboard[sc_K])                        // K = give keys
        {
                CenterWindow(16,3);
                PrintY+=6;
                US_Print("  Kulcs adasa (1-4): ");
                VW_UpdateScreen();
                esc = !US_LineInput (px,py,str,NULL,true,1,0);
                if (!esc)
                {
                        level = atoi (str);
                        if (level>0 && level<5)
                                GiveKey(level-1);
                }
                return 1;
        }
        else if (Keyboard[sc_L])                        // L = level ratios
        {
                byte x,start,end=LRpack;
                
                if (end == 29)   // wolf3d
                {
                        CenterWindow(19,10);
                        start = 0;
                }                        
                else            // sod
                {
                        CenterWindow(19,12);
                        start = 0; end = 10;
                }
                again:
                for(x=start;x<end;x++)
                {
                        US_PrintUnsigned(x+1);
                        US_Print(" ");
                        US_PrintUnsigned(LevelRatios[x].time/60);
                        US_Print(":");
                        if (LevelRatios[x].time%60 < 10)
                                US_Print("0");
                        US_PrintUnsigned(LevelRatios[x].time%60);
                        US_Print(" ");
                        US_PrintUnsigned(LevelRatios[x].kill);
                        US_Print("% ");
                        US_PrintUnsigned(LevelRatios[x].secret);
                        US_Print("% ");
                        US_PrintUnsigned(LevelRatios[x].treasure);
                        US_Print("%\n");
                }                        
                VW_UpdateScreen();
                IN_Ack();
                if (end == 10 && gamestate.mapon > 9)
                {
                        start = 10; end = 20;
                        CenterWindow(19,12);
                        goto again;
                }
                if (end == 20 && gamestate.mapon > 19)
                {
                        start = 20; end = 30;
                        CenterWindow(19,12);
                        goto again;
                }

                return 1;
        }

        else if (Keyboard[sc_M])                        // M = memory info
        {
                int main, extra, room;
                CenterWindow (30,3);
                // put calculation stuff here
                main = 24; extra = 5; room = 32;
                US_Print (" Vedett: kb. ");
                US_PrintUnsigned(extra);
                US_Print ("M hasznalt ");
                US_PrintUnsigned(room);
                US_Print ("M\nFo/bazis: kb. ");
                US_PrintUnsigned(main);
                US_Print ("k hasznalt 640k-bol");
                VW_UpdateScreen();
                IN_Ack();
                return 1;
        }
        else if (Keyboard[sc_N])                        // N = no clip
        {
                noclip^=1;
                CenterWindow (18,3);
                if (noclip)
                        US_PrintCentered ("Falon valo atmenese BE");
                else
                        US_PrintCentered ("Falon valo atmenese KI");
                VW_UpdateScreen();
                IN_Ack ();
                return 1;
        }
        else if (Keyboard[sc_O])                        // O = basic overhead
        {
                BasicOverhead();
                return 1;
        }
        else if(Keyboard[sc_P])                         // P = Ripper's picture grabber
        {
                PictureGrabber();
                return 1;
        }
        else if (Keyboard[sc_Q])                        // Q = fast quit
                Quit (NULL);
        else if (Keyboard[sc_S])                        // S = slow motion
        {
                CenterWindow(30,3);
                PrintY+=6;
                US_Print(" Lassu mozgo lepesek (alapertelmezett 14): ");
                VW_UpdateScreen();
                esc = !US_LineInput (px,py,str,NULL,true,2,0);
                if (!esc)
                {
                        level = atoi (str);
                        if (level>=0 && level<=50)
                                singlestep = level;
                }
                return 1;
        }
        else if (Keyboard[sc_T])                        // T = shape test
        {
                ShapeTest ();
                return 1;
        }
        else if (Keyboard[sc_V])                        // V = extra VBLs
        {
                CenterWindow(30,3);
                PrintY+=6;
                US_Print("  Adja meg az extra VBL-ek sokassagat (0-8): ");
                VW_UpdateScreen();
                esc = !US_LineInput (px,py,str,NULL,true,1,0);
                if (!esc)
                {
                        level = atoi (str);
                        if (level>=0 && level<=8)
                                extravbls = level;
                }
                return 1;
        }
        else if (Keyboard[sc_W])                        // W = warp to level
        {
                CenterWindow(26,3);
                PrintY+=6;
#ifndef SPEAR
                US_Print("  Ugras milyen szintre (1-16): ");
#else
                US_Print("  Ugras milyen szintre (1-21): ");
#endif
                VW_UpdateScreen();
                esc = !US_LineInput (px,py,str,NULL,true,2,0);
                if (!esc)
                {
                        level = atoi (str);
#ifndef SPEAR
                        if (level>0 && level<17)
#else
                        if (level>0 && level<22)
#endif
                        {
                                gamestate.mapon = level-1;
                                playstate = ex_warped;
                        }
                }
                return 1;
        }
        else if (Keyboard[sc_X])                        // X = item cheat
        {
                CenterWindow (12,3);
                US_PrintCentered ("Extra dolog!");
                VW_UpdateScreen();
                // DEBUG: put stuff here
                IN_Ack ();
                return 1;
        }

        return 0;
}


#if 0
/*
===================
=
= OverheadRefresh
=
===================
*/

void OverheadRefresh (void)
{
        unsigned        x,y,endx,endy,sx,sy;
        unsigned        tile;


        endx = maporgx+VIEWTILEX;
        endy = maporgy+VIEWTILEY;

        for (y=maporgy;y<endy;y++)
                for (x=maporgx;x<endx;x++)
                {
                        sx = (x-maporgx)*16;
                        sy = (y-maporgy)*16;

                        switch (viewtype)
                        {
#if 0
                        case mapview:
                                tile = *(mapsegs[0]+farmapylookup[y]+x);
                                break;

                        case tilemapview:
                                tile = tilemap[x][y];
                                break;

                        case visview:
                                tile = spotvis[x][y];
                                break;
#endif
                        case actoratview:
                                tile = (unsigned)actorat[x][y];
                                break;
                        }

                        if (tile<MAXWALLTILES)
                                LatchDrawTile(sx,sy,tile);
                        else
                        {
                                LatchDrawChar(sx,sy,NUMBERCHARS+((tile&0xf000)>>12));
                                LatchDrawChar(sx+8,sy,NUMBERCHARS+((tile&0x0f00)>>8));
                                LatchDrawChar(sx,sy+8,NUMBERCHARS+((tile&0x00f0)>>4));
                                LatchDrawChar(sx+8,sy+8,NUMBERCHARS+(tile&0x000f));
                        }
                }

}
#endif

#if 0
/*
===================
=
= ViewMap
=
===================
*/

void ViewMap (void)
{
        boolean         button0held;

        viewtype = actoratview;
//      button0held = false;


        maporgx = player->tilex - VIEWTILEX/2;
        if (maporgx<0)
                maporgx = 0;
        if (maporgx>MAPSIZE-VIEWTILEX)
                maporgx=MAPSIZE-VIEWTILEX;
        maporgy = player->tiley - VIEWTILEY/2;
        if (maporgy<0)
                maporgy = 0;
        if (maporgy>MAPSIZE-VIEWTILEY)
                maporgy=MAPSIZE-VIEWTILEY;

        do
        {
//
// let user pan around
//
                PollControls ();
                if (controlx < 0 && maporgx>0)
                        maporgx--;
                if (controlx > 0 && maporgx<mapwidth-VIEWTILEX)
                        maporgx++;
                if (controly < 0 && maporgy>0)
                        maporgy--;
                if (controly > 0 && maporgy<mapheight-VIEWTILEY)
                        maporgy++;

#if 0
                if (c.button0 && !button0held)
                {
                        button0held = true;
                        viewtype++;
                        if (viewtype>visview)
                                viewtype = mapview;
                }
                if (!c.button0)
                        button0held = false;
#endif

                OverheadRefresh ();

        } while (!Keyboard[sc_Escape]);

        IN_ClearKeysDown ();
}
#endif
#endif
