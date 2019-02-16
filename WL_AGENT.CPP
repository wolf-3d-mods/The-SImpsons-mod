// WL_AGENT.C

#include "WL_DEF.H"
#pragma hdrstop

/*
=============================================================================

                                                 LOCAL CONSTANTS

=============================================================================
*/

#define MAXMOUSETURN    10


#define MOVESCALE               150l
#define BACKMOVESCALE   100l
#define ANGLESCALE              20

/*
=============================================================================

                                                 GLOBAL VARIABLES

=============================================================================
*/



//
// player state info
//
boolean         running;
long            thrustspeed;

word plux,pluy;                 // player coordinates scaled to unsigned

short                   anglefrac;
short                   gotgatgun;      // JR

objtype         *LastAttacker;

/*
=============================================================================

                                                 LOCAL VARIABLES

=============================================================================
*/


void    T_Player (objtype *ob);
void    T_Attack (objtype *ob);
void    WarpPlayer (void);

statetype s_player = {false,0,0,(statefunc) T_Player,NULL,NULL};
statetype s_attack = {false,0,0,(statefunc) T_Attack,NULL,NULL};


long    playerxmove,playerymove;

struct atkinf
{
        char    tics,attack,frame;              // attack is 1 for gun, 2 for knife
} attackinfo[6][14] =
{
{ {6,0,1},{6,2,2},{6,0,3},{6,-1,4} },
{ {6,0,1},{6,1,2},{6,0,3},{6,-1,4} },
{ {6,0,1},{6,1,2},{6,3,3},{6,-1,4} },
{ {6,0,1},{6,1,2},{6,4,3},{6,-1,4} },
{ {6,0,1},{6,1,2},{6,0,3},{6,0,4},{6,0,5},{9,10,6},{9,0,7},{6,-1,0} },
{ {6,0,1},{6,1,2},{6,0,3},{6,0,4},{6,0,5},{6,0,6},{9,10,7},{9,0,8},{6,-1,0} },
};


short strafeangle[9] = {0,90,180,270,45,135,225,315,0};

//===========================================================================

//----------

void Attack (void);
void Use (void);
void Search (objtype *ob);
void SelectWeapon (void);
void SelectItem (void);

//----------

boolean TryMove (objtype *ob);
void T_Player (objtype *ob);

void ClipMove (objtype *ob, long xmove, long ymove);

/*
=============================================================================

                                                CONTROL STUFF

=============================================================================
*/

// Checks to make sure you have ammo for weapon 
// returns "true" if yes and "false" if no 
int ChkAtkAmmo (int weapon)
{ 
   switch (weapon) 
   {    
      case wp_knife:    
           return 1; 						
      case wp_pistol: 
           if (gamestate.ammo > 0) return 1; 
         break; 
      case wp_machinegun: 
           if (gamestate.ammo > 0) return 1; 
         break; 
      case wp_chaingun: 
           if (gamestate.ammo > 0) return 1; 
         break; 
      case wp_shotgun: 
           if (gamestate.shells > 0) return 1; 
         break;     
      case wp_dshotgun: 
           if (gamestate.shells > 0) return 1; 
         break;   
      case wp_assault: 
           if (gamestate.clip > 0) return 1; 
         break; 
      case wp_dpistol: 
           if (gamestate.ammo > 0) return 1; 
         break;                                                      		   
   } 
   return 0; 
} 

/*
======================
=
= CheckWeaponChange
=
= Keys 1-6 change weapons
=
======================
*/

void CheckWeaponChange (void)
{
//   int   i,buttons; 

   if (gamestate.chosenweapon == wp_assault)
   {
     if (Keyboard[sc_Z])
     {
        static int zoom = 0;
        gamestate.zoom = zoom;

        zoom ^= true;
        BasicZoom( zoom ? 150 : 0 );
        Keyboard[sc_Z] = false;
        IN_ClearKeysDown ();
     }
   }

   if (!gamestate.ammo && !gamestate.shells && !gamestate.clip)      // must use knife with no ammo 
   { 
      gamestate.weapon = wp_knife; 
      return; 
   }  
   if (gamestate.chosenweapon == wp_pistol && !gamestate.weaponexists[wp_pistol] && !gamestate.shells && !gamestate.clip) 
   { 
      gamestate.chosenweapon = gamestate.weapon = wp_knife;  
   } 

/*    
      for (i=wp_knife ; i<=gamestate.bestweapon ; i++) 
      if (buttonstate[bt_readyknife+i-wp_knife]) 
      { 
         gamestate.weapon = gamestate.chosenweapon = i; 
         DrawWeapon (); 
         return; 
      } 
*/ 
   
    if (Keyboard[sc_1] && gamestate.weaponexists[wp_knife] && gamestate.weapon != wp_knife) 
    {
      if (bobmode)
      {
      gamestate.changingweapon = true;
      gamestate.goingdown = true;
      }
      gamestate.nextweapon = gamestate.chosenweapon = wp_knife; 
    }
    if (Keyboard[sc_2]) 
    { 
      if (gamestate.weaponexists[wp_pistol] && gamestate.weapon != wp_pistol)
      {	
       if (bobmode)
       {
   	   gamestate.changingweapon = true;
	   gamestate.goingdown = true;
       }
        gamestate.nextweapon = gamestate.chosenweapon = wp_pistol;
      }
      else if (gamestate.weaponexists[wp_dpistol] && gamestate.weapon != wp_dpistol)
      {	
       if (bobmode)
       {
   	   gamestate.changingweapon = true;
	   gamestate.goingdown = true;
       }

        gamestate.nextweapon = gamestate.chosenweapon = wp_dpistol;
      }
      IN_ClearKeysDown(); 
    }
    if (Keyboard[sc_3] && gamestate.weaponexists[wp_machinegun] && gamestate.weapon != wp_machinegun) 
    { 
      if (bobmode)
      {
   	  gamestate.changingweapon = true;
	  gamestate.goingdown = true;
      }
      gamestate.nextweapon = gamestate.chosenweapon = wp_machinegun; 
    } 
    if (Keyboard[sc_4] && gamestate.weaponexists[wp_chaingun] && gamestate.weapon != wp_chaingun) 
    { 
      if (bobmode)
      {
   	  gamestate.changingweapon = true;
	  gamestate.goingdown = true;
      }
      gamestate.nextweapon = gamestate.chosenweapon = wp_chaingun; 
    }
    if (Keyboard[sc_5]) 
    { 
      if (gamestate.weaponexists[wp_shotgun] && gamestate.weapon != wp_shotgun)
      {	
       if (bobmode)
       {
   	   gamestate.changingweapon = true;
	   gamestate.goingdown = true;
       }
        gamestate.nextweapon = gamestate.chosenweapon = wp_shotgun;
      }
      else if (gamestate.weaponexists[wp_dshotgun] && gamestate.weapon != wp_dshotgun)
      {	
       if (bobmode)
       {
   	   gamestate.changingweapon = true;
	   gamestate.goingdown = true;
       }

        gamestate.nextweapon = gamestate.chosenweapon = wp_dshotgun;
      }
      IN_ClearKeysDown();
    }
    if (Keyboard[sc_6] && gamestate.weaponexists[wp_assault] && gamestate.weapon != wp_assault) 
    { 
      if (bobmode)
      {
   	  gamestate.changingweapon = true;
	  gamestate.goingdown = true;
      }
      gamestate.nextweapon = gamestate.chosenweapon = wp_assault; 
    } 
    
   if (!bobmode)
        gamestate.weapon = gamestate.chosenweapon;    
    
   DrawWeapon ();
   DrawAmmo ();
}    

/*
=======================
=
= ControlMovement
=
= Takes controlx,controly, and buttonstate[bt_strafe]
=
= Changes the player's angle and position
=
= There is an angle hack because when going 70 fps, the roundoff becomes
= significant
=
=======================
*/

void ControlMovement (objtype *ob)
{
        long    oldx,oldy;
        int             angle;//,maxxmove;
        int             angleunits;

        thrustspeed = 0;

        oldx = player->x;
        oldy = player->y;

//
// side to side move
//
       if (!gamestate.car == true)     
       { 
        if (buttonstate[bt_strafe])
        {
        //
        // strafing
        //
        //
                if (controlx > 0)
                {
                        angle = ob->angle - ANGLES/4;
                        if (angle < 0)
                                angle += ANGLES;
                        Thrust (angle,controlx*MOVESCALE);      // move to left
                }
                else if (controlx < 0)
                {
                        angle = ob->angle + ANGLES/4;
                        if (angle >= ANGLES)
                                angle -= ANGLES;
                        Thrust (angle,-controlx*MOVESCALE);     // move to right
                }
        }
        else
        {
        //
        // not strafing
        //
                anglefrac += controlx;
                angleunits = anglefrac/ANGLESCALE;
                anglefrac -= angleunits*ANGLESCALE;
                ob->angle -= angleunits;

                if (ob->angle >= ANGLES)
                        ob->angle -= ANGLES;
                if (ob->angle < 0)
                        ob->angle += ANGLES;

        }

//
// forward/backwards move
//
        if (controly < 0)
        {
                Thrust (ob->angle,-controly*MOVESCALE); // move forwards
        }
        else if (controly > 0)
        {
                angle = ob->angle + ANGLES/2;
                if (angle >= ANGLES)
                        angle -= ANGLES;
                Thrust (angle,controly*BACKMOVESCALE);          // move backwards
        }
      } 
      else 
      { 
       if (controlx > 0)      
       { 
       gamestate.left = true; 
       gamestate.right = false; 
       } 
       else if (controlx < 0) 
       { 
       gamestate.left = false; 
       gamestate.right = true; 
       } 
       else 
       { 
       gamestate.left = false; 
       gamestate.right = false; 
       } 

       anglefrac += controlx*.5;     
       angleunits = anglefrac/ANGLESCALE; 
       anglefrac -= angleunits*ANGLESCALE; 
       ob->angle -= angleunits; 

       if (ob->angle >= ANGLES) 
          ob->angle -= ANGLES; 
       if (ob->angle < 0) 
          ob->angle += ANGLES; 

       if (controly > 0 )              
       { 
        if (gamestate.carspeed > controly) 
          gamestate.carspeed -= controly*2; 
        else if (gamestate.carspeed >= 0-controly) 
          gamestate.carspeed = 0; 
        else 
          gamestate.carspeed += controly*2;    
       } 
       else if (controly < 0)            // accelerate 
       { 
        if (gamestate.carspeed < 30000) 
          gamestate.carspeed -= controly*.5; 
        else 
          gamestate.carspeed = 30000; 
        SD_PlaySound (HITWALLSND); 
       } 
       else   
       { 
        if (gamestate.carspeed < 0) 
          gamestate.carspeed += 10; 
        else if (gamestate.carspeed > 0) 
          gamestate.carspeed -= 1; 
         } 
         Thrust (ob->angle,gamestate.carspeed); 
        } 
    
        if (gamestate.victoryflag)              // watching the BJ actor
                return;

//
// calculate total move
//
        playerxmove = player->x - oldx;
        playerymove = player->y - oldy;
}

/*
=============================================================================

                                        STATUS WINDOW STUFF

=============================================================================
*/


/*
==================
=
= StatusDrawPic
=
==================
*/

void StatusDrawPic (unsigned x, unsigned y, unsigned picnum)
{
        byte *temp;

        temp = vbuf;

        vbuf = (byte *)(0xa0000 + PAGE1START+(200-STATUSLINES)*SCREENWIDTH);
        LatchDrawPic (x,y,picnum);
        vbuf = (byte *)(0xa0000 + PAGE2START+(200-STATUSLINES)*SCREENWIDTH);
        LatchDrawPic (x,y,picnum);
        vbuf = (byte *)(0xa0000 + PAGE3START+(200-STATUSLINES)*SCREENWIDTH);
        LatchDrawPic (x,y,picnum);

        vbuf = temp;
}


/*
==================
=
= DrawFace
=
==================
*/

void DrawFace (void)
{
        if (gamestate.health)
        {
                //#ifdef SPEAR
                if (gamestate.godmode || gamestate.godcheat)
                        StatusDrawPic (17,4,GODMODEFACE1PIC+gamestate.faceframe);
                else
                //#endif
                if (gamestate.health > 100)
                StatusDrawPic (17,4,FACE1APIC+3*((100-100)/16)+gamestate.faceframe);
                else
                StatusDrawPic (17,4,FACE1APIC+3*((100-gamestate.health)/16)+gamestate.faceframe);
        }
        else
        {
//#ifndef SPEAR
//         if (LastAttacker->obclass == needleobj)
//           StatusDrawPic (17,4,MUTANTBJPIC);
//         else
//#endif
           StatusDrawPic (17,4,FACE8APIC);
        }
}

/*
===============
=
= UpdateFace
=
= Calls draw face if time to change
=
===============
*/

#define FACETICS        70

int     facecount=0;

void    UpdateFace (void)
{
        if (SD_SoundPlaying() == GETGATLINGSND)
          return;

        facecount += tics;
        if (facecount > US_RndT())
        {
                gamestate.faceframe = (US_RndT()>>6);
                if (gamestate.faceframe==3)
                        gamestate.faceframe = 1;

                facecount = 0;
                DrawFace ();
        }
}



/*
===============
=
= LatchNumber
=
= right justifies and pads with blanks
=
===============
*/

void    LatchNumber (int x, int y, int width, long number)
{
        unsigned length,c;
        char    str[20];

        ltoa (number,str,10);

        length = strlen (str);

        while (length<width)
        {
                StatusDrawPic (x,y,N_BLANKPIC);
                x++;
                width--;
        }

        c= length <= width ? 0 : length-width;

        while (c<length)
        {
                StatusDrawPic (x,y,str[c]-'0'+ N_0PIC);
                x++;
                c++;
        }
}


/*
===============
=
= DrawHealth
=
===============
*/

void    DrawHealth (void)
{
        LatchNumber (21,16,3,gamestate.health);
}


/*
===============
=
= TakeDamage
=
===============
*/

void	TakeDamage (int points,objtype *attacker)
{
	int damage;
	
	LastAttacker = attacker;

	if (gamestate.victoryflag || gamestate.godmode || gamestate.godcheat)
		return;
	if (gamestate.difficulty==gd_baby)
    {
        if (points != 0)
        {
            points = (points >= 4 ? points >> 2 : 1);
        }
    }

    if (!gamestate.godmode && !gamestate.godcheat)
    {
        if (gamestate.armor > 0)
        {
            const int ARMOR_PROTECTION = 65;
            if (points >= gamestate.armor)
            {
                gamestate.health -= (gamestate.armor * (100 - ARMOR_PROTECTION)) / 100;
                points -= gamestate.armor;
                gamestate.armor = 0;
            }
            else // points < gamestate.armor
            {
                damage = (points * (100 - ARMOR_PROTECTION)) / 100;
                damage = (damage > 0 ? damage : 1);
                gamestate.health -= damage;
                gamestate.armor -= points;
                points = 0;
            }
            DrawLives (); // draws armor

            gamestate.health -= points;
        }
        else
        {
            gamestate.health -= points;
        }
    }

    if (gamestate.health<=0)
    {
        gamestate.health = 0;
        playstate = ex_died;
        killerobj = attacker;
    }

    if (!gamestate.godmode && !gamestate.godcheat)
    {
        StartDamageFlash (points);
    }

	gotgatgun=0;

	DrawHealth ();
	DrawFace ();
	

        //
        // MAKE BJ'S EYES BUG IF MAJOR DAMAGE!
        //
        //#ifdef SPEAR
        if (points > 30 && gamestate.health!=0 && !gamestate.godmode && !gamestate.godcheat)
        {
                //StatusDrawPic (17,4,BJOUCHPIC);
                StatusDrawPic (17,4,MUTANTBJPIC);
                facecount = 0;
                SD_PlaySound (TAKEDAMAGESND);
                GetMessage("Do!");
        }
        //#endif
	
}

/*
===============
=
= HealSelf
=
===============
*/

void    HealSelf (int points)
{
        if (gamestate.health >= 200)
            return;
                
        gamestate.health += points;
        if (gamestate.health>100)
                gamestate.health = 100;

        DrawHealth ();
        gotgatgun = 0;  // JR
        DrawFace ();
}


/*
===============
=
= HealSelf2
=
===============
*/

void    HealSelf2 (int points)
{                
        gamestate.health += points;
        if (gamestate.health>200)
                gamestate.health = 200;

        DrawHealth ();
        gotgatgun = 0;  // JR
        DrawFace ();
}


//===========================================================================


/*
===============
=
= DrawLevel
=
===============
*/

void    DrawLevel (void)
{
#ifdef SPEAR
        if (gamestate.mapon == 20)
                LatchNumber (2,16,2,18);
        else
#endif
        LatchNumber (2,16,2,gamestate.mapon+1);
}

//===========================================================================


/*
===============
=
= DrawLives (DrawArmor)
=
===============
*/

void    DrawLives (void)
{
        //LatchNumber (14,16,1,gamestate.lives);
        LatchNumber (13,16,3,gamestate.armor);        
}


/*
===============
=
= GiveExtraMan (GiveArmor)
=
===============
*/

/*
void    GiveExtraMan (void)
{
        if (gamestate.lives<9)
                gamestate.lives++;
        DrawLives ();
        SD_PlaySound (BONUS1UPSND);
}
*/

void GiveExtraMan (int points)
{
    if (gamestate.armor >= 200)
        return;
        
    gamestate.armor += points;
    if (gamestate.armor>200)
        gamestate.armor = 200;

    DrawLives ();
}

//===========================================================================

/*
===============
=
= DrawScore
=
===============
*/

void    DrawScore (void)
{
    if (gamestate.score >= 1000000)
    {
        gamestate.score = -gamestate.score;     // temp marker
        LatchNumber (5,16,7,gamestate.score);
        gamestate.score = -gamestate.score;
    }
    else
    {
       LatchNumber (6,16,6,gamestate.score);
    } 
}

/*
===============
=
= GivePoints
=
===============
*/

void    GivePoints (long points)
{
        gamestate.score += points;
        //while (gamestate.score >= gamestate.nextextra)
        //{
        //        gamestate.nextextra += EXTRAPOINTS;
        //        GiveExtraMan ();
        //}
        DrawScore ();
}

//===========================================================================

/*
==================
=
= DrawWeapon
=
==================
*/

void DrawWeapon (void)
{ 
if (gamestate.chosenweapon == wp_shotgun) 
{
	StatusDrawPic (32,8,SHOTGUNPIC);
}
else if (gamestate.chosenweapon == wp_dshotgun) 
{
	StatusDrawPic (32,8,DSHOTGUNPIC);
}
else if (gamestate.chosenweapon == wp_assault) 
{
	StatusDrawPic (32,8,ASSAULTPIC);
}
else if (gamestate.chosenweapon == wp_dpistol) 
{ 
	StatusDrawPic (32,8,DGUNPIC); 
}
else if (gamestate.chosenweapon == wp_pistol) 
{
	StatusDrawPic (32,8,GUNPIC);
}
else if (gamestate.chosenweapon == wp_machinegun) 
{
	StatusDrawPic (32,8,MACHINEGUNPIC);
}
else if (gamestate.chosenweapon == wp_chaingun) 
{
	StatusDrawPic (32,8,GATLINGGUNPIC);
}
else if (gamestate.chosenweapon == wp_knife) 
{
	StatusDrawPic (32,8,KNIFEPIC);
}
}


/*
==================
=
= DrawKeys
=
==================
*/

void DrawKeys (void)
{
        if (gamestate.keys & 1)
                StatusDrawPic (30,4,GOLDKEYPIC);
        else
                StatusDrawPic (30,4,NOKEYPIC);

        if (gamestate.keys & 2)
                StatusDrawPic (30,20,SILVERKEYPIC);
        else
                StatusDrawPic (30,20,NOKEYPIC);
                
        if (gamestate.keys & 4)
                StatusDrawPic (38,4,REDKEYPIC);
        else
                StatusDrawPic (38,4,NOKEYPIC);

        if (gamestate.keys & 8)
                StatusDrawPic (38,20,GREENKEYPIC);
        else
                StatusDrawPic (38,20,NOKEYPIC);                
}

/*
==================
=
= GiveWeapon
=
==================
*/

void GiveWeapon (int weapon)
{
    if (weapon == wp_pistol) 
        GiveAmmo (6);    
    if (weapon == wp_machinegun) 
        GiveAmmo (10); 
    if (weapon == wp_chaingun) 
        GiveAmmo (26);      
    if (weapon == wp_shotgun) 
        GiveShells (8);  
    if (weapon == wp_dshotgun) 
        GiveShells (16);
    if (weapon == wp_assault) 
        GiveClip (15);  
    if (weapon == wp_dpistol) 
        GiveAmmo (12);             

    if (gamestate.weaponexists[weapon])
         return;

    gamestate.weaponexists[weapon] = true;

    gamestate.nextweapon =
    gamestate.weapon =
    gamestate.chosenweapon = (weapontype) weapon;

    if (bobmode)
    {     
       gamestate.changingweapon = true;
       gamestate.goingdown = true;	
    }   
     DrawWeapon ();    
} 

//===========================================================================

/*
===============
=
= DrawAmmo
=
===============
*/

void    DrawAmmo (void)
{
    switch (gamestate.chosenweapon)
    {
		case wp_shotgun:
		case wp_dshotgun:
           LatchNumber (26,16,3,gamestate.shells);
           break;
		case wp_assault:
           LatchNumber (26,16,3,gamestate.clip);
           break;                          
        case wp_knife: 
           LatchNumber (26,16,3,0);
           break;
        default:
           LatchNumber (26,16,3,gamestate.ammo); 
       }
}

/*
===============
=
= GiveAmmo
=
===============
*/

void    GiveAmmo (int ammo)
{
        if (!gamestate.ammo)                            // knife was out
        {
                if (!gamestate.attackframe)
                {
                        gamestate.weapon = gamestate.chosenweapon;
                        DrawWeapon ();
                }
        }
        gamestate.ammo += ammo;
        if (gamestate.ammo > gamestate.maxammo)
                gamestate.ammo = gamestate.maxammo;
        DrawAmmo ();
}

/*
===============
=
= GiveShells
=
===============
*/

void    GiveShells (int shells)
{
        if (!gamestate.shells)                            // knife was out
        {
                if (!gamestate.attackframe)
                {
                        gamestate.weapon = gamestate.chosenweapon;
                        DrawWeapon ();
                }
        }
        gamestate.shells += shells;
        if (gamestate.shells > gamestate.maxshells)
                gamestate.shells = gamestate.maxshells;
        DrawAmmo ();
}

/*
===============
=
= GiveClip
=
===============
*/

void    GiveClip (int clip)
{
        if (!gamestate.clip)                            // knife was out
        {
                if (!gamestate.attackframe)
                {
                        gamestate.weapon = gamestate.chosenweapon;
                        DrawWeapon ();
                }
        }
        gamestate.clip += clip;
        if (gamestate.clip > gamestate.maxclip)
                gamestate.clip = gamestate.maxclip;
        DrawAmmo ();
}

/*
==============================
=
= GiveBackpack 
=
==============================
*/

void GiveBackpack (void)
{
     gamestate.maxammo += 300;
     gamestate.maxshells += 200;
     gamestate.maxclip += 125;

     if (gamestate.maxammo > 600)
         gamestate.maxammo = 600;

     if (gamestate.maxshells > 400)
        gamestate.maxshells = 400;
        
     if (gamestate.maxclip > 250)
        gamestate.maxclip = 250;        	 
}

//===========================================================================

/*
==================
=
= GiveKey
=
==================
*/

void GiveKey (int key)
{
        gamestate.keys |= (1<<key);
        DrawKeys ();
}



/*
=============================================================================

                                                        MOVEMENT

=============================================================================
*/


/*
===================
=
= GetBonus
=
===================
*/
void GetBonus (statobj_t *check)
{
        switch (check->itemnumber)
        {
        case    bo_firstaid:
                if (gamestate.health >= 100)
                        return;

                SD_PlaySound (ROSESND);
                HealSelf (25);
                GetMessage("Sorike!");
                break;

        case    bo_key1:
        case    bo_key2:
        case    bo_key3:
        case    bo_key4:
                GiveKey (check->itemnumber - bo_key1);
                SD_PlaySound (GETKEYSND);
                               
                if (check->itemnumber == bo_key1) {GetMessage("Sarga kulcs"); }
                if (check->itemnumber == bo_key2) {GetMessage("Kek kulcs"); }
                if (check->itemnumber == bo_key3) {GetMessage("Piros kulcs"); }
                if (check->itemnumber == bo_key4) {GetMessage("Zold kulcs"); }
                break;

        case    bo_cross:
                SD_PlaySound (BONUS1SND);
                GivePoints (100);
                gamestate.treasurecount++;
                GetMessage("Kereszt");
                break;
        case    bo_chalice:
                SD_PlaySound (BONUS2SND);
                GivePoints (500);
                gamestate.treasurecount++;
                GetMessage("Serleg");
                break;
        case    bo_bible:
                SD_PlaySound (BONUS3SND);
                GivePoints (1000);
                gamestate.treasurecount++;
                GetMessage("Kincses lada");
                break;
        case    bo_crown:
                SD_PlaySound (BONUS4SND);
                GivePoints (5000);
                gamestate.treasurecount++;
                GetMessage("Gyemant");
                break;
        	case	bo_god:
                if (gamestate.godmodetics < 0)
                    gamestate.godmodetics = GODMODELENGTH * 70;
                else
                    gamestate.godmodetics += GODMODELENGTH * 70;           
                gamestate.godmode = true;
                GetTimer(GODMODELENGTH, T_GOD);
                SD_PlaySound(YEAHSND);
                GivePoints (10000); 
  		        gamestate.treasurecount++; 
  		        GetMessage("Juhu! Orokelet mod");
		        break;	                  

        case    bo_clip:
                if (gamestate.ammo == gamestate.maxammo)
                        return;

                SD_PlaySound (GETAMMOSND);
                GiveAmmo (8);
                GetMessage("Tolteny");
                break;
        case    bo_clip2:
                if (gamestate.ammo == gamestate.maxammo)
                        return;

                SD_PlaySound (GETAMMOSND);
                GiveAmmo (4);
                GetMessage("Tolteny");
                break;
        case    bo_clip3:
                if (gamestate.ammo == gamestate.maxammo)
                        return;

                SD_PlaySound (GETAMMOSND);
                GiveAmmo (16);
                GetMessage("Tolteny");
                break;       

//#ifdef SPEAR
        case    bo_25clip:
                if (gamestate.ammo == gamestate.maxammo)
                  return;

                SD_PlaySound (GETAMMOSND);
                GiveAmmo (25);
                GetMessage("Toltenyes doboz");
                break;
//#endif

        case    bo_shells:
                if (gamestate.shells == gamestate.maxshells) 
                  return; 

               SD_PlaySound (GETAMMOSND); 
               GiveShells (8); 
               GetMessage("Puska tolteny");
               break; 
               
        case    bo_shellsbox:
                if (gamestate.shells == gamestate.maxshells) 
                  return; 

               SD_PlaySound (GETAMMOSND); 
               GiveShells (16); 
               GetMessage("Puska toltenyes doboz");
               break;   
               
        case    bo_ammo:
                if (gamestate.clip == gamestate.maxclip)
                  return;

                SD_PlaySound (GETAMMOSND);
                GiveClip (8);
                GetMessage("Mesterlovesz tolteny");
                break;                   
               
        case    bo_ammobox:
                if (gamestate.clip == gamestate.maxclip)
                  return;

                SD_PlaySound (GETAMMOSND);
                GiveClip (25);
                GetMessage("Mesterlovesz toltenyes doboz");
                break;                            

        case    bo_pistol:
                SD_PlaySound (GETMACHINESND);
                GiveWeapon (wp_pistol); 
                GetMessage("Pisztoly");
                break;
        case    bo_machinegun:
                SD_PlaySound (GETGATLINGSND);
                GiveWeapon (wp_machinegun);
                GetMessage("Gepfegyver");
                break;
        case    bo_chaingun:
                SD_PlaySound (GETGATLINGSND);
                SD_PlaySound (YEAHSND);
                GiveWeapon (wp_chaingun);

                StatusDrawPic (17,4,GOTGATLINGPIC);
                facecount = 0;
                gotgatgun = 1;
                GetMessage("Eljen! Golyoszoro!");
                break;               
                
        case    bo_shotgun:
                SD_PlaySound (EVASND);
                GiveWeapon (wp_shotgun);
				GetMessage("Puska"); 
                break;
                
        case    bo_dshotgun:
                SD_PlaySound (EVASND);
                GiveWeapon (wp_dshotgun); 
                GetMessage("Duplacsovu puska");
                break;     
                
        case    bo_assault:
                SD_PlaySound (EVASND);
                GiveWeapon (wp_assault); 
                GetMessage("Mesterlovesz");
                break; 
                
        case    bo_dpistol:
                SD_PlaySound (GETMACHINESND);
                GiveWeapon (wp_dpistol); 
                GetMessage("Dupla pisztoly");
                break;                  
                
        case   bo_backpack: 
                SD_PlaySound (BONUS1UPSND);
	            GiveBackpack ();
                GiveAmmo (25);
                GiveShells (16);
                GiveClip (25);
                GiveExtraMan (35); 
                GetMessage("Hatizsak");
	            break;                                                                            

        case    bo_fullheal:
                SD_PlaySound (BONUS1UPSND);
                if (gamestate.health < 125) 
                   gamestate.health = 125;
                else if (gamestate.health < 150) 
                   gamestate.health = 150;
                else if (gamestate.health < 175) 
                   gamestate.health = 175;          
                else 
                   gamestate.health = 200;  
                   
                DrawHealth ();
                gamestate.treasurecount++;
                GetMessage("Extra elet");
                break;
                
        case    bo_armor:
                SD_PlaySound (GETMACHINESND);
                if (gamestate.armor < 125) 
                   gamestate.armor = 125;
                else if (gamestate.armor < 150) 
                   gamestate.armor = 150;
                else if (gamestate.armor < 175) 
                   gamestate.armor = 175;          
                else 
                   gamestate.armor = 200;  
                   
                DrawLives ();   
                GetMessage("Pajzs");
                break;              

        case    bo_food:
                if (gamestate.health >= 100)
                        return;

                SD_PlaySound (DONNERSND);
                HealSelf (10);
                GetMessage("Hm. Fank!");
                break;

        case    bo_alpo:
                if (gamestate.health >= 100)
                        return;

                SD_PlaySound (DONNERSND);
                HealSelf (4);
                GetMessage("Hm. Husika!");
                break;

        case    bo_gibs:
                if (gamestate.health >10)
                        return;

                SD_PlaySound (SLURPIESND);
                HealSelf (1);
                GetMessage("Fuj! Ver!");  
                break;      

#ifdef SPEAR
        case    bo_spear:
                spearflag = true;
                spearx = player->x;
                speary = player->y;
                spearangle = player->angle;
                playstate = ex_completed;
#endif
        }

        StartBonusFlash ();
        check->shapenum = -1;                   // remove from list
}

/*
===================
=
= TryMove
=
= returns true if move ok
= debug: use pointers to optimize
===================
*/

boolean TryMove (objtype *ob)
{
        int                     xl,yl,xh,yh,x,y;
        objtype         *check;
        long            deltax,deltay;

        xl = (ob->x-PLAYERSIZE) >>TILESHIFT;
        yl = (ob->y-PLAYERSIZE) >>TILESHIFT;

        xh = (ob->x+PLAYERSIZE) >>TILESHIFT;
        yh = (ob->y+PLAYERSIZE) >>TILESHIFT;

		  #define PUSHWALLMINDIST (0x9000)

//
// check for solid walls
//
        for (y=yl;y<=yh;y++)
				for (x=xl;x<=xh;x++)
				{
					check = actorat[x][y];
					if (check && check<objlist)
					{
						if(tilemap[x][y]==64 && x==pwallx && y==pwally)	// front of moving pushwall?
						{
							switch(pwalldir)
							{
								case di_north:
									if(ob->y-PUSHWALLMINDIST<=(pwally<<TILESHIFT)+((63-pwallpos)<<10))
										return false;
									break;
								case di_west:
									if(ob->x-PUSHWALLMINDIST<=(pwallx<<TILESHIFT)+((63-pwallpos)<<10))
										return false;
									break;
								case di_east:
									if(ob->x+PUSHWALLMINDIST>=(pwallx<<TILESHIFT)+(pwallpos<<10))
										return false;
									break;
								case di_south:
									if(ob->y+PUSHWALLMINDIST>=(pwally<<TILESHIFT)+(pwallpos<<10))
										return false;
									break;
							}
						}
						else return false;
					}
				}

//
// check for actors
//
        if (yl>0)
                yl--;
        if (yh<MAPSIZE-1)
                yh++;
        if (xl>0)
                xl--;
        if (xh<MAPSIZE-1)
                xh++;

        for (y=yl;y<=yh;y++)
                for (x=xl;x<=xh;x++)
                {
                        check = actorat[x][y];
                        if (check > objlist && (check->flags & FL_SHOOTABLE) )
                        {
                                deltax = ob->x - check->x;
                                if (deltax < -MINACTORDIST || deltax > MINACTORDIST)
                                        continue;
                                deltay = ob->y - check->y;
                                if (deltay < -MINACTORDIST || deltay > MINACTORDIST)
                                        continue;
                                        
                                if (gamestate.car == true && (gamestate.carspeed >= 2000 || gamestate.carspeed <= -2000))   
                                { 
                                 KillActor(actorat[x][y]); 
                                 return true; 
                                }             

                                return false;
                        }
                }

        return true;
}


/*
===================
=
= ClipMove
=
===================
*/

void ClipMove (objtype *ob, long xmove, long ymove)
{
        long    basex,basey;

        basex = ob->x;
        basey = ob->y;

        ob->x = basex+xmove;
        ob->y = basey+ymove;
        if (TryMove (ob))
                return;

#ifndef REMDEBUG
        if (noclip && ob->x > 2*TILEGLOBAL && ob->y > 2*TILEGLOBAL
                        && ob->x < (((long)(mapwidth-1))<<TILESHIFT)
                        && ob->y < (((long)(mapheight-1))<<TILESHIFT) )
                return;         // walk through walls
#endif

        if (gamestate.car == true) 
        { 
         if (gamestate.carspeed <= 200)     
           gamestate.carspeed = 0; 
         else 
         {   
          if (gamestate.carspeed >= 5000) 
          { 
           TakeDamage (gamestate.carspeed/500,NULL); 
          } 
          gamestate.carspeed =- 0.3*gamestate.carspeed;
         }      
         ob->x = basex; 
         ob->y = basey; 
         return; 
        } 

        if (!SD_SoundPlaying())
                SD_PlaySound (HITWALLSND);

        ob->x = basex+xmove;
        ob->y = basey;
        if (TryMove (ob))
                return;

        ob->x = basex;
        ob->y = basey+ymove;
        if (TryMove (ob))
                return;

        ob->x = basex;
        ob->y = basey;
}

//==========================================================================

/*
===================
=
= VictoryTile
=
===================
*/

void VictoryTile (void)
{
         ResetZoomEffects();     
     
if (mapon == 29)
{
//#ifndef SPEAR
        SpawnBJVictory ();
//#endif

        gamestate.victoryflag = true;
}
else if (gamestate.mapon == 0)
{
	playstate = ex_completed;	
}
else if (gamestate.mapon == 3)
{
	playstate = ex_completed;	
}
else if (gamestate.mapon == 4)
{
	playstate = ex_completed;	
}
else if (gamestate.mapon == 8)
{
	playstate = ex_completed;	
}
else if (gamestate.mapon == 9)
{
	playstate = ex_completed;	
}
else if (gamestate.mapon == 10)
{
	playstate = ex_completed;	
}
else if (gamestate.mapon == 14)
{
	playstate = ex_completed;	
}
else
{    
	playstate = ex_completed;
}
}

/*
===================
=
= Thrust
=
===================
*/

void Thrust (int angle, long speed)
{
        long xmove,ymove;
        unsigned offset;

        if (gamestate.bobdir != 1) gamestate.bobber++; else gamestate.bobber--; 
        if (gamestate.bobber == 15 || gamestate.bobber == 0) gamestate.bobdir ^= 1; 
        if (gamestate.bobdir2 != 1) gamestate.bobber2++; else gamestate.bobber2--; 
        if (gamestate.bobber2 == 30 || gamestate.bobber2 == 0) gamestate.bobdir2 ^= 1;   

        //
        // ZERO FUNNY COUNTER IF MOVED!
        //
        #ifdef SPEAR
        if (speed)
                funnyticount = 0;
        #endif

        thrustspeed += speed;
//
// moving bounds speed
//
        if (speed >= MINDIST*2)
                speed = MINDIST*2-1;

        xmove = FixedMul(speed,costable[angle]);
        ymove = -FixedMul(speed,sintable[angle]);

        ClipMove(player,xmove,ymove);

        player->tilex = (short)(player->x >> TILESHIFT);                // scale to tile values
        player->tiley = (short)(player->y >> TILESHIFT);

        offset = (player->tiley<<mapshift)+player->tilex;
        player->areanumber = *(mapsegs[0] + offset) -AREATILE;

        if (*(mapsegs[1] + offset) == EXITTILE)
                VictoryTile ();  
        if (*(mapsegs[1] + offset) == WARPTILE ||
            *(mapsegs[1] + offset) == WARPTILE2)
               WarpPlayer();   
}


/*
=============================================================================

                                                                ACTIONS

=============================================================================
*/


/*
===============
=
= Cmd_Fire
=
===============
*/

void Cmd_Fire (void)
{
        if (gamestate.changingweapon == true) 
             return;      
     
        buttonheld[bt_attack] = true;

        gamestate.weaponframe = 0;

        player->state = &s_attack;

        gamestate.attackframe = 0;
        //gamestate.attackcount =
        //        attackinfo[gamestate.weapon][gamestate.attackframe].tics;
        //gamestate.weaponframe =
        //        attackinfo[gamestate.weapon][gamestate.attackframe].frame;
    
      	if (gamestate.chosenweapon != wp_assault && gamestate.chosenweapon != wp_dpistol)
     	{
		    gamestate.attackcount =
		    	attackinfo[gamestate.weapon][gamestate.attackframe].tics;
    	}
    	else if (gamestate.chosenweapon == wp_assault)
     	{
	    	gamestate.attackcount =
	          	attackinfo[wp_pistol][gamestate.attackframe].tics;
    	}
	    else if (gamestate.chosenweapon == wp_dpistol)
     	{
	    	gamestate.attackcount =
	          	attackinfo[wp_machinegun][gamestate.attackframe].tics;
    	}
      	if (gamestate.chosenweapon != wp_assault && gamestate.chosenweapon != wp_dpistol)
     	{
	      	gamestate.weaponframe =
	    		attackinfo[gamestate.weapon][gamestate.attackframe].frame;
     	}
    	else if (gamestate.chosenweapon == wp_assault)
	    {
	    	gamestate.weaponframe =
		    	attackinfo[wp_pistol][gamestate.attackframe].frame;
    	}         	
    	else if (gamestate.chosenweapon == wp_dpistol)
	    {
	    	gamestate.weaponframe =
		    	attackinfo[wp_machinegun][gamestate.attackframe].frame;
    	}                
}

//===========================================================================

/*
===============
=
= Cmd_Use
=
===============
*/

void Cmd_Use (void)
{
        int                     checkx,checky,doornum,dir,currentTile;
        boolean         elevatorok;

//
// find which cardinal direction the player is facing
//
        if (player->angle < ANGLES/8 || player->angle > 7*ANGLES/8)
        {
                checkx = player->tilex + 1;
                checky = player->tiley;
                dir = di_east;
                elevatorok = true;
        }
        else if (player->angle < 3*ANGLES/8)
        {
                checkx = player->tilex;
                checky = player->tiley-1;
                dir = di_north;
                elevatorok = true;
        }
        else if (player->angle < 5*ANGLES/8)
        {
                checkx = player->tilex - 1;
                checky = player->tiley;
                dir = di_west;
                elevatorok = true;
        }
        else
        {
                checkx = player->tilex;
                checky = player->tiley + 1;
                dir = di_south;
                elevatorok = true;
        }

        doornum = tilemap[checkx][checky];
        currentTile = MAPSPOT(checkx,checky,1);
        
        switch (currentTile) 
        {
         case KARAOKETILE:
             if (!buttonheld[bt_use])
             SD_PlaySound(NEINSOVASSND);
             GetLyrics("Ezt szeretem! Azert vagyok,hogy\nszeresselek bebi!" 
                        "Azert voltal nekem,hogy\nengem szeress!"); 
             break;    
        }            
        if (*(mapsegs[1]+(checky<<mapshift)+checkx) == PUSHABLETILE)
        {
        //
        // pushable wall
        //

                PushWall (checkx,checky,dir);
                return;
        }                                                  
        if (!buttonheld[bt_use] && doornum == ELEVATORTILE && elevatorok)
        {
        //
        // use elevator
        //
                buttonheld[bt_use] = true;

                tilemap[checkx][checky]++;              // flip switch
                if (*(mapsegs[0]+(player->tiley<<mapshift)+player->tilex) == ALTELEVATORTILE)
                        playstate = ex_secretlevel;
                else
                        playstate = ex_completed;
                SD_PlaySound (LEVELDONESND);
                SD_WaitSoundDone();
        }
        else if (!buttonheld[bt_use] && doornum & 0x80)
        {
                buttonheld[bt_use] = true;
                OperateDoor (doornum & ~0x80);
        }
        else
                SD_PlaySound (DONOTHINGSND);
}

/*
=============================================================================

                                                   PLAYER CONTROL

=============================================================================
*/



/*
===============
=
= SpawnPlayer
=
===============
*/

void SpawnPlayer (int tilex, int tiley, int dir)
{
        player->obclass = playerobj;
        player->active = ac_yes;
        player->tilex = tilex;
        player->tiley = tiley;
        player->areanumber = (byte) *(mapsegs[0]+(player->tiley<<mapshift)+player->tilex);
        player->x = ((long)tilex<<TILESHIFT)+TILEGLOBAL/2;
        player->y = ((long)tiley<<TILESHIFT)+TILEGLOBAL/2;
        player->state = &s_player;
        player->angle = (1-dir)*90;
        if (player->angle<0)
                player->angle += ANGLES;
        player->flags = FL_NEVERMARK;
        Thrust (0,0);                           // set some variables

        InitAreas ();
}


//===========================================================================

/*
===============
=
= T_KnifeAttack
=
= Update player hands, and try to do damage when the proper frame is reached
=
===============
*/

void    KnifeAttack (objtype *ob)
{
        objtype *check,*closest;
        long    dist;

        SD_PlaySound (ATKKNIFESND);
// actually fire
        dist = 0x7fffffff;
        closest = NULL;
        for (check=ob->next ; check ; check=check->next)
                if ( (check->flags & FL_SHOOTABLE)
                && (check->flags & FL_VISABLE)
                && abs(check->viewx-centerx) < shootdelta
                )
                {
                        if (check->transx < dist)
                        {
                                dist = check->transx;
                                closest = check;
                        }
                }

        if (!closest || dist> 0x18000l)
        {
        // missed

                return;
        }

// hit something
        DamageActor (closest,US_RndT() >> 4);
}



void    GunAttack (objtype *ob)
{
        objtype *check,*closest,*oldclosest;
        int             damage;
        int             dx,dy,dist;
        long    viewdist;

        switch (gamestate.weapon)
        {
        case wp_pistol:
                SD_PlaySound (ATKPISTOLSND);
                madenoise = true;
                break;
        case wp_machinegun:
                SD_PlaySound (ATKMACHINEGUNSND);
                madenoise = true;
                break;
        case wp_chaingun:
                SD_PlaySound (ATKGATLINGSND);
                madenoise = true;
                break;
        case wp_shotgun:
                SD_PlaySound (NAZIFIRESND);
                madenoise = true;
                break;
        case wp_dshotgun:
                SD_PlaySound (SSFIRESND);
                madenoise = true;
                break;
        case wp_assault:
                SD_PlaySound (DIESND);
                madenoise = true;
                break;  
        case wp_dpistol:
                SD_PlaySound (BOSSFIRESND);
                SD_PlaySound (BOSSFIRESND);
                madenoise = true;
                break;                                                 
        }

//        madenoise = true;

//
// find potential targets
//
        viewdist = 0x7fffffffl;
        closest = NULL;

        while (1)
        {
                oldclosest = closest;

                for (check=ob->next ; check ; check=check->next)
                        if ((check->flags & FL_SHOOTABLE) && (check->flags & FL_VISABLE)
                                        && abs(check->viewx-centerx) < shootdelta)
                        {
                                if (check->transx < viewdist)
                                {
                                        viewdist = check->transx;
                                        closest = check;
                                }
                        }

                if (closest == oldclosest)
                        return;                                         // no more targets, all missed

        //
        // trace a line from player to enemey
        //
                if (CheckLine(closest))
                        break;
        }

//
// hit something
//
        dx = ABS(closest->tilex - player->tilex);
        dy = ABS(closest->tiley - player->tiley);
        dist = dx>dy ? dx:dy;
        if (dist<2)
                damage = US_RndT() / 4;
        else if (dist<4)
                damage = US_RndT() / 6;
        else
        {
                if ( (US_RndT() / 12) < dist)           // missed
                        return;
                damage = US_RndT() / 6;
        }
        
        if (gamestate.zoom == 0)
        {
            if (gamestate.chosenweapon == wp_assault)
                damage = 60;
        }
        else if (dist<2)
        {
            if (gamestate.chosenweapon == wp_assault)
                damage = 20;
        else
                damage = US_RndT() / 4;
        }
        else if (dist<4)
        {
            if (gamestate.chosenweapon == wp_assault)
                damage = 40;
        else
                damage = US_RndT() / 6;
        }
        else if ( (US_RndT() / 12) < dist)           // missed
        {
               if (dist>16)
               {
                 if (gamestate.chosenweapon == wp_assault)
                 return;
               } 
               if (dist>8)
               {
                 if (gamestate.chosenweapon == wp_assault)
                     damage = 50;
                 else
                     return;
               }
        }
        else
        {
               if (dist>16)
               {
                 if (gamestate.chosenweapon == wp_assault)
                 return;
               } 
               if (dist>8)
               {
                 if (gamestate.chosenweapon == wp_assault)
                     damage = 50;
                 else
                     damage = US_RndT() / 6; 
            }
        }        
        switch (gamestate.weapon)
        {
        case wp_pistol:
                damage = (int)((double)damage * 0.9f); 
                break;
        case wp_machinegun:
                damage = (int)((double)damage * 1.15f); 
                break;
        case wp_chaingun:
                damage = (int)((double)damage * 2.30f); 
                break;
        case wp_shotgun:
				damage = (int)((double)damage * 1.30f); 
                break;
        case wp_dshotgun:
                damage = (int)((double)damage * 1.60f); 
                break;               
        case wp_assault:	
                damage = (int)((double)damage * 2.0f); 
                break; 	
        case wp_dpistol:
                damage = (int)((double)damage * 1.8f); 
                break;                			           
        }
        DamageActor (closest,damage);
}

//===========================================================================

/*
===============
=
= VictorySpin
=
===============
*/

void VictorySpin (void)
{
        long    desty;

        if (player->angle > 270)
        {
                player->angle -= (short)(tics * 3);
                if (player->angle < 270)
                        player->angle = 270;
        }
        else if (player->angle < 270)
        {
                player->angle += (short)(tics * 3);
                if (player->angle > 270)
                        player->angle = 270;
        }

        desty = (((long)player->tiley-5)<<TILESHIFT)-0x3000;

        if (player->y > desty)
        {
                player->y -= tics*4096;
                if (player->y < desty)
                        player->y = desty;
        }
}


//===========================================================================

/*
===============
=
= T_Attack
=
===============
*/

void    T_Attack (objtype *ob)
{
        struct  atkinf  *cur;

        UpdateFace ();

        if (gamestate.victoryflag)              // watching the BJ actor
        {
                VictorySpin ();
                return;
        }

        if ( buttonstate[bt_use] && !buttonheld[bt_use] )
                buttonstate[bt_use] = false;

        if ( buttonstate[bt_attack] && !buttonheld[bt_attack])
                buttonstate[bt_attack] = false;

        ControlMovement (ob);
        if (gamestate.victoryflag)              // watching the BJ actor
                return;

        plux = (word) (player->x >> UNSIGNEDSHIFT);                     // scale to fit in unsigned
        pluy = (word) (player->y >> UNSIGNEDSHIFT);
        player->tilex = (short)(player->x >> TILESHIFT);                // scale to tile values
        player->tiley = (short)(player->y >> TILESHIFT);

//
// change frame and fire
//
        gamestate.attackcount -= (short) tics;
        while (gamestate.attackcount <= 0)
        {
                //cur = &attackinfo[gamestate.weapon][gamestate.attackframe];
                if (gamestate.chosenweapon != wp_assault && gamestate.chosenweapon != wp_dpistol)
       	            cur = &attackinfo[gamestate.weapon][gamestate.attackframe];
       	        else if (gamestate.chosenweapon == wp_assault)
		        	cur = &attackinfo[wp_pistol][gamestate.attackframe];    
	        	else if (gamestate.chosenweapon == wp_dpistol)
		        	cur = &attackinfo[wp_machinegun][gamestate.attackframe];
                switch (cur->attack)
                {
                case -1:
                        ob->state = &s_player;
                        if (!gamestate.ammo && gamestate.weapon == wp_pistol ||
                            !gamestate.ammo && gamestate.weapon == wp_machinegun ||
                            !gamestate.ammo && gamestate.weapon == wp_chaingun ||
			        		!gamestate.shells && gamestate.weapon == wp_shotgun ||
				        	!gamestate.shells && gamestate.weapon == wp_dshotgun ||
                            !gamestate.clip && gamestate.weapon == wp_assault ||
                            !gamestate.ammo && gamestate.weapon == wp_dpistol)
                        {
                                gamestate.weapon = gamestate.chosenweapon = wp_knife;
                                DrawWeapon ();
                        }
                        else
                        {
//                                if (gamestate.weapon != gamestate.chosenweapon)
//                                {
                                        gamestate.weapon = gamestate.chosenweapon;
                                        DrawWeapon ();
//                                }
                        }
                        gamestate.attackframe = gamestate.weaponframe = 0;
                        return;

                case 4:
                case 6:
                        if (!gamestate.ammo && !gamestate.shells && !gamestate.clip)
                                break;
                        if (buttonstate[bt_attack])
                                gamestate.attackframe -= 2;
                case 1:
                case 5:
                        if (!gamestate.ammo && gamestate.weapon == wp_pistol ||
                            !gamestate.ammo && gamestate.weapon == wp_machinegun ||
                            !gamestate.ammo && gamestate.weapon == wp_chaingun ||
			        		!gamestate.shells && gamestate.weapon == wp_shotgun ||
				        	!gamestate.shells && gamestate.weapon == wp_dshotgun ||
                            !gamestate.clip && gamestate.weapon == wp_assault ||
                            !gamestate.ammo && gamestate.weapon == wp_dpistol)
                        {       // can only happen with chain gun
                                gamestate.attackframe++;
                                break;
                        }
           					        GunAttack (ob);
                        if (gamestate.chosenweapon == wp_pistol)
                        {
                            gamestate.ammo--;
                            DrawAmmo ();
                        }
                        if (gamestate.chosenweapon == wp_machinegun)
                        {
                            gamestate.ammo--;
                            DrawAmmo ();
                        }
                        if (gamestate.chosenweapon == wp_chaingun)
                        {
                            gamestate.ammo--;
                            DrawAmmo ();
                        }
                        if (gamestate.chosenweapon == wp_shotgun)
                        {
                            gamestate.shells--;
                            DrawAmmo ();
                        }
                        if (gamestate.chosenweapon == wp_dshotgun)
                        {
                            gamestate.shells-=2;
                            DrawAmmo ();
                        }
                        if (gamestate.chosenweapon == wp_assault)
                        {
                            gamestate.clip--;
                            DrawAmmo ();
                        }
                        if (gamestate.chosenweapon == wp_dpistol)
                        {
                            gamestate.ammo-=2;
                            DrawAmmo ();
                        }
                        break;

                case 2:
                        KnifeAttack (ob);
                        break;

                case 3:
                case 7:
                        if (ChkAtkAmmo(gamestate.weapon) && buttonstate[bt_attack])
                                gamestate.attackframe -= 2;
                        break;
                   
                case 8:        
                case 9:
                        buttonstate[bt_attack] = false;
                        break;                        
                        
                case 10:
                        SD_PlaySound (EVASND);
                        break;                        
                }

                gamestate.attackcount += cur->tics;
                gamestate.attackframe++;
                //gamestate.weaponframe =
                //        attackinfo[gamestate.weapon][gamestate.attackframe].frame;
	         	if (gamestate.chosenweapon != wp_assault && gamestate.chosenweapon != wp_dpistol)
	        	{
		           	gamestate.weaponframe =
			        	attackinfo[gamestate.weapon][gamestate.attackframe].frame;
	           	}
	           	else if (gamestate.chosenweapon == wp_assault)
	        	{
		         	gamestate.weaponframe =
			        	attackinfo[wp_pistol][gamestate.attackframe].frame;
	        	}         	           	
	           	else if (gamestate.chosenweapon == wp_dpistol)
	        	{
		         	gamestate.weaponframe =
			        	attackinfo[wp_machinegun][gamestate.attackframe].frame;
	        	}                
        }

}



//===========================================================================

/*
===============
=
= T_Player
=
===============
*/

void    T_Player (objtype *ob)
{
        if (gamestate.victoryflag)              // watching the BJ actor
        {
                VictorySpin ();
                return;
        }

        UpdateFace ();
        
        if (!gamestate.car == true) 
        { 
        CheckWeaponChange ();
        
        if ( buttonstate[bt_attack] && !buttonheld[bt_attack])
                Cmd_Fire ();
        }         

        if ( buttonstate[bt_use] )
                Cmd_Use ();

        ControlMovement (ob);
        if (gamestate.victoryflag)              // watching the BJ actor
                return;

        plux = (word) (player->x >> UNSIGNEDSHIFT);                     // scale to fit in unsigned
        pluy = (word) (player->y >> UNSIGNEDSHIFT);
        player->tilex = (short)(player->x >> TILESHIFT);                // scale to tile values
        player->tiley = (short)(player->y >> TILESHIFT);
}



/*
===============
=
= WarpPlayer
=
===============
*/

void WarpPlayer (void)
{
        objtype  *check;
        int warpx,warpy;

        warpx = player->tilex;
        warpy = player->tiley;

        if (tilemap[warpx-1][warpy])
        {
                warpx -= 3;
                SpawnPlayer(warpx,warpy,WEST);
        }
        else if (tilemap[warpx+1][warpy])
        {
                warpx += 3; 
                SpawnPlayer(warpx,warpy,EAST);
        }
        else if (tilemap[warpx][warpy-1])
        {
                warpy -= 3; 
                SpawnPlayer(warpx,warpy,NORTH);
        }
        else if (tilemap[warpx][warpy+1])
        {
                warpy += 3;
                SpawnPlayer(warpx,warpy,SOUTH);
        }

        check = actorat[warpx][warpy];
        if (check > objlist && (check->flags & FL_SHOOTABLE))
                KillActor(check);

        buttonstate[bt_attack] = false;
        buttonheld[bt_attack] = false;
        gamestate.attackframe = 0;
        gamestate.weaponframe = 0;
        ThreeDRefresh();
}
