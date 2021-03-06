/*
 * TAXIWAY.h
 *
 * Part of Fly! Legacy project
 *
 * Copyright 2003-2005 Chris Wallace
 * Copyright 2008      Jean Sabatier
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
#ifndef EXPORT_H
#define EXPORT_H

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


#include "../Include/FlyLegacy.h"
//============================================================================
class C_QTR;
class CAptObject;
class CAirport;
class CPaveQ;
class CPolyShop;
class C_TRN;
class SqlMGR;
//============================================================================
//	Export M3D models
//============================================================================
#define EXP_MSG1 0
#define EXP_INIT 1
#define EXP_FBIN 2
#define EXP_NBIN 3
#define EXP_WBIN 4
#define EXP_FSMF 5
#define EXP_NSMF 6
#define EXP_WSMF 7
#define EXP_END  8
#define EXP_OUT  9
#define EXP_RLAX 10
//============================================================================
//	Export TRN files
//============================================================================
#define EXP_TRN_INIT  1	
#define EXP_TRN_MOUNT	2										// Initial mounting sceneries
#define EXP_TRN_FFILE 3										// First file
#define EXP_TRN_NFILE 4										// Next file
#define EXP_TRN_WRITE 5										// Export one file
#define EXP_TRN_WTEXT 6										// Process texture
#define EXP_TRN_COMPR 7										// Compress supertile
#define EXP_TRN_EXIT	8
//============================================================================
//	Export OBJ files
//============================================================================
#define EXP_OBJ_INIT	1						
#define EXP_OBJ_MOUNT 2										// Initial mounting
#define EXP_OBJ_FFILE 3										// First file
#define EXP_OBJ_NFILE	4										// Next file
#define EXP_OBJ_WRITE 5										// Write OBJ
#define EXP_OBJ_EXIT	6										// Exit
//============================================================================
//	Export TXY files
//============================================================================
#define EXP_TXY_MOUNT (1)
#define EXP_TXY_1SKIP (2)
#define EXP_TXY_PFILE (3)

//============================================================================
//	IMPORT Airport Objects
//============================================================================
#define IMP_APT_INIT  (1)
//---------------------------------------------------------------
#define EXP_MNOT	0
#define EXP_MW3D	1												// Export M3D models
#define EXP_TRNF	2												// Export TRN files
#define EXP_WOBJ	3												// Export world objects
#define IMP_APTO  4												// Import airport objects
#define EXP_TAXI	5												// Export Taxiways
//============================================================================
//	Texture parameters 
//============================================================================
#define RAW_TEX_MARGE (8)
#define RAW_TEX_HSIZE (240)
//============================================================================
//  Class to export DATA for SQL Database
//============================================================================
class CExport {
  typedef void(CExport::*partFN)(M3D_PART_INFO &pif);  // Part function
	PFS    *pfs;
	SqlMGR *sqm;
	U_INT   rowid;
  //-----ATTRIBUTES ---------------------------------------------
	char	stop;
  char	Mode;
	int		State;														// State routine
	U_INT	count;														// Export count
	char	Clear;														// Clear request
  TEXT_INFO  inf;                         // Texture info
  M3D_PART_INFO pif;                      // Part info
	SQL_DB   *DB;														// Curent database
	//--- Stream file ---------------------------------------------
	CStreamFile sf;
	CFuiManager *fui;
  //-------------------------------------------------------------
  char gen;                               // Export indicator
  char elv;                               // Export ELV
  char sea;                               // Export SEA
  char txy;                               // Export taxiway
  char m3d;                               // Export model3D
  char gtx;                               // Generic texture
  char wob;                               // World Objects
	char trn;																// Export TRN
  char Sep[8];                            // Separator
	char edt[PATH_MAX];
	char *Pod;															// POD file
  //----3D Model control -----------------------------------------
  char *mName;                            // Model Name
  char  rStat;                            // State to restart
  char  mRed;                             // Reduction ON
  U_INT mCnt;                             // Model count
  U_INT face;                             // Face counter
  U_INT minp;                             // Minimum to keep
  CPolyShop *Polys;                       // PolyShop 
  C3Dmodel  *Mod;                         // Model
	//--- Used by all ----------------------------------------------
	char *fName;														// File name
	char  Key[8];														// Ident Key
	//--- TRN control ----------------------------------------------
	char			eof;													// End of file
	char		 *pod;													//
	char			podN[PATH_MAX];								// Pod name
	char			Root[PATH_MAX];								// root name
	U_INT			Lut[256];											// Gamma table
	TEXT_INFO supx;													// Output texture
	C_STile  *asp;
	U_INT			stqx;													// QGT  start X
	U_INT     stqz;													// QGT  Start Z
	//--- TRN parameters -------------------------------------------
	CTextureDef *texQ;											// Texture Queue
	int   noTX;															// Texture No
  //----Generic parameters ---------------------------------------
	U_INT qKey;															// QGT key
  U_INT bx;                               // Base QGT X
  U_INT bz;                               // Base QGT Z
  U_INT qx;                               // QGT X
  U_INT qz;                               // QGT Z
	U_INT	gx;																// Global Tile X
	U_INT	gz;																// Gloabl tile Z
  U_INT ax;                               // Tile X
  U_INT az;                               // Tile Z
	U_INT kx;																// Key X
	U_INT	kz;																// Key Z
	U_INT	sx;																// SUPER TILE X
	U_INT	sz;																// SUPER TILE Z
	U_INT dx;																// Detail X
	U_INT	dz;																// Detail Z
	U_INT	sno;															// Supertile number
  //----SEA Values -----------------------------------------------
  int   row;
  int   col;
  int   qdr;
  int   total;
  COAST_REC rec;
  //---Taxiway values --------------------------------------------
  CAptObject *apo;                        // Airport Object
  CAirport   *apt;                        // Airport record
  //--------------------------------------------------------------
  U_INT noRec;                            // Record number
  C_QTR *qtr;                             // QTR file
  int    qNo;                             // QTR index
	C_TRN	*ftrn;														// TRN file
  //---Resources to free after usage ----------------------------
  CCamera    *Cam;                        // Camera
  //-------------------------------------------------------------
public:
  CExport();
 ~CExport();
  bool  NoDatabase(CDatabase *db);
  void  ExportGenericAsCVS();
  //-------------------------------------------------------------
  void  ListAirport(U_INT nt,ClQueue &qhd);
  void  ListNavaid (U_INT nt,ClQueue &qhd);
  void  ListILS    (U_INT nt,ClQueue &qhd);
  void  ListCOM    (U_INT nt,ClQueue &qhd);
  void  ListWPT    (U_INT nt,ClQueue &qhd);
  //----EXPORT ROUTINES     ------------------------------------
  void  ExportAirportsAsCVS();
  void  ExportRunwaysAsCVS();
  void  ExportNavaidsAsCVS();
  void  ExportILSAsCVS();
  void  ExportComAsCVS();
  void  ExportWptAsCVS();
  void  ExportCtyAsCVS();
  void  ExportStaAsCVS();
  //-----GENERIC NAVIGATION OBJECTS----------------------------
  void  ExportAPT(U_INT gx,U_INT gz,char *sep);
  void  ExportNAV(U_INT gx,U_INT gz,char *sep);
  void  ExportILS(U_INT gx,U_INT gz,char *sep);
  void  ExportCOM(U_INT gx,U_INT gz,char *sep);
  void  ExportWPT(U_INT gx,U_INT gz,char *sep);
  void  ExportCTY(char *sep);
  void  ExportSTA(char *sep);
  //----COAST DATA---------------------------------------------
  void  ExportCoastInDB();
  void  ExportCoastFile(char *name,int gx,int gz);
  COAST_VERTEX *CoastCount(COAST_VERTEX *pol);
  bool  NextCoastKey();
  //----3D MODELS ---------------------------------------------
  void  InitModelPosition();
  void  DrawModel();
  void  Export3Dmodels();
  bool  Prepare3Dmodel(char *fn,char opt);
  void  WriteTheModel();
  void  OneM3DPart(M3D_PART_INFO &pif);
  void  Export3DMlodQ(char * name, int n);
  void  M3DMsgIntro();
  void  CloseSceneries();
  int   ExecuteMOD();
  void  KeyW3D(U_INT key,U_INT mod);
	int	  Pause();
	//--- Update 3D models -------------------------------------
	void	Update3DModels();
	void	LoadUpdModel(char *name);
	void	UpdateTheModel(char *name);
  //-----TAXIWAYS---------------------------------------------
	void	ErrorTXY(char *msg);
	void	ProcessTXY();
	void	NoticesTXY();
  void  ExportTaxiways();
	void	FormatAptKey(char *fn);
	int		ExecuteTXY();
  //void  ExportOneTMS(CAirport *apt);
  CPaveRWY  *BuildPave(CPaveQ *qhd);
  CBaseLITE *BuildLite(CLiteQ *qhd,U_CHAR col);
  void  ExportPaveQ(char *key);
  void  ExportEdgeQ(char *key);
  void  ExportCentQ(char *key);
  void  ExportLiteQ(char *key,U_CHAR col);
  //------GENERIC TEXTURES -----------------------------------
  void  ExportGenTextures();
  void  DecodeTexture(char *name);
  void  WriteMskTexture(char *name,int dim);
  void  WriteRwyTexture(char *name,char seg,char tsp);
  void  WritePNGTexture(char *name,char tsp);
	//------COMPRESSED TEXTURES ---------------------------------
	void  CompressTextures();
	void	ScanDirectory(int x, int z);
  //-----------------------------------------------------------
  int   TimeSlice(float dT);
  void  Keyboard(U_INT key,U_INT mod);
  //----Scenery OBJECTS --------------------------------------------
	void  ExportSceneryOBJ();
	int		ExecuteOBJ();
	void	InitOBJmsg();
	void	GetFirstOBJ();
	void	GetNextOBJ();
	void	WriteOBJ();
	void	BuildOBJname(char *fn);
	void	ExportOBJ(char *fn);
  void  WriteQgtOBJ();
	void	CheckSceneryFiles();
	void	CheckThisFile(char *fn);
	//--- SCENERY FILES (TRN) -----------------------------------
	int		ExecuteTRN();
	void	ExportAllTRNs();
	void	InitTRNmsg();
	bool	BuildTRNname(char *fn);
	int 	GetFirstTRN();
	int		GetNextTRN();
	bool	BuildTRNdata(char *fn);
	void	WriteTRNelevations();
	void	ImportSUPelevations(C_STile *asp);
	bool	LoadSuperTile();
	//------------------------------------------------------------
	bool  FileInDatabase(SQL_DB &db);
	//------------------------------------------------------------
	bool	NextSupertile();
	int	  WriteTRNtextures();
	int		CompressTRNtexture();
	void	CompressTexture(CTextureDef *txd,U_INT ax,U_INT az);
	void	WriteDayTexture(CTextureDef *txd);
	//------------------------------------------------------------
	void	ReadRawTexture(CTextureDef *txd);
	void	ExtractTexture(U_CHAR res);
	//------------------------------------------------------------
	void  PrepareReading(CTextureDef *txd);
	void	ErrorTRN01();
	void	WarnTRN();
};
//============================END OF FILE =================================================
#endif  // EXPORT_H
