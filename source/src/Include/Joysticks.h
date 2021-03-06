/*
 * Joysticks.h
 *
 * Part of Fly! Legacy project
 *
 * Copyright 2005 Chris Wallace
 *
 * Fly! Legacy is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 * Fly! Legacy is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 *   along with Fly! Legacy; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef JOYSTICKS_H
#define JOYSTICKS_H

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "../Include/FlyLegacy.h"
#include <list>
#include <bitset>
#include <mmsystem.h>
//=============================================================================
class CKeyDefinition;
class SJoyDEF;
class JoyDEV;
class CSimAxe;
//=====================================================================================
#define JOY_MAX_DEV		(16)
#define JOY_AXE_NBR   (6)
#define JOY_LOW_VAL   (float(-32767))
#define JOY_UPR_VAL   (float(+32767))
#define JOY_RNG_VAL   (JOY_UPR_VAL - JOY_LOW_VAL)
#define JOY_THRESH		(JOY_RNG_VAL / 256)
//=====================================================================================
#define JOY_AXE_X			(0)
#define JOY_AXE_Y			(1)
#define JOY_AXE_Z			(2)
#define JOY_AXE_R			(3)
#define JOY_AXE_U			(4)
#define JOY_AXE_V			(5)
//=====================================================================================
#define JOY_AXE_MOVE		(1)
#define JOY_HAT_MOVE    (2)
#define JOY_BUT_MOVE		(3)
//=====================================================================================
//  CONTROL CONNECTOR MASK
//	Used for locking associated axis when autopilot is ON
//=====================================================================================
enum JoyConnector {
		//--- Standard controls --------------------------------------
		JS_AILR_BIT	= 0x0001,		// Ailerons
		JS_ELVR_BIT = 0x0002,		// Elevator
		JS_RUDR_BIT = 0x0004,		// Rudder
		//------------------------------------------------------------
		JS_AILT_BIT = 0x0008,								// Aileron trim
		JS_ELVT_BIT	= 0x0010,								// Elevator trim	
		JS_RUDT_BIT = 0x0020,								// Rudder trim
		JS_THRO_BIT = 0x0040,								// THROTTLE lever
		JS_MIXT_BIT = 0x0080,								// MIXT lever
		JS_PROP_BIT = 0x0100,								// Propellor control
		JS_BRAK_BIT = 0x0200,								// brake axis
		JS_HELI_BIT = 0x1000,								// Helico bcontrol
		JS_VEHI_BIT	= 0x2000,								// Ground vehicle
		//--- Assignation -------------------------------------------
		JS_FIXE_BIT = 0x10000000,						// Cant change axe
		JS_STER_BIT = 0x4000 + JS_FIXE_BIT,	// Steering wheel
		//--- Identifies AERO_CONTROLS-------------------------------
		JS_AERO_BIT = (JS_AILR_BIT | JS_ELVR_BIT | JS_RUDR_BIT),
		//-----------------------------------------------------------
		JS_SURF_APL = (JS_AILR_BIT | JS_ELVR_BIT),
		JS_SURF_ALL = (JS_AILR_BIT | JS_ELVR_BIT | JS_RUDR_BIT),
		JS_TRIM_ALL = (JS_AILT_BIT | JS_ELVT_BIT | JS_RUDT_BIT),
		JS_GROUPBIT = (JS_THRO_BIT | JS_MIXT_BIT | JS_PROP_BIT),
		//-----------------------------------------------------------
		JS_AUTO_BIT = (JS_SURF_APL | JS_TRIM_ALL | JS_GROUPBIT | JS_BRAK_BIT),
};

//============================================================================
//  Class to describe a button
//============================================================================
class CSimButton: public CStreamObject
{
public:
  char					 devc[64];
  JoyDEV				*jdev;					// Joystick device		
  Tag            kset;          // Ket set
  Tag            cmde;          // Key command
  int            nBut;          // Button number
  U_INT          Stat;          // ON-OFF state
  CKeyDefinition *kdf;          // Associated key
  //----Public methods ---------------------------------
public:
  CSimButton();
 ~CSimButton();
  int   Read(SStream * stream, Tag tag);
  bool  Tr01(U_INT st);
	//----------------------------------------------------
  //---Inline ------------------------------------------
  inline void LinkTo(CKeyDefinition *d) {kdf = d;}
  inline CKeyDefinition *GetKey() {return kdf;}
	//----------------------------------------------------
	JoyDEV *GetDevice()			{return jdev; }
	int     GetNo()					{return nBut; }
};
//=============================================================================
//		Neutral area 
//=============================================================================
struct JOY_NEUTRAL {
		float		lo;
		float		hi;
		float   rf;
};
//=============================================================================
//  Structure to describe a Joystick
//=============================================================================
class JoyDEV
{ 
public:
  short           njs;				// Internal number
  short           nax;				// Number of axes
	//--------------------------------------------------------------
	char						nbt;				// Number of buttons
	char						nht;				// Number of hat
	char						uht;				// Use this hat
	char						use;				// Use this joystick
	//--------------------------------------------------------------
  float           axeData[JOY_AXE_NBR];		// array of float per axes
	float				    axePrev[JOY_AXE_NBR];		// previous value
	float           axeIncr[JOY_AXE_NBR];		// Axe increment
  CSimButton    * mBut[32];		// Array of pointer to button
	U_INT						msk;				// Mask for changing button
  U_INT			      but;				// One bit per button
	U_INT						hat;				// Hat position
	U_INT						phat;				// Previous hat value
	U_INT						hpos;				// Hat position
	char            dName[64];	// Device name 
	//--- Windows area ---------------------------------------------
	JOYCAPS         jCap;				// Capability
  //--------------------------------------------------------------
public:
  JoyDEV(int k);
 ~JoyDEV();
	U_INT HasMoved(CSimAxe *axe);
	void	GetJoystickName(int index, char *key, char *buf);
	void	Refresh();
	void	StoreAxe(char No,DWORD raw);
	void	HandleHat();
	//-------------------------------------------------------------------------
	bool	HatUnmoved();
	bool	NotNamed(char *n)   {return (strcmp(dName,n) != 0);}
	void	SaveVal();
	void	RemoveButton(CSimButton *btn);
	void  StoreButton(int n,CSimButton *b);	
	void	Reset();
  //-------------------------------------------------------------------------
	inline char   *getDevName()				  {	return dName;}
	inline void		 SwapMask()						{	msk = but;}
	inline U_INT   IsON(U_INT bit)		  {	return (but & msk & bit);}
	inline U_INT   Val (U_INT bit)			{	return (but & bit);}
  //-------------------------------------------------------------------------
	inline char 				jNumber()													{return njs;}
	inline char					hNumber()													{return nht;}
	inline char					HatUsed()													{return uht;}
  inline CSimButton  *GetButton(int n)                  {n &= 31; return mBut[n];}
  inline int          JoystickNo()                      {return njs;}
	inline void					SetHat(char n)										{uht = n;}
};
//===========================================================================
//  Class for Axe description
//===========================================================================
class CSimAxe: public CStreamObject
{
public:
	char				mem[4];		// Memory print
	//---------------------------------------------------------
  U_CHAR      No;       // Entry Index
  U_CHAR      type;     // 0 plane 1 heli
  U_CHAR      end;      // Last entry
  U_CHAR      group;    // Group name
	//---------------------------------------------------------
	U_INT	      msk;			// Connecting mask
	//---------------------------------------------------------
	char			  idn[8];		// Tag name for debug
	//---------------------------------------------------------
  char       *name;     // axe name to display in user window
	char        devc[64];	// Device controler
  Tag         gen;      // Generic name
  Tag         cmd;      // Datatag
	JoyDEV		 *jdev;			// Device pointer
	short       joyn;			// Joystick number
  short       iAxe;     // device related axe number
  float       inv;			// invert value sign
  bool        pos;      // Positive only
  U_CHAR      neutral;  // Neutral control type
  SMessage    msg;      // Send message control
  float       attn;     // Attenuation 
	//-----------------------------------------------------------
  static float aCOEF[];
  static float bCOEF[];
  //--------------------------------------------------------------
public:
  CSimAxe();
 ~CSimAxe();
  int   Read (SStream * st, Tag tag);
  void  Copy (CSimAxe *from);
  bool  NotAs(CSimAxe *axe);
	float	Value (JOY_NEUTRAL &nz);
	float RawVal(JOY_NEUTRAL &nz);
	void	Assign(CSimAxe *axn);
	void  Assignment(char *edt,int s);
	void	ChangeForce(float a);
	//--------------------------------------------------------------
	bool	NoDevice()		{return (*devc == 0);}
	bool  NulDev()			{return (strcmp(devc,"None") == 0);}
	bool	IsConnected(U_INT m)
	{	U_INT sel = m & msk;	return ((sel) && (jdev != 0));  }
	bool	IsAero()			{		return ((msk & JS_AERO_BIT)!= 0);}
	bool  IsFixe()			{		return ((msk & JS_FIXE_BIT)!= 0);}
	//--------------------------------------------------------------
	float	GetATTN()			{return attn;}
  //--------------------------------------------------------------
	void				Invert()				{inv = -inv;}
	char       *GetDevice()    {return (jdev)?(jdev->dName):("");}
	void				Clear()				{jdev = 0; inv =+1; *devc = 0;}
  char       *GetName()      {return name;}
  int         Positive()     {return pos;}
  bool        IsUnassigned() {return (0 == jdev);}
  int         AxeNo()        {return iAxe;}
  int         GetInvert()    {return (inv==1)?(0):(1);}
};

//=============================================================================
typedef enum
{ JS_AILERON   = 'ailr',
  JS_ELEVATOR  = 'elev',
  JS_RUDDER    = 'rudr',
  JS_TRIM      = 'trim',
  JS_RITE_TOE	 = 'rtoe',
  JS_LEFT_TOE  = 'ltoe',
  JS_THROTTLE_0= 'thr0',  // Not a real AXE
  JS_THROTTLE_1= 'thr1',
  JS_THROTTLE_2= 'thr2',
  JS_THROTTLE_3= 'thr3',
  JS_THROTTLE_4= 'thr4',
  JS_MIXTURE_0 = 'mix0',  // Not a real axe
  JS_MIXTURE_1 = 'mix1',
  JS_MIXTURE_2 = 'mix2',
  JS_MIXTURE_3 = 'mix3',
  JS_MIXTURE_4 = 'mix4',
  JS_PROP_0    = 'pro0',  // Not a real axe
  JS_PROP_1    = 'pro1',
  JS_PROP_2    = 'pro2',
  JS_PROP_3    = 'pro3',
  JS_PROP_4    = 'pro4',
  JS_ROLL_CYCLIC   = 'rCyc',
  JS_PITCH_CYCLIC  = 'pCyc',
  JS_TAIL_ROTOR    = 'tRot',
  JS_PITCHTRIM     = 'pTrm',
  JS_COLLECTIVE    = 'Coli',
  JS_THROTTLE      = 'Thro',
  JS_STEER         = 'ster',
  JS_GAS           = 'gas_'

} EAllAxes;
//=====================================================================================
enum AxeIndex {
	JS_AILR	= 0,													// 0 Ailerons
	JS_ELVR = 1,													// 1 Elevators
	JS_RUDR = 2,													// 2 Rudder
	JS_STER = 3,													// 3 Steering wheel
	JS_ETRM	= 4,													// 4 Elevator trim
	JS_RTOE	= 5,													// 5 Right toe
  JS_LTOE = 6,													// 6 Left toe
	//---------------------------------------------------
	JS_THR1 = 7,													// 7 Throttle 0
	JS_THR2 = 8,													// 8 Throttle 1
	JS_THR3	= 9,													// 9 Throttle 2
	JS_THR4 = 10,													//10 Throttle 3
	//---------------------------------------------------
	JS_MIX1 = 11,													//11 Mixture 1
	JS_MIX2 = 12,													//12 Mixture 2
	JS_MIX3 = 13,													//13 Mixture 3
	JS_MIX4 = 14,													//14 Mixture 4
	//---------------------------------------------------
	JS_PRP1 = 15,													//15 Prop 1
	JS_PRP2 = 16,													//16 Prop 2
	JS_PRP3 = 17,													//17 Prop 3
	JS_PRP4 = 18,													//18 Prop 4
	};

//=====================================================================================
typedef void(JoyDetectCB(char code,JoyDEV *J,CSimAxe *A,int B,Tag W));
//=====================================================================================
#define JOY_AXIS_NUMBER   32
#define JOY_TYPE_PLAN   0         // Plane axis
#define JOY_TYPE_HELI   1         // Heli axis
#define JOY_TYPE_GVEH   2         // Ground vehicle axis
#define JOY_TYPE_PMT    3         // Group Prop-Mix-Gas
//-------------------------------------------------------------
#define JOY_FIRST_PLAN   0        // entry for First plane axe in list
#define JOY_FIRST_HELI  18        // entry for First heli  axe in list
#define JOY_FIRST_GVEH  24        // entry for first vehicle axe in list
#define JOY_FIRST_PMT   JS_THR1   // Group control for planes firts entry
//----------------------------------------------------------------------
#define JOY_GROUP_TOES   JS_RTOE        // Index of first TOE axis 
#define JOY_GROUP_THRO   JS_THR1				// Index of first THRO axis
#define JOY_GROUP_MIXT   JS_MIX1        // Index of first mixture axis
#define JOY_GROUP_PROP   JS_PRP1        // Index of first Prop (blad) axis 
//----Group bits ------------------------------------------------------
#define JOY_GROUP_PMT   0x10
#define JOY_THROTTLE    0x01 + JOY_GROUP_PMT
#define JOY_MIXTURE     0x02 + JOY_GROUP_PMT
#define JOY_PROPEL      0x04 + JOY_GROUP_PMT
//----NEUTRAL CONTROL TYPE --------------------------------------------
#define JOY_NEUTRAL_STICK 1
//=====================================================================================
//  JOYSTICK MANAGER
//=====================================================================================
class CJoysticksManager : public CStreamObject
{	//---Initial vector --------------------------------------------------
	typedef void (CJoysticksManager::*VINIT)(SJoyDEF *jp);	// Rending vector
	//--------------------------------------------------------------------
public:
	CJoysticksManager();
 ~CJoysticksManager();
	void						PreInit();
  void            Init( );
	void						CreateDevList(char **men,U_INT n);
	void						CollectDevices();
	void						LimitButton(char *dvn,int lim);
	//--------------------------------------------------------------------
	bool						IsBusy();
	void						SetFree();
  //------------------------------------------------------------------------
  // CStreamObject methods
  int   Read (SStream *stream, Tag tag);
	//--------------------------------------------------------------------
  void            InitAxe (int nx,U_CHAR t,U_INT msk,char *nm,Tag c,int gp, bool p,float inv = +1);
  void            SetMessage(int m,Tag des,int unit,Tag cmd);
  void            EndMark(int nx);
  void            NeutralMark(int nx,U_CHAR type);
	//--------------------------------------------------------------------
	void						Register(CObject *v);
	void            Update();
	void						DetectMove(JoyDEV * p);
	void            CheckControl(Tag tag);
	//--------------------------------------------------------------------
	void						Reconnect(U_INT m);
	//--------------------------------------------------------------------
  float           AxeVal(CSimAxe *pa);
	float           RawVal(CSimAxe *pa);
	bool						StartDetection(JoyDetectCB *F, Tag W);
	//--- AIRCRAFT INTERFACE -----------------------------------------------
	void						SetNbEngines(CObject *obj,char n);
  void            MapTo(CObject *obj,Tag gen, Tag usr, CSubsystem *sys);
	void						JoyConnectAll(CObject *obj);
	void						JoyDisconnect(CObject *obj,U_INT m);
	void						Poll(CObject *obj,EAllAxes a,float &v);
  void            SendGroupPMT(CObject *obj,U_CHAR nbu);
  void            SendGroup(U_INT gr,Tag cmd);
  void            ClearGroupPMT(CObject *obj);
	//----------------------------------------------------------------------
	JoyDEV         *Find(char *name);
	JoyDEV         *GetJoystickNo(U_INT No);
  //------Remap a message to given tag -------------------------------------
  Tag             TagFrom(Tag gen);
  //------------------------------------------------------------------------
  CSimAxe *Axe(Tag tag)  {return GetAxe(tag);}
  CSimAxe *NextAxe(CSimAxe *from,int type);
  //--- READ CONFIG FILE ---------------------------------------------------
	void						ProcessHat(SStream *stream);
  bool            ProcessButton(CSimButton *sbt);
	void            RemoveButton(CSimButton *btn);
  CSimButton     *AddButton(JoyDEV *J,int nbt,CKeyDefinition *kdf);
  bool            AssignCallBack(JoyDEV * J,int k);
	//------------------------------------------------------------------------
  void            AssignAxe(CSimAxe *axe, CSimAxe *axn, U_CHAR all);
	void						Clear(CSimAxe *axn,U_CHAR all);
	void						UseHat(JoyDEV *jsp,char s);
  //------------------------------------------------------------------------
  void            ProcessAxe(CSimAxe *from);
  CSimAxe        *GetAxe(Tag tag);
  bool            HasAxe(EAllAxes axe);
  void            Invert(CSimAxe *axn,U_CHAR all);
  void            ReleaseAxe(CSimAxe *axn);
  void            SetInvert(EAllAxes tag,int p);
  int             GetInvert(EAllAxes tag);
	double					GetAxeAttenuation(U_INT axn);
	void						SetAxeAttenuation(U_INT axn,double c);
  //------------------------------------------------------------------------
	void						SaveOneButton   (CStreamFile &sf,CSimButton *sbt);
	void            SaveButtonConfig(CStreamFile &sf,JoyDEV *jsd);
  void            SaveAxisConfig(CStreamFile &sf);
  void            SaveConfiguration();
  void            SetNulleArea(float n); 
  //------------------------------------------------------------------------
  float			GetNulleArea()				{return nzon.hi;}
	void  		Modifier()						{modify = 1;}
	char			GasDisconnected()		  {return (axeCNX & JS_THRO_BIT)?(0):(1);}
  //------------------------------------------------------------------------
protected:
	void					EnumSDL();
	void          HandleButton(JoyDEV * jdv);
  //----ATTRIBUTES ----------------------------------------------------------
private:
	CObject *mveh;						// current aircraft
	//-------------------------------------------------------------------------
	U_INT		axeCNX;					// Connected axis
	//-------------------------------------------------------------------------
	char    modify;
  char		busy;		// Library used 0 = PU 1 = SDL
	char		nDev;		// Number of devices
	char    engNB;	// Number of engines
	//--- HAT control -------------------------------------
	char    uht;										// Hat used
	char		jsh;										// Hat joystick
	//--- list of components -------------------------------
	std::vector<JoyDEV *>     devQ;
  std::map<Tag,CSimAxe *>   mapAxe;
  CSimAxe AxesList[JOY_AXIS_NUMBER];
  CSimAxe AxeMoved;
	std::vector<CSimButton*> butQ;
	//------------------------------------------------------
  Tag     wmID;											// CFuiWindow ID for move callback
	JoyDetectCB		 *jFunCB;						// Detecte joystick moves
	//-----Control commande -------------------------------
	Tag			cmde;										// Current command
	//--- Neutral zone -------------------------------------
	JOY_NEUTRAL			nzon;						// Neutral area

};

#endif JOYSTICKS_H
//=============================END OF FILE ==========================================

