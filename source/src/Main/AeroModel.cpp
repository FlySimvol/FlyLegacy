/*
 * AeroModel.cpp
 *
 * Part of Fly! Legacy project
 *
 * Copyright 2003-2005 Chris Wallace
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

/*! \file AeroModel.cpp
 *  \brief Implements AeroModel classes
 */


#include "../Include/Globals.h"
#include "../Include/FlyLegacy.h"
#include "../Include/AeroModel.h"
#include "../Include/Atmosphere.h"
#include "../Include/AeroModelDraw.h"

#include <string>
#include <map>

using namespace std;

///////// DEBUG STUFF TO REMOVE LATER ////////////////////////////////
//#ifdef  _DEBUG
  //#define _DEBUG_AERO           //print lc DDEBUG file ... remove later 
  #define _DEBUG_SCREEN_LINES   //print lc DDEBUG file ... remove later
//#endif
//////////////////////////////////////////////////////////////////////



CLogFile* CAerodynamicModel::log = NULL;
void CAerodynamicModel::LogVector(const SVector &v, const char* name) {
  if (log)	log->Write("  %s = <%f, %f, %f>", name, v.x, v.y, v.z);
}

void CAerodynamicModel::LogScalar(const double &d, const char* name) {
  if (log)	log->Write("  %s = %f", name, d);
}
//=========================================================================
//	Trace identity
//=========================================================================
char *aeroNAME[]	= {
	"",
	"Aero Force",
	"Aero Moment",
	"",
	};
//=========================================================================
// JSDEV* CAerodynamicModel
//	Make it an included subsystem
//=========================================================================
CAerodynamicModel::CAerodynamicModel ()
{ int opt = 0;
  GetIniVar ("Logs", "logAeroModel", &opt);
  if (opt) log = new CLogFile("logs/Aeromodel.txt", "w");
  if (log) {
    log->Write ("CAerodynamicModel data log\n");
  } else log = NULL;
	//--- Get Trace option --------------------------------
	if (0 == opt)	opt = HasIniKey("TRACE","AeroForce")? (1):(0);
	if (0 == opt) opt = HasIniKey("TRACE","AeroMoment")?(2):(0);
	T	= opt;
	if (T) AERO("TRACE %s format is (time, X, Y, Z - AGL)",aeroNAME[T]);
	*wnam = 0;
  //--- Initialize --------------------------------------
	rudder	= 0;
  dofa.x	= dofa.y = dofa.z = 0.0;
  laca		=  ADJ_AERO_CENTR; // 0.0f;
  GetIniFloat ("PHYSICS", "adjustAeroCenter", &laca);
  laca = FN_METRE_FROM_FEET (laca); // convert to meter
  cd   = ADJ_TOTL_DRAG; // 1.0f;
  GetIniFloat ("PHYSICS", "adjustTotalDrag", &cd);
  grnd =  false;
  geff =  0.0f;
  gAGL =  0.0f;
  debugOutput = false;
  force.Raz();													// JS was: x = force.y = force.z = 0.0;
  moment.Raz();												  // JS wasx = moment.y = moment.z = 0.0;
	Time	= 0;
}
//----------------------------------------------------------------------
// Init read parameters
//----------------------------------------------------------------------
void	CAerodynamicModel::Init(char* wngFilename)
{	
  //--- Read from stream file -----------
  SStream s(this,"WORLD",wngFilename);
}
//----------------------------------------------------------------------
//  Destroy it
//----------------------------------------------------------------------
CAerodynamicModel::~CAerodynamicModel (void)
{ TRACE("Destroy wng");
	// Delete all members of airfoil and wing maps
  std::map<string,CAeroAirfoil*>::iterator ra;
  for (ra=airfoilMap.begin(); ra!=airfoilMap.end(); ra++) delete ra->second;
	airfoilMap.clear();
	//---------------------------------------------------------------
  std::map<string,CAeroWingSection*>::iterator rw;
  for (rw=wingMap.begin(); rw!=wingMap.end(); rw++) delete rw->second;
	wingMap.clear();
	//---------------------------------------------------------------
  SAFE_DELETE (log);
}
//----------------------------------------------------------------------
//  Read all parameter
//----------------------------------------------------------------------
int CAerodynamicModel::Read (SStream *stream, Tag tag)
{
  switch (tag) {
  case 'dofa':
  case 'DofA':
    // Vector distance from empty CG to aerodynamic datum (ft)
    ReadVector (&dofa, stream);
 	  // Luc's comment : This statement is RH (for an LH version. remove the statement)
    // VectorDistanceLeftToRight  (dofa); // 
    VectorScale(dofa, METRES_PER_FOOT); // convert to meter
    return TAG_READ;
  case '+ac+':
    // longitudinal aerodynamic center adjust (ft)
    ReadFloat (&laca, stream);
    laca = FN_METRE_FROM_FEET(laca); // convert to meter
    return TAG_READ;
  case 'grnd':
    // Global ground effect enabled
    {
      int ge_ = GRND_EFFECT; // 1;
      GetIniVar ("PHYSICS", "groundEffect", &ge_);
      if (ge_) grnd = true;
      DEBUGLOG ("groundEffect = %d", ge_);
    }
    return TAG_READ;
  case 'geff':
    // Global ground effect factor
    { ReadFloat (&geff, stream);
      float geff_K = 1.0f;
      GetIniFloat ("PHYSICS", "groundEffectAdjust", &geff_K);
      geff *= geff_K;
      DEBUGLOG ("groundEffectAdjust = %f", geff_K);
    }
    return TAG_READ;
  case 'gAGL':
    // Global ground effect altitude
    ReadFloat (&gAGL, stream);
    gAGL = FN_METRE_FROM_FEET(gAGL); // convert to meter
    return TAG_READ;
  case '+cd+':
    // Global drag coefficient fudge factor
    ReadFloat (&cd, stream);
    return TAG_READ;
  case 'foil':
    // Airfoil
    { CAeroAirfoil *foil = new CAeroAirfoil(this);
      ReadFrom (foil, stream);
      string name = foil->GetAirfoilName ();
      airfoilMap[name] = foil;
    }
    return TAG_READ;
  case 'wing':
    // Wing section
    { char name[80];
      ReadString (name, 80, stream);
      CAeroWingSection *wing = new CAeroWingSection(mveh,name);
      ReadFrom (wing, stream);
      wingMap[name] = wing;
    }
    return TAG_READ;
  case 'outp':
    // Enable real-time debug output
    debugOutput = true;
    return TAG_READ;
  }

    // Tag was not processed by this object, it is unrecognized
  WARNINGLOG ("CAerodynamicModel::Read : Unrecognized tag <%s>", TagToString(tag));
  return TAG_IGNORED;
}
//----------------------------------------------------------------------
//  All parameters are read
//----------------------------------------------------------------------
void CAerodynamicModel::ReadFinished()
{ Setup();
  return;
}
//----------------------------------------------------------------------------
//  Propagate channel pointer to each wing object
//----------------------------------------------------------------------------
void CAerodynamicModel::SetWingChannel(CAeroControlChannel *aero)
{ // Apply to each wing section
  std::map<string,CAeroWingSection*>::iterator i;
  for (i=wingMap.begin(); i!=wingMap.end(); i++) {
    i->second->SetWingChannel (aero);
  }
  return;
}
//----------------------------------------------------------------------------
// Get a wingsection by name
//----------------------------------------------------------------------------
CAeroWingSection* CAerodynamicModel::GetWingSection(const std::string &name) {
  std::map<string,CAeroWingSection*>::iterator pos = wingMap.find(name);
  if (pos != wingMap.end()) return pos->second;
  else return 0;
}
//----------------------------------------------------------------------------
// Get an airfoil by name
//----------------------------------------------------------------------------
CAeroAirfoil* CAerodynamicModel::GetAirfoil(char *name) {
  std::map<string,CAeroAirfoil*>::iterator pos = airfoilMap.find(name);
  if (pos != airfoilMap.end()) return pos->second;
  else return 0;
}
//----------------------------------------------------------------------------
// Set up the aerodynamic model for calculations
//----------------------------------------------------------------------------
void CAerodynamicModel::Setup() {
  if (log)	log->Write("Start AerodynamicModel::Setup()");
  LogVector(dofa, "dofa");
	
  // Luc's comment : Adding parasite drag support
  // set up the Airfoils
  std::map<string,CAeroAirfoil*>::iterator iterFoil;
  for (iterFoil=airfoilMap.begin(); iterFoil!=airfoilMap.end(); iterFoil++) {
    ((CAeroAirfoil*)iterFoil->second)->Setup(this);
  }

  //--- Set up the WingSections
  std::map<string,CAeroWingSection*>::iterator iterWing;
  for (iterWing=wingMap.begin(); iterWing!=wingMap.end(); iterWing++) {
    CAeroWingSection* ws = iterWing->second;
		if (ws->IsRudder())		rudder = ws;
    if (grnd) {
      // global ground effect enabled, override wingsection settings
      ws->grnd = grnd;
      ws->geff = geff;
      ws->gAGL = gAGL;
    }
    ws->Setup(this);
  }
  if (log) {
    log->Write("End AerodynamicModel::Setup()");
    log->Write("----------------------------------------------------------------------------------\n");
  }
}


static void VectorPrint(FILE* f, SVector &v, char* label) {
  fprintf(f, "  %s = <%f,%f,%f>\n", label, v.x, v.y, v.z);
}
//---------------------------------------------------------------------------------
// Calculate aerodynamic force and moments
//---------------------------------------------------------------------------------
void CAerodynamicModel::Timeslice(float dT) {
  //---read control surfaces -------------------------------
	Time	+= dT;
  std::map<string,CAeroWingSection*>::iterator iter;
  for (iter=wingMap.begin(); iter!=wingMap.end(); iter++) {
    CAeroWingSection* ws = iter->second;
    ws->GetChannelValues();
  }
	//---------------------------------------------------------------------
  // Get input data : inertial frame
  const SVector *v = mveh->GetAirspeed();                  ///< m/s

  // note  : if v = 0 (no motion) we shouldn't go further

  // Get input data : inertial frame
  const SVector *omega = mveh->GetBodyAngularVelocityVector ();                ///< rad/s 

  // Get input data : local frame
  // get the necessary data. Convert to SI units as needed
	// Luc's comment : conversions from fps+slugs towards SI (mks) happen at multiple place in this module.
	//		Worth investigating about keeping one single unit system.
	//		For now, I work towards getting ComputeForces() independant of the unit system.
	//		It will compute forces as per the unit matching the unti of rho*relV*relV*area as made available.
	// Luc's comment : Modifying ComputeForces()
  double hAgl = FN_METRE_FROM_FEET (mveh->GetBodyAGL()); ///< meters;
  // value rho 1.16->1.34 at 1 atm
  double rho = globals->atm->GetDensityKgM3 ();             ///< GetDensitySlugsFt3() * 515.317882;
  // value soundspeed : In SI Units with dry air at 20 �C (68 �F), the speed of sound is 343 m/s.
  // This also equates to 1235 km/h, 767 mph, 1125 ft/s, 343.055 m/s
  double soundSpeed = globals->atm->GetSoundSpeed_ISU ();   ///< GetSoundSpeed() * FN_METRE_FROM_FEET;

  // Get input data : body frame
  SVector *cgPos = mveh->wgh.GetNewCG_ISU (); 
  SVector  cgPos_; cgPos_.x = -cgPos->x; cgPos_.y = cgPos->z; cgPos_.z = cgPos->y; // RH->LH
  SVector  cgOffset = VectorDifference(cgPos_, dofa);       // 
  cgOffset.z -= laca; ///< add <+ac+> value to adjust longitudinal aerod.center // 
	//--------------------------------------------------------------------------------------------------------
#ifdef _DEBUG
  if (log) {
  LogVector (*v                                                        , "  v                            ");
  LogVector (*omega                                                    , "  omega                        ");
  LogVector (cgPos_                                                    , "  cgPos                        ");
  LogVector (cgOffset                                                  , "  cgOffset                     ");
  LogScalar (hAgl                                                      , "  hAgl                         ");
  LogScalar (rho                                                       , "  rho                          ");
  LogScalar (soundSpeed                                                , "  soundSpeed                   ");
  //! other informations
  CVector tmp_ori = mveh->GetOrientation ();
  VectorDistanceLeftToRight  (tmp_ori);
  LogVector (tmp_ori                                                   , "  user global orientation LH   ");
  LogScalar (mveh->GetMassInKgs ()                                     , "  mass in Kg                   ");
  CVector tmp_mi = *(mveh->GetMomentOfInertia ());
  VectorOrientLeftToRight (tmp_mi);
  LogVector (tmp_mi                                                    , "  Moment of Inertia ISU LH     ");
  if (log) log->Write("    gravity vector - Sorry : no data for gravity vector from OPAL");
  LogVector (*(mveh->GetBodyAccelerationVector ())       , "  linear local acceleration LH ");
  LogVector (*(mveh->GetBodyAngularAccelerationVector ()), "  angular local acceleration LH");
  //
  }
#endif
  force.Raz();											// JS replace VectorScale(force, 0.0); // clear old force and moment
  moment.Raz();											// JS Replace VectorScale(moment, 0.0);
	//------------------------------------------------------------------------
  // compute, and add up forces and moments from the WingSections
	//	NOTE JS:  Curiously, the rudder is included in this section.  It seems that the
	//						rudder action is from parasite drag?
	//						 
	//-------------------------------------------------------------------------
  for (iter=wingMap.begin(); iter!=wingMap.end(); iter++) {
    CAeroWingSection* ws = iter->second;
		if (mveh->UnderRegulation() && (ws == rudder))		continue;
    // bPos : body frame
    SVector bPos = VectorDifference(ws->bPos, cgOffset);                 ///< pos relative to actual cg meters
#ifdef _DEBUG
  if (log) {
  LogVector (ws->bPos, "  ws->bPos                     ");
  LogVector (bPos    , "  bPos                         ");
  //
  }
#endif 
	// inertial frame
    SVector relV;                                                 ///< relative air speed for ws m/s
    SVector omegaV;                                               ///< additional speed due to rotation m/s
    // Luc's comment : dF and dM are quite misleading names.
    //		They suggest the variables hold the force differential and the moment differential, which they are not.
    //		I suggest replacing with abbreviation for words "element" or "cell" or "partial". Here is an implementation using "elm"
    SVector elmF;                                                 ///< force produced by a WingSection element, ws Newtons
    SVector elmM;                                                 ///< moment produced by a WingSection element, Kg.m�

    // Luc's comment : Need to check the cross product operand order : (*omega x bPos) or (bPos x *omega)) ?
    VectorCrossProduct(omegaV, *omega, bPos);					            ///< Luc's comment : VectorCrossProduct() is independant of LH/RH orientation
    relV = VectorSum(*v, omegaV);                                 ///< m/s 
    // Luc's comment : Modifying ComputeForces()
    ws->ComputeForces(relV, rho, soundSpeed, hAgl);               /// inertial frame
		//TRACE("VELOCITY x=%.4lf y=%.4lf z=%.4lf",relV.x,relV.y,relV.z);

    elmF  = VectorSum(ws->GetLiftVector(), VectorMultiply (ws->GetDragVector(), cd));   ///< total force
    force = VectorSum(force, elmF);
    //
    #ifdef _DEBUG_AERO
    {	FILE *fp_debug;
	    if(!(fp_debug = fopen("__DDEBUG_AERO.txt", "a")) == NULL)
	    {
		    fprintf(fp_debug, "bPos(%f %f %f)\tL(%f %f %f)\td(%f %f %f)\n",
              bPos.x, bPos.y, bPos.z,
              ws->GetLiftVector().x, ws->GetLiftVector().y, ws->GetLiftVector().z,
              ws->GetDragVector().x, ws->GetDragVector().y, ws->GetDragVector().z
              );
		    fclose(fp_debug);
    }	}
    #endif

    // Luc's comment : There are 2 moments produced by a WingSection element
    //		- one is a moment caused by the relative wind producing a differential pressure 
    //      on various places of the WingSection surface.
    //		  This is an internal moment of the element and would cause it to turn if it were 
    //      not attached to the aeroplane.
    //		- one is a moment caused by the fact that the resulting aerodynamic force on the 
    //      WingSection element is not applied at the COG of the aeroplane.
    //		  This is a composite moment of the element and aeroplane structure. 
    //      It would not cause the element to turn if it were not attached to the aeroplane.
    // Luc's comment : Need to check the cross product operand order : (elmF x bPos) or (bPos x elmF)) ?
    VectorCrossProduct(elmM, bPos, elmF);							      ///< moment of element aerodynamic force
    elmM   = VectorSum(elmM, ws->GetMomentVector()); 					///< add "internal" moment 
    moment = VectorSum(moment, elmM);
#ifdef _DEBUG
    LogVector(relV,   "  relV  ");
    LogVector(bPos,   "  bPos  ");
    LogVector(force,  "  Force ");
    LogVector(moment, "  Moment");
		if (log) log->Write("-----Time slice end------------------------------------------------------\n");
#endif
  }
//---Trace if requested ---------------------------------------------------------------
if (1 == T) AERO(  "%.6f     %.4lf %.4lf %.4lf    %.2lf", Time,force.x, force.y,  force.z, hAgl); 
if (2 == T) AERO(  "%.6f     %.4lf %.4lf %.4lf    %.2lf", Time,moment.x,moment.y,moment.z, hAgl);
//-------------------------------------------------------------------------------------
}


SVector& CAerodynamicModel::GetForce() {
  return force;
}

SVector& CAerodynamicModel::GetMoment() {
  return moment;
}

void CAerodynamicModel::DrawAerodelData (const double &lenght) 
{
  // size of all the lines in this section
  glLineWidth (1.0f);
  //
#ifdef _DEBUG_SCREEN_LINES
  //DebugScreenAero (NULL/*sf*/, "test");
  CAeroControl *p = mveh->amp.etrm;
  if (p) {
    char buffer [128] = {0};
    float txt = p->Val ();
    if (txt < 1.0f) _snprintf (buffer,127, "%-.2f", txt * 10.0f);
    else            _snprintf (buffer,127, "%-.2f", txt);
    DebugScreenAeroTxt (buffer, (GLfloat) -20.0f, (GLfloat) 10.0f, (GLfloat) 128.0f);
  }
#endif

  DebugScreenAeroWFModel (lenght);
  
  // prevent any extra computation in slew mode
  if (globals->slw->IsEnabled()) return;
  
  DebugScreenAeroWFNewCG (lenght / 4.0);
  DebugScreenAeroWFEnginesForce ();
  //DebugScreenAeroWFWingsMoment (GetMoment());

  std::map<string,CAeroWingSection*>::iterator iter;
  for (iter=wingMap.begin(); iter!=wingMap.end(); iter++) {
    CAeroWingSection* ws = iter->second;

    DebugScreenAeroWSForce (ws->bPos, ws->GetLiftVector()); // 
    //DebugScreenAeroWSForce (ws->bPos, ws->GetMomentVector());
    DebugScreenAeroWSDrag  (ws->bPos, VectorMultiply (ws->GetDragVector(), cd));
  }
}

//================================================================================
// CAeroAirfoil
//================================================================================
CAeroAirfoil::CAeroAirfoil (CAerodynamicModel *a)
{	 aero	= a;
  // Luc's comment : Adding parasite drag support ---
  stallAlphaMin = stallAlphaMax = parasiteDrag = 0.0f;
  //---Init table lookup ----------------------------
  mlift       = 0;					// Assign null Map
  mdrag       = 0;
  mmoment     = 0;
  mliftMach   = 0;
  mdragMach   = 0;
  mmomentMach = 0;
}
//--------------------------------------------------------------------------------
//  Destroy all associated resources (table lookup, etc)
CAeroAirfoil::~CAeroAirfoil (void)
{ SAFE_DELETE (name);
	SAFE_DELETE (mlift);
  SAFE_DELETE (mdrag);
  SAFE_DELETE (mmoment);
  SAFE_DELETE (mliftMach);
  SAFE_DELETE (mdragMach);
  SAFE_DELETE (mmomentMach);
}

int CAeroAirfoil::Read (SStream *stream, Tag tag)
{
  switch (tag) {
  case 'name':
    { // Airfoil name, used to reference the data from Wing Sections
      char s[80];
      ReadString (s, 80, stream);
      name = DupplicateString(s,64);				//name = s;
			//--- Check for trace entry -----------------------
			char t[80];
			GetIniString("TRACE","Foil",t,80);
			T	= (strcmp(name,t)==0)?(1):(0);
    }
    return TAG_READ;
  case 'samn':
    // Minimum stall angle of attack (radians)
    ReadFloat (&stallAlphaMin, stream);
    return  TAG_READ;

  case 'samx':
    // Maximum stall angle of attack (radians)
    ReadFloat (&stallAlphaMax, stream);
    return TAG_READ;

  case 'lift':
    { // Lift vs AOA data map
      CDataSearch map(stream);
      mlift = map.GetTable();
      return TAG_READ;
    }
  case 'drag':
    { // Induced drag vs AOA data map
      CDataSearch map(stream);
      mdrag = map.GetTable();
      return TAG_READ;
    }
  case 'mome':
    { // Moment vs AOA data map
      CDataSearch map(stream);
      mmoment = map.GetTable();
      return TAG_READ;
    }
  case 'lMch':
    { // Lift Gain vs Mach number data map
      CDataSearch map(stream);
      mliftMach = map.GetTable();
      return TAG_READ;
    }
  case 'dMch':
    { // Drag Gain vs Mach number data map
      CDataSearch map(stream);
      mdragMach = map.GetTable();
      return TAG_READ;
    }
  case 'mMch':
    { // Moment Gain vs Mach number data map
      CDataSearch map(stream);
      mmomentMach = map.GetTable();
      return TAG_READ;
    }
  }

   // Tag was not processed by this object, it is unrecognized
  WARNINGLOG ("CAeroAirfoil::Read : Unrecognized tag <%s>", TagToString(tag));
  return TAG_IGNORED;
}

// Luc's comment : Adding parasite drag support
// Do as much of the calculations as possible before the simulation begins
void CAeroAirfoil::Setup(CAerodynamicModel *wng) {
  if (CAerodynamicModel::log)	CAerodynamicModel::log->Write("  Start Airfoil::Setup(%s)", name);

  // set the parasite drag coefficient
  // Parasite drag = drag for the aoa that produces 0 lift
  // until the ReverseLookup() function is available, approximate with aoa = 0.0f
  // parasiteDrag = drag->Lookup(lift->ReverseLookup(0.0f));
  if (mdrag) parasiteDrag = mdrag->Lookup(0.0f);
}

//--------------------------------------------------------------------------------------------
//	Return the lift coefficient
//--------------------------------------------------------------------------------------------
const double CAeroAirfoil::GetLiftCoefficient(double aoa, double mach)
{
  double cl;
  if (mlift)  cl = mlift->Lookup(float(aoa));
  // Luc's comment : Need to rewrite this formula. Correct formula is : cl = 2*PI*aoa * AR/(AR+2)
  else cl = 2*PI*aoa; // use theorical lift slope if no data
  if (mliftMach) cl *= mliftMach->Lookup(float(mach));
  return cl;
}
//--------------------------------------------------------------------------------------------
//	Return the Drag coefficient
//---------------------------------------------------------------------------------------------

// Luc's comment : I separated induced drag and parasite drag.
//	GetDragCoefficient() is replaced with GetInducedDragCoefficient() which just returns the induced drag.
//	A separate function GetParasiteDragCoefficient() returns the parasite drag.
const double CAeroAirfoil::GetInducedDragCoefficient(double aoa, double mach)
{
  double cdi;
  if (mdrag) cdi = mdrag->Lookup(float(aoa)) - parasiteDrag;

  else {
    double cl = GetLiftCoefficient(aoa, mach);
  // Luc's comment : Need to rewrite the below formula. Correct formula is : cd = cdp + cdi = cdp cl*cl/(AR*E*PI)
  // with cdp is the parasite drag, and AR is total wingspan divided by total wing projective area
  // cd = 0.005 + cl*cl*0.0067; // No data, guesstimate // Commented ; removing the parasite drag component
    cdi = cl*cl*0.0067; // No data, guesstimate;
  }
  // Luc's comment : Effective mach number only concerns induced drag
  if (mdragMach) cdi *= mdragMach->Lookup(float(mach)); 

  return cdi;
}
//---------------------------------------------------------------------------------------------
//	Return the moment coefficient
//---------------------------------------------------------------------------------------------

const double CAeroAirfoil::GetMomentCoefficient(double aoa, double mach)
{
  double cm = 0;
  if (mmoment) cm = mmoment->Lookup(float(aoa));
  if (mmomentMach) cm *= mmomentMach->Lookup(float(mach));
  return cm;
}

// Luc's comment : Adding parasite drag support
const double CAeroAirfoil::GetParasiteDragCoefficient (void)
{
  return parasiteDrag;
}

//===========================================================================================
//
// CAeroMovingPart
//  JS NOTE:  Each chanel is defined by a name in the WNG file. The ruuder appears in 2 chanels
//            Normal rudder and Front rudder
//===========================================================================================
CAeroMovingPart::CAeroMovingPart ( CVehicleObject *veh,CAeroWingSection *w)
{ mveh		= veh;
	wing    = w;
  chan    = 0;
  invert  = false;
  deflectRadians  = 0;
  adj.kd  = 1;
  adj.kf  = 1;
  adj.km  = 1;
  
  mlift   = 0;
  mdrag   = 0;
  mmoment = 0;
 

}
//---------------------------------------------------------------------------
//  Destroy resources
//---------------------------------------------------------------------------
CAeroMovingPart::~CAeroMovingPart()
{ 
  SAFE_DELETE(mlift);
  SAFE_DELETE(mdrag);
  SAFE_DELETE(mmoment);
  
}
//---------------------------------------------------------------------------
//  Read all parameters
//---------------------------------------------------------------------------
int CAeroMovingPart::Read (SStream *stream, Tag tag)
{ char s[80];

  switch (tag) {
  case 'chan':
    // Aero model channel name for this movable flap
    ReadString (s, 80, stream);
    channel = s;
    wing->PhyCoef(s,adj);
    return TAG_READ;
  case 'part':
    // External model part name linked to this aero model channel
    ReadString (s, 80, stream);
    if (strncmp(channel.c_str(),"Flap",4) == 0) {wing->AddFlap(s); }
    else parts.insert (s);
    return TAG_READ;

  case 'lift':
    { 
      CDataSearch map(stream);
      mlift = map.GetTable();
      return TAG_READ;
    }
  case 'drag':
    { // Drag vs. deflection
      CDataSearch map(stream);
      mdrag = map.GetTable();
      return TAG_READ;
    }
  case 'mome':
    { // Moment vs. deflection
      CDataSearch map(stream);
      mmoment = map.GetTable();
      return TAG_READ;
    }
  case 'nvrt':
    // Invert animation frames
    invert = true;
    return TAG_READ;
  }

    // Tag was not processed by this object, it is unrecognized
  WARNINGLOG ("CAeroMovingPart::Read : Unrecognized tag <%s>", TagToString(tag));

  return TAG_IGNORED;
}
//--------------------------------------------------------------------------
//  Return channel name
//--------------------------------------------------------------------------
const char* CAeroMovingPart::GetChannelName (void)
{
  return channel.c_str();
}

//--------------------------------------------------------------------------
//  Read Channel values
//--------------------------------------------------------------------------
void CAeroMovingPart::ReadChannel()
{ if (0 == chan)  return;
  float keyframe  = chan->GetKeyframe();
  deflectRadians  = chan->GetRadians();
  if (invert)     keyframe = float(1) - keyframe;
  //TRACE("--Channel %s: defl=%-.5f rad=%-.5f",(char*)channel.c_str(),deflectRadians);
  //----------------------------------------------------
  // Set keyframe of all associated animated parts
  set<string>::iterator i;
  for (i=parts.begin(); i!=parts.end(); i++) {
    mveh->SetPartKeyframe ((char*)i->c_str(), keyframe);
  }
  return;
}
//--------------------------------------------------------------------------
// Get the lift coefficient lookup
//
// @returns float Lift coefficient
//---------------------------------------------------------------------------
float CAeroMovingPart::GetLiftInc() {
  return (mlift)?(adj.kf * mlift->Lookup(deflectRadians)):(0);
}
//--------------------------------------------------------------------------
// Get the drag coefficient lookup
//
// @returns float Drag coefficient
//--------------------------------------------------------------------------
float CAeroMovingPart::GetDragInc() {
  return (mdrag)?(adj.kd * mdrag->Lookup(deflectRadians)):(0);
}
//--------------------------------------------------------------------------
// Get the moment coefficient lookup
//
// @returns float Moment coefficient
//--------------------------------------------------------------------------
float CAeroMovingPart::GetMomentInc() {
  return (mmoment)?(adj.km * mmoment->Lookup(deflectRadians)):(0);
}
//--------------------------------------------------------------------------

void CAeroMovingPart::Print3D () {
}

//=================================================================================
// CAeroWingSection
//=================================================================================
CAeroWingSection::CAeroWingSection (CVehicleObject *v,char* name)
{ mveh       = v;
  mflap      = new CAcmFlap(v);
  this->name = DupplicateString(name,64);
	foil			 = 0;
  area = span = 0.0f;
  bPos.x = bPos.y = bPos.z = 0.0f;
  bAng.x = bAng.y = bAng.z = 0.0f;
  effectiveLift = effectiveDrag = effectiveMoment = 1.0f;
  orie = 0;
  grnd = false;
  geff = gAGL = 0.0f;
  idcf = 0.0f;
  hinge = 0.0f;
  load = 0.0f;
  damage = NULL;
  aoa = 0.0;              ///< angle of attack
  cl = cdi = cm = 0.0;
  col_ = cod_ = com_ = 1.0f;
  mflpS = NULL;
  CPhysicModelAdj *phy = mveh->GetPHY();
  col_ = double(phy->Klft);
  cod_ = double(phy->Kdrg);
  com_ = double(phy->Kmmt);
 // DEBUGLOG ("CAeroWingSection PHY : %s\n\  col=%f cod=%f com=%f (%p)",
 //  name, col_, cod_, com_, phy);
  T = AERO_LNONE;
	//--- Check for trace level ---------------------------
	char l[32];
	GetIniString("TRACE","AeroLevel",l,32);
	if (strncmp(l,"WINGS",5)==0) T = AERO_LWING;
	if (strncmp(l,"FLAPS",5)==0) T = AERO_LFLAP;
	if (strncmp(l,"TRIMS",5)==0) T = AERO_LTRIM;
	if (strncmp(l,"SPOIL",5)==0) T = AERO_LSPLR;
	if (strncmp(l,"TOTAL",5)==0) T = AERO_LVALL;
	//-----------------------------------------------------
}
//------------------------------------------------------------------------
//  Destroy this object
//------------------------------------------------------------------------
CAeroWingSection::~CAeroWingSection (void)
{ SAFE_DELETE (mflap);
  std::map<string,CAeroMovingPart*>::iterator i;
  for (i=flapMap.begin(); i!=flapMap.end(); i++)	delete i->second;
	flapMap.clear();
	//----------------------------------------------------
  for (i=spoilerMap.begin(); i!=spoilerMap.end(); i++) delete i->second;
  spoilerMap.clear();
	//----------------------------------------------------
  for (i=trimMap.begin(); i!=trimMap.end(); i++)	delete i->second;
  trimMap.clear();
	//---------------------------------------------------
  SAFE_DELETE (damage);
  SAFE_DELETE (mflpS);
	SAFE_DELETE (foil);
	SAFE_DELETE (name);
}
//------------------------------------------------------------------------
//  Read parameters
//------------------------------------------------------------------------
int CAeroWingSection::Read (SStream *stream, Tag tag)
{
  int rc = TAG_IGNORED;

  switch (tag) {
  case 'foil':
    // Reference Airfoil name
    {
      char s[80];
      ReadString (s, 80, stream);
      foil = DupplicateString(s,80);
    }
    return TAG_READ;
  case 'span':
    // Wingspan (ft)
    ReadFloat (&span, stream);
    span = float(FN_METRE_FROM_FEET(span));		 // convert to meter
    return TAG_READ;
  case 'area':
    // Wing section area (sq. ft.)
    ReadFloat (&area, stream);
    area = (float)(area*SQMETRES_PER_SQFOOT); // convert to m^2
    return TAG_READ;
  case 'bPos':
    // Vector distance from aerodynamic centre to the location of the wing section
    ReadVector (&bPos, stream);
 	  // Luc's comment : This statement is RH (for an LH version. remove the statement)
    //VectorDistanceLeftToRight  (bPos); // 
    VectorScale(bPos, METRES_PER_FOOT); // convert to meter
    // aftermost structure position for the wind tail moment
    mveh->SetWindPos(bPos.z);
    return TAG_READ;
  case 'bAng':
    // Orientation vector of the wing section
    ReadVector (&bAng, stream);
 	  // Luc's comment : This statement is RH (for an LH version. remove the statement)
    //VectorOrientLeftToRight (bAng); // 
    return TAG_READ;
  case 'flap':
    { CAeroMovingPart* flap = new CAeroMovingPart(mveh,this);
      ReadFrom (flap, stream);
      char name[80];
      strncpy (name, flap->GetChannelName(),79);
			name[79] = 0;
      flapMap[name] = flap;
    }
    return TAG_READ;
  case 'splr':
    { CAeroMovingPart* splr = new CAeroMovingPart(mveh,this);
      ReadFrom (splr, stream);
      spoilerMap[splr->GetChannelName()] = splr;
    }
    return TAG_READ;
  case 'trim':
    { CAeroMovingPart* trim = new CAeroMovingPart(mveh,this);
      ReadFrom (trim, stream);
      trimMap[trim->GetChannelName()] = trim;
    }
    return TAG_READ;
  case 'orie':
  case 'orei':
    // Orientation, set to 1 if string is "VERTICAL"
    {
      char s[80];
      ReadString (s, 80, stream);
      if (stricmp (s, "VERTICAL") == 0 || stricmp (s, "1") == 0) {
        orie = true;
      }
    }
    return TAG_READ;
  case 'grnd':
    // Ground effect enabled
    {
      int ge_ = GRND_EFFECT; // 1;
      GetIniVar ("PHYSICS", "groundEffect", &ge_);
      if (ge_) grnd = true;
      DEBUGLOG ("groundEffect %s= %d", name, ge_);
    }
    return TAG_READ;
  case 'geff':
    // Ground effect factor
    {
      ReadFloat (&geff, stream);
      float geff_K = 1.0f;
      GetIniFloat ("PHYSICS", "groundEffectAdjust", &geff_K);
      geff *= geff_K;
      DEBUGLOG ("groundEffectAdjust %s= %f", name, geff_K);
    }
    return TAG_READ;
  case 'gAGL':
    // Ground effect altitude
    ReadFloat (&gAGL, stream);
    rc = TAG_READ;
    gAGL = float(FN_METRE_FROM_FEET(gAGL));			//(gAGL*METRES_PER_FOOT); // convert to meter
    return TAG_READ;
  case 'effL':
    // Effective Lift
    ReadFloat (&effectiveLift, stream);
    return TAG_READ;
  case 'effD':
    // Effective Drag
    ReadFloat (&effectiveDrag, stream);
    return TAG_READ;
  case 'effM':
    // Effective Moment
    ReadFloat (&effectiveMoment, stream);
    return TAG_READ;
  case 'flpL':
  case 'flpD':
  case 'flpM':
    // deprecated
    return TAG_READ;
  case 'hnge':
    // Flap deployment moment
    ReadFloat (&hinge, stream);
    return TAG_READ;
  case 'flpS':
    { // flap speed table
      CDataSearch map(stream);
      mflpS = map.GetTable();
      return TAG_READ;
    }
  case 'idcf':
    // Ice drag coefficient
    ReadFloat (&idcf, stream);
    return TAG_READ;
  case 'dmge':
    // Wing damage object
    damage = new CDamageModel;
    ReadFrom (damage, stream);
    return TAG_READ;
  case 'load':
    // Wing loading damage factor
    ReadFloat (&load, stream);
    return TAG_READ;
  }

  // Tag was not processed by this object, it is unrecognized
  WARNINGLOG ("CAeroWingSection::Read : Unrecognized tag <%s>", TagToString(tag));
  return TAG_IGNORED;
}
//---------------------------------------------------------------------
//	Return PHY coefficients
//---------------------------------------------------------------------
void CAeroWingSection::PhyCoef(char *name,AERO_ADJ &itm)
{	CPhysicModelAdj  *phy = mveh->GetPHY();
	if (phy)  phy->GetCoef(name,itm);
	return;
}
//---------------------------------------------------------------------------------
//  Store a channel pointer into each wing section that uses this channel
//---------------------------------------------------------------------------------
void CAeroWingSection::SetWingChannel(CAeroControlChannel *chn)
{ std::map<string,CAeroMovingPart*>::iterator i;
 //----Flap parts -----------------------------------------
  for (i=flapMap.begin(); i!=flapMap.end(); i++) 
  { CAeroMovingPart *flp = i->second;
    char *name = (char*)i->first.c_str();
    if (chn->SameName(name)) flp->Store(chn);
  }
 //--- Trim parts -----------------------------------------
  for (i=trimMap.begin(); i!=trimMap.end(); i++) 
  { CAeroMovingPart *flp = i->second;
    char *name = (char*)i->first.c_str();
    if (chn->SameName(name)) flp->Store(chn);
  }
  //--- Spoiler parts -------------------------------------
  for (i=spoilerMap.begin(); i!=spoilerMap.end(); i++) 
  { CAeroMovingPart *flp = i->second;
    char *name = (char*)i->first.c_str();
    if (chn->SameName(name)) flp->Store(chn);
  }
  return;
}
//---------------------------------------------------------------------------------
//  Read channel for all control surface
//---------------------------------------------------------------------------------
void CAeroWingSection::GetChannelValues()
{ std::map<string,CAeroMovingPart*>::iterator i;
  //----Flap parts -----------------------------------------
  for (i=flapMap.begin(); i!=flapMap.end(); i++) 
  { CAeroMovingPart *flp = i->second;
    flp->ReadChannel();
  }
  //--- Trim parts -----------------------------------------
  for (i=trimMap.begin(); i!=trimMap.end(); i++) 
  { CAeroMovingPart *flp = i->second;
    flp->ReadChannel();
  }
  //--- Spoiler parts -------------------------------------
  for (i=spoilerMap.begin(); i!=spoilerMap.end(); i++) 
  { CAeroMovingPart *flp = i->second;
    flp->ReadChannel();
  }
  return;
}
//---------------------------------------------------------------------------------
// Do as much of the calculations as possible before the simulation begins
//---------------------------------------------------------------------------------
void CAeroWingSection::Setup(CAerodynamicModel *wng) {
  aero	= wng;
  if (CAerodynamicModel::log)	CAerodynamicModel::log->Write("  Start WingSection::Setup(%s)", name);

  // global ground effect override
  // Luc's comment : This paragraph does NOTlook right. We are not in the scope of CAeroAirfoil class, therefore global ground effect boolean is not accessible.
  // Further, this attempts to duplicate the code in lines 219 - 224
  if (grnd) {
    this->grnd = true;
    this->geff = geff;
    this->gAGL = gAGL;
  }
  // Luc's comment : If "Orientation" parameter is set to vertical, force the dihedral angle to be -90�
  if (this->orie) {
    bAng.z = +90.0; // 
  }
  airfoil = wng->GetAirfoil(foil);
	if (0 == airfoil->GetTrace())	T= AERO_LNONE;
  chord = area / span;
  // Luc's comment : Need to verify the Euler angle decomposition. I think it is different in LH or RH.
  // Further, here the angle transform order MUST be dihedral, sweep, incidence
  // I shall probably modify CRotationMatrix for supporting several Euler angles definition
  SVector radAng;
  radAng.x = bAng.x * DEG2RAD;
  radAng.y = bAng.y * DEG2RAD;
  radAng.z = bAng.z * DEG2RAD;

  bAngMatrix_bhp.Setup(radAng /* bAng */);

#ifdef _DEBUG
  if (CAerodynamicModel::log) {
	CAerodynamicModel::LogScalar(area, "  area");
	CAerodynamicModel::LogScalar(span, "  span");
	CAerodynamicModel::LogVector(bPos, "  bPos");
	CAerodynamicModel::LogVector(bAng, "  bAng");
	// Logging the rotation matrix "bAngMatrix"
  SVector refVector;   // ref coordinate vector in child referential
  SVector checkVector; // transformed vector
	refVector.x = 1; refVector.y = 0; refVector.z = 0;
  bAngMatrix_bhp.ChildToParent(checkVector, refVector);
	CAerodynamicModel::LogVector(checkVector, "  bAngMatrix<m00,m01,m02>");
	refVector.x = 0; refVector.y = 1; refVector.z = 0;
  bAngMatrix_bhp.ChildToParent(checkVector, refVector);
	CAerodynamicModel::LogVector(checkVector, "  bAngMatrix<m10,m11,m12>");
	refVector.x = 0; refVector.y = 0; refVector.z = 1;
  bAngMatrix_bhp.ChildToParent(checkVector, refVector);
	CAerodynamicModel::LogVector(checkVector, "  bAngMatrix<m20,m21,m22>");
	CAerodynamicModel::log->Write("  End WingSection::Setup()");
  }
#endif
}
//----------------------------------------------------------------------------------------
// Luc's comment : Modifying ComputeForces()
// compute the forces on this WingSection as if it were an isolated flying wing
//----------------------------------------------------------------------------------------
void CAeroWingSection::ComputeForces(SVector &v_, double rho, double soundSpeed, double hAgl) {
	float time = aero->GetTime();
#ifdef _DEBUG
  if (CAerodynamicModel::log)	{
    CAerodynamicModel::log->Write("  Start CAeroWingSection::ComputeForces(%s)", name);
    CAerodynamicModel::LogVector(v_, "  speedVector");
  }
#endif
  //------------------------------------------------------------------------------------------------
  // get the necessary data.
  // Aerodynamic speed and dynamic pressure are to be computed from forward and
  // up components of local speed (in wing section coordinates).
  // Therefore, only y and z speed components play a role in lift and induced drag computation.
  // Sideward component of speed (direction x) only plays a role in computing parasite drag. 
  // Actually parasite drag depends on all components of local speed.
  // CVector speedVector; ///< v transformed to local coordinates ///< class member
  // Luc's comment : See comment above. Need to customize the rotation matrix setup via Euler angles.
	//	JS Notes: Position orientation is coming from FlyII and is
	//			x+ => Right side of aircraft
	//			y+ => Upper top of aircraft
	//			z+ => Forward (nose)
	//	Now force computation for OPAL are
	//			x+ => Left direction 
	//			y+ => Up direction 
	//			z+ => Forward direction
	//			
	//---------------------------------------------------------------------------------------------------
	//	JS:  I dont understand how the rudder works in computation as there is no distinctive code.
	//			 Seems to me thata rudder lift should generate sideway forces.  May be the direction of lift
	//			is encoded into the aoa computation?
  //---------------------------------------------------------------------------------------------------
  bAngMatrix_bhp.ParentToChild(speedVector, v_);																		// Local coordinate

  double ad_Speed2 = speedVector.y * speedVector.y + speedVector.z * speedVector.z;	///< This is the aerodynamic speed squared
  																					                                        ///< This formula is true for both RH and LH referentials
  																					                                        ///< since they both have forward and up being y and z or z and y
  double ad_speed		= sqrt (ad_Speed2);
  double speed2			= ad_Speed2 + speedVector.x * speedVector.x;///< This is the total local speed squared, used for computing parasite drag.
  double cf_speed		= sqrt (speed2);
  double mach				= ad_speed / soundSpeed;
  double q		= 0.5 * rho * ad_Speed2;
  double qS		= q * area;
  double qSc	= qS * chord;


  // get angle of attack, and "sideslip/sweep" angle
  // Luc's comment : This is LH code with positive aoa for relative wind from downside,
  //		speedVector being the speed of the plane and not the speed of the relative wind
  aoa = safeAtan2 (-speedVector.y, fabs (speedVector.z));  ///< class member 
  double col = col_;// 
  double cod = cod_;// 
  double com = com_;
  if (speedVector.z < 0.0) {   col = cod = com = -0.6; }
	//--- compute AOA components from Y-Z relative speed 
  double sa = (ad_speed != 0)?(-speedVector.y / ad_speed):(0);						//	 sin(aoa);
  double ca = (ad_speed != 0)?( speedVector.z / ad_speed):(1);						//	cos(aoa);
  // Luc's comment : Below is the RH version of the computation above
  //  double aoa = atan2(-speedVector.z, fabs (speedVector.y)); // 
  //  double co = (speedVector.y >= 0.0) ? 1.0 : -0.6;
  //  double sa = -speedVector.z / ad_speed; // sin(aoa);
  //  double ca = speedVector.y / ad_speed; //cos(aoa);

  // double aos = atan2(speedVector.x, cf_speed); // Not needed
  // Luc's comment : Not needed
  // Effective mach depends on sweep angle aos, as mach = ( cos(aos) * speedVector.Length() ) / soundSpeed;
  // However, mach = cf_speed / soundSpeed; already incorporates cos(aos) as cf_speed = cos(aos) * speedVector.Length()

  // Luc's comment : cl, cdi, cdp and cm are values expressed in a composite referential.
  //	This referential is based on the local WingSection referential, but with a rotation
  //	in the wing section chord plane such that coordinate z (z in LH, but y in RH) is parallel
  //	with the relative wind component for the chord plane.
  cl  = airfoil->GetLiftCoefficient(aoa, mach);
  cdi = airfoil->GetInducedDragCoefficient(aoa, mach); ///< class member
  // Luc's comment : Need to check the sign of cm. I understans that a positive cm means a pitch down cm, which is LH convention.
  cm = airfoil->GetMomentCoefficient(aoa, mach); ///< class member
  // Luc's comment : Adding parasite drag support
  double cdp = airfoil->GetParasiteDragCoefficient();

#ifdef _DEBUG
  if (CAerodynamicModel::log) {
    CAerodynamicModel::LogVector(speedVector, "  speedVector");
    CAerodynamicModel::log->Write("    rho = %f, cf_speed = %f, Mach = %f", rho, cf_speed, mach);
    CAerodynamicModel::log->Write("    aoa = %f d(%f) ori(%f)", aoa, RadToDeg (aoa), RadToDeg ((mveh->GetOrientation ()).y));
    CAerodynamicModel::log->Write("    cl = %f, cdi = %f, cdp = %f, cm = %f", cl, cdi, cdp, cm);
  }
#endif
	//--- Trace if requested ------------------------------------------------------------
	if (AERO_LWING == T)	WING("WINGS foils:%-32s %.6f %.4lf %.4lf %.4lf %.4lf",name,time,aoa,cl,cdi,cm);
  //--- control surface effects from flap surface s------------------------------------
  std::map<string,CAeroMovingPart*>::iterator iter;
  for (iter=flapMap.begin(); iter!=flapMap.end(); iter++) {
    CAeroMovingPart *flap = iter->second;
    cl  += flap->GetLiftInc();// lbs_to_newton = magic number to confirm or investigate
    cdi += flap->GetDragInc();
    /// \todo try to guess how <hnge> works ...
    cm  += flap->GetMomentInc() - (flap->GetLiftInc() * hinge);
#ifdef _DEBUG
	if (CAerodynamicModel::log)	
      CAerodynamicModel::log->Write("    flap cl = %.2f, flap cdi = %.2f, flap cm = %.2f hnge = %.2f",
        flap->GetLiftInc(), flap->GetDragInc(), flap->GetMomentInc(), hinge);

#endif
  }
	if (AERO_LFLAP == T)	WING("FLAPS foils:%-32s %.6f %.4lf %.4lf %.4lf %.4lf",name, time,aoa,cl,cdi,cm);
	//--- trim contribution ------------------------------------------------------------------
  for (iter=trimMap.begin(); iter!=trimMap.end(); iter++) {
    CAeroMovingPart *trim = iter->second;
    cl  -= trim->GetLiftInc();
    cdi += trim->GetDragInc();
    cm  += trim->GetMomentInc();
#ifdef _DEBUG
	if (CAerodynamicModel::log)	
      CAerodynamicModel::log->Write("    trim cl = %.2f, trim cdi = %.2f, trim cm = %.2f", trim->GetLiftInc(), trim->GetDragInc(), trim->GetMomentInc());

#endif
  }
	if (AERO_LTRIM == T)	WING("TRIMS %-32s %.6f %.4lf %.4lf %.4lf %.4lf",name, aero->GetTime(),aoa,cl,cdi,cm);
	//--- Spoiler contribution ---------------------------------------------------------------
  for (iter=spoilerMap.begin(); iter!=spoilerMap.end(); iter++) {
    CAeroMovingPart *spoiler = iter->second;
    cl  += spoiler->GetLiftInc();
    cdi += spoiler->GetDragInc();
    cm  += spoiler->GetMomentInc();
#ifdef _DEBUG
	if (CAerodynamicModel::log)	
      CAerodynamicModel::log->Write("    spoiler cl = %.2f, spoiler cdi = %.2f, spoiler cm = %.2f", spoiler->GetLiftInc(), spoiler->GetDragInc(), spoiler->GetMomentInc());

#endif
  }
	if (AERO_LSPLR == T)	WING("SPOIL %-32s %.6f %.4lf %.4lf %.4lf %.4lf",name, aero->GetTime(),aoa,cl,cdi,cm);
	//-----------------------------------------------------------------------------------------
  // ground effect
  if (grnd && (hAgl < gAGL)) {
  	// Luc's comment : This formula is WRONG!!!
  	//		It gives cl = 0 for hAgl = gAGL (or rather hAgl slightly lesser than gAGL).
  	//		Need to cross-check thoroughly, but I suspect that simply '*=' needds to be replaced with '+='
  	//		Making temporary replacement until verification complete.
//    cl *= (1.0 - hAgl/gAGL) * geff;
    cl += (1.0 - hAgl/double(gAGL)) * double(geff);
   	if (CAerodynamicModel::log)	{
      CAerodynamicModel::log->Write("    cl = %f, g = %f, geff = %f H = %f, Gh = %f",
        cl,
        (1.0 - hAgl/static_cast<double> (gAGL)) * static_cast<double> (geff),
        geff,
        hAgl, gAGL);
    }
  }

  //// adjust for effective lift, drag etc // 
  cl  *= col * effectiveLift;
  cdi *= cod * effectiveDrag;
  cdp *= effectiveDrag;
  cm  *= com * effectiveMoment;

#ifdef _DEBUG
  if (CAerodynamicModel::log)	CAerodynamicModel::log->Write("    Eff: cl = %f, cdi = %f, cdp = %f, cm = %f", cl, cdi, cdp, cm);
#endif

  // total effect
  double lift = qS * cl;
  double inducedDrag = qS * cdi;
  double parasiteDrag = 0.5 * rho * speed2 * area * cdp;
  double pitchMoment = qSc * cm;
	if (AERO_LVALL == T)	WING("TOTAL %.6f   Aoa=%.4lf L=%.4lf D=%.4lf M=%.4lf",aero->GetTime(),aoa,lift,inducedDrag,pitchMoment);
  // Force vectors in WingSection coordinates
  // Luc's comment : This paragraph is LH (an RH version is commented below)
  SVector relLiftVector = { 0.0, lift * ca, lift * sa };
  SVector relDragVector = { 0.0, inducedDrag * sa, -inducedDrag * ca }; ///< Luc's comment : This is just induced drag. Parasite drag is still to be added
	double  parDrag				= (cf_speed != 0)?(-parasiteDrag / cf_speed):(0);
	relDragVector = VectorSum (relDragVector, VectorMultiply (speedVector,parDrag));	// Luc's comment : Adding parasite drag
  // relDragVector = VectorSum (relDragVector, VectorMultiply (speedVector, -parasiteDrag / cf_speed));	// Luc's comment : Adding parasite drag
  // 
  // Luc's comment : Need to check the sign of cm. I understans that a positive cm means a pitch down cm, which is LH convention.
  SVector relMomentVector = { pitchMoment, 0.0, 0.0 };

  // Luc's comment : Below is RH version of the computation above
  //SVector relLiftVector = { 0, lift * sa, lift * ca };
  //SVector relDragVector = { 0, -inducedDrag * ca, inducedDrag * sa };	///< Luc's comment : This is just induced drag. Parasite drag is still to be added
  //relDragVector = VectorSum( relDragVector, VectorMultiply(speedVector, -parasiteDrag / cf_speed);// Luc's comment : Adding parasite drag
  //SVector relMomentVector = { -pitchMoment, 0, 0 };

  // Luc's comment : See comment above. Need to customize the rotation matrix setup via Euler angles.
  // Transform forces back to aircraft coordinates
  bAngMatrix_bhp.ChildToParent(liftVector, relLiftVector); // 
  bAngMatrix_bhp.ChildToParent(dragVector, relDragVector); // 
  bAngMatrix_bhp.ChildToParent(momentVector, relMomentVector); // 

#ifdef _DEBUG
  if (CAerodynamicModel::log) {
    CAerodynamicModel::LogVector  (relLiftVector,   "  relLift");
    CAerodynamicModel::LogVector  (relDragVector,   "  relDrag");
    CAerodynamicModel::LogVector  (relMomentVector, "  relMoment");
    CAerodynamicModel::LogVector  (liftVector,      "  lift");
    CAerodynamicModel::LogVector  (dragVector,      "  drag");
    CAerodynamicModel::LogVector  (momentVector,    "  moment");
    CAerodynamicModel::log->Write (                 "  End CAeroWingSection::ComputeForces()");
  }
#endif
}

const SVector& CAeroWingSection::GetLiftVector() const {
  return liftVector;
}

const SVector& CAeroWingSection::GetDragVector() const {
  return dragVector;
}

const SVector& CAeroWingSection::GetMomentVector() const {
  return momentVector;
}

///---------------------------------------------------------------
/// CPhysicModelAdj
///	
///---------------------------------------------------------------
CPhysicModelAdj::CPhysicModelAdj ()
{
#ifdef _DEBUG
  DEBUGLOG ("CPhysicModelAdj : constructor");
#endif
  // Initialize default
  Kdrg = ADJ_DRAG_COEFF; // 1.0f;   /// coeff of drag
  Klft = ADJ_LIFT_COEFF; // 1.1f;   /// coeff of lift
  Kmmt = ADJ_MMNT_COEFF; // 1.0f;   /// coeff of moment
  Kdeh = ADJ_DHDL_COEFF; // 500.0f; /// dihedral coeff
  Krud = 1.0f;           // 1.000f  /// acrd fudge factor
  Ktst = ADJ_ENGN_THRST; // 1.6f;   /// thrust coeff
  Kpth = ADJ_PTCH_COEFF; // 10.0f;  /// pitch coeff
  Pmin	= ADJ_PTCH_MINE;  // 1.0f;   /// pitch mine
  Rmin	= ADJ_ROLL_MINE;  // 2.0f;   /// roll mine
  Wlft	= 1.0f;                      /// unused
  Ymin	= ADJ_YAW_MINE;   // 2.50f;  /// yaw mine
  Kixx	= 1.0;             // 1.0f;  /// propeller inertia amplifier
  KgrR	= 1.0;             // 1.0f;  /// propeller gear ratio amplifier
  Kwnd	= ADJ_WIND_COEFF; // 10.0f   /// wind effect on aircraft
  mixC	= 0.0f          ; // 1.00f   /// used with engine with no MIXT
  KfcP	= 1.0f;           // 1.00f   /// P factor fudge factor
  KdrG	= ADJ_GEAR_DRAG;  // 0.975f  /// drag from gear
  Kegt	= 1.0f;           // 1.00f   /// EGT coeff
  Kpmn	= 100.0f;         // 100.0f  /// propeller magic number
  Ghgt	= 0.0f;           // 0.00f   /// gear adjust const
	return;
}
//----------------------------------------------------------------
//  Read the parameters
//----------------------------------------------------------------
void CPhysicModelAdj::Init(char *phyFilename)
{ //-- Read from stream file --------------------------------
  SStream s(this,"WORLD",phyFilename);
	//--- If no filr, use default -----------------------------
	return;
}
//----------------------------------------------------------------
//  Destructor
//----------------------------------------------------------------
CPhysicModelAdj::~CPhysicModelAdj (void)
{ std::map<std::string,AERO_ADJ*>::iterator it;
  for (it = aero.begin(); it != aero.end(); it++) delete it->second;
	aero.clear();
#ifdef _DEBUG
  DEBUGLOG ("CPhysicModelAdj destructor");
#endif
}
//----------------------------------------------------------------
//  Read  channel parameters:
//  <chan>          // Tag
//  LeftAileron     // Channel name    
//  1.2             // Force
//  1.5             // Drag
//----------------------------------------------------------------
void CPhysicModelAdj::ReadChannel(SStream *st)
{ float val;
  char  chn[64];
  ReadString(chn,64,st);
  //MEMORY_LEAK_MARKER ("aero_adj");
  AERO_ADJ *itm = new AERO_ADJ;
  //MEMORY_LEAK_MARKER ("aero_adj");
  itm->kf        = 0;
  itm->kd        = 0;
  itm->kf        = 0;
  ReadFloat(&val,st);
  itm->kf        = val;
  ReadFloat(&val,st);
  itm->kd        = val;
  ReadFloat(&val,st);
  itm->km        = val;
  aero[chn]      = itm;
  return;
}
//----------------------------------------------------------------
//  Read parameters
//----------------------------------------------------------------
int CPhysicModelAdj::Read (SStream *stream, Tag tag)
{
  int rc = TAG_IGNORED;

  switch (tag) {
  case 'Kdrg' :
    ReadFloat (&Kdrg, stream);
    rc = TAG_READ;
    break;

  case 'Klft' :
    ReadFloat (&Klft, stream);
    rc = TAG_READ;
    break;

  case 'Kmmt' :
    ReadFloat (&Kmmt, stream);
    rc = TAG_READ;
    break;

  case 'Kdeh' :
    ReadFloat (&Kdeh, stream);
    rc = TAG_READ;
    break;

  case 'Krud' :
    ReadFloat (&Krud, stream);
    rc = TAG_READ;
    break;

  case 'Ktst' :
    ReadFloat (&Ktst, stream);
    rc = TAG_READ;
    break;

  case 'Kpth' :
    ReadFloat (&Kpth, stream);
    rc = TAG_READ;
    break;

  case 'Pmin' :
    ReadFloat (&Pmin, stream);
    rc = TAG_READ;
    break;

  case 'Rmin' :
    ReadFloat (&Rmin, stream);
    rc = TAG_READ;
    break;

  case 'Ymin' :
    ReadFloat (&Ymin, stream);
    rc = TAG_READ;
    break;

  case 'Kixx' :
    ReadFloat (&Kixx, stream);
    rc = TAG_READ;
    break;

  case 'KgrR' :
    ReadFloat (&KgrR, stream);
    rc = TAG_READ;
    break;

  case 'Kwnd' :
    ReadFloat (&Kwnd, stream);
    rc = TAG_READ;
    break;

  case 'mixC' :
    ReadFloat (&mixC, stream);
    rc = TAG_READ;
    break;
  case 'KfcP' :
    ReadFloat (&KfcP, stream);
    rc = TAG_READ;
    break;

  case 'KdrG' :
    ReadFloat (&KdrG, stream);
    rc = TAG_READ;
    break;
    
  case 'Kegt' :
    ReadFloat (&Kegt, stream);
    rc = TAG_READ;
    break;

  case 'Ghgt' :
    ReadFloat (&Ghgt, stream);
    rc = TAG_READ;
    break;
  
  case 'Kpmn' :
    ReadFloat (&Kpmn, stream);
    rc = TAG_READ;
    break;

  case 'chan':
    ReadChannel(stream);
    return TAG_READ;
  }

  if (rc != TAG_READ) {
    // Tag was not processed by this object, it is unrecognized
    WARNINGLOG ("CPhysicModelAdj::Read : Unrecognized tag <%s>", TagToString(tag));
  }

  return rc;
}
//----------------------------------------------------------------
//  All parameters are read
//----------------------------------------------------------------
void CPhysicModelAdj::ReadFinished (void)
{
  DEBUGLOG ("CPhysicModelAdj::ReadFinished Kd%.4f Kl%.4f Km%.4f Kdh%.4f\n\
    Krud%.4f Th%.4f P%.4f Pm%.4f Rm%.4f Ym%.4f Wl%.4f\n\
    Kiix%.4f KgrR%.4f\n\
    Kwnd%.4f mixC%.4f KdrG%.4f KegtK%.4f Kfcp%.4f Kpmm%.4f Ghgt%.4f",
  Kdrg, /// coeff of drag
  Klft, /// coeff of lift
  Kmmt, /// coeff of moment
  Kdeh, /// dihedral coeff
  Krud, /// acrd fudge factor
  Ktst, /// thrust coeff
  Kpth, /// pitch coeff
  Pmin, /// pitch mine
  Rmin, /// roll mine
  Ymin, /// yaw mine
  Wlft, /// unused
  Kixx, /// propeller inertia
  KgrR, /// propeller gear ratio
  Kwnd, /// wind coeff
  mixC, /// mixture const
  KdrG, /// drag from gear
  Kegt, /// EGT coeff
  KfcP, /// P factor fudge factor
  Kpmn, /// propeller magic number
  Ghgt);/// gear adjust const

  CStreamObject::ReadFinished ();
}
//----------------------------------------------------------------
//  Get coefficients for the requested channel
//----------------------------------------------------------------
void CPhysicModelAdj::GetCoef(char *name,AERO_ADJ &itm)
{ itm.kf = 1;
  itm.kd = 1;
  itm.km = 1;
  std::map<std::string,AERO_ADJ*>::iterator it = aero.find(name);
  if (it == aero.end())   return;
  AERO_ADJ *va = (*it).second;
  itm = *va;
  return;
}
//====================END OF FILE ===========================================