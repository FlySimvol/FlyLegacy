//=====================================================================================================
//	AIRPORT MANAGEMENT
//=====================================================================================================
/*
 * AIRPORT.cpp
 *
 * Part of Fly! Legacy project
 *
 * Copyright 2003-2005 Chris Wallace
 * Copyright 2007      Jean Sabatier
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
///----------------------------------------------------------------------------
//  NOTES ON COORDINATE SYSTEM
//  All coordinates (pavement and runways) are expressed with the following rules:
//  In the files, for any vertex V(x,y,z) the coordinates x,y and z are all in feet, 
//  relatives to the AIRPORT origin O(x,y,z)
//  1) X and Y are transformed in arcsec from Airport origin.
//    For any vertex V(x,y,z) the new coordinates X,Y and Z are
//    X = ARCSEC_PER_FEET(x * cp);
//    Y = ARCSEC_PER_FEET(y);
//    Z = z;
//  2) Distances along X are inflated with a compensation factor (cp) that depends on the 
//    latitude. This is because when going toward the pole, the tile Y dimension
//    (in arcsec) is shrinking.  At rendering time, X and Y  dimensions are translated
//    in feet by different scaling factor to make a square tile of side Y.  In this
//    process, the X dimension is reduced to the Y dimension.  The inflate factor
//    just ensures that distances are correctly set. As all coordinate are relative 
//    to the airport origin, the x value of a vertex is the vertex distance to
//    airport origin.
//  3) At rendering time, a translation from aircraft to object is set.  Then
//     object vertices are rendered.
//=========================================================================================
//=========================================================================================
#include "../Include/Airport.h"
#include "../Include/Taxiway.h"
#include "../Include/WinTaxiway.h"
#include "../Include/Terraintexture.h"
#include "../Include/LightSystem.h"
#include "../Include/FileParser.h"
#include "../Include/RunwayGenerator.h"
#include <math.h>
//=============================================================================
extern float *TexRES[];
//=============================================================================
//  Runway distance for PAPI
//=============================================================================
extern TC_COLOR ColorTAB[];		// In LightSystem.cpp
//=============================================================================
//  Runway altitude above threshold
//=============================================================================
double rwyATHR[] = {
  0,                  // 0 not existing
  15,                 // 1 runway < 800m
  22,                 // 2 runway < 1200m
  25,                 // 3 runway < 1800m
  30,                 // 4 runway > 1800m
};
//=============================================================================
//  STACK OF Ligth profiles.
//=============================================================================
RWY_EPF rwyRLP[32];
//=============================================================================
//  LEFT NUMBER POSITION for each side
//=============================================================================
SVector RwyNPOS[]= {
  {-0.50,+0.50},                    // Hi pos left
  {+0.15,-0.15},                    // Hi pos right
  {+0.50,-0.50},                    // Lo pos left
  {-0.15,+0.15},                    // Lo pos right
};
//=============================================================================
//  THRESHOLD BAND position
//=============================================================================
SVector RwyBPOS[] = {
  {-1.0,+1.0},                      // Hi pos left
  {+1.0,-1.0},                      // Hi pos right
  {+1.0,-1.0},                      // Lo pos left
  {-1.0,+1.0},                      // Lo pos right
};
//=============================================================================
// Mid point table for cuting runway into segments
//=============================================================================
extern TC_RMP_DEF RwyMID[];					// In CRunwayGenerator.cpp
//=============================================================================
//  Beacon color 1
//=============================================================================
U_CHAR  LiteBC1[] = {
  TC_WHI_LITE,                  // 0 Unknown=> Flash WHITE
  0,                            // 1 No beacon
  TC_GRN_LITE,                  // 2 Flash-GREEN
  TC_YEL_LITE,                  // 3 Flash-YELLOW
  TC_GRN_LITE,                  // 4 Flash-GREEN-YELLOW
  TC_GRN_LITE,                  // 5 SPLIT-FLASH-GREEN
  TC_YEL_LITE,                  // 6 YELLOW
  TC_GRN_LITE,                  // 7 GREEN
};
//=============================================================================
//  Beacon color 2
//=============================================================================
U_CHAR  LiteBC2[] = {
  0,                            // 0 Unknown=> Flash WHITE
  0,                            // 1 No beacon
  0,                            // 2 Flash-GREEN
  0,                            // 3 Flash-YELLOW
  TC_YEL_LITE,                  // 4 Flash-GREEN-YELLOW
  0,                            // 5 SPLIT-FLASH-GREEN
  TC_YEL_LITE,                  // 6 YELLOW
  TC_GRN_LITE,                  // 7 GREEN
};
//=============================================================================
//  THRESHOLD MARK POLYGONS
//  Painted bands are designed as polygons.
//  -Coordinates are in feet
//  -Origin for the Hi END is the southwest corner of the HTR segment
//	The definition is organized for TRIANGLE_STRIP
//=============================================================================
//=============================================================================
//  VBO for Bands and letters
//=============================================================================
F3_VERTEX vboBUF[] = {
	//--- White bands -(from left to right) ---------
	{10,20},{10,120},{ 16,20},{ 16,120},		// OFS 00
  {22,20},{22,120},{ 28,20},{ 28,120},		// OFS 04
  {34,20},{34,120},{ 40,20},{ 40,120},		// OFS 08
  {46,20},{46,120},{ 52,20},{ 52,120},		// OFS 12
  {58,20},{58,120},{ 64,20},{ 64,120},		// OFS 16
  {70,20},{70,120},{ 76,20},{ 76,120},		// OFS 20
  {82,20},{82,120},{ 88,20},{ 88,120},		// OFS 24
  {94,20},{94,120},{100,20},{100,120},		// OFS 28
	//--- Number 0 ----------------------------------
	{ 0,70},{10,80},{50,70},{40,80},				// OFS 32
  { 0,70},{10,70},{ 0,10},{10,10},				// OFS 36
  {40,70},{50,70},{40,10},{50,10},				// OFS 40
  { 0,10},{50,10},{10, 0},{40, 0},				// OFS 44
	//--- NUMBER 1 ----------------------------------
  {10,60},{20,80},{20,60},{30,80},				// OFS 48
  {20,80},{30,80},{20,10},{30,10},				// OFS 52
  {10,10},{40,10},{10, 0},{40, 0},				// OFS 56
	//--- NUMBER 2 ----------------------------------
  { 0,60},{ 0,70},{10,60},{10,70},				// OFS 60
  { 0,70},{10,80},{50,70},{40,80},				// OFS 64
  {40,70},{50,70},{40,50},{50,40},				// OFS 68
  {40,50},{50,40},{ 0,20},{10,10},				// OFS 72
  { 0,20},{10,10},{ 0, 0},{10, 0},				// OFS 76
  {10,10},{50,10},{10, 0},{50, 0},				// OFS 80
	//--- NUMBER 3 ----------------------------------
  { 0,60},{ 0,70},{10,60},{10,70},				// OFS 84
  { 0,70},{10,80},{50,70},{40,80},				// OFS 88
  {40,70},{50,70},{40,50},{50,50},				// OFS 92
  {40,50},{50,50},{30,40},{40,40},				// OFS 96
  {30,40},{40,40},{40,30},{50,30},				// OFS 100
  {40,30},{50,30},{40,10},{50,10},				// OFS 104
  { 0,10},{50,10},{10, 0},{40, 0},				// OFS 108
  { 0,20},{10,20},{ 0,10},{10,10},				// OFS 112
	//--- NUMBER 4 -----------------------------------
  { 0,30},{30,80},{ 0,20},{40,80},				// OFS 116
  { 0,30},{50,30},{ 0,20},{50,20},				// OFS 120
  {30,40},{40,40},{30, 0},{40, 0},				// OFS 124
	//--- NUMBER 5 -----------------------------------
  { 0,80},{40,80},{10,70},{40,70},				// OFS 128
  { 0,80},{10,70},{ 0,40},{10,50},				// OFS 132
  { 0,40},{10,50},{50,40},{40,50},				// OFS 136
  {40,40},{50,40},{40,10},{50,10},				// OFS 140
  { 0,10},{50,10},{10, 0},{40, 0},				// OFS 144
  { 0,20},{10,20},{ 0,10},{10,10},				// OFS 148
	//--- NUMBER 6 -----------------------------------
  {  0,70},{10,80},{50,70},{40,80},				// OFS 152
  {  0,70},{10,70},{ 0,10},{10,10},				// OFS 156
  { 40,70},{50,70},{40,60},{50,60},				// OFS 160
  {10,50},{40,50},{10,40},{50,40},				// OFS 164
  {40,40},{50,40},{40,10},{50,10},				// OFS 168
  { 0,10},{50,10},{10, 0},{40, 0},				// OFS 172
	//--- NUMBER 7 -----------------------------------
  { 0,70},{10,70},{ 0,60},{10,60},				// OFS 176
  { 0,70},{ 0,80},{40,70},{40,80},				// OFS 180
  {40,80},{50,80},{10, 0},{20, 0},				// OFS 184
  {10,40},{30,40},{10,20},{30,20},				// OFS 188
	//--- NUMBER 8 -----------------------------------
  { 0,70},{10,80},{50,70},{40,80},				// OFS 192
  { 0,70},{10,70},{ 0,60},{10,60},				// OFS 196
  {40,70},{50,70},{40,60},{50,60},				// OFS 200
  { 0,60},{10,60},{10,50},{20,50},				// OFS 204
  {40,60},{50,60},{30,50},{40,50},				// OFS 208
  {10,50},{40,50},{10,40},{40,40},				// OFS 212
  {10,40},{20,40},{ 0,30},{10,30},				// OFS 216
  {30,40},{40,40},{40,30},{50,30},				// OFS 220
  { 0,30},{10,30},{ 0,10},{10,10},				// OFS 224
  {40,30},{50,30},{40,10},{50,10},				// OFS 228
  { 0,10},{50,10},{10, 0},{40, 0},				// OFS 232
	//--- NUMBER 9 -----------------------------------
  { 0,70},{10,70},{ 0,50},{10,50},				// OFS 236
  { 0,70},{10,80},{50,70},{40,80},				// OFS 240
  {40,70},{50,70},{40,50},{50,50},				// OFS 244
  { 0,50},{50,50},{10,40},{40,40},				// OFS 248
  {40,40},{50,50},{40,10},{50,10},				// OFS 252
  { 0,10},{50,10},{10, 0},{40, 0},				// OFS 256
  { 0,20},{10,20},{ 0,10},{10,10},				// OFS 260
	//--- LETTER L (left)	----------------------------
  {-30,  0},{-20,  0},{-30,-80},{-20,-80},	// OFS 264
  {-20,-70},{ 20,-70},{-20,-80},{ 20,-80},	// OFS 268
	//--- LETTER C (center) ----------------------------
  {-20,  0},{ 10,  0},{-30,-10},{ 20,-10},	// OFS 272
  {-30,-10},{-20,-10},{-30,-70},{-20,-70},	// OFS 276
  {-30,-70},{ 20,-70},{-20,-80},{ 10,-80},	// OFS 280
  { 10,-10},{ 20,-10},{ 10,-20},{ 20,-20},	// OFS 284
  { 10,-60},{ 20,-60},{ 10,-70},{ 20,-70},	// OFS 288
	//--- LETTER R (right) -----------------------------
  {-30,  0},{ 10,  0},{-20,-10},{ 20,-10},	// OFS 292
  {-30,  0},{-20,-10},{-30,-80},{-20,-80},	// OFS 296
  {-20,-40},{ 10,-40},{-20,-50},{ 10,-50},	// OFS 300
  { 10,-10},{ 20,-10},{ 10,-30},{ 20,-30},	// OFS 304
  { 10,-30},{ 20,-30},{  0,-40},{ 10,-40},	// OFS 308
  {  0,-50},{ 10,-50},{ 10,-80},{ 20,-80},	// OFS 312
};
//=============================================================================
//  Offset for NUMBER 0 to 9 and letter L,C,R
//=============================================================================
GLint ofsBD[] = { 0, 4, 8,12,16,20,24,28};			// Up to 8 bands
GLint ofsN0[] = {32,36,40,44,};
GLint ofsN1[] = {48,52,56,};
GLint ofsN2[] = {60,64,68,72,76,80};
GLint ofsN3[] = {84,88,92,96,100,104,108,112};
GLint ofsN4[] = {116,120,124};
GLint ofsN5[] = {128,132,136,140,144,148};
GLint ofsN6[] = {152,156,160,164,168,172};
GLint ofsN7[] = {176,180,184,188};
GLint ofsN8[] = {192,196,200,204,208,212,216,220,224,228,232};
GLint ofsN9[] = {236,240,244,248,252,256,260};
GLint ofsLL[] = {264,268};
GLint ofsLC[] = {272,276,280,284,288};
GLint ofsLR[] = {292,296,300,304,308,312};
//----------------------------------------------------------------
GLint nbrPM[] = {  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4, 4, 4, 4};
//=============================================================================
//  Vector to offset list
//=============================================================================
GLint *ofsTAB[] = {
	ofsN0,ofsN1,ofsN2,ofsN3,ofsN4,ofsN5,ofsN6,ofsN7,ofsN8,ofsN9,
	ofsLL,ofsLC,ofsLR	};
//=============================================================================
//  Number of primitives for letters
//=============================================================================
GLint nbrPOL[]	= {
		sizeof(ofsN0) / sizeof(GLint),			// Number 0
		sizeof(ofsN1) / sizeof(GLint),			// Number 1
		sizeof(ofsN2) / sizeof(GLint),			// Number 2
		sizeof(ofsN3) / sizeof(GLint),			// Number 3
		sizeof(ofsN4) / sizeof(GLint),			// Number 4
		sizeof(ofsN5) / sizeof(GLint),			// Number 5
		sizeof(ofsN6) / sizeof(GLint),			// Number 6
		sizeof(ofsN7) / sizeof(GLint),			// NUmber 7
		sizeof(ofsN8) / sizeof(GLint),			// Number 8
		sizeof(ofsN9) / sizeof(GLint),			// Number 9
		sizeof(ofsLL) / sizeof(GLint),			// Letter L
		sizeof(ofsLC) / sizeof(GLint),			// Letter C
		sizeof(ofsLR) / sizeof(GLint),			// Letter R
};
//=========================================================================================
//  END OF TABLES
//=========================================================================================
//  Destroy Pavement Q
//=========================================================================================
CPaveQ::~CPaveQ()
{ CPaveRWY *pav = Pop();
  while (pav) { delete pav; pav = Pop();}
}
//============================================================================
//
//  Class CPaveRWY to store pavement polygons
//
//============================================================================
CPaveRWY::CPaveRWY(U_INT nb)
{ ptab.SetPolygons(nb);
  Type = PAVE_TYPE_PAVE;
  aUse = PAVE_USE_TAXI;
}
//----------------------------------------------------------------------------
//  Destructor
//----------------------------------------------------------------------------
CPaveRWY::~CPaveRWY()
{ }
//----------------------------------------------------------------------------
//  Set elevation for  vertex k
//----------------------------------------------------------------------------
void CPaveRWY::SetElevation(float e,int k)
{ TC_VTAB *tab = ptab.GetVTAB();
  tab[k].VT_Z = e;
  return;
}
//----------------------------------------------------------------------------
//  Copy the vertices to the destination table
//----------------------------------------------------------------------------
int CPaveRWY::CopyTo(TC_VTAB *buf)
{	int nbc = GetNBVT() * sizeof(TC_VTAB);
  memcpy(buf,GetVTAB(),nbc);
	return GetNBVT();
}
//----------------------------------------------------------------------------
//  Append the source pavement and cut polygon into triangle
//----------------------------------------------------------------------------
void CPaveRWY::AppendAsTriangle(CPaveRWY *pav)
{ TC_VTAB *src = pav->GetVTAB();
  TC_VTAB *dst = ptab.GetVTAB() + ptab.GetNBVT();
  TC_VTAB *org = src++;               // Origin of fan
	if (0 == pav->GetNBVT())		return;
  int      nbt = pav->GetNBVT() - 2;  // Number of triangles
  for (int k = 0; k != nbt; k++)
  { *dst++ = *org;                    // First vertex of triangle
    *dst++ = *src++;              
    *dst++ = *src;
		//======SET TRAP HERE =====================================
     ptab.AddCount(3);                // 3 vertices added
  }
  return;
}

//=========================================================================================
//	CTarmac is a descriptor for drawing one runway
//=========================================================================================
CTarmac::CTarmac(CAptObject *a,CRunway *r)
{	apo		= a;
	rwy		= r;							// Save runway
	xKey	= 0;							// Texture key
	xOBJ	= 0;							// Texture object
	oVBO	= 0;							// VBO object
	flag	= 0;							// Flag
	blnd	= 0;							// Blending
	xIND	= 0;							// Current index
	nVRT	= 0;							// Number of vertices
	nPRM	= 0;							// Number of primitives
	glGenBuffers(1,&oVBO);
}
//-----------------------------------------------------------------------------
//	Destructor
//-----------------------------------------------------------------------------
CTarmac::~CTarmac()
{	if (oVBO)	glDeleteBuffers(1,&oVBO);
  globals->txw->FreeSharedKey(xKey);
}
//-----------------------------------------------------------------------------
//	return vertex slot and reserve room for n
//-----------------------------------------------------------------------------
TC_VTAB *CTarmac::Reserve(int n)
{	if (nVRT > TARMAC_VRT)	gtfo("Tarmac no more Vertex room");
	nPRM++;
	ind[xIND++]		=	nVRT;									// Start index
	TC_VTAB *avt	= tab + nVRT;
	nVRT	+= n;
	return avt;
}
//-----------------------------------------------------------------------------
//	Load VBO
//-----------------------------------------------------------------------------
void CTarmac::LoadVBO()
{	int	dim = nVRT * sizeof(TC_VTAB);
	glBindBuffer(GL_ARRAY_BUFFER,oVBO);
	glBufferData(GL_ARRAY_BUFFER,dim,tab,GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER,0);
	return;
}
//-----------------------------------------------------------------------------
//	Draw tarmac
//-----------------------------------------------------------------------------
void CTarmac::Draw()
{	if (blnd)  glEnable(GL_BLEND);
	glBindTexture(GL_TEXTURE_2D, xOBJ);
	glBindBuffer(GL_ARRAY_BUFFER,oVBO);
	glVertexPointer  (3,GL_FLOAT,sizeof(TC_VTAB),OFFSET_VBO(2*sizeof(float)));
	glTexCoordPointer(2,GL_FLOAT,sizeof(TC_VTAB), 0);
	for (int k=0; k < nPRM; k++)	glDrawArrays(GL_TRIANGLE_STRIP,ind[k],4);
	//glBindBuffer(GL_ARRAY_BUFFER,0);
	rwy->DrawDesignators(apo);
	if (blnd)  glDisable(GL_BLEND);
	return;
}
//=========================================================================================
//	The following methods pertain to the CRunway Object
//  Draw Threshold bands for Hi and Lo ends
//=========================================================================================
void CRunway::DrawThreshold(SVector &sl,U_CHAR rs)
{ SVector ct = {0};							// Starting position
  //--Draw left part ------------------------------
  GetLTR(ct,rs);                 // Letf part
  glPushMatrix();
  glTranslatef(ct.x,ct.y,ct.z);				//ct.z);
  glScaled(sl.x,sl.y,1);
  glRotated(GetROT(rs),0,0,1);
  DrawBand(GetNBT(rs));
  glPopMatrix();
  //--Draw right part -----------------------------
  GetRTR(ct,rs);                 // right part
  glPushMatrix();
  glTranslatef(ct.x,ct.y,ct.z);			//ct.z);
  glScaled(sl.x,sl.y,1);
  glRotated(GetROT(rs),0,0,1);
  DrawBand(GetNBT(rs));
  glPopMatrix();
  return;
}
//-----------------------------------------------------------------------------------------
//  Draw the designator of the runway
//  Runway Designator is made of 2 number with one optional lettre (example 10L or 03C)
//		Letters are painted as polygons
//		The process is
//  -Translate to the letter position
//  -Scale to runway dimension
//  -Rotate the letter
//-----------------------------------------------------------------------------------------
void CRunway::DrawRID(CAptObject *apo,int rs,char *rid)
{ SVector ct = {0};
  SVector sl;
  SVector sb;
  //----Compute scale factor ----------------
  sl.x = GetXscale();
  sl.y = GetYscale();
  //-----Draw Left Number -------------------
  GetLPS(ct,rs);
  glPushMatrix();
  glTranslatef(ct.x,ct.y,ct.z);
  glScaled(sl.x,sl.y,1);
  glRotated(GetROT(rs),0,0,1);
  DrawNumber(rid[0]);
  glPopMatrix();
  //-----Draw right Number -------------------
  GetRPS(ct,rs);
  glPushMatrix();
  glTranslatef(ct.x,ct.y,ct.z);
  glScaled(sl.x,sl.y,1);
  glRotated(GetROT(rs),0,0,1);
  DrawNumber(rid[1]);
  glPopMatrix();
  //-----Draw id letter --------------------------
  char No = GetLET(rs);
  if (No)
    { GetCPS(ct,rs);
      glPushMatrix();
      glTranslatef(ct.x,ct.y,ct.z);
      glScaled(sl.x,sl.y,1);
      glRotated(GetROT(rs),0,0,1);
      DrawLetter(No);
      glPopMatrix();
  }
  //----Draw Threshold bands ---------------------
  apo->GetSTH(sb);
  int nbb = GetNBT(rs);
  if (nbb)  DrawThreshold(sb,rs);
  return;
}
//----------------------------------------------------------------------------
//  Draw the runway letter designator
//----------------------------------------------------------------------------
void CRunway::DrawLetter(char No)
{ if (No == 0)    return;
	int		nbp = nbrPOL[No];
	GLint *pl = ofsTAB[No];
	GLint *cn = nbrPM;
	glMultiDrawArrays(GL_TRIANGLE_STRIP,pl,cn,nbp);
	return;
}
//----------------------------------------------------------------------------
//  Draw the runway designator
//----------------------------------------------------------------------------
void CRunway::DrawNumber(char cr)
{ if ((cr < ('0')) || (cr > '9')) return;
  int		No  = cr - '0';
	int		nbp = nbrPOL[No];
	GLint *pl = ofsTAB[No];
	GLint *cn = nbrPM;
	glMultiDrawArrays(GL_TRIANGLE_STRIP,pl,cn,nbp);
  return;
}
//----------------------------------------------------------------------------
//  Get number of vertices need for the number
//	Each number is defined by a number of polygons given by table nbrPOL
//	Each polygon has 4 vertices
//----------------------------------------------------------------------------
U_CHAR CRunway::SetNumberNBV(char N)
{	if ((N < ('0')) || (N > '9')) return 0;
  int		No  = N - '0';
	//--- Compute number of polygons needed ---
	int		nbs = nbrPOL[No];			// Number of polygons
	nbvt += (nbs << 2);					// Multiplied by 4
	return U_CHAR(No);
}
//----------------------------------------------------------------------------
//  Get number of vertices need for the letter
//	Each letter is defined by a number of polygons given by table nbrPOL
//	Each polygon has 4 vertices
//----------------------------------------------------------------------------
U_CHAR CRunway::SetLetterNBV(char N)
{	if (0 == N) return 0;
	//--- Compute number of polygons needed ---
	int		nbs = nbrPOL[N];			// Number of polygons
	nbvt += (nbs << 2);					// Multiplied by 4
	return U_CHAR(N);
}

//----------------------------------------------------------------------------
//  Draw the runway THRESHOLD BAND
//----------------------------------------------------------------------------
void CRunway::DrawBand(int nbp)
{ if (nbp == 0)    return;
  GLint *cn = nbrPM;
  glMultiDrawArrays(GL_TRIANGLE_STRIP,ofsBD,cn,nbp);
  return;
}
//----------------------------------------------------------------------------
//  Draw both runway DESIGNATORS
//----------------------------------------------------------------------------
void CRunway::DrawDesignators(CAptObject *apo)
{ if (GetPaved() != TC_RWY_PAVED)  return;
  glDisable(GL_TEXTURE_2D);
	apo->apm->bindLETTERs();
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
  DrawRID(apo,TC_HI,GetHiEnd());
  DrawRID(apo,TC_LO,GetLoEnd());
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glBindBuffer(GL_ARRAY_BUFFER,0);
  glEnable(GL_TEXTURE_2D);
  return;
}
//---------------------------------------------------------------------------------
//  Set Letter position
//	Extract position from segment table
//---------------------------------------------------------------------------------
void CRunway::SetLETTER(int k,U_CHAR rs)
{ SVector pos;
  //---- Letter position --------------
  pos.x  = RwyMID[k].px; 
  pos.y  = RwyMID[k].py; 
  pos.z  = RwyMID[k].pz;
  SetCPS(pos,rs);
  return;
}
//---------------------------------------------------------------------------------
//  Set Number position
//	nmx and nmy are factor to compute the perpendicular position corresponding
//	to a middle point along the runway
//---------------------------------------------------------------------------------
void CRunway::SetNUMBER(int k,U_CHAR rs,double nmx,double nmy)
{ SVector pos;
  int   inx = (rs << 1);                  // Index value
  double x0 = RwyMID[k].px;
  double y0 = RwyMID[k].py;
  double z0 = RwyMID[k].pz;
  //--- Left position ----------------
  pos.x  = x0 + (nmx * RwyNPOS[inx].x);		//apo->GetXnormal(RwyNPOS[inx].x);
  pos.y  = y0 + (nmy * RwyNPOS[inx].y);		//apo->GetYnormal(RwyNPOS[inx].y);
  pos.z  = z0;
  SetLPS(pos,rs);
  inx++;
  //--- Right position ----------------
  pos.x  = x0 + (nmx * RwyNPOS[inx].x);		//apo->GetXnormal(RwyNPOS[inx].x);
  pos.y  = y0 + (nmy * RwyNPOS[inx].y);		//apo->GetYnormal(RwyNPOS[inx].y);
  pos.z  = z0;
  SetRPS(pos,rs);
  return;
}
//----------------------------------------------------------------------------
//  Collect designator vertices for one runway end (Hi or Lo)
//	S is the scaling factor for band coordinates
//----------------------------------------------------------------------------
F3_VERTEX *CRunway::CollectDesignator(char RE,SVector S,F3_VERTEX *buf)
{	HTransformer H;
	int tot = nbvt;								// Total for control
	if (0 == tot)				return buf;
	RwyID *rend = GetEndDEF(RE);
	//--- Collect band vertices -----------------------------
	U_INT			nbp = rend->nbTB;			// Number of polygons
	U_INT			ofs	= 0;							// Offset in polygon list
	int      *tab = 0;							// Depart in polygon
	U_CHAR    itm;									// Item (number / letter)
	F3_VERTEX *dst	= buf;
	//--- Init rotation for runway end ------------------
	H.SetROT(rend->aRot);
	H.Scale(S);
	//--- Collect left part -----------------------------
	H.Translation(rend->lTRH);			// Translate to Letf part
	for (U_INT k=0; k< nbp; k++)
	{	ofs = ofsBD[k];								// Polygon offset in table
		dst	= H.TransformSRT(4,vboBUF+ofs,dst);
		nbvt	-= 4;
	}
	if (nbvt < 0) gtfo("CRunway::CollectDesignator() PB");
	//--- Collect right part ----------------------------
	H.Translation(rend ->rTRH);			//	Translate to Right part
	for (U_INT k=0; k< nbp; k++)
	{	ofs = ofsBD[k];								// Polygon offset in table
		dst	= H.TransformSRT(4,vboBUF+ofs,dst);
		nbvt	-= 4;
	}
	if (nbvt < 0) gtfo("CRunway::CollectDesignator() PB");
	//--- Collect number and letter for Hi runway  end --
	H.Scale(scl);										// Scale for designator
	//--- Collect left number -----------------------------------
	H.Translation(rend->lPos);			// Translation to left number
	itm	= rend->LeftN;							// Left number
	tab	= ofsTAB[itm];							// Offset
	nbp	= nbrPOL[itm];							// Number of polygons
	for (U_INT k=0; k< nbp; k++)
	{	ofs = tab[k];									// Polygon offset in table
		dst	= H.TransformSRT(4,vboBUF+ofs,dst);
		nbvt	-= 4;
	}
	if (nbvt < 0) gtfo("CRunway::CollectDesignator() PB");
	//--- Collect right number -----------------------------------
	H.Translation(rend->rPos);		// Translation to left number
	itm	= rend->RiteN;						// right number
	tab	= ofsTAB[itm];						// Offset
	nbp	= nbrPOL[itm];						// Number of polygons
	for (U_INT k=0; k< nbp; k++)
	{	ofs = tab[k];								// Polygon offset in table
		dst	= H.TransformSRT(4,vboBUF+ofs,dst);
		nbvt	-= 4;
	}
	if (nbvt < 0) gtfo("CRunway::CollectDesignator() PB");
	//--- Collect letter     -----------------------------------
	itm	= rend->LetID;						// right number
	if (0 == itm)					return dst;
	H.Translation(rend->cPos);		// Translation to left number
	tab	= ofsTAB[itm];						// Offset
	nbp	= nbrPOL[itm];						// Number of polygons
	for (U_INT k=0; k< nbp; k++)
	{	ofs = tab[k];								// Polygon offset in table
		dst	= H.TransformSRT(4,vboBUF+ofs,dst);
		nbvt -= 4;
	}
	if (nbvt < 0) gtfo("CRunway::CollectDesignator() PB");
	return dst;
}

//=========================================================================================
//  Destroy Airport Q
//=========================================================================================
CAptQueue::~CAptQueue()
{ CAptObject *apo = Pop();
  while (apo) { delete apo; apo = Pop(); }
}
//=========================================================================================
//  Build Airport Object
//=========================================================================================
CAptObject::CAptObject(CAirportMgr *md, CAirport *apt)
{	state		= APT_CREATED;
	apm     = md;
	dead		= 0;
  pApt    = apt;
  Airp    = apt;
	nmiles  = apt->GetNmiles();
	lSiz		= md->LightSize();
	//---- Clear vbo buffer to avoid drawing -----------------------
	pVBO	  = 0;
	eVBO		= 0;
	cVBO		= 0;
	rVBO		= 0;
	//---- Locate QGT ----------------------------------------------
  Org     = Airp->GetPosition();
	Org.alt	= 0;
	//--------------------------------------------------------------
  apt->SetAPO(this);
  GetLatitudeFactor(Org.lat,rdf,xpf);
  ground  = Org.alt;
  oTAXI   = globals->txw->GetTaxiTexture();
  cutOF   = 1500;                         // Altitude cut-off
  //--------------------------------------------------------------
  tcm     = globals->tcm;
  scale   = tcm->GetScale();
  //-----Colors --------------------------------------------------
  white  = (float*)&ColorTAB[TC_WHI_LITE];
  yellow = (float*)&ColorTAB[TC_YEL_LITE];
  //-----Drawing parameters --------------------------------------
  sta3D   = TC_AP_FADE_IN;
  alpha   = 0;
  //-----Compute scale factor for designator ---------------------
  scl.x = FN_ARCS_FROM_FEET(1.4) * xpf;
  scl.y = FN_ARCS_FROM_FEET(1.4);
  //-----Scale factor for threshold bands ------------------------
  sct.x = FN_ARCS_FROM_FEET(xpf);
  sct.y = FN_ARCS_FROM_FEET(1);
	//--- VBO Management -------------------------------------------
	nDES	= 0;
  //----Add profile to POD ---------------------------------------
  AddPOD();
  //-----Lighting control -----------------------------------------
  lTim    = 0;                              // Light timer
  lrwy    = 0;                              // Lighting runway
  swlt    = 0;                              // State off
  lreq    = 0;                              // No request
  //----Init Trace option ---------------------------------------
	int op  = 0;
  GetIniVar ("TRACE", "Airport", &op);
  tr			= op;
	if (tr) TRACE("===AIRPORT CONSTRUCTION: %s",apt->GetName());
	//--- Clear parameters  ---------------------------------------
	taxiMGR = 0;									// Taxi manager
	lnDW		= 0;									// landing to draw
}
//----------------------------------------------------------------------------------
//  Constructor for export only
//----------------------------------------------------------------------------------
CAptObject::CAptObject(CAirport *apt)
{ apm     = 0;
	dead		= 0;
  pApt    = 0;
  Airp    = apt;
  Org     = apt->GetPosition();
  ground  = Org.alt;
  GetLatitudeFactor(Org.lat,rdf,xpf);
  tcm     = globals->tcm;
  scale   = tcm->GetScale();
	taxiMGR = 0;									// Taxi manager
	//---- Clear vbo buffer to avoid drawing -----------------------
	pVBO	  = 0;
	eVBO		= 0;
	cVBO		= 0;
	rVBO		= 0;
}
//----------------------------------------------------------------------------------
//  Init position
//----------------------------------------------------------------------------------
bool CAptObject::InitBound()
{ GroundSpot spot(Org.lon,Org.lat);
  globals->tcm->GetGroundAt(spot);
	Airp->SetElevation(spot.alt);					// Set terrain altitude			
  glim.xmax = glim.xmin = AbsoluteTileKey(int(spot.qx),int(spot.tx));
  glim.zmax = glim.zmin = AbsoluteTileKey(int(spot.qz),int(spot.tz));
  return spot.HasQGT();
}
//----------------------------------------------------------------------------------
//  Add files for this airport
//  /RUNWAYS/XXX.RLP            For runway light profile
//  /DIAGRAM/XXX.PNG            For airport Diagram
//----------------------------------------------------------------------------------
void CAptObject::AddPOD()
{ char  fnm[PATH_MAX];
  char  key[PATH_MAX];
  _snprintf(key,(PATH_MAX-1),"%s.RLP",GetAptName());
  _snprintf(fnm,(PATH_MAX-1),"RUNWAYS/%s",key);
  pAddDisk(&globals->pfs,key,fnm);
  return;
}
//---------------------------------------------------------------------------------
//  Remove Profile from POD
//---------------------------------------------------------------------------------
void CAptObject::RemPOD()
{ char  fnm[PATH_MAX];
  char  key[PATH_MAX];
  _snprintf(key,(PATH_MAX-1),"%s.RLP",GetAptName());
  _snprintf(fnm,(PATH_MAX-1),"RUNWAYS/%s",key);
  pRemDisk(&globals->pfs,key,fnm);
  return;
}
//---------------------------------------------------------------------------------
//  destroy Airport Object
//  NOTE:		We must avoid the teleport
//					case where the QGT are destroyed first followed some time later by
//					the airport object (then the Quad and the textureDef are no longer allocated);  
//					Thus each time a ground tile is marked, the QGT user count is increment
//					to prevent QGT deletion
// 
//----------------------------------------------------------------------------------
CAptObject::~CAptObject()
{ UnmarkGround();
	if (apm) RemPOD();
  if (pVBO)	glDeleteBuffers(1,&pVBO);
	if (eVBO)	glDeleteBuffers(1,&eVBO);
	if (cVBO)	glDeleteBuffers(1,&cVBO);
	if (rVBO) glDeleteBuffers(1,&rVBO);
  //--- free tarmac segments ------------------
	for (U_INT k = 0; k < tmcQ.size(); k++) delete tmcQ[k];
	tmcQ.clear();
	//-------------------------------------------
	if (taxiMGR)	delete taxiMGR;
	//-------------------------------------------
}
//---------------------------------------------------------------------------------
//  Kill this airport
//---------------------------------------------------------------------------------
void	CAptObject::Kill()
{	dead	= 1;
	CRunway  *rwy = 0;
  //-----Build non paved runway first -----------------
  for (rwy = Airp->GetNextRunway(rwy); rwy != 0;rwy = Airp->GetNextRunway(rwy))
	{	rwy->RazRLP();	}
	return;
}
//---------------------------------------------------------------------------------
//  Compute airport extension in term of Detail Tiles
//---------------------------------------------------------------------------------
void CAptObject::AptExtension(GroundSpot &gs)
{ U_INT xk = AbsoluteTileKey(gs.qx,gs.tx);
  if (TileIsLeft(xk,glim.xmin))   glim.xmin = xk;
  if (TileIsLeft(glim.xmax,xk))   glim.xmax = xk;
  U_INT zk = AbsoluteTileKey(gs.qz,gs.tz);
  if (zk < glim.zmin)             glim.zmin = zk;
  if (zk > glim.zmax)             glim.zmax = zk;
  return;
}
//---------------------------------------------------------------------------------
//  Build part of airport until everything is OK
//---------------------------------------------------------------------------------
bool CAptObject::BuildAll()
{	switch (state)	{
			case APT_CREATED:
				return SetRunway();
			case APT_HASRUNWAY:
				return BuildEnd();
	}
	return false;
}
//---------------------------------------------------------------------------------
//  For each runway, build a set of segment polygons
//  NOTE: All pavements use the common ground from airport position
//        All coordinates are in arcsec, relative to airport origin
//---------------------------------------------------------------------------------
bool CAptObject::SetRunway()
{ CAirport *apt = GetAirport();
  if (!apt->HasRunway())          return false;
  if (!globals->tcm->MeshReady()) return false;
  if (!InitBound())               return false;
  apm->SetRunwayProfile(apt);
  CRunway  *rwy = 0;
  //-----Build non paved runway first -----------------
  for (rwy = apt->GetNextRunway(rwy); rwy != 0;rwy = apt->GetNextRunway(rwy))
  { U_CHAR   type = rwy->GetPaved();
    //----Now build runway segments -------------------
    if (type != TC_RWY_OTHER) continue;
		CRwyGenerator(rwy,this,2);
  }
  //----Build paved runway after ----------------------
  for (rwy = apt->GetNextRunway(rwy); rwy != 0;rwy = apt->GetNextRunway(rwy))
  { U_CHAR   type = rwy->GetPaved();
    //----Now build runway segments -------------------
    if (type != TC_RWY_PAVED) continue;
		CRwyGenerator(rwy,this,1);
		nDES	+= rwy->GetNBVT();				// Total vertices for designators
  }
  //-----Locate airport ground    ---------------------
  LocateGround();                   // Locate detail tiles
	state = APT_HASRUNWAY;
  return true;
}
//---------------------------------------------------------------------------------
//	Compact runway and locate ground tiles
//---------------------------------------------------------------------------------
bool CAptObject::BuildEnd()
{	//-----Read the pavement data -----------------------
  GetTaxiways();
	CompactRWY();
	//CompactDesignator();
	state = APT_IS_READY;
	return true;
}
//---------------------------------------------------------------------------------
//	Trace runway parameters 
//---------------------------------------------------------------------------------
void CAptObject::TraceRWY(CRunway *rwy)
{ RwyID *hi = rwy->GetEndDEF(RWY_HI_END);
	TRACE("****Rwy %4s-%4s head=%.4f magh=%.4f ori=%.4f lnd=%.2f",
				Airp->GetIdent(),rwy->GetHiEnd(),rwy->GetHiDir(),rwy->GetHmDir(),
				hi->aRot,rwy->GetLandDir(RWY_HI_END));
	RwyID *lo = rwy->GetEndDEF(RWY_LO_END);
	TRACE("****Rwy %4s-%4s head=%.4f magh=%.4f ori=%.4f lnd=%.2f",
				Airp->GetIdent(),rwy->GetLoEnd(),rwy->GetLoDir(),rwy->GetLmDir(),
				lo->aRot,rwy->GetLandDir(RWY_LO_END));
	return;
}
//-------------------------------------------------------------------------------
//    Get elevation for paved vertex
//  TODO must check for world wrap arround the 0 meridian
//-------------------------------------------------------------------------------
void CAptObject::ComputeElevation(TC_VTAB *tab)
{ double lon = tab->VT_X + Org.lon;
  double lat = tab->VT_Y + Org.lat;
  GroundSpot spot(lon,lat);
  globals->tcm->GetGroundAt(spot);
  tab->VT_Z  = (spot.alt - Org.alt);
  return;
}

//---------------------------------------------------------------------------------
//  Build one VBO for each queue
//---------------------------------------------------------------------------------
void CAptObject::CompactRWY()
{	U_INT     tot = 0;
	TC_VTAB  *buf = 0;
	//--- Pavement VBO ---------------------------
	buf		= PutInVBO(pavQ,nPAV);
	if (buf)
	{	tot = nPAV * sizeof(TC_VTAB);
		glGenBuffers(1,&pVBO);
		glBindBuffer(GL_ARRAY_BUFFER,pVBO);
		glBufferData(GL_ARRAY_BUFFER,tot,buf,GL_STATIC_DRAW);
		delete [] buf;
	}
	//--- Edge VBO --------------------------------
	buf		= PutInVBO(edgQ,nEDG);
	if (buf)
	{	tot = nEDG * sizeof(TC_VTAB);
		glGenBuffers(1,&eVBO);
		glBindBuffer(GL_ARRAY_BUFFER,eVBO);
		glBufferData(GL_ARRAY_BUFFER,tot,buf,GL_STATIC_DRAW);
		delete [] buf;
	}
	//--- Center VBO ------------------------------
	buf		= PutInVBO(cntQ,nCTR);
	if (buf)
	{	tot = nCTR * sizeof(TC_VTAB);
		glGenBuffers(1,&cVBO);
		glBindBuffer(GL_ARRAY_BUFFER,cVBO);
		glBufferData(GL_ARRAY_BUFFER,tot,buf,GL_STATIC_DRAW);
		delete [] buf;
	}
  glBindBuffer(GL_ARRAY_BUFFER,0);
	return;
}
//---------------------------------------------------------------------------------
//  Compact runway designator
//---------------------------------------------------------------------------------
void CAptObject::CompactDesignator()
{	if (0 == nDES)			return;
	CAirport *apt = GetAirport();
	F3_VERTEX *buf = new F3_VERTEX[nDES];
	F3_VERTEX *dst = buf;
	CRunway   *rwy = 0;
  //-----Build non paved runway first --------------------------------------
  for (rwy = apt->GetNextRunway(rwy); rwy != 0;rwy = apt->GetNextRunway(rwy))
	{	dst	= rwy->CollectDesignator(TC_HI,sct,dst);
		dst	= rwy->CollectDesignator(TC_LO,sct,dst);
	}
	//--- Allocate a VBO -----------------------------------------------------
	int dim = nDES * sizeof(F3_VERTEX);
	glGenBuffers(1,&rVBO);
	glBindBuffer(GL_ARRAY_BUFFER,rVBO);
	glBufferData(GL_ARRAY_BUFFER,dim,buf,GL_STATIC_DRAW);
	delete [] buf;
	return;
}
//---------------------------------------------------------------------------------
//  Build one VBO from the Queue
//---------------------------------------------------------------------------------
TC_VTAB *CAptObject::PutInVBO(CPaveQ &hq, U_INT n)
{ if (0 == n)	return 0;
	TC_VTAB   *vbo  = new TC_VTAB[n];
	TC_VTAB   *dst	= vbo;
	
	CPaveRWY	*pave = hq.Pop();
	while (pave)
	{	dst += pave->CopyTo(dst);
	  delete pave;
		pave = hq.Pop();
	}
	return vbo;	}
//---------------------------------------------------------------------------------
//  Build Taxiways from files
//---------------------------------------------------------------------------------
int CAptObject::BuildTaxiways()
{ char  *fn  = 0;
  char  fname[64];
  fn = Airp->GetIdentity();
  _snprintf(fname,64,"DATA/%s.TMS",fn);
  if (pexists (&globals->pfs, fname))               // Description
  {   CDataTMS tms(this);                           // TMS decoder
      tms.DecodeBinary(fname);                      // Decode file
  }
  return 1;
}
//---------------------------------------------------------------------------------
//  Check for SQL taxiways
//---------------------------------------------------------------------------------
int CAptObject::GetTaxiways()
{ nPAV	= 0;
	nEDG	= 0;
	nCTR	= 0;
	//--- Load yellow texture --------------------------
  char txy = globals->sqm->UseTxyDB();
	if (0 == txy) return BuildTaxiways();
  //---Read taxiways from SQL database ---------------
  int np = globals->sqm->DecodePAVE(this);
  if (0 == np)  return BuildTaxiways();
  int nl = globals->sqm->DecodeLITE(this);
	//--- Compact all data into VBO --------------------
  return 1;
}
//---------------------------------------------------------------------------------
//  Build Taxiway management
//---------------------------------------------------------------------------------
void CAptObject::LoadTaxiways()
{	if (taxiMGR)		return;
	taxiMGR = new TaxiwayMGR(this);
}
//---------------------------------------------------------------------------------
//  Clear Taxiway management
//---------------------------------------------------------------------------------
void CAptObject::ClearTaxiways()
{	if (taxiMGR)	delete taxiMGR;
	taxiMGR = 0;
	return;
}
//---------------------------------------------------------------------------------
//  Add one pavement to Queue
//---------------------------------------------------------------------------------
void CAptObject::OnePavement(CPaveRWY *p,U_INT n)
{switch (p->GetType()) {
  case PAVE_TYPE_PAVE:
    pavQ.PutEnd(p);
		nPAV	+= n;
    return;
  case PAVE_TYPE_EDGE:
    edgQ.PutEnd(p);
		nEDG	+= n;
    return;
  case PAVE_TYPE_CENT:
    cntQ.PutEnd(p);
		nCTR	+= n;
    return;
  }
//---Unknown type ---------------------------
  delete p;
}
//---------------------------------------------------------------------------------
//  Airport beacon
//---------------------------------------------------------------------------------
void CAptObject::BeaconLight()
{ float ofs = float(FN_ARCS_FROM_FEET(100));
  U_CHAR inx = Airp->GetBeacon();
  if ((inx == 0) || (inx  > 7)) return;
  CFlshLITE *lit = new CFlshLITE();
  lit->TimeON (2.0);
  lit->TimeOFF(0.5);
  TC_VTAB  ent;
  lit->col1     = LiteBC1[inx];
  lit->col2     = LiteBC2[inx];
  lit->ntex     = TC_WHI_TEX;
  ent.VT_S = 0.5;
  ent.VT_T = 0.5;
  ent.VT_X = ofs;              // Offset beacon location
  ent.VT_Y = ofs;              // Offset beacon location
  ent.VT_Z = 30;
  lit->SpotAt(0,ent);
  becS.AddLight(lit);
  becS.SetMiniSize(16);
  becS.SetS1(1);
  return;
}
//=================================================================================
//	AIRPORT GROUND MANAGEMENT
//	NOTE: Airport ground must be drawed first as tarmac, bands, letters, taxiway, 
//				etc, are set uppon the ground with no Z buffer test.  Mountains in near
//				range may overlay an airport drawing. If we where drawing tarmacs after
//			  drawin all the terrain, then airport features  would be placated uppon 
//				the near mountain.
//---------------------------------------------------------------------------------
//  Locate the set of ground tiles
//  Bounding coordinates are relatives to airport origin
//---------------------------------------------------------------------------------
void CAptObject::LocateGround()
{ CmQUAD *qdl = globals->tcm->GetTileQuad(glim.xmin,glim.zmin);
  CmQUAD *qdu = globals->tcm->GetTileQuad(glim.xmax,glim.zmax);
  MarkGround(glim);           // Mark ground tiles
  return;
}
//---------------------------------------------------------------------------------
//  Draw airport grid (Used for test only)
//---------------------------------------------------------------------------------
void CAptObject::DrawGrid()
{ double x0 = llc.lon - Org.lon;
  double y0 = llc.lat - Org.lat;
  glColor3f(1,0,0);
  glBegin(GL_LINE_LOOP);
  glVertex3f(x0,y0,0);
  glVertex3f(x0,0,0);
  glVertex3f(0,0,0);
  glVertex3f(0,y0,0);
  glEnd();
}
//-----------------------------------------------------------------------
//  Mark the ground tiles
//	Number of vertice is collected for VBO allocation
//-----------------------------------------------------------------------
void CAptObject::MarkGround(TC_BOUND &bnd)
{ U_INT  txe = TC_NEXT_INDICE(bnd.xmax);
  U_INT  tze = TC_NEXT_INDICE(bnd.zmax);
  U_INT   lz = 0;
  U_INT   cx = 0;
  TCacheMGR *tcm = globals->tcm;
  //-----Mark all tiles making airport ground ----
  for   (lz = bnd.zmin; lz != tze; lz = TC_NEXT_INDICE(lz))
  { for (cx = bnd.xmin; cx != txe; cx = TC_NEXT_INDICE(cx))
    { //--- Get tile descriptor -----------------
			U_INT qx = QGT_PART(cx);
			U_INT qz = QGT_PART(lz);
			C_QGT *qgt = globals->tcm->GetQGT(qx,qz);
			U_INT	tx = TILE_PART(cx);
			U_INT tz = TILE_PART(lz);
			CTextureDef *txd = qgt->GetTexDescriptor(tx,tz);
			CmQUAD *quad = tcm->GetTileQuad(cx,lz);
			gtile.push_back(txd);
			qgt->IncUser();
    }
  }
  return;
}
//-----------------------------------------------------------------------
//  UnMark the ground tiles
//  NOTE:  Check for txn existence (teleport case)
//-----------------------------------------------------------------------
void CAptObject::UnmarkGround()
{ //------------------------------------------------
	std::vector<CTextureDef *>::iterator ig;
	for (ig = gtile.begin(); ig != gtile.end(); ig++)
    { CTextureDef *txd		= (*ig);
	    txd->quad->ClearGround();
			txd->quad->GetSuperTile()->GetQGT()->DecUser();
    }
  gtile.clear();
  return;
}
//-----------------------------------------------------------------------
//  Swap light control
//  Defer order if a commande is already running
//-----------------------------------------------------------------------
void CAptObject::ChangeLights(char ls)
{ if (lrwy)     return;                // Defer
  CAirport *apt = (CAirport*)pApt.Pointer();
  lrwy  = apt->GetNextRunway(0);
  lTim  = 0;
  swlt  = ls;
  return;
}
//-----------------------------------------------------------------------
//  Update lights
//  
//-----------------------------------------------------------------------
void CAptObject::UpdateLights(float dT)
{//---Process lighting order ---------------------------
  lTim += 1;
  if (lTim & 0x01)        return;
  lTim  = 0;
  char ok = lrwy->ChangeLights(swlt);
  if (ok)                 return;
  lrwy    = lrwy->GetNext();
  return;
}
//=================================================================================
//	AIRPORT Landing management
//=================================================================================
//Build line up waypoint
//-----------------------------------------------------------------------
void	CAptObject::BuildLandingLane(LND_DATA *lnd)
{	char *id = "land";
	linWP.SetNOD(0);
	linWP.SetParameters('fixe',id,Airp->GetName(),&lnd->fwdP);
	linWP.SetRefDirection(lnd->orie);
	lnRWY = lnd;
	return;
}

//-----------------------------------------------------------------------
//  Time slice
//  
//-----------------------------------------------------------------------
void CAptObject::TimeSlice(float dT)
{ //-----------------------------------------------------	
	//--- Process lighting order --------------------------
	float lum = globals->tcm->GetLuminosity();
  if (lrwy)           return UpdateLights(dT) ;
  bool nowl     = (lum < 1.2145) || (lreq);       //
  bool oldl     = (swlt != 0);
  if (nowl == oldl)   return;
  //--- Start to change light --------------------------
  swlt          = nowl;
  CAirport *apt = (CAirport*)pApt.Pointer();
  lrwy  = apt->GetNextRunway(0);
  lTim  = 0;
  return;
}
//==================================================================================
//  For each runway, build a light profile (RLP) and a set of segment polygons
//  NOTE: All pavements use the common ground from airport position
//        All coordinates are in arcsec, relative to airport origin
//==================================================================================
int CAirportMgr::SetRunwayProfile(CAirport *apt)
{ U_CHAR mod  = 0;
  char  *fn   = 0;
  nProf       = 0;                            // Stack empty
  CRLParser prs(this,apt->GetAptName());      // Read user profiles
  CRunway  *rwy = 0;
  for (rwy = apt->GetNextRunway(rwy); rwy != 0;rwy = apt->GetNextRunway(rwy))
	{ if (rwy->HasProfile())  continue;					//return 0;
    //---Build the runway default profile -----------------------
    CRLP    *lpf = new CRLP(rwy);             // Build runway light profile
    //----Locate Hi and Lo user profile -------------------------
    RWY_EPF *hip = GetEndProfile(rwy->GetHiEnd());
    if (hip)  lpf->MergeHiUser(hip);
    RWY_EPF *lop = GetEndProfile(rwy->GetLoEnd());
    if (lop)  lpf->MergeLoUser(lop);
    rwy->SetRLP(lpf);                         // Store in runway
  }
  return 1;
}
//---------------------------------------------------------------------------------
//  Add profil to global table
//	For a given airport, each profile decoded from the configuration file are
//	add to this table
//---------------------------------------------------------------------------------
void CAirportMgr::AddProfile(RWY_EPF &epf)
{ rwyRLP[nProf++] = epf;
  return;
}
//---------------------------------------------------------------------------------
//  Locate end profile
//---------------------------------------------------------------------------------
RWY_EPF *CAirportMgr::GetEndProfile(char *id)
{ for (int k=0; k<=nProf;k++) if (strcmp(rwyRLP[k].end,id) == 0)  return (rwyRLP + k);
  return 0;
}
//-----------------------------------------------------------------------------------------
//  Draw from a VBO  
//-----------------------------------------------------------------------------------------
void CAptObject::DrawVBO(U_INT vbo,U_INT n)
{	if (0 == vbo)	return;
  glBindBuffer(GL_ARRAY_BUFFER,vbo);
	glVertexPointer  (3,GL_FLOAT,sizeof(TC_VTAB),OFFSET_VBO(2*sizeof(float)));
  glTexCoordPointer(2,GL_FLOAT,sizeof(TC_VTAB),0);
	glDrawArrays(GL_TRIANGLES,0,n);
	return;
}
//-----------------------------------------------------------------------------------------
//  Draw designator  
//-----------------------------------------------------------------------------------------
void CAptObject::DrawDesignators()
{	if (0 == rVBO)	return;
	glDisable(GL_TEXTURE_2D);
  glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glBindBuffer(GL_ARRAY_BUFFER,rVBO);
	glVertexPointer  (3,GL_FLOAT,0,0);
	glDrawArrays(GL_TRIANGLE_STRIP,0,nDES);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
//	glBindBuffer(GL_ARRAY_BUFFER,0);
	glEnable(GL_TEXTURE_2D);
	return;
}
//---------------------------------------------------------------------------------
//  Draw the Airport  Objects
//  All coordinates are in arcseconds relative to airport origin
//  A translation is made from aircarft location to airport location, then
//  all objects are drawed relative to the airport origin
//        A)The ground tiles are first drawed, using the same transformations
//          as the terrain one.
//        B)Then taxiways and runway are placated on the tiles
//        C) Then other aspects of terrain are rendered
//         This order is important.  If not respected, taxiway and runway
//         would be see through mountains in certain situations
//        D) The last pass is dedicated to draw the lights
//
//  NOTES: During Step A and B above, the deepth buffer is not written.  This speeds
//         up rendering and avoid flickering effect when 3D airport draws on the
//         airport ground (pavement, taxiways, etc);
//         
//         Taxiway Pavement and runway should be rendered together as they are
//         Textured. The order is also important
//         Edges and center lines are rendered when altitude is under a certain value
//         to avoid flickering.
//---------------------------------------------------------------------------------
void CAptObject::Draw()
{ TCacheMGR *tcm = globals->tcm;
  //--- Draw Airport ground ----------------------------------------------
  glDisable(GL_BLEND);
	glEnable(GL_TEXTURE_2D);
	DrawGround();
  //----Compute translation offset from aircraft to airport origin --------
  ofap.x  = LongitudeDifference(Org.lon,apos.lon);
  ofap.y  = Org.lat - apos.lat;
  ofap.z  = Org.alt - apos.alt;
  Alt     = -ofap.z;
  //----Cull airport based on ground tile corners -------------------------
  if (globals->noAPT)     return;
	if (NotVisible())				return;
  //-----Prepare taxiway drawing ------------------------------------------
  SetAlphaColor(alpha);
  glDisable(GL_DEPTH_TEST);
  glDepthMask(false);
  glPolygonMode (GL_FRONT, GL_FILL);          
  glPushMatrix();                                   // Mark T1
  glTranslated(ofap.x,ofap.y,ofap.z);               // Camera at airport origin
  //-----Draw all pavements -----------------------------------------------
  glFrontFace(GL_CW);
	ColorGL(COLOR_WHITE);
  glBindTexture(GL_TEXTURE_2D,oTAXI);
	//if (tr) TRACE("TCM: --Draw pavement %s",Airp->GetName());
	//if (tr) TRACE("pVBO =%d , nPAV = %d",pVBO,nPAV);
	DrawVBO(pVBO,nPAV);
	//if (tr) TRACE("TCM: --Leave pavement");
  //-----Draw all edges ---------------------------------------------------
	if (nmiles < 2)  { apm->BindYellow(); DrawVBO(eVBO,nEDG); }
  //-----Draw runways -----------------------------------------------------
	ColorGL(COLOR_WHITE);
	std::vector<CTarmac*>::iterator tm;
	for(tm=tmcQ.begin(); tm!=tmcQ.end(); tm++) (*tm)->Draw();
	//--- Draw designators---------------------------------------------------
	glBindBuffer(GL_ARRAY_BUFFER,0);
  //-----Draw Center marks if distance < 2Nm  -----------------------------
	if (nmiles < 2)  { apm->BindYellow();	DrawVBO(cVBO,nCTR); }
  glDepthMask(true);
  //-----------------------------------------------------------------------
  // Restore states
  //-----------------------------------------------------------------------
  glEnable(GL_DEPTH_TEST);
  glFrontFace(GL_CCW);
  //-----------------------------------------------------------------------
  glPopMatrix ();                           // Back to T1
  //-----------------------------------------------------------------------
  ResetAlphaColor();
  if (sta3D == TC_AP_FADE_IN) Update3Dstate();

  //----Check error -------------------------------------------------------
 /*
  {GLenum e = glGetError ();
   if (e != GL_NO_ERROR) 
    WARNINGLOG ("OpenGL Error 0x%04X : %s", e, gluErrorString(e));
  }
 */
  return;
}
//-----------------------------------------------------------------------------------------
//  Pre Draw:  Called by camera  
//-----------------------------------------------------------------------------------------
void CAptObject::PreDraw(CCamera *cam)
{ glPushAttrib(GL_ALL_ATTRIB_BITS);
  glPushClientAttrib (GL_CLIENT_VERTEX_ARRAY_BIT);
  glMaterialfv (GL_FRONT, GL_EMISSION, tcm->GetDeftEmission()); 
  glEnable(GL_TEXTURE_2D);
  glDepthMask(false);
  glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);          
  glShadeModel(GL_FLAT);
  return;
}
//-----------------------------------------------------------------------------------------
//  Draw for VectorMap:  Called by camera to render runways
//  NOTE: Camera is supposed to be at airport origin
//-----------------------------------------------------------------------------------------
void CAptObject::CamDraw(CCamera *ac)
{ SVector    trs;
  //----Compute translation to airport origin -----------------------------
  trs.x = LongitudeDifference(Org.lon,ac->GetTargetLon());
  trs.y = Org.lat - ac->GetTargetLat();
  trs.z = Org.alt;
	//--- Prepare client state ----------------------------------------------
  glPushClientAttrib (GL_CLIENT_ALL_ATTRIB_BITS);
  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  //-----Prepare taxiway drawing ------------------------------------------
  float *white = (float*)&ColorTAB[TC_WHI_LITE];
  glScaled(scale->x,scale->y, 1.0);                 // T1 Scale X,Y to feet coordinate
  glPushMatrix();
  glTranslated(trs.x,trs.y,trs.z);                  // Airport origin
  glFrontFace(GL_CW);
  
  //-----Draw all pavements -----------------------------------------------
  glBindTexture(GL_TEXTURE_2D,oTAXI);
  //glColor4fv(white);
	ColorGL(COLOR_WHITE);
	DrawVBO(pVBO,nPAV);
  //-----Draw all edges ---------------------------------------------------
	apm->BindYellow();
  DrawVBO(eVBO,nEDG);
  //-----Draw runways -----------------------------------------------------
	glBindBuffer(GL_ARRAY_BUFFER,0);
	std::vector<CTarmac*>::iterator tm;
	for(tm=tmcQ.begin(); tm!=tmcQ.end(); tm++) (*tm)->Draw();
  //-----Draw Center marks -------------------------------------------------
  apm->BindYellow();
  DrawVBO(cVBO,nCTR);
  glBindBuffer(GL_ARRAY_BUFFER,0);
	//--- Restore state ----------------------------------------------------
  glPopClientAttrib();
  //-----Translate to aircraft . Put an icon on tarmac ground --------------
  float  elv = tcm->GetGroundAltitude();    // ground at aircraft position
  glFrontFace(GL_CCW);
  glColor4fv(yellow);
	ColorGL(COLOR_YELLOW);
  trs.x  = LongitudeDifference(apos.lon,ac->GetTargetLon());
  trs.y  = apos.lat - ac->GetTargetLat();
  trs.z  = elv - ac->GetTargetAlt();
  //--- Get Aircraft heading  ------------------------------------------
  double rot = globals->dang.z;
  glPopMatrix();                            // Back to aircraft position
  glTranslated(trs.x,trs.y,trs.z);          // Plane origin
  glScaled(xpf,1.0, 1.0);                   // T1 Scale X,Y to feet coordinate
  glRotated(rot,0,0,1);
  //--- Draw pictogram --------------------------------------------------
  CPicQUAD *icp = apm->GetIcon();
  icp->SetElevation(elv);
  glEnable(GL_BLEND);
  icp->Draw();
  return;
}
//-----------------------------------------------------------------------------------------
//  End Draw by camera.  
//-----------------------------------------------------------------------------------------
void CAptObject::EndDraw(CCamera *cam)
{ glPopAttrib();
  glPopClientAttrib ();
  return;
}
//==========================================================================================
//	DRAW GROUND
//	Camera is set at airport position
//	NOTE:  Due to a precision problem, we must draw terrain elevation as a near airport
//				object, otherwise, ground and lights are flickering and this is a visible
//				artefact.  To do so, all tiles that belong to an airports are isolated and
//				relative coordinates (to airport center) are computed, and tiles are drawed
//				first. However, as second artifact is also present, at the junction of 
//				airport Tiles and normal terrain. So we must draw draw over hte texture agin
//				this time, using the normal terrain coordinates.
//				May be some better way is to be found to get better performances.
//==========================================================================================
void CAptObject::DrawGround()
{ visible = 0;
	glMaterialfv (GL_FRONT, GL_EMISSION, tcm->GetDeftEmission()); 
	glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
  glFrontFace(GL_CCW);
	//-------------------------------------------------------
	glPushClientAttrib (GL_CLIENT_ALL_ATTRIB_BITS);
	glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	CVector T;
	std::vector<CTextureDef*>::iterator ig;
	for (ig = gtile.begin(); ig != gtile.end(); ig++)
	{	CTextureDef  *txd = (*ig);
		CmQUAD *quad		= txd->quad;
		if (!quad->GetSuperTile()->Visibility()) continue;
		visible					= 1;
		C_QGT  *qgt			= quad->GetSuperTile()->GetQGT();
		SPosition P			= qgt->GetBase();
		SVector  vtr		= SubtractPositionInArcs(apos,P);
		glPushMatrix();
		glTranslated(vtr.x, vtr.y, vtr.z);
		glBindTexture(GL_TEXTURE_2D,txd->dOBJ);
		quad->DrawIND();
		glPopMatrix();
	}
	glPopClientAttrib();
	//-------------------------------------------------------
	glBindBuffer(GL_ARRAY_BUFFER,0);
	//-------------------------------------------------------
	return;
}
//-----------------------------------------------------------------------------------------
//  update alpha chanel
//-----------------------------------------------------------------------------------------
void CAptObject::Update3Dstate()
{ alpha += 0.005f;
  if (alpha < 1)  return;
  alpha = 1;
  sta3D = TC_AP_IN_VIEW;
  return;
}
//-----------------------------------------------------------------------------------------
//  Compute offset of position to airport origin in arcsecs
//-----------------------------------------------------------------------------------------
void CAptObject::Offset(SPosition &p, SVector &v)
  {v.x = LongitudeDifference(p.lon,Org.lon);
   v.y = p.lat - Org.lat;
   v.z = p.alt - Org.alt;
  }
//-----------------------------------------------------------------------------------------
//  Draw all lights when airport is visible.
//  All light systems are relative to airport origin
//  Lights must be drawed as last objects due to the transparency of halo
//-----------------------------------------------------------------------------------------
void CAptObject::DrawLights(CCamera *cc)
{ if (cam != cc) cam = cc;
  glPushMatrix();                                   // Mark Matrix
  glScaled(scale->x,scale->y, 1.0);                 // T1 Scale X,Y to feet coordinate
  glTranslated(ofap.x,ofap.y,ofap.z);               // Camera at Airport origin
  //-----------------------------------------------------------------------
  SetCameraPosition();
  glPushAttrib(GL_POINT_BIT);
	//---- Set Drawing conditions -------------------------------------------
	GLfloat p[] = {0.0f,0.01f,0.00001f};
  glPointParameterfv(GL_POINT_DISTANCE_ATTENUATION,p);
  glPointParameterf (GL_POINT_SIZE_MIN,1);
  glPointParameterf (GL_POINT_SIZE_MAX,64);
  glPointSize(lSiz);
  glEnable(GL_POINT_SPRITE);
  glTexEnvi(GL_POINT_SPRITE,GL_COORD_REPLACE,GL_TRUE);
  //-----Draw runways light -----------------------------------------------
  CRunway *rwy = 0;
  for (rwy = Airp->GetNextRunway(rwy); rwy != 0;rwy = Airp->GetNextRunway(rwy))
  { CRLP *lpf = rwy->GetRLP();
    if (0 == lpf)                 continue;
		lpf->DistanceNow(nmiles);
    lpf->DrawLITE(cpos);                      // Draw night lights
  }
  //----Draw Taxiway lights -----------------------------------------------
	if (swlt && (nmiles < 4))			taxS.DrawSystem(cpos);
  if (swlt)                     becS.DrawSystem(cpos);
  //-----------------------------------------------------------------------
  glPopAttrib();
  glPopMatrix();
  return;
}
//-----------------------------------------------------------------------------------------
//  Return camera position (X,Y) in arcsec relative to airport origin
//  Camera position is used to compute correct color of Dual colored light
//  that depends on the camera position
//-----------------------------------------------------------------------------------------
void CAptObject::SetCameraPosition()
{ cam->GetOffset(cpos);                          // In feet relative to aircraft
  cpos.x = AddLongitude(FN_ARCS_FROM_FEET(cpos.x) * xpf,apos.lon);
  cpos.y = FN_ARCS_FROM_FEET(cpos.y) + apos.lat;
  cpos.x = LongitudeDifference(cpos.x,Org.lon);
  cpos.y = cpos.y - Org.lat;
  return;
}
//-----------------------------------------------------------------------------------------
//  Rebuild the light system for the requested runway
//-----------------------------------------------------------------------------------------
void CAptObject::RebuildLight(CRunway *rwy)
{	CRwyGenerator(rwy,this,3);
  return;
}
//-----------------------------------------------------------------------------------------
//  Draw a sphere at airport center for test 
//-----------------------------------------------------------------------------------------
void CAptObject::DrawCenter()
{//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
  SVector *v = globals->tcm->GetScale();
  glPushMatrix();
  glPushAttrib(GL_ALL_ATTRIB_BITS);
  glShadeModel(GL_SMOOTH);
  glEnable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
  glFrontFace(GL_CCW);
  glScaled((1/v->x),(1/v->y),1);
//  gluSphere(sphere,100,32,32);
  glPopAttrib();
  glPopMatrix();
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
  return;
}
//---------------------------------------------------------------------------------
//  For debug purpose Draw current ILS Landing position on Tarmac
//  NOTE: Current transformation must be at airport origin
//---------------------------------------------------------------------------------
void CAptObject::DrawILS()
{ float col[4] = {1,0,0,0.2f};
  LND_DATA *lnd = lnDW;
	if (0 == lnd)								return;
	//if (apm->NotNearest(this))	return;
  //--- Draw a  sphere at Landing point ---------
  SVector v1 = {0,0,0};
  //SPosition *land = ils->GetLandingPoint();
	SPosition *land = &lnd->lndP;
  Offset(*land,v1);
  SVector v2 = {0,0,0};
	SPosition *farp = &lnd->fwdP;
  Offset(*farp,v2);
  glPushMatrix();
  glPushAttrib(GL_ALL_ATTRIB_BITS);
  glShadeModel(GL_FLAT);
  glEnable(GL_DEPTH_TEST);
  glDisable(GL_TEXTURE_2D);
  glEnable(GL_BLEND);
  glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
  glDisable(GL_CULL_FACE);
  glScaled(scale->x,scale->y, 1.0);                 // T1 Scale X,Y to feet coordinate
  glTranslated(ofap.x,ofap.y,ofap.z);               // Camera at Airport origin

  //----------------------------------------------
  glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,col);
  glColor4fv(col);
  glBegin(GL_TRIANGLES);
  glVertex3f(v1.x,v1.y,v1.z);
  glVertex3f(v2.x,v2.y,v2.z);
  glVertex3f(v2.x,v2.y,v1.z);
  glEnd();
  //----------------------------------------------
  glPopAttrib();
  glPopMatrix();
  return;
}
//=========================================================================================
//  CAirportMgr constructor
//  Airport model collects all airports in a given radius and
//  call each airport to build the components for drawing
//=========================================================================================
CAirportMgr::CAirportMgr(TCacheMGR *tm)
{ clock   = 0;
  dbm     = globals->dbc;
  tcm     = tm;
  Dist    = 15.0;                     // Collect airport from 15 nmiles around
	Limit   = Dist;
	//--- Light size ---------------------------------
	lSiz		= 64;
	float   ls = 1;
	GetIniFloat("Sim","AirportLights",&ls);
	lSiz    = 64 * ls;
  //--- Taxi texture scale -------------------------
  PavSize = 50;
  PavArc  = 1 / FN_ARCS_FROM_FEET(PavSize);
	//--- Allocate VBO for band and letters ----------
	int tot	= sizeof(vboBUF);
	glGenBuffers(1,&bVBO);
	glBindBuffer(GL_ARRAY_BUFFER,bVBO);
	glBufferData(GL_ARRAY_BUFFER,tot,vboBUF,GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER,0);
	//--- Load yellow texture -------------------------
	TEXT_INFO txf;
	CArtParser img(TX_HIGHTR);
  strcpy(txf.path,"ART/YLWMARK.BMP");
  img.GetAnyTexture(txf);
	xOBJ	= globals->txw->GetTexOBJ(txf,0,GL_RGBA);
  //--- Aicraft icon -------------------------------
  avion   = new CPicQUAD();
  avion->GetBitmap("ART/PICON.BMP");
  //----Register in globals ------------------------
  globals->apm = this;
  //----Check for No Airport -----------------------
  int NoAP    = 0;
  GetIniVar("Sim", "NoAirport", &NoAP);
  if (NoAP) globals->noAPT++;
	//--- Current location ---------------------------
	nApt				= 0;
	endp				= 0;
  //-----For test. ------------------------------------------------
}
//=========================================================================================
//  Destroy airports
//=========================================================================================
CAirportMgr::~CAirportMgr()
{   globals->apm = 0;
    if (avion)  delete avion;
		if (bVBO)		glDeleteBuffers(1,&bVBO);
		if (xOBJ)		glDeleteTextures(1,&xOBJ);
}
//=========================================================================================
//  Find all airports in the requested radius
//  NOTE: Due to the way the database cache is working, runways for airport are loaded
//        some frame latter after the airport is loaded.  Thus the update process
//        must scan for runway presence after airport are introduced in the aptQ
//=========================================================================================
void CAirportMgr::TimeSlice(float dT)
{	if (globals->noAPT)  return;
	globals->module = "APM TimeSlice";
  CAptObject *apo = 0;
  CAptObject *prv = 0;
  CAirport   *apt = 0;
  char       *fn  = 0;
  //---Update the timers ---------------------------------------
  clock = ++clock & 0x03;
  if (clock)								return;
	if (tcm->MeshBusy())			return;
	//----Update runway for current airports in queue -------------
  for   (apo = aptQ.GetFirst(); apo != 0; apo = aptQ.GetNext(apo))
  { if (apo->BuildAll())		return;
  }
  //----Scan airport queue for Airport leaving the radius ------
  for ( apo = aptQ.GetFirst(); apo != 0; apo = aptQ.GetNext(apo))
      { apt = apo->GetAirport();
				float dst = GetRealFlatDistance(apt);
				apo->SetMiles(dst);
        apo->TimeSlice(dT);
				SaveNearest(apo);									// Save nearest airport
				if (dst > Limit)		apo->Kill();
        if (apo->IsAlive()) continue;
        //-----Destroy airport when out of reach --------------------
				if (apo == nApt)	nApt = 0;				// No more nearest
        apt->SetAPO(0);										// Remove pointer
				endp		= 0;
        prv = aptQ.Detach(apo);
        delete apo;
        apo = (prv)?(prv):(aptQ.GetFirst());
        break;
      }
  //----Scan database for new entry ----------------------------
  apt = 0;
  for (apt = dbm->FindAPTbyDistance(apt,Limit);apt != 0; apt = dbm->FindAPTbyDistance(apt,Limit))
    { if (apt->IsSelected())                continue;   // Already in Queue
      fn  = apt->GetIdentity();
      apo = new CAptObject(this,apt);                   // Create airport object
      apo->SetCamera(cam);                              // Current camera
      aptQ.PutEnd(apo);                                 // Enter new Airport in Queue
    }
  return;
}
//----------------------------------------------------------------------------------
//	Save nearest airport
//----------------------------------------------------------------------------------
void CAirportMgr::SaveNearest(CAptObject *apo)
{	float dis = apo->GetNmiles();
	if (nApt && nApt->GetNmiles() < dis)	return;	// Still to far
	if (nApt	== apo)											return;
	//--- Have a new nearest  airport ----------------------------------
	if (nApt)	nApt->ClearTaxiways();						// taxiway no more needed
	nApt	= apo;																// New candidate
	apo->LoadTaxiways();												// Load Taxiways
	return;
}
//----------------------------------------------------------------------------------
//	Check if we are at airport defined by key
//----------------------------------------------------------------------------------
bool CAirportMgr::AreWeAt(char *key)
{	if (0 == nApt)											return false;
	CAirport *apt = nApt->GetAirport();
	if (strcmp(apt->GetKey(),key) == 0)	return true;
	return false;
}
//----------------------------------------------------------------------------------
//	Position aircraft at the runway threshold
//----------------------------------------------------------------------------------
LND_DATA *CAirportMgr::SetOnRunway(CAirport *apt,char *idn)
{	LND_DATA *rwd = 0;
	CAirport *dep = (nApt)?(nApt->GetAirport()):(0);
	if (apt)	dep = apt;
	if (0 == dep)		return 0;
	//------------------------------------------------
	float rot =     dep->GetTakeOffSpot(idn,&tko,&rwd);
  if (0 == tko)		return 0;
	CAirplane *pln = globals->pln;
  if (0 == pln)		return 0;
	//--- Teleport on runway with good orientation---
	SVector ori   = pln->GetOrientation();
	ori.z					= DegToRad(rot);
	ori.x					= 0;
	ori.y					= 0;
	globals->sit->ShortTeleport(tko,&ori);
	return rwd;
}
//----------------------------------------------------------------------------------
//	Return the nearest direction 
//----------------------------------------------------------------------------------
bool CAirportMgr::GetTakeOffDirection(SPosition **opp,SPosition *p)
{	CAirport *dep = (nApt)?(nApt->GetAirport()):(0);
	if (0 == dep)			return false;
	LND_DATA *ils = dep->GetNearestRwyEnd(p,opp);
	if (0 == ils)			return false;
	return true;
}
//----------------------------------------------------------------------------------
//	Return the nearest airport Key
//----------------------------------------------------------------------------------
char *CAirportMgr::NearestIdent()
{	if (0 == nApt)	return "----";
	return nApt->GetAirport()->GetIdent(); }
//----------------------------------------------------------------------------------
//	Bind buffer
//----------------------------------------------------------------------------------
void CAirportMgr::bindLETTERs()
{	glBindBuffer(GL_ARRAY_BUFFER,bVBO);
	glVertexPointer  (3,GL_FLOAT,sizeof(F3_VERTEX),0);
	return;
}
//----------------------------------------------------------------------------------
//  Draw Airports
//  pos is aircraft position
//	Camera is at T1:  Scaled for arsec conversion to ffet
//----------------------------------------------------------------------------------
void CAirportMgr::Draw(SPosition pos)
{	//--- Prepare client state ----------------------------------------------
  glPushClientAttrib (GL_CLIENT_ALL_ATTRIB_BITS);
  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	//--- Draw airports ----------------------------------------------------
	CAptObject *apo = 0;
  for (apo = aptQ.GetFirst(); apo != 0; apo = aptQ.GetNext(apo))
  { apo->SetAircraft(pos);
    apo->Draw();
  }
	//--- Restore state ----------------------------------------------------
  glPopClientAttrib();
  return;
}

//----------------------------------------------------------------------------------
//  Draw Airports Lights
//----------------------------------------------------------------------------------
void CAirportMgr::DrawLights()
{ if (globals->noAPT)       return;
  CAptObject *apo = 0;
  glEnable(GL_TEXTURE_2D);
  for (apo = aptQ.GetFirst(); apo != 0; apo = aptQ.GetNext(apo))
  { if (apo->HasDILS())	apo->DrawILS();
    if (apo->NotVisible())  continue;
    apo->DrawLights(cam);
  }
  glDisable(GL_TEXTURE_2D);
  return;
}

//============================END OF FILE =================================================
