/*
 * Pod.cpp
 *
 * Part of Fly! Legacy project
 *
 * Copyright 2003-2005 Chris Wallace
 * Copyright 2007-2008 Jean Sabatier
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
 */

//==================================================================================
//  This interface the simulator DATABASE
//==================================================================================
#include "../Include/Globals.h"
#include "../Include/Endian.h"
#include "../Include/Utility.h"
#include "../Include/SqlMGR.h"
#include "../Include/Database.h"
#include "../Include/FuiParts.h"
#include "../Include/Export.h"
#include "../Include/Import.h"
#include "../Include/Pod.h"
#include "../Include/TerrainElevation.h"
#include "../Include/TerrainCache.h"
#include "../Include/TerrainTexture.h"
#include "../Include/Airport.h"
#include "../Include/LightSystem.h"
#include "../Include/PlanDeVol.h"
#include "../Include/3dMath.h"
#include "../Include/ScenerySet.h"
//-------------------------------------------------------------------------
#include <io.h>
//=============================================================================
//	Transform index [0-16] to column index in SUpertile
//=============================================================================
char colInSUP[] = {'0','1','2','3', '0','1','2','3', '0','1','2','3', '0','1','2','3'};
//=============================================================================
//	Transform index [0-16] to row index in SUpertile
//=============================================================================
char rowInSUP[] = {'0','0','0','0', '1','1','1','1', '2','2','2','2', '3','3','3','3'};

//==================================================================================
//  Database request to compile
//==================================================================================
char   *AttachWPT =  "ATTACH DATABASE 'SQL/WPT.db' AS WPT;";
char   *AttachELV =  "ATTACH DATABASE 'SQL/ELV.db' AS ELV;";

SQL_REQ SelAPT_DB = {"SELECT * FROM APT WHERE tile == %%%%%%%% ;*"};
SQL_REQ SelRWY_DB = {"SELECT * FROM RWY WHERE rapt == \"%%%%%%%%%%\";*"};
SQL_REQ SelNAV_DB = {"SELECT * FROM NAV WHERE tile == %%%%%%%% ;*"};
SQL_REQ SelILS_DB = {"SELECT * FROM ILS WHERE iapt == \"%%%%%%%%%%\";*"};
SQL_REQ SelCOM_DB = {"SELECT * FROM COM WHERE tile == %%%%%%%% ;*"};
//=================================================================================
//  Global call back
//=================================================================================
//----Enter COM to Cache -----------------------------------
void COMtoCache(CmHead *apt,CTileCache *tc)
  {globals->dbc->ComToCache(apt,tc);}
//----Enter Airport in Cache -------------------------------
void APTtoCache(CmHead *apt,CTileCache *tc)
  {globals->dbc->AptToCache(apt,tc);}
//----Enter Object to Cache --------------------------------
void OBJtoCache(CmHead *obj,CTileCache *tc)
  {globals->dbc->ObjToCache(obj,tc);}
//----Runway entering cache --------------------------------
void RWYtoCache(CRunway *rwy,CTileCache *tc)
  {globals->dbc->RwyToCache(rwy,tc);}
//----------------------------------------------------------------------------
//  Call back to set elevation into a QGT (from sql THREAD)
//---------------------------------------------------------------------------
//  Set Region Unique Elevation (one for the region)
//  Region is a square whose base det is the SW corner
//---------------------------------------------------------------------------
void SetUnicElevation(REGION_REC &reg)
{ int   tx = reg.dtx;
  int   tz = reg.dtz - 1;
  float el = float(reg.val);
  for   (U_INT z = 0; z < reg.lgz; z++)
  { tz++;
    tx   = reg.dtx;
    for (U_INT x = 0; x < reg.lgx; x++)
    { reg.qgt->DetailElevation(tx,tz,el);
      tx++;
    }
  }
  return;
}
//---------------------------------------------------------------------------
//  Set Region mono Elevation (one per detail tile center)
//  Region is a square whose base det is the SW corner
//---------------------------------------------------------------------------
void SetMonoElevation(REGION_REC &reg)
{ int   *src = reg.data;
  int     tx = reg.dtx;
  int     tz = reg.dtz - 1;
  for   (U_INT z = 0; z < reg.lgz; z++)
  { tz++;
    tx   = reg.dtx;
    for (U_INT x = 0; x < reg.lgx; x++)
    { float el = *src++;
      reg.qgt->DetailElevation(tx,tz,el);
      tx++;
    }
  }
  return;
}
//---------------------------------------------------------------------------
//  Set Region multi Elevation (one per detail tile center)
//  Region is a square whose base det is the SW corner
//	Used by import eleveation
//---------------------------------------------------------------------------
void SetMultElevation(REGION_REC &reg)
{ TRN_HDTL  hdt;
  U_INT     ftx = reg.dtx + reg.lgx;
  U_INT     ftz = reg.dtz + reg.lgz;
  int       inc = reg.sub * reg.sub;
  hdt.SetArray(reg.data);
  for   (U_INT z=reg.dtz; z<ftz; z++)
  { for (U_INT x=reg.dtx; x<ftx; x++)
    { hdt.SetTile(x,z);
			hdt.SetDIM(reg.sub);
			hdt.SetRES(reg.sub - 1);
      reg.qgt->DivideHDTL(&hdt);
			hdt.IncArray(inc);
    }
  }
  return;
}
//---------------------------------------------------------------------------
//  Set Region Elevation
//  Region is a square whose base det is the SW corner
//---------------------------------------------------------------------------
void ELVtoCache(REGION_REC &reg)
{ if (EL_MULT == reg.type)  return SetMultElevation(reg);
  if (EL_UNIC == reg.type)  return SetUnicElevation(reg);
  if (EL_MONO == reg.type)  return SetMonoElevation(reg);
  return;
}
//---------------------------------------------------------------------------
//  Set Region Elevation
//  to import
//---------------------------------------------------------------------------
void ELVtoSlots(REGION_REC &reg)
{ globals->imp->ElevationFromREG(reg);
  return;
}
//==================================================================================
//
//  SQL OBJ MANAGER
//
//==================================================================================
//-----------------------------------------------------------------------------
//  Init SQL object 
//-----------------------------------------------------------------------------
SqlOBJ::SqlOBJ()
{	qgt		= 0;
	sup		= 0;
}
//-----------------------------------------------------------------------------
//  Close all open bases
//	NOTE: Database structures with fix attribut are not destroyed 
//-----------------------------------------------------------------------------
SqlOBJ::~SqlOBJ()
{	std::map<std::string,SQL_DB*>::iterator rb;
	for (rb = dbase.begin(); rb != dbase.end(); rb++)
	{	SQL_DB *db = (*rb).second;
		sqlite3_close(db->sqlOB);
		if (0 == db->fix)	delete db;
	}
	dbase.clear();
}
//-----------------------------------------------------------------------------
//  Initialize databases
//-----------------------------------------------------------------------------
void SqlOBJ::Init()
{ int   lgr  = (MAX_PATH - 1);
  char *prm  = "None";
  //---Generic database ---------------------------------------------
	genDBE.fix  = 1;
	genDBE.vers	= 0;																// Minimum version
  strncpy(genDBE.path,"SQL",63);
  GetIniString("SQL","GENDB",genDBE.path,lgr);
	strncpy(genDBE.name,"GEN*.db",63);
  genDBE.mgr	= SQL_MGR;
  strncpy(genDBE.dbn,"Generic files",32);
	genDBE.mode = SQLITE_OPEN_READONLY;
	genDBE.param	= prm;
  //---Waypoint database -------------------------------------------
	wptDBE.fix	= 1;
	wptDBE.vers	= 0;																// Minimum version
  strncpy(wptDBE.path,"SQL",63);
  GetIniString("SQL","WPTDB",wptDBE.path,lgr);
	strncpy(wptDBE.name,"WPT*.db",63);
  wptDBE.mgr = SQL_MGR;
  strncpy(wptDBE.dbn,"Waypoints",32);
	wptDBE.mode		= SQLITE_OPEN_READONLY;
	wptDBE.param	= "ExpWPT";
  //---Elevation database ------------------------------------------
	elvDBE.fix	= 1;
	elvDBE.vers	= 7;																// Minimum version
  strncpy(elvDBE.path,"SQL",63);
  GetIniString("SQL","ELVDB",elvDBE.path,lgr);
	strncpy(elvDBE.name,"ELV*.db",63);
  elvDBE.mgr	= SQL_THR | SQL_MGR; 
  strncpy(elvDBE.dbn,"Elevation Data",32);
	elvDBE.mode		= SQLITE_OPEN_READWRITE;
	elvDBE.param	= "ExpELV";
  //---Coast data database -----------------------------------------
	seaDBE.fix	= 1;
	seaDBE.vers	= 1;																// Minimum version
  strncpy(seaDBE.path,"SQL",63);
  GetIniString("SQL","SEADB",seaDBE.path,lgr);
	strncpy(seaDBE.name,"SEA*.db",63);
  seaDBE.mgr	= SQL_THR;
  strncpy(seaDBE.dbn,"Coast data",32);
	seaDBE.mode		= SQLITE_OPEN_READONLY;
	seaDBE.param  = "ExpSEA";
  //---Taxiways database ------------------------------------------
	txyDBE.fix	= 1;
	txyDBE.vers	= 1;																// Minimum version
  strncpy(txyDBE.path,"SQL",63);
  GetIniString("SQL","TXYDB",txyDBE.path,lgr);
	strncpy(txyDBE.name,"TXY*.db",63);
  txyDBE.mgr = SQL_MGR;
  strncpy(txyDBE.dbn,"Taxiway data",32);
	txyDBE.mode		= SQLITE_OPEN_READWRITE;
	txyDBE.param	= "ExpTXY";
  //---Model 3D database ------------------------------------------
	modDBE.fix	= 1;
	modDBE.vers	= 2;																// Minimum version
  strncpy(modDBE.path,"SQL",63);
  GetIniString("SQL","M3DDB",modDBE.path,lgr);
	strncpy(modDBE.name,"M3D*.db",63);
  modDBE.mgr =  SQL_THR;
  strncpy(modDBE.dbn,"Model3D data",32);
	modDBE.mode		= SQLITE_OPEN_READONLY;
	modDBE.param	= "ExpM3D";
  //---Generic textures database ----------------------------------
	texDBE.fix	= 1;
	texDBE.vers	= 0;																// Minimum version
  strncpy(texDBE.path,"SQL",63);
  GetIniString("SQL","TEXDB",texDBE.path,lgr);
	strncpy(texDBE.name,"TEX*.db",63);
  texDBE.mgr =  SQL_THR + SQL_MGR;
  strncpy(texDBE.dbn,"Generic Textures",32);
	texDBE.mode		= SQLITE_OPEN_READONLY;
	texDBE.param	= "ExptEX";
  //---World Object database --------------------------------------
	objDBE.fix	= 1;
	objDBE.vers	= 3;																// Minimum version
  strncpy(objDBE.path,"SQL",63);
  GetIniString("SQL","OBJDB",objDBE.path,lgr);
	strncpy(objDBE.name,"OBJ*.db",63);
  objDBE.mgr	=  SQL_MGR;
  strncpy(objDBE.dbn,"World Objects",32);
	objDBE.mode		= SQLITE_OPEN_READWRITE;
	objDBE.param	= "ExpOBJ";
  return;
}
//-----------------------------------------------------------------------------
//  Create texture database path and name name
//-----------------------------------------------------------------------------
void SqlOBJ::CreateCompressedDBname(char *pn, char *fn, U_INT qx, U_INT qz)
{	//---- Compute globe area indices (divide by 8) -----
  glx = (qx >> 3);								// AREA x indice
	glz = (qz >> 3);								// AREA z indice
	//--- Compute base globe tile of area ---------------
	U_INT bx	= (glx << 2);						// GTx indice
	U_INT bz	= (glz << 2);						// GTz indice
	//--- Format path ----------------------------------
	strncpy(pn,"DTX",32);									// Default path
  GetIniString("SQL","DTXDB",pn,(MAX_PATH - 1));
	_snprintf(fn,63,"T%03d_%03d.db",bx,bz);
	return;
}
//-----------------------------------------------------------------------------
//  Open export texture database
//-----------------------------------------------------------------------------
int SqlOBJ::OpenDTX()
{ if (sqlTYP != SQL_MGR)						return 0;
	if (!HasIniKey("SQL","ExpTRN"))		return 0;
	//---------Check for export TRN files ------------------
	char prm[128];
	int x,z;
  GetIniString("SQL","ExpTRN",prm,127);
	int nf = sscanf(prm,"QGT ( %d - %d ) %s ",&x,&z);
	if  (2 != nf)				gtfo("Use TRN=(x-y)");
	//--- Buid name and set mode ------------------------
	CreateCompressedDBname(dtxDBE.path,dtxDBE.name,x,z);
	ImportConfiguration(dtxDBE.name);
	elvDBE.exp |= 1;
	elvDBE.mgr = SQL_MGR;
	//--- Compression requested ---------------------------
	if (0 == globals->dxt5Supported)	gtfo("Compression not supported");
	//--- Init the dtx dabase for import ------------------
	dtxDBE.param	= "ExpTRN";
	dtxDBE.fix	= 1;
	dtxDBE.vers	= 0;																// Minimum version
  dtxDBE.mgr	=  SQL_THR + SQL_MGR;
  strncpy(dtxDBE.dbn,"TRN Texture",32);
	dtxDBE.exp = 1;
	//--- Open the database ------------------------------
	int use = Open(dtxDBE);
	if (0 == use)											gtfo("Cant open database %s",dtxDBE.name); 
	//----------------------------------------------------
	dtxDBE.mgr = SQL_MGR;
	dtxDBE.exp |= 1;
	return 0;
}
//=============================================================================
//  Open all fixed database
//	NOTE: The global mux prevent reentrency problem in SQLITE
//=============================================================================
void SqlOBJ::OpenBases()
{ globals->import = 0;
	pthread_mutex_lock(&globals->mux);
	//---Open Generic database -----------------------------------------
  Open(genDBE);
  //---Open Taxiway database -----------------------------------------
  Open(txyDBE);
  //---Open Elevation database ---------------------------------------
  Open(elvDBE);
  //---Open 3D model database ----------------------------------------
  Open(modDBE);
  //---Open Sea database ---------------------------------------------
  Open(seaDBE);
  //---Open texture database -----------------------------------------
  Open(texDBE);
  //---Open World Object database ------------------------------------
  Open(objDBE);
	//---Open Texture 2D database for export ---------------------------
	pthread_mutex_unlock(&globals->mux);
  return;
}
//-----------------------------------------------------------------------------
//		Import configuration
//-----------------------------------------------------------------------------
void SqlOBJ::ImportConfiguration(char *fn)
{	globals->fui->ExportMessage(fn);
	globals->noEXT	= 7;
	globals->noINT	= 7;
	globals->Disp.Lock(PRIO_UPLANE);
	globals->noAPT  = 7;
	globals->noOBJ  = 7;
	globals->noOSM	= 7;
	globals->import = 1;
	return;
}
//-----------------------------------------------------------------------------
//  Open the requested database
//  A database is opened if
//  -The current manager match the allowed manager (THREAD or MAIN)
//   or the database should be open for export by hte SQL manager (main)
//  
//-----------------------------------------------------------------------------
int SqlOBJ::Open(SQL_DB &db)
{ //--- Process  export flags -------------------------
	if (HasIniKey("SQL",db.param))	genDBE.mgr = SQL_MGR;
	//--- Try to Open the database ----------------------
	char fnm[MAX_PATH];
  int lgr = (MAX_PATH-1);
	if (db.exp)	db.mode = SQLITE_OPEN_READWRITE;
	if (0 == db.use)		return 0;
	//--- Check manager right ---------------------------
	U_CHAR mgr = db.mgr & sqlTYP;
	if (0 == mgr)				return 0;
	//--- Check if file exist ---------------------------
	_snprintf(fnm,lgr,"%s/%s",db.path,db.name);
	db.path[lgr] = 0;
	_finddata_t fileinfo;
	strcpy(fileinfo.name,"NONE");
	intptr_t h1 = _findfirst(fnm,&fileinfo);
	_snprintf(fnm,lgr,"%s/%s",db.path,fileinfo.name);
	fnm[lgr] = 0;
	strncpy(db.path,fnm,FNAM_MAX);
	//---------------------------------------------------
  int rep = sqlite3_open_v2(fnm,  &db.sqlOB,db.mode,0 );
  if (rep)  WarnE(db);
  else      Warn1(db);
  db.use  = (db.opn == 1) && (db.exp == 0); 
	_findclose(h1);
	//--- Warn fui manager ----------------------------------
	if (db.exp) ImportConfiguration(fnm);
	//--- Now check for minimum version ---------------------
	if (0 == db.opn)		return 0;			// Not open
	db.use = ReadVersion(db);
	if (0 == db.use)		return 0;
	//--- Now enter the database list -----------------------
	dbase[db.path] = &db;
  return db.use;
}

//-----------------------------------------------------------------------------
//  Warning log
//-----------------------------------------------------------------------------
void SqlOBJ::WarnE(SQL_DB &db)
{ char *sql = (SQL_THR == sqlTYP)?("THREAD"):("MAIN ");
  TRACE("%s SQL Database %s MSG %s",sql,db.path,(char*)sqlite3_errmsg(db.sqlOB));
  TRACE("%s SQL Using POD for %s",sql,db.dbn);
  return;
}
//-----------------------------------------------------------------------------
//  Warning log
//-----------------------------------------------------------------------------
void SqlOBJ::Warn1(SQL_DB &db)
{ db.opn = 1;
  char *sql = (SQL_THR == sqlTYP)?("THREAD"):("MAIN ");
  TRACE("%s Using SQL %s for %s",sql,db.path,db.dbn);
  return;
}

//=============================================================================
//  Compile a Statement for a given database
//=============================================================================
 sqlite3_stmt *SqlOBJ::CompileREQ(char *req,SQL_DB &db)
{ sqlite3_stmt *stm = 0;
  const char   *end;
  int rep = sqlite3_prepare_v2(
            db.sqlOB,          // Database handle 
            req,               // SQL statement, UTF-8 encoded 
            -1,                // Maximum length of zSql in bytes. 
            &stm,              // OUT: Statement handle 
            &end               // OUT: Pointer to unused portion of zSql 
          );
  return stm;
}
//-----------------------------------------------------------------------------
//  ABORT for error
//-----------------------------------------------------------------------------
void SqlOBJ::Abort(SQL_DB &db)
{ gtfo("SQL Database %s error %s",db.path,(char*)sqlite3_errmsg(db.sqlOB));
  return;
}
//-----------------------------------------------------------------------------
//  Set a warning in log and close
//-----------------------------------------------------------------------------
void SqlOBJ::Warn2(SQL_DB &db,int er)
{	STREETLOG("Error code %d in database %s",er, db.path);
	db.use = 0;
	return;
}
//==============================================================================
// Read version
//==============================================================================
int SqlOBJ::ReadVersion(SQL_DB &db)
{ int  vers	  = -1;
	char *query = "PRAGMA user_version;*";
  sqlite3_stmt *stm = CompileREQ(query,db);
	if (SQLITE_ROW == sqlite3_step(stm)) vers = sqlite3_column_int(stm,0);
	sqlite3_finalize(stm);                      // Close statement

	if (vers >= db.vers)		return 1;
	//--- warning ------------------------------------------
	WARNINGLOG("Database %s obsolete. Get version %d",db.path,db.vers);
	sqlite3_close(db.sqlOB);
	return 0;
}
//==============================================================================
// Set Page size
//==============================================================================
void SqlOBJ::SetPageSize(int dim,SQL_DB &db)
{ char req[1024];
	_snprintf(req,1023,"PRAGMA page_size = %d;*",dim);
  sqlite3_stmt *stm = CompileREQ(req,db);
	bool ok = (SQLITE_DONE == sqlite3_step(stm));
	sqlite3_finalize(stm);                      // Close statement
	return;
}

//==============================================================================
//  ELEVATION DATABASE
//==============================================================================
void SqlOBJ::DecodeREG(sqlite3_stmt *stm,REGION_REC &reg)
{ int det   = sqlite3_column_int(stm,CLN_REG_DET);
  reg.dtx   = (det >> 16);
  reg.dtz   = (det  & 0x0000FFFF);
  reg.lgx   = sqlite3_column_int(stm,CLN_REG_LGX);
  reg.lgz   = sqlite3_column_int(stm,CLN_REG_LGZ);
  reg.sub   = sqlite3_column_int(stm,CLN_REG_SUB);
  reg.type  = sqlite3_column_int(stm,CLN_REG_TYP);
  reg.val   = sqlite3_column_int(stm,CLN_REG_VAL);
  reg.nbv   = 0;
	//--- Check for mono elevation -----------------
	if (EL_UNIC == reg.type)
	{ reg.val &= 0x7FFFFFFF;
		return;
	}
	//--- Read matrix value ------------------------
  ReadElevation(reg);
  return;
}
//==============================================================================
//  Read elevation data
//	Table DET contains elevation matrix for a given region
//==============================================================================
void  SqlOBJ::ReadElevation(REGION_REC &reg)
{ char query[1024];
  _snprintf(query,1024,"SELECT * FROM DET WHERE key = %d;*",reg.val);
  sqlite3_stmt *stm = CompileREQ(query,elvDBE);
  //----Execute select -------------------------------------------
  if (SQLITE_ROW == sqlite3_step(stm))
  { int nbv   = sqlite3_column_int(stm, CLN_HTR_NBE);
    reg.nbv   = nbv;
    int *dst  = new int[nbv];
    reg.data  = dst;
    int *src = (int*)sqlite3_column_blob(stm,CLN_HTR_MAT);
    //---Transfer elevation array of int -------------------------
		for (int k=0; k < nbv; k++) *dst++ = *src++;
  }
  else Abort(elvDBE);
  sqlite3_finalize(stm);                      // Close statement
  return;
}
//==============================================================================
//  Set region elevation for the QGT defined in Reg
//	TABLE reg contains region descriptors ordered per QGT
//==============================================================================
void  SqlOBJ::GetQgtElevation(REGION_REC &reg,ElvFunCB *fun)
{ //----Compute base detail tile key -----------------------------
  char  req[1024];
  _snprintf(req,1024,"SELECT * FROM reg WHERE qgt = %d;*",reg.key);
  sqlite3_stmt *stm = CompileREQ(req,elvDBE);
  reg.data = 0;
  //---Execute select --------------------------------------------
  while (SQLITE_ROW == sqlite3_step(stm))
  { DecodeREG(stm,reg);
    fun(reg);       // Call elevation function
    if (0  == reg.data)   continue;
    delete [] reg.data;
    reg.data = 0;
  }
  sqlite3_finalize(stm);                      // Close statement
  return;
}
//==============================================================================
//  Delete all data for the requested QGT
//==============================================================================
void SqlOBJ::DeleteElvDetail(U_INT key)
{ char req[1024];
  _snprintf(req,1024,"DELETE FROM det where qgt = %d;*",key);
  sqlite3_stmt *stm			= CompileREQ(req,elvDBE);
  int rep = sqlite3_step(stm);
  if (rep != SQLITE_DONE) Abort(elvDBE);
  sqlite3_finalize(stm);  // lc 052310 + Close statement
  return;
}
//--------------------------------------------------------------------
//  Delete region elevation from database
//--------------------------------------------------------------------
void SqlOBJ::DeleteElvRegion(U_INT key)
{ char req[1024];
  _snprintf(req,1024,"DELETE FROM reg where qgt = %d;*",key);
  sqlite3_stmt *stm = CompileREQ(req,elvDBE);
  int rep = sqlite3_step(stm);
  if (rep != SQLITE_DONE) Abort(elvDBE);
  sqlite3_finalize(stm);  // lc 052310 + Close statement
  return;
}
//--------------------------------------------------------------------
//  Delete QGT elevation from database
//--------------------------------------------------------------------
void SqlOBJ::DeleteElevation(U_INT key)
{ DeleteElvDetail(key);
  DeleteElvRegion(key);
  return;
}
//==============================================================================
//  Check for POD-OBJ in Database
//==============================================================================
unsigned long SqlOBJ::FileInBase(char *fn,char *pn,SQL_DB &dbObj)
{	char rq[1024];
	unsigned long row = 0;
	_snprintf(rq,1023,"SELECT key from FNM where file LIKE '%%%s%%%s%%';*",fn,pn);
	sqlite3_stmt *stm = CompileREQ(rq,dbObj);
	bool in = (SQLITE_ROW == sqlite3_step(stm));
	if (in)		row = sqlite3_column_int(stm, 0);
	sqlite3_finalize(stm);                      // Close statement
	return row;
}
//==============================================================================
//  Write POD-TRN in Database
//==============================================================================
int SqlOBJ::WriteFileNameInBase(char *fn,SQL_DB &db)
{	char rq[1024];
	_snprintf(rq,1023,"INSERT INTO FNM (file) VALUES(?1);*");
	sqlite3_stmt *stm = CompileREQ(rq,db);
	//--- Origin file ---------------------------------------
  int rep = sqlite3_bind_text(stm,1,fn,-1,SQLITE_TRANSIENT);
  if (rep != SQLITE_OK) Abort(db);
  //---Execute insert -------------------------------------------------------
  rep      = sqlite3_step(stm);               // Insert value in database
  if (rep != SQLITE_DONE) Abort(db);
	sqlite3_finalize(stm);                      // Close statement
	//--- return rowid --------------------------------------------------------
  unsigned long row = 0;
	_snprintf(rq,1023,"SELECT key from FNM where file = '%s';*",fn);
	stm = CompileREQ(rq,db);
	bool in = (SQLITE_ROW == sqlite3_step(stm));
	if (in)		row = sqlite3_column_int(stm, 0);
	sqlite3_finalize(stm);                      // Close statement
	return row;
}

//==============================================================================
//	Create the database first using script
//==============================================================================
SQL_DB *SqlOBJ::CreateSQLbase(SQL_DB *db,char **S)
{ if (0 == S)	{delete db; return 0; }
	char *fn = db->path;
	db->mode = (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
	int rep  = sqlite3_open_v2(fn,  &db->sqlOB,db->mode,0 );
	//--- Check for any error ----------------------------------
	if (rep != SQLITE_OK)	
	{	WarnE(*db);
		delete db;
		return 0;
	}
	//-- now execute the script uppon the database -------------
	char **tab = S;
	char  *req = 0;
	for (req = *tab; (strcmp(req,"***") != 0); tab++, req = *tab)
	{	sqlite3_stmt * stm = CompileREQ(req,*db);
		int rep = sqlite3_step(stm);
		sqlite3_finalize(stm);					// Close statement
		if (rep == SQLITE_DONE) continue;
		//--- any error: close and exit -----------------
		WARNINGLOG("SQL error %d on %s:",rep,req);
		db->opn = 0;
		db->use = 0;
		sqlite3_close(db->sqlOB);
		delete db;
		return 0;
	}
	//--- Set Create status ---------------------------
	db->opn	= 2;
	db->use = 2;
	return db;
}
//--------------------------------------------------------------------
//  Open requested database (OSM dedicated see TRACE msg)
//	If the file does not exists and script S is supplied*
//	then create the base with script S
//--------------------------------------------------------------------
SQL_DB *SqlOBJ::OpenSQLbase(char *fn,char **S,char *name)
{ SQL_DB *db = 0;
	std::map<std::string,SQL_DB*>::iterator rb = dbase.find(fn);
	if (rb != dbase.end())
	{	db = (*rb).second;
		db->IncUser();
		TRACE("SQL %d-ucnt %d Found database %s",sqlTYP,db->ucnt,db->path);
		return db;
	}
	db = new SQL_DB();
	strncpy(db->dbn,name,31);
	db->dbn[31]	= 0;
	strncpy(db->path,fn,FNAM_MAX);
	db->mode  = SQLITE_OPEN_READWRITE;
	TRACE("SQL %d-ucnt %d Open  database %s",sqlTYP,db->ucnt,db->path);
	//--- Check if file exist ----------------------------
	FILE *pf	= fopen(fn,"r");
	bool  ok		= (pf != 0);
	if (pf)	fclose(pf);
	//--- if no file create DB with script S (optional)----
	if (!ok)	return CreateSQLbase(db,S);
	//--- OK file exist -----------------------------------
	int rep  = sqlite3_open_v2(fn,  &db->sqlOB,db->mode,0 );
  if (rep)  WarnE(*db);
  else      Warn1(*db);
  db->use  = (db->opn == 1);
	dbase[fn] = db;				// Enter database in list
	return db;
}
//==================================================================================
//
//  MAIN SQL MANAGER
//  Open the curent database
//
//==================================================================================
SqlMGR::SqlMGR()
{ sqlTYP    = SQL_MGR;
  char req[1024];
  //--- Resolution parameters ---------------------------------------
  low = 128 * 128 * 4;
  med = 256 * 256 * 4;
  //-----------------------------------------------------------------
	sup				= new C_STile();
  Init();
	//--- Check for TRN import ----------------------------------------
	OpenDTX(); 
  OpenBases();
  //----Locate Statement parameters ----------------------------------
  SelAPT_DB.arg[0] = strchr(SelAPT_DB.txt,'%');
  SelRWY_DB.arg[0] = strchr(SelRWY_DB.txt,'%');
  SelNAV_DB.arg[0] = strchr(SelNAV_DB.txt,'%');
  SelILS_DB.arg[0] = strchr(SelILS_DB.txt,'%');
  SelCOM_DB.arg[0] = strchr(SelCOM_DB.txt,'%');
  //----ATTACH THE WAYPOINT DATABASE---------------------------------
  _snprintf(req,1024, "ATTACH DATABASE '%s' AS WPT;", wptDBE.path);
  if (0 == genDBE.opn)      return;
  sqlite3_stmt *stm = CompileREQ(req,genDBE);
	//--- Execute Statement -------------------------------------------
  int rep = sqlite3_step(stm);
  if (rep != SQLITE_DONE) WARNINGLOG("MAIN SQL Not using WPT");
  else                    WARNINGLOG("MAIN Using SQL %s for WayPoints", wptDBE.path);
  //-----Close request ----------------------------------------------
  sqlite3_finalize(stm);
}
//-----------------------------------------------------------------------------
//  Close all databases
//-----------------------------------------------------------------------------
SqlMGR::~SqlMGR()
{ if (sup) delete sup;
}
//-----------------------------------------------------------------------------
//  Decode Airport from current statement
//-----------------------------------------------------------------------------
CAirport *SqlMGR::DecodeAPT(sqlite3_stmt *stm, OTYPE obt)
{ CAirport  *apt = new CAirport(obt,APT);
  char      *txt = 0;
  double     val = 0;
  txt = (char*)sqlite3_column_text(stm,CLN_APT_UKEY);
  strncpy(apt->akey,txt,10);
  txt = (char*)sqlite3_column_text(stm,CLN_APT_AFAA);
  strncpy(apt->afaa,txt,4);
  txt = (char*)sqlite3_column_text(stm,CLN_APT_AICA);
  strncpy(apt->aica,txt,5);
  txt = (char*)sqlite3_column_text(stm,CLN_APT_ANAM);
  strncpy(apt->name,txt,40);
  txt = (char*)sqlite3_column_text(stm,CLN_APT_ACTR);
  strncpy(apt->actr,txt,3);
  txt = (char*)sqlite3_column_text(stm,CLN_APT_ASTA);
  strncpy(apt->asta,txt,3);
  //--------------------------------------------------------
  apt->atyp = short(sqlite3_column_int(stm,CLN_APT_ATYP));
  apt->apos.lat = sqlite3_column_double(stm,CLN_APT_ALAT);
  apt->apos.lon = sqlite3_column_double(stm,CLN_APT_ALON);
  apt->apos.alt = sqlite3_column_int(stm,CLN_APT_AELE);
  apt->aown = char (sqlite3_column_int(stm,CLN_APT_AOWN));
  apt->ause = char (sqlite3_column_int(stm,CLN_APT_AUSE));
  apt->amag = float(sqlite3_column_double(stm,CLN_APT_AMAG));
  apt->aalt = int  (sqlite3_column_int(stm,CLN_APT_AALT));
  apt->afsa = char (sqlite3_column_int(stm,CLN_APT_AFSA));
  apt->antd = char (sqlite3_column_int(stm,CLN_APT_ANTD));
  apt->aata = char (sqlite3_column_int(stm,CLN_APT_AATA));
  apt->aseg = char (sqlite3_column_int(stm,CLN_APT_ASEG));
  apt->alnd = char (sqlite3_column_int(stm,CLN_APT_ALND));
  apt->amjc = char (sqlite3_column_int(stm,CLN_APT_AMJC));
  apt->afue = short(sqlite3_column_int(stm,CLN_APT_AFUE));
  apt->afrm = char (sqlite3_column_int(stm,CLN_APT_AFRM));
  apt->aeng = char (sqlite3_column_int(stm,CLN_APT_AENG));
  apt->abto = char (sqlite3_column_int(stm,CLN_APT_ABTO));
  apt->ablo = char (sqlite3_column_int(stm,CLN_APT_ABLO));
  apt->alen = char (sqlite3_column_int(stm,CLN_APT_ALEN));
  apt->acft = short(sqlite3_column_int(stm,CLN_APT_ACFT));
  apt->aatf = int  (sqlite3_column_int(stm,CLN_APT_AATF));
  apt->aats = int  (sqlite3_column_int(stm,CLN_APT_AATS));
  apt->aate = int  (sqlite3_column_int(stm,CLN_APT_AATE));
  apt->altf = char (sqlite3_column_int(stm,CLN_APT_ALTF));
  apt->alts = int  (sqlite3_column_int(stm,CLN_APT_ALTS));
  apt->alte = int  (sqlite3_column_int(stm,CLN_APT_ALTE));
  apt->atyc = char (sqlite3_column_int(stm,CLN_APT_ATYC));
  apt->aicn = char (sqlite3_column_int(stm,CLN_APT_AICN));
  apt->anrw = char (sqlite3_column_int(stm,CLN_APT_ANRW));
  apt->alrw = int  (sqlite3_column_int(stm,CLN_APT_ALRW));
  apt->avor = char (sqlite3_column_int(stm,CLN_APT_AVOR));
  apt->aatc = char (sqlite3_column_int(stm,CLN_APT_AATC));
  apt->ails = char (sqlite3_column_int(stm,CLN_APT_AILS));
  //-------------------------------------------------------
  apt->SetAttributes();
  return apt;
}
//-----------------------------------------------------------------------------
//  Decode Airport for a line slot
//-----------------------------------------------------------------------------
CAptLine *SqlMGR::GetAptSlot(sqlite3_stmt *stm)
{ CAptLine  *lin = new CAptLine();
  char      *txt = 0;
  char       n;
  SPosition  pos;
  txt = (char*)sqlite3_column_text(stm,CLN_APT_UKEY);
  lin->SetAkey(txt);
  txt = (char*)sqlite3_column_text(stm,CLN_APT_ANAM);
  lin->SetSlotName(txt);
  txt = (char*)sqlite3_column_text(stm,CLN_APT_AICA);
  lin->SetAica(txt);
  txt = (char*)sqlite3_column_text(stm,CLN_APT_AFAA);
  lin->SetIfaa(txt);
  txt = (char*)sqlite3_column_text(stm,CLN_APT_ACTR);
  lin->SetActy(txt);                          // Country
  txt = (char*)sqlite3_column_text(stm,CLN_APT_ASTA);
  lin->SetAsta(txt);                         // State
  n       =  char (sqlite3_column_int(stm,CLN_APT_AOWN));
  lin->SetAown((U_CHAR)n);
  pos.lat = sqlite3_column_double(stm,CLN_APT_ALAT);
  pos.lon = sqlite3_column_double(stm,CLN_APT_ALON);
  pos.alt = sqlite3_column_int(stm,CLN_APT_AELE);
  lin->SetPosition(pos);
  return lin;
}
//-----------------------------------------------------------------------------
//  Decode Runway
//-----------------------------------------------------------------------------
CRunway *SqlMGR::DecodeRWY(sqlite3_stmt *stm,OTYPE obt)
{ CRunway *rwy = new CRunway(obt,RWY);
  char      *txt = 0;
  double     val = 0;
  txt = (char*)sqlite3_column_text(stm,CLN_RWY_RAPT);
  strncpy(rwy->rapt,txt,10);
  //-----------------------------------------------------
  rwy->rlen   = int  (sqlite3_column_int(stm,CLN_RWY_RLEN));
  rwy->rwid   = int  (sqlite3_column_int(stm,CLN_RWY_RWID));
  rwy->rsfc   = int  (sqlite3_column_int(stm,CLN_RWY_RSFC));
  rwy->rcon   = int  (sqlite3_column_int(stm,CLN_RWY_RCON));
  rwy->rpav   = int  (sqlite3_column_int(stm,CLN_RWY_RPAV));
  rwy->rcon   = int  (sqlite3_column_int(stm,CLN_RWY_RCON));
  rwy->rpsi   = int  (sqlite3_column_int(stm,CLN_RWY_RPSI));
  rwy->rpat   = char (sqlite3_column_int(stm,CLN_RWY_RPAT));
  rwy->rcls   = char (sqlite3_column_int(stm,CLN_RWY_RCLS));
  rwy->rpcl   = char (sqlite3_column_int(stm,CLN_RWY_RPCL));
  //--------------------------------------------------------
  txt = (char*)sqlite3_column_text(stm,CLN_RWY_RHID);
  strncpy(rwy->rhid,txt,4);
  rwy->pshi.lat = sqlite3_column_double(stm,CLN_RWY_RHLA);
  rwy->pshi.lon = sqlite3_column_double(stm,CLN_RWY_RHLO);
  rwy->pshi.alt = sqlite3_column_int(stm,CLN_RWY_RHHT);
  rwy->rhhd     = float(sqlite3_column_double(stm,CLN_RWY_RHHD));
  rwy->rhmh     = float(sqlite3_column_double(stm,CLN_RWY_RHMH));
  rwy->rhdt     = int  (sqlite3_column_int(stm,CLN_RWY_RHDT));
  rwy->rhte     = int  (sqlite3_column_int(stm,CLN_RWY_RHTE));
  rwy->rhel     = char (sqlite3_column_int(stm,CLN_RWY_RHEL));
  rwy->rhal     = char (sqlite3_column_int(stm,CLN_RWY_RHAL));
  rwy->rhcl     = char (sqlite3_column_int(stm,CLN_RWY_RHCL));
  rwy->rhtl     = char (sqlite3_column_int(stm,CLN_RWY_RHTL));
  rwy->rhth     = char (sqlite3_column_int(stm,CLN_RWY_RHTH));
  rwy->rhli     = char (sqlite3_column_int(stm,CLN_RWY_RHLI));
  rwy->rhsl     = char (sqlite3_column_int(stm,CLN_RWY_RHSL));
  rwy->rhmk     = char (sqlite3_column_int(stm,CLN_RWY_RHMK));
  rwy->rh8l[0]  = char (sqlite3_column_int(stm,CLN_RWY_RHL1));
  rwy->rh8l[1]  = char (sqlite3_column_int(stm,CLN_RWY_RHL2));
  rwy->rh8l[2]  = char (sqlite3_column_int(stm,CLN_RWY_RHL3));
  rwy->rh8l[3]  = char (sqlite3_column_int(stm,CLN_RWY_RHL4));
  rwy->rh8l[4]  = char (sqlite3_column_int(stm,CLN_RWY_RHL5));
  rwy->rh8l[5]  = char (sqlite3_column_int(stm,CLN_RWY_RHL6));
  rwy->rh8l[6]  = char (sqlite3_column_int(stm,CLN_RWY_RHL7));
  rwy->rh8l[7]  = char (sqlite3_column_int(stm,CLN_RWY_RHL8));
    //--------------------------------------------------------
  txt = (char*)sqlite3_column_text(stm,CLN_RWY_RLID);
  strncpy(rwy->rlid,txt,4);
  rwy->pslo.lat = sqlite3_column_double(stm,CLN_RWY_RLLA);
  rwy->pslo.lon = sqlite3_column_double(stm,CLN_RWY_RLLO);
  rwy->pslo.alt = sqlite3_column_int(stm,CLN_RWY_RLHT);
  rwy->rlhd     = float(sqlite3_column_double(stm,CLN_RWY_RLHD));
  rwy->rlmh     = float(sqlite3_column_double(stm,CLN_RWY_RLMH));
  rwy->rldt     = int  (sqlite3_column_int(stm,CLN_RWY_RLDT));
  rwy->rlte     = int  (sqlite3_column_int(stm,CLN_RWY_RLTE));
  rwy->rlel     = char (sqlite3_column_int(stm,CLN_RWY_RLEL));
  rwy->rlal     = char (sqlite3_column_int(stm,CLN_RWY_RLAL));
  rwy->rlcl     = char (sqlite3_column_int(stm,CLN_RWY_RLCL));
  rwy->rltl     = char (sqlite3_column_int(stm,CLN_RWY_RLTL));
  rwy->rlth     = char (sqlite3_column_int(stm,CLN_RWY_RLTH));
  rwy->rlli     = char (sqlite3_column_int(stm,CLN_RWY_RLLI));
  rwy->rlsl     = char (sqlite3_column_int(stm,CLN_RWY_RLSL));
  rwy->rlmk     = char (sqlite3_column_int(stm,CLN_RWY_RLMK));
  rwy->rl8l[0]  = char (sqlite3_column_int(stm,CLN_RWY_RLL1));
  rwy->rl8l[1]  = char (sqlite3_column_int(stm,CLN_RWY_RLL2));
  rwy->rl8l[2]  = char (sqlite3_column_int(stm,CLN_RWY_RLL3));
  rwy->rl8l[3]  = char (sqlite3_column_int(stm,CLN_RWY_RLL4));
  rwy->rl8l[4]  = char (sqlite3_column_int(stm,CLN_RWY_RLL5));
  rwy->rl8l[5]  = char (sqlite3_column_int(stm,CLN_RWY_RLL6));
  rwy->rl8l[6]  = char (sqlite3_column_int(stm,CLN_RWY_RLL7));
  rwy->rl8l[7]  = char (sqlite3_column_int(stm,CLN_RWY_RLL8));
  //-----Additional attributes  --------------------------------
  rwy->SetAttributes();
  return rwy;
}
//-----------------------------------------------------------------------------
//  Decode Runway for a line slot
//-----------------------------------------------------------------------------
CRwyLine *SqlMGR::GetRwySlot(sqlite3_stmt *stm)
{ RWEND     rend;
  CRwyLine *lin = new CRwyLine();
  char     *txt = (char*)sqlite3_column_text(stm,CLN_RWY_RAPT);
  lin->SetSlotKey(txt);
  int       n   = int  (sqlite3_column_int(stm,CLN_RWY_RLEN));
  lin->SetLeng(n);
  txt = (char*)sqlite3_column_text(stm,CLN_RWY_RHID);
  strncpy(rend.rwid,txt,4);
  rend.pos.lat  =  sqlite3_column_double(stm,CLN_RWY_RHLA);
  rend.pos.lon  =  sqlite3_column_double(stm,CLN_RWY_RHLO);
  rend.pos.alt  =  sqlite3_column_int(stm,CLN_RWY_RHHT);
  lin->SetHend(rend);
  //--------------------------------------------------------
  n   = sqlite3_column_int(stm,CLN_RWY_RWID);
  lin->SetRWID(n);
  txt = (char*)sqlite3_column_text(stm,CLN_RWY_RLID);
  strncpy(rend.rwid,txt,4);
  rend.pos.lat  =  sqlite3_column_double(stm,CLN_RWY_RLLA);
  rend.pos.lon  =  sqlite3_column_double(stm,CLN_RWY_RLLO);
  rend.pos.alt  =  sqlite3_column_int(stm,CLN_RWY_RLHT);
  lin->SetLend(rend);
  //-----Add extra data ------------------------------------
  return lin;
}
//-----------------------------------------------------------------------------
//  Decode Navaid
//-----------------------------------------------------------------------------
CNavaid *SqlMGR::DecodeNAV(sqlite3_stmt *stm,OTYPE obt)
{ CNavaid   *nav = new CNavaid(obt,VOR);
  char      *txt = 0;
  double     val = 0;
  txt = (char*)sqlite3_column_text(stm,CLN_NAV_UKEY);
  strncpy(nav->nkey,txt,10);
  txt = (char*)sqlite3_column_text(stm,CLN_NAV_NAID);
  strncpy(nav->naid,txt, 5);
  txt = (char*)sqlite3_column_text(stm,CLN_NAV_NAME);
  strncpy(nav->name,txt,40);
  txt = (char*)sqlite3_column_text(stm,CLN_NAV_NCTY);
  strncpy(nav->ncty,txt, 3);
  txt = (char*)sqlite3_column_text(stm,CLN_NAV_NSTA);
  strncpy(nav->nsta,txt, 3);
  //---------------------------------------------------
  nav->freq     = float(sqlite3_column_double(stm,CLN_NAV_NFRQ));
  nav->mDev     = float(sqlite3_column_double(stm,CLN_NAV_NMDV));
  nav->nsvr     = float(sqlite3_column_double(stm,CLN_NAV_NSVR));
  nav->nsvr     = float(sqlite3_column_double(stm,CLN_NAV_NSVR));
  nav->type     = short(sqlite3_column_int(stm,CLN_NAV_NTYP));
  nav->ncla     = short(sqlite3_column_int(stm,CLN_NAV_NCLS));
  nav->usge     = short(sqlite3_column_int(stm,CLN_NAV_NUSE));
  nav->pos.lat  = sqlite3_column_double(stm,CLN_NAV_NLAT);
  nav->pos.lon  = sqlite3_column_double(stm,CLN_NAV_NLON);
  nav->pos.alt  = sqlite3_column_int(stm,CLN_NAV_NALT);
  nav->npow     = float(sqlite3_column_double(stm,CLN_NAV_NPOW));
  nav->rang     = float(sqlite3_column_double(stm,CLN_NAV_NRGN));
  //--------Additional attributes ---------------------
  nav->SetAttributes();
  return nav;
}
//-----------------------------------------------------------------------------
//  Decode Navaid for a line slot
//-----------------------------------------------------------------------------
CNavLine *SqlMGR::GetNavSlot(sqlite3_stmt *stm)
{ CNavLine *lin = new CNavLine();
  char      *txt = 0;
  U_SHORT    s;
  SPosition pos;
  txt = (char*)sqlite3_column_text(stm,CLN_NAV_UKEY);
  lin->SetSlotKey(txt);
  txt = (char*)sqlite3_column_text(stm,CLN_NAV_NAME);
  lin->SetSlotName(txt);
  txt = (char*)sqlite3_column_text(stm,CLN_NAV_NAID);
  lin->SetVaid(txt);
  txt = (char*)sqlite3_column_text(stm,CLN_NAV_NCTY);
  lin->SetVcty(txt);
  txt = (char*)sqlite3_column_text(stm,CLN_NAV_NSTA);
  lin->SetVsta(txt);
  //----------------------------------------------------
  s = (U_SHORT)(sqlite3_column_int(stm,CLN_NAV_NTYP));
  lin->SetType(s);
  pos.lat = sqlite3_column_double(stm,CLN_NAV_NLAT);
  pos.lon = sqlite3_column_double(stm,CLN_NAV_NLON);
  pos.alt = sqlite3_column_int(stm,CLN_NAV_NALT);
  lin->SetPosition(pos);
  return lin;
}
//-----------------------------------------------------------------------------
//  Decode ILS
//-----------------------------------------------------------------------------
CILS *SqlMGR::DecodeILS(sqlite3_stmt *stm,OTYPE obt)
{ CILS   *ils = new CILS(obt,ILS);
  char   *txt = 0;
  double  val = 0;
  txt = (char*)sqlite3_column_text(stm,CLN_ILS_IKEY);
  strncpy(ils->ikey,txt,10);
  txt = (char*)sqlite3_column_text(stm,CLN_ILS_IAPT);
  strncpy(ils->iapt,txt,10);
  txt = (char*)sqlite3_column_text(stm,CLN_ILS_IRWY);
  strncpy(ils->irwy,txt, 4);
  txt = (char*)sqlite3_column_text(stm,CLN_ILS_IILS);
  strncpy(ils->iils,txt, 6);
  txt = (char*)sqlite3_column_text(stm,CLN_ILS_NAME);
  strncpy(ils->name,txt,40);
  //----------------------------------------------------
  ils->type     = int(sqlite3_column_int(stm,CLN_ILS_NTYP));
  ils->pos.lat  = sqlite3_column_double (stm,CLN_ILS_ILAT);
  ils->pos.lon  = sqlite3_column_double (stm,CLN_ILS_ILON);
  ils->pos.alt  = sqlite3_column_int    (stm,CLN_ILS_IALT);
  //-----------------------------------------------------
  ils->freq     = float(sqlite3_column_double(stm,CLN_ILS_FREQ));
  ils->gsan     = float(sqlite3_column_double(stm,CLN_ILS_GSAN));
  ils->mDev     = float(sqlite3_column_double(stm,CLN_ILS_MDEV));
  ils->rang     = float(sqlite3_column_double(stm,CLN_ILS_RANG));
  //----ADDITIONAL PARAMETERS ---------------------------
  ils->SetAttributes();
  return ils;
}
//-----------------------------------------------------------------------------
//  Decode COM
//-----------------------------------------------------------------------------
CCOM *SqlMGR::DecodeCOM(sqlite3_stmt *stm,OTYPE obt)
{ float frq   = float(sqlite3_column_double(stm,CLN_COM_FRQ1));
  if ((frq < 118) ||  (frq > 137)) return 0;
  CCOM   *com = new CCOM(obt,COM);
  char   *txt = 0;
  double  val = 0;
  txt = (char*)sqlite3_column_text(stm,CLN_COM_CKEY);
  strncpy(com->ckey,txt,10);
  txt = (char*)sqlite3_column_text(stm,CLN_COM_CAPT);
  strncpy(com->capt,txt,10);
  txt = (char*)sqlite3_column_text(stm,CLN_COM_CNAM);
  strncpy(com->cnam,txt,20);
  //------------------------------------------------------------
  com->ctyp   = int  (sqlite3_column_int(stm,CLN_COM_CTYP));
  com->cfr1   = float(sqlite3_column_double(stm,CLN_COM_FRQ1));
  com->cfr2   = float(sqlite3_column_double(stm,CLN_COM_FRQ2));
  com->cfr3   = float(sqlite3_column_double(stm,CLN_COM_FRQ3));
  com->cfr4   = float(sqlite3_column_double(stm,CLN_COM_FRQ4));
  com->cfr5   = float(sqlite3_column_double(stm,CLN_COM_FRQ5));
  //-------------------------------------------------------------
  com->pos.lat  = sqlite3_column_double (stm,CLN_COM_CLAT);
  com->pos.lon  = sqlite3_column_double (stm,CLN_COM_CLON);
  com->pos.alt  = 0;
  //----ADDITIONAL ATTRIBUTES ------------------------------------
  com->SetAttributes();
  return com;
}
//-----------------------------------------------------------------------------
//  Decode COM for a line slot using mask filter
//-----------------------------------------------------------------------------
CComLine *SqlMGR::GetComSlot(sqlite3_stmt *stm,CDataBaseREQ *req)
{ float f = float(sqlite3_column_double(stm,CLN_COM_FRQ1));
  if (!GoodCOMfrequency(f)) return 0;
  int m = int  (sqlite3_column_int(stm,CLN_COM_CTYP));
  if (!req->Match(m))                     return 0;
  CComLine *lin = new CComLine();
  lin->SetMask(m);
  lin->SetFreq(f);
  lin->FillCom();
  return lin;
}
//-----------------------------------------------------------------------------
//  Decode ILS for a line slot
//-----------------------------------------------------------------------------
CComLine *SqlMGR::GetIlsSlot(sqlite3_stmt *stm)
{ float f = float(sqlite3_column_double(stm,CLN_ILS_FREQ));
  if (!GoodILSfrequency(f)) return 0;
  CComLine *lin = new CComLine();
  char     *txt = (char*)sqlite3_column_text(stm,CLN_ILS_IRWY);
  lin->SetIRWY(txt);
  lin->SetFreq(f);
  lin->FillILS();
  return lin;
}
//-----------------------------------------------------------------------------
//  Decode WPT
//-----------------------------------------------------------------------------
CWPT *SqlMGR::DecodeWPT(sqlite3_stmt *stm,OTYPE obt)
{ CWPT   *wpt = new CWPT(obt,WPT);
  char   *txt = (char*)sqlite3_column_text(stm,CLN_WPT_UKEY);
  wpt->SetKey(txt);
  txt = (char*)sqlite3_column_text(stm,CLN_WPT_WNAM);
  wpt->SetNAM(txt);
  txt = (char*)sqlite3_column_text(stm,CLN_WPT_WCTY);
  wpt->SetCTY(txt);
  txt = (char*)sqlite3_column_text(stm,CLN_WPT_WSTA);
  wpt->SetSTA(txt);
  int n = sqlite3_column_int(stm,CLN_WPT_WLOC);
  wpt->SetLOC(n);
  n = sqlite3_column_int(stm,CLN_WPT_WTYP);
  wpt->SetTYP(n);
  n = sqlite3_column_int(stm,CLN_WPT_WUSE);
  wpt->SetUSE(n);
  //-----------------------------------------------------
  wpt->wpos.lat  = sqlite3_column_double (stm,CLN_WPT_WLAT);
  wpt->wpos.lon  = sqlite3_column_double (stm,CLN_WPT_WLON);
  wpt->wpos.alt  = sqlite3_column_int    (stm,CLN_WPT_WALT);
  //-------------------------------------------------------
  //float f = float( sqlite3_column_double(stm,CLN_WPT_WMAG));
  //wpt->SetMGD(f);
  //----NOTE: Fields after are not decoded for now --------
  //------------------------------------------------------------------------
  //  Normalize name of waypoint. Eliminate ( and )
  //------------------------------------------------------------------------
  txt = wpt->wnam;
  if  (*txt == '(')  strcpy(txt,txt+1);
  char *end = strrchr(txt,')');
  if (end) *end = 0;
  //------Set ident as the first 4 characters of name ----
  strncpy(wpt->widn,txt,4);
  wpt->widn[4] = 0;
  //-------------------------------------
  wpt->SetAttributes();
  return wpt;
}
//-----------------------------------------------------------------------------
//  Decode WPT for a line slot
//-----------------------------------------------------------------------------
CWptLine *SqlMGR::GetWptSlot(sqlite3_stmt *stm)
{ CWptLine *lin = new CWptLine();
  char *txt;
  short   s;
  SPosition pos;
  txt = (char*)sqlite3_column_text(stm,CLN_WPT_UKEY);
  lin->SetSlotKey(txt);
  txt = (char*)sqlite3_column_text(stm,CLN_WPT_WNAM);
  lin->SetSlotName(txt);
  lin->SetWaid("INT");
  txt = (char*)sqlite3_column_text(stm,CLN_WPT_WCTY);
  lin->SetWcty(txt);
  txt = (char*)sqlite3_column_text(stm,CLN_WPT_WSTA);
  lin->SetWsta(txt);
  s   = short  (sqlite3_column_int(stm,CLN_WPT_WTYP));
  lin->SetWtyp((U_SHORT)s);
  pos.lat = sqlite3_column_double (stm,CLN_WPT_WLAT);
  pos.lon = sqlite3_column_double (stm,CLN_WPT_WLON);
  pos.alt = sqlite3_column_int(stm,CLN_WPT_WALT);
  lin->SetPosition(pos);
  //------------------------------------------------------
  //  Normalize name of waypoint. Eliminate ( and )
  //------------------------------------------------------
  txt = lin->GetSlotName();
  if  (*txt == '(')  strcpy(txt,txt+1);
  char *end = strrchr(txt,')');
  if (end) *end = 0;
  return lin;
}
//-----------------------------------------------------------------------------
//  Decode CTY for a line slot
//-----------------------------------------------------------------------------
CCtyLine *SqlMGR::GetCtySlot(sqlite3_stmt *stm)
{ CCtyLine *lin = new CCtyLine();
  char     *txt = (char*)sqlite3_column_text(stm,CLN_CTY_CNAM);
  lin->SetSlotName(txt);
  txt = (char*)sqlite3_column_text(stm,CLN_CTY_CUID);
  lin->SetSlotKey(txt);
  return lin;
}
//-----------------------------------------------------------------------------
//  Decode State for a line slot
//-----------------------------------------------------------------------------
CStaLine *SqlMGR::GetStaSlot(sqlite3_stmt *stm)
{ CStaLine *lin = new CStaLine();
  char *txt = (char*)sqlite3_column_text(stm,CLN_STA_NAME);
  lin->SetSlotName(txt);
  txt = (char*)sqlite3_column_text(stm,CLN_STA_SKEY);
  lin->SetSlotKey(txt);
  txt = (char*)sqlite3_column_text(stm,CLN_STA_SCTY);
  lin->SetCTY(txt);
  return lin;
}
 //----------------------------------------------------------------------------
 // Get all airport and pass each one to function fun
 //----------------------------------------------------------------------------
 void  SqlMGR::GetAllAirports(AptFunCB *fun)
 {	char req[1024];
		strncpy(req,"SELECT * FROM APT;*",1023);
    stm = CompileREQ(req,genDBE);
    //-----Get the results -------------------------------------------------
    while (SQLITE_ROW == sqlite3_step(stm))
    { CAirport *apt = DecodeAPT(stm,DBM);
      if (fun)  fun(apt);
    }
    //-----Close request ---------------------------------------------------
    sqlite3_finalize(stm);
    return;
 }
//-----------------------------------------------------------------------------
//  Get All airports by tile keys
//-----------------------------------------------------------------------------
void SqlMGR::GetAptByTile(int key,HdrFunCB *fun,CTileCache *tc)
{ SQL_REQ      *req = &SelAPT_DB;
  char          arg[16];
  //-----Make a key ------------------------------------------------------
  _snprintf(arg,16,"%08d",key);
  strncpy(req->arg[0],arg,8);
  stm = CompileREQ(req->txt,genDBE);
  //-----Get the results -------------------------------------------------
  while (SQLITE_ROW == sqlite3_step(stm))
  { CAirport *apt = DecodeAPT(stm,DBM);
    if (fun)  fun(apt,tc);
  }
  //-----Close request ---------------------------------------------------
  sqlite3_finalize(stm);
}
//-----------------------------------------------------------------------------
// Get Runway for airport
//-----------------------------------------------------------------------------
void SqlMGR::GetRwyForAirport(CAirport *apt,RwyFunCB *fun,CTileCache *tc)
{ SQL_REQ      *req = &SelRWY_DB;
  char          arg[16];
  //-----Make a key ------------------------------------------------------
  _snprintf(arg,16,"%s\";",apt->akey);
  strncpy(req->arg[0],arg,10);
  sqlite3_stmt *stm = CompileREQ(req->txt,genDBE);
  //-----Get the results -------------------------------------------------
  while (SQLITE_ROW == sqlite3_step(stm))
  { CRunway *rwy = DecodeRWY(stm,DBM);
    apt->AddRunway(rwy);                              // Add in Q1
    if (rwy->rlen > 8079.0f)  apt->SetLongRWY();      // Long RWY property
    if (rwy->rsfc != 0)       apt->SetHardRWY();      // Hard runway property
    if (fun)  fun(rwy,tc);
  }
  //-----Close request ---------------------------------------------------
  sqlite3_finalize(stm);
  return;
}
//-----------------------------------------------------------------------------
//  Get All Navaids by tile keys
//-----------------------------------------------------------------------------
void SqlMGR::GetNavByTile(int key,HdrFunCB *fun,CTileCache *tc)
{ SQL_REQ      *req = &SelNAV_DB;
  char          arg[16];
  //-----Make a key ------------------------------------------------------
  _snprintf(arg,16,"%08d",key);
  strncpy(req->arg[0],arg,8);
  sqlite3_stmt *stm = CompileREQ(req->txt,genDBE);
  //-----Get the results -------------------------------------------------
  while (SQLITE_ROW == sqlite3_step(stm))
  { CNavaid *nav = DecodeNAV(stm,DBM);
    if (fun)  fun(nav,tc);
  }
  //-----Close request ---------------------------------------------------
  sqlite3_finalize(stm);
  return;
}
//-----------------------------------------------------------------------------
// Get Ils for airport
//-----------------------------------------------------------------------------
void SqlMGR::GetIlsForAirport(CAirport *apt,HdrFunCB *fun,CTileCache *tc)
{ char req[1024];
	_snprintf(req,1024,"SELECT * FROM ILS WHERE iapt == \"%s\" and ntyp == 64;*",apt->akey);
  sqlite3_stmt *stm = CompileREQ(req,genDBE);
  //-----Get the results -------------------------------------------------
  while (SQLITE_ROW == sqlite3_step(stm))
  { CILS *ils = DecodeILS(stm,DBM);
    ils->LinkAPT(apt);
    CRunway *rwy  = apt->FindRunway(ils->irwy);
    if (0 == rwy) {delete ils; continue;}
    rwy->InitILS(ils);
    apt->AddOneILS();
    //---Add to Cache ------------------------------
    if (fun)  fun(ils,tc);
  }
  //---Free statement --------------------------------------------------
  sqlite3_finalize(stm);
  return;
}
//-----------------------------------------------------------------------------
//  Get All COM by tile keys
//-----------------------------------------------------------------------------
void SqlMGR::GetComByTile(int key,HdrFunCB *fun,CTileCache *tc)
{ SQL_REQ      *req = &SelCOM_DB;
  char          arg[16];
  //-----Make a key ------------------------------------------------------
  _snprintf(arg,16,"%08d",key);
  strncpy(req->arg[0],arg,8);
  sqlite3_stmt *stm = CompileREQ(req->txt,genDBE);
  //-----Get the results -------------------------------------------------
  while (SQLITE_ROW == sqlite3_step(stm))
  { CCOM *com = DecodeCOM(stm,DBM);
    if (0 == com) continue;
    globals->dbc->SetAirportIdent(com);
    if (fun)  fun(com,tc);
  }
  //-----Close request ---------------------------------------------------
  sqlite3_finalize(stm);
  return;
}
//==============================================================================
//  WINDOWS INTERFACE
//==============================================================================
//------------------------------------------------------------------------------
//  Prepare Argument from request
//------------------------------------------------------------------------------
void SqlMGR::BuildARG(CDataBaseREQ *req)
{ char arg[128];
  if (req->NeedCTY()) 
  { if (0 == nArg)  strcat(query,"WHERE ");
    nArg++;
    _snprintf(arg,128,"(ctry == '%s') ",req->GetCTYkey());
    strcat(query,arg);
  }
  if (req->NeedSTA())
  { if (0 == nArg)  strcat(query,"WHERE ");
    else            strcat(query," AND ");
    nArg++;
    _snprintf(arg,128,"(stat == '%s') ",req->GetSTAkey()+2);
    strcat(query,arg);
  }
  if (req->NeedST1())
  { if (0 == nArg)  strcat(query,"WHERE ");
    else            strcat(query," AND ");
    nArg++;
    _snprintf(arg,128,"(name like '%%%s%%')",req->GetST1key());
    strcat(query,arg);
  }
  if (req->NeedST2())
  { if (0 == nArg)  strcat(query,"WHERE ");
    else            strcat(query," AND ");
    nArg++;
    _snprintf(arg,128,"(iden like '%%%s%%')",req->GetST2key());
    strcat(query,arg);
  }
  strcat(query,";");
  return;
}
//==============================================================================
//  Prepare a SQL statement from Request
//==============================================================================
void SqlMGR::BuildSQL(CDataBaseREQ *req,char *qr)
{ query = qr;
  nArg  = 0;
  strcpy(query,"SELECT * FROM ");
  switch (req->GetReqCode())  {
    case COM_BY_AIRPORT:
      break;
    case RWY_BY_AIRPORT:
      break;
    case CTY_BY_ALLLOT:
      break;
    case APT_BY_FILTER:
      strcat(query,"APT ");
      BuildARG(req);
      break;
    case NAV_BY_FILTER:
      strcat(query,"NAV ");
      BuildARG(req);
      break;
    case WPT_BY_FILTER:
      strcat(query,"WPT ");
      BuildARG(req);
      break;
    case STA_BY_COUNTRY:
      break;
    case APT_BY_OFFSET:
      break;
    case NAV_BY_OFFSET:
      break;
    case NDB_BY_OFFSET:
      break;
}
  return;
}
//===================================================================================
//  SELECTION ROUTINES FOR VARIOUS WINDOWS
//===================================================================================

//-----------------------------------------------------------------------------------
//  Select all airports maching arguments
//-----------------------------------------------------------------------------------
void SqlMGR::GetAirportByArg(CDataBaseREQ *rqb)
{ char req[1024];
	BuildSQL(rqb,req);
  sqlite3_stmt *stm = CompileREQ(req,genDBE);
  //----Execute the Query --------------------------------------
  while (SQLITE_ROW == sqlite3_step(stm))
  { CAptLine *lin = GetAptSlot(stm);
    rqb->Wind->AddDBrecord(lin,(DBCODE)rqb->Code);
  }
  //-----Close request ---------------------------------------------------
  sqlite3_finalize(stm);
  return;
}
//-----------------------------------------------------------------------------------
//  Select all Navaids maching arguments
//-----------------------------------------------------------------------------------
void SqlMGR::GetNavaidByArg(CDataBaseREQ *rqb)
{ char req[1024];
	BuildSQL(rqb,req);
  sqlite3_stmt *stm = CompileREQ(req,genDBE);
  //----Execute the Query --------------------------------------
  while (SQLITE_ROW == sqlite3_step(stm))
  { CNavLine *lin = GetNavSlot(stm);
    if (rqb->NeedVOR() && lin->IsVOR()) {rqb->Wind->AddDBrecord(lin,(DBCODE)rqb->Code); continue;}
    if (rqb->NeedNDB() && lin->IsNDB()) {rqb->Wind->AddDBrecord(lin,(DBCODE)rqb->Code); continue;}
    delete lin;
  }
  //-----Close request ---------------------------------------------------
  sqlite3_finalize(stm);
  return;
}
//-----------------------------------------------------------------------------------
//  Select all Waypoints maching arguments
//-----------------------------------------------------------------------------------
void SqlMGR::GetWaypointByArg(CDataBaseREQ *rqb)
{ char req[1024];
	BuildSQL(rqb,req);
  sqlite3_stmt *stm = CompileREQ(req,genDBE);
  //----Execute the Query --------------------------------------
  while (SQLITE_ROW == sqlite3_step(stm))
  { CWptLine *lin = GetWptSlot(stm);
    rqb->Wind->AddDBrecord(lin,(DBCODE)rqb->Code);
  }
  //-----Close request ---------------------------------------------------
  sqlite3_finalize(stm);
  return;
}
//----------------------------------------------------------------------------------
//  Get ALL COUNTRIES
//----------------------------------------------------------------------------------
void SqlMGR::GetAllCountries(CDataBaseREQ *rqb)
{ char req[1024];
	strcpy(req,"SELECT * FROM CTY;*");
  sqlite3_stmt *stm = CompileREQ(req,genDBE);
  while (SQLITE_ROW == sqlite3_step(stm))
  { CCtyLine *lin = GetCtySlot(stm);
    rqb->Wind->AddDBrecord(lin,(DBCODE)rqb->Code);
  }
  //-----Close request ---------------------------------------------------
  sqlite3_finalize(stm);
  return;
}
//----------------------------------------------------------------------------------
//  Get ALL COUNTRIES NAMES
//----------------------------------------------------------------------------------
void SqlMGR::GetCountryName(TCacheMGR *tcm)
{ char req[1024];
	strcpy(req,"SELECT * FROM CTY;*");
  sqlite3_stmt *stm = CompileREQ(req,genDBE);
  while (SQLITE_ROW == sqlite3_step(stm))
  { 
    CCtyLine *cty = GetCtySlot(stm);
    tcm->OneCountry(cty);
    delete cty;
  }
  //-----Close request ---------------------------------------------------
  sqlite3_finalize(stm);
  return;
}

//----------------------------------------------------------------------------------
//  Get ONE COUNTRY
//----------------------------------------------------------------------------------
void SqlMGR::GetOneCountry(CDataBaseREQ *rqb)
{ char req[1024];
	_snprintf(req,1024,"SELECT * FROM CTY WHERE cuid = '%s';*",rqb->GetCTYkey());
  sqlite3_stmt *stm = CompileREQ(req,genDBE);
  if (SQLITE_ROW == sqlite3_step(stm))
  { CCtyLine *lin = GetCtySlot(stm);
    rqb->Wind->AddDBrecord(lin,(DBCODE)rqb->Code);
  }
  //-----Close request ---------------------------------------------------
  sqlite3_finalize(stm);
  return;
}

//----------------------------------------------------------------------------------
//  Get ALL state for a country
//----------------------------------------------------------------------------------
void SqlMGR::GetStateByCountry(CDataBaseREQ *rqb)
{ char req[1024];
	char  *key = rqb->GetCTYkey();
  _snprintf(req,1024,"SELECT * FROM STA where sctr = '%s';*",key);
  sqlite3_stmt *stm = CompileREQ(req,genDBE);
  while (SQLITE_ROW == sqlite3_step(stm))
  { CStaLine *lin = GetStaSlot(stm);
    rqb->Wind->AddDBrecord(lin,(DBCODE)rqb->Code);
  }
  //-----Close request ---------------------------------------------------
  sqlite3_finalize(stm);
  return;
}
//-----------------------------------------------------------------------------------
//  Select all COM maching Airport
//-----------------------------------------------------------------------------------
void SqlMGR::ComFromThisAirport(CDataBaseREQ *rqb)
{ char req[1024];
	char *key = rqb->GetOBJkey();
  _snprintf(req,1024,"SELECT * FROM COM WHERE capt = '%s'; ",key);
  sqlite3_stmt *stm = CompileREQ(req,genDBE);
  //----Execute the Query --------------------------------------
  while (SQLITE_ROW == sqlite3_step(stm))
  { CComLine *lin = GetComSlot(stm,rqb);
    if (lin) rqb->Wind->AddDBrecord(lin,(DBCODE)rqb->Code);
  }
  //-----Close request ---------------------------------------------------
  sqlite3_finalize(stm);
  return;
}
//----------------------------------------------------------------------------------
//  Get one airport by Key
//----------------------------------------------------------------------------------
void SqlMGR::APTByKey(CDataBaseREQ *rqb)
{ char req[1024];
	char *key = rqb->GetOBJkey();
  _snprintf(req,1024,"SELECT * FROM APT WHERE ukey = '%s'; ",key);
  sqlite3_stmt *stm = CompileREQ(req,genDBE);
  if (SQLITE_ROW == sqlite3_step(stm))
  { CAirport *apt = DecodeAPT(stm,ANY);
    rqb->Wind->AddDBrecord(apt,(DBCODE)rqb->Code);
  }
  //-----Close request ---------------------------------------------------
  sqlite3_finalize(stm);
  return;
}
//----------------------------------------------------------------------------------
//  Get one airport by Identity
//----------------------------------------------------------------------------------
void SqlMGR::GetAirportByIden  (char *idn,CAirport **ptr)
{ char req[1024];
	_snprintf(req,1024,"SELECT * FROM APT WHERE (iden = '%s') OR (afaa = '%s'); ",idn,(idn + 1));
  sqlite3_stmt *stm = CompileREQ(req,genDBE);
 *ptr = 0;
  if (SQLITE_ROW == sqlite3_step(stm))
  { CAirport *apt = DecodeAPT(stm,ANY);
   *ptr = apt;
  }
  //-----Close request ---------------------------------------------------
  sqlite3_finalize(stm);
  return ;
}

//----------------------------------------------------------------------------------
//  Get one Navaid by Key
//----------------------------------------------------------------------------------
void  SqlMGR::NAVByKey(CDataBaseREQ *rqb)
{ char req[1024];
	char *key = rqb->GetOBJkey();
  _snprintf(req,1024,"SELECT * FROM NAV WHERE ukey = '%s'; ",key);
  sqlite3_stmt *stm = CompileREQ(req,genDBE);
  if (SQLITE_ROW == sqlite3_step(stm))
  { CNavaid *nav = DecodeNAV(stm,ANY);
    rqb->Wind->AddDBrecord(nav,(DBCODE)rqb->Code);
  }
  //-----Close request ---------------------------------------------------
  sqlite3_finalize(stm);
  return;
}
//----------------------------------------------------------------------------------
//  Get Runway slot from airport
//----------------------------------------------------------------------------------
void SqlMGR::RWYbyAPTkey(CDataBaseREQ *rqb)
{ char req[1024];
	int No = 0;
	SPosition org = {0,0,0};
  char *key = rqb->GetOBJkey();
  _snprintf(req,1024,"SELECT * FROM RWY WHERE rapt = '%s'; ",key);
  sqlite3_stmt *stm = CompileREQ(req,genDBE);
  while (SQLITE_ROW == sqlite3_step(stm))
  { CRwyLine *lin = GetRwySlot(stm);
    if (No++ == 0)  org = lin->GetHiPosition();
    globals->dbc->ComputeDistance(lin,org);
    lin->EditRWY();
    rqb->Wind->AddDBrecord(lin,(DBCODE)rqb->Code);
    ILSbyRWYkey(rqb,lin);
  }
  //-----Close request ---------------------------------------------------
  sqlite3_finalize(stm);
  return;
}
//----------------------------------------------------------------------------------
//  Get ILS for RWY
//----------------------------------------------------------------------------------
void SqlMGR::ILSbyRWYkey(CDataBaseREQ *rqb,CRwyLine *lin)
{ char req[1024];
	char *k1 = lin->GetSlotKey();
  char *k2 = lin->GetHiEndID();
  char *k3 = lin->GetLoEndID();
  _snprintf(req,1024,"SELECT * FROM ILS WHERE iapt == '%s' AND irwy IN ('%s','%s');",k1,k2,k3);
  sqlite3_stmt *stm = CompileREQ(req,genDBE);
  while (SQLITE_ROW == sqlite3_step(stm))
  { CComLine *com = GetIlsSlot(stm);
    if (com) rqb->Wind->AddDBrecord(com,(DBCODE)COM_BY_AIRPORT);
  }
  //-----Close request ---------------------------------------------------
  sqlite3_finalize(stm);
}
//===================================================================================
//  SELECTION ROUTINES FOR GPS
//===================================================================================
//  Locate runways for airport
//------------------------------------------------------------------------
void SqlMGR::AirportRWYforGPS(CGPSrequest *rqg,CAirport *apt)
{ char req[1024];
	char *key = apt->GetKey();
  _snprintf(req,1024,"SELECT * FROM RWY WHERE rapt = '%s';*",key);
  sqlite3_stmt *stm = CompileREQ(req,genDBE);
  while (SQLITE_ROW == sqlite3_step(stm))
  { CRunway *rwy = DecodeRWY(stm,SHR);
    rqg->rwyQ.LastInQ1(rwy);
    if (rwy->rlen > 8079.0f)  apt->SetLongRWY();      // Long RWY property
    if (rwy->rsfc != 0)       apt->SetHardRWY();      // Hard runway property
  }
  //-----Close request ---------------------------------------------------
  sqlite3_finalize(stm);
  return;
}
//-------------------------------------------------------------------------
//  Locate COMs for airport
//-------------------------------------------------------------------------
void SqlMGR::AirportCOMforGPS(CGPSrequest *rqg,CAirport *apt)
{ char req[1024];
	char *key = apt->GetKey();
  _snprintf(req,1024,"SELECT * FROM COM WHERE capt = '%s';*",key);
  sqlite3_stmt *stm = CompileREQ(req,genDBE);
  while (SQLITE_ROW == sqlite3_step(stm))
  { CCOM *com = DecodeCOM(stm,SHR);
    if (0 == com) continue;
    rqg->comQ.LastInQ1(com);
  }
  //-----Close request ---------------------------------------------------
  sqlite3_finalize(stm);
  return;
}
//------------------------------------------------------------------------
//  Locate ILS for Runway
//------------------------------------------------------------------------
void SqlMGR::RunwayILSforGPS(CGPSrequest *rqg, char *akey,char *kend)
{ char req[1024];
	_snprintf(req,1024,"SELECT * FROM ILS WHERE iapt == '%s' AND irwy = '%s';*",akey,kend);
  sqlite3_stmt *stm = CompileREQ(req,genDBE);
  while (SQLITE_ROW == sqlite3_step(stm))
  { CILS *ils = DecodeILS(stm,SHR);
    rqg->ilsQ.LastInQ1(ils);
  }
  //-----Close request ---------------------------------------------------
  sqlite3_finalize(stm);
  return;
}
//-----------------------------------------------------------------------
//  Get First item matching the key in request
//-----------------------------------------------------------------------
sqlite3_stmt *SqlMGR::CompileMatchGPS(CGPSrequest *rqg,char *tab)
{ char req[1024];
	int nArg = 0;
  char Arg[64];
  _snprintf(req,1024,"SELECT * FROM %s ",tab);
  if (rqg->NeedST1())
  { if (0 == nArg)  strcat(req,"WHERE ");
    nArg++;
    _snprintf(Arg,64,"name LIKE '%s%%' ",rqg->GetST1key()+1);
    strcat(req,Arg);
  }
  if (rqg->NeedST2())
  { if (0 == nArg)  strcat(req,"WHERE ");
    else            strcat(req,"AND ");
    nArg++;
    _snprintf(Arg,64,"iden LIKE '%s%%' ",rqg->GetST2key()+1);
    strcat(req,Arg);
  }
  strcat(req,";*");
  return CompileREQ(req,genDBE);
}

//-----------------------------------------------------------------------
//  Get First airport matching the key in request
//-----------------------------------------------------------------------
void SqlMGR::MatchAPTforGPS(CGPSrequest *rqg,int dir)
{ sqlite3_stmt *stm = CompileMatchGPS(rqg,"APT");
  if (SQLITE_ROW == sqlite3_step(stm))  rqg->obj = DecodeAPT(stm,SHR);
  //-----Close request ---------------------------------------------------
  sqlite3_finalize(stm);
  return;
}
//-----------------------------------------------------------------------
//  Get First NAVAID matching the key in request
//-----------------------------------------------------------------------
void  SqlMGR::MatchNAVforGPS(CGPSrequest *rqg,int dir)
{ sqlite3_stmt *stm = CompileMatchGPS(rqg,"NAV");
  while (SQLITE_ROW == sqlite3_step(stm))
  { CNavaid *nav = DecodeNAV(stm,SHR);
    if (rqg->NeedNDB() && nav->IsNDB()) {rqg->obj = nav; break; }
    if (rqg->NeedVOR() && nav->IsVOR()) {rqg->obj = nav; break; }
  }
  //-----Close request ---------------------------------------------------
  sqlite3_finalize(stm);
  return;
}
//-----------------------------------------------------------------------
//  Get First WAYPOINT matching the key in request
//-----------------------------------------------------------------------
void  SqlMGR::MatchWPTforGPS(CGPSrequest *rqg,int dir)
{ sqlite3_stmt *stm = CompileMatchGPS(rqg,"WPT");
  if (SQLITE_ROW == sqlite3_step(stm))
  { CWPT *wpt = DecodeWPT(stm,SHR);
    rqg->obj  = wpt;
  }
  //-----Close request ---------------------------------------------------
  sqlite3_finalize(stm);
  return;
}
//-----------------------------------------------------------------------
//  Get any object by ident
//-----------------------------------------------------------------------
void SqlMGR::GetOBJbyIdent(CGPSrequest *rgq, char *tab, Tag type)
{ char req[1024];
	CmHead *obj = 0;
  char *idn = rgq->GetST2key();
  _snprintf(req,1024,"SELECT * FROM %s WHERE iden LIKE '%s%%';*",tab,idn);
  sqlite3_stmt *stm = CompileREQ(req,genDBE);
  while (SQLITE_ROW == sqlite3_step(stm))
  { switch (type) {
    case 'APT*':
      obj = DecodeAPT(stm,SHR);
      break;
    case 'NAV*':
      obj = DecodeNAV(stm,SHR);
      break;
    case 'WPT*':
      obj = DecodeWPT(stm,SHR);
      break;
    default :
      continue;
    }
    obj->IncUser();
    rgq->wptQ.LastInQ1(obj);
    if (20 < rgq->wptQ.GetNbrObj())  continue;
  }
  //-----Close request ---------------------------------------------------
  sqlite3_finalize(stm);
  return;
}
//=======================================================================
//  ACCESS FOR FLIGHT PLAN
//=======================================================================
//-----------------------------------------------------------------------
//  Get any object by its unique key
//-----------------------------------------------------------------------
void SqlMGR::GetFlightPlanWPT(CWPoint* wpt)
{ char req[1024];
	char   *key = wpt->GetDbKey();
  char   *tab = wpt->GetSQLtab();
  CmHead *obj = 0;
  _snprintf(req,1024,"SELECT * FROM %s WHERE ukey = '%s';*",tab,key);
  sqlite3_stmt *stm = CompileREQ(req,genDBE);
  if (SQLITE_ROW == sqlite3_step(stm))
  {   switch(wpt->GetKind())
      {   case 'airp':
            obj = DecodeAPT(stm,SHR);
            break;
          case 'snav':
            obj = DecodeNAV(stm,SHR);
            break;
          case 'dbwp':
            obj = DecodeWPT(stm,SHR);
            break;
      }
  }
  wpt->SetDBwpt(obj);
  //-----Close request ---------------------------------------------------
  sqlite3_finalize(stm);
  return;
}
//==============================================================================
//  INSERT ROUTINES
//==============================================================================
//  Insert BLOB into detail elevation. 
//  The blob is an array of nbe items of integer type.
//------------------------------------------------------------------------
int SqlMGR::WriteElevationBlob(REGION_REC &reg)
{ char *rq = "INSERT INTO DET (key,qgt,nbe,elv) VALUES(?1,?2,?3,?4);*";
  sqlite3_stmt *stm = CompileREQ(rq,elvDBE);
  //---Record key --------------------------------------------------------
  int  kx  = (reg.qtx << TC_BY32) | reg.dtx;
  int  kz  = (reg.qtz << TC_BY32) | reg.dtz;
  int  key = (kx << 16) | kz;
	//---Key ---------------------------------------------------------------
  int  rep = sqlite3_bind_int(stm, 1,key);
  if (rep != SQLITE_OK)   Abort(elvDBE);
  //---QGT ----------------------------------------------------------------
  int  qgk = (reg.qtx << 16) | (reg.qtz);
  rep      = sqlite3_bind_int(stm, 2,qgk);
  if (rep != SQLITE_OK)   Abort(elvDBE);
  //---Number of vertices -------------------------------------------------
  rep      = sqlite3_bind_int(stm, 3,reg.nbv);
  if (rep != SQLITE_OK)   Abort(elvDBE);
  //---Blob ---------------------------------------------------------------
  int dim  = reg.nbv * sizeof(int);
  rep = sqlite3_bind_blob(stm, 4,reg.data, dim, SQLITE_TRANSIENT);
  if (rep != SQLITE_OK)   Abort(elvDBE);
	//--- Execute statement--------------------------------------------------
  rep      = sqlite3_step(stm);               // Insert value in database
  if (rep != SQLITE_DONE) Abort(elvDBE);

  sqlite3_finalize(stm);                      // Close statement
  return key;
}
//==============================================================================
//  Insert Elevation record
//==============================================================================
void SqlMGR::WriteElevationRegion(REGION_REC &reg)
{ int  rep = 0;  
  char *rq =  "INSERT INTO REG (qgt,det,lgx,lgz,sub,type,val) "  
              "VALUES(?1,?2,?3,?4,?5,?6,?7);*";
  sqlite3_stmt *stm = CompileREQ(rq,elvDBE);
  int  qgk = (reg.qtx << 16) | (reg.qtz);
  rep = sqlite3_bind_int(stm, 1,qgk);
  if (rep != SQLITE_OK) Abort(elvDBE);

  int  rdk = (reg.dtx << 16) | (reg.dtz);
  rep = sqlite3_bind_int(stm, 2,rdk);
  if (rep != SQLITE_OK) Abort(elvDBE);

  rep = sqlite3_bind_int(stm, 3, reg.lgx);
  if (rep != SQLITE_OK) Abort(elvDBE);

  rep = sqlite3_bind_int(stm, 4, reg.lgz);
  if (rep != SQLITE_OK) Abort(elvDBE);

  rep = sqlite3_bind_int(stm, 5,reg.sub);
  if (rep != SQLITE_OK) Abort(elvDBE);

  rep = sqlite3_bind_int(stm, 6,reg.type);
  if (rep != SQLITE_OK) Abort(elvDBE);

  rep = sqlite3_bind_int(stm, 7,reg.val);
  if (rep != SQLITE_OK) Abort(elvDBE);

  //---Execute insert -------------------------------------------------------
  rep      = sqlite3_step(stm);               // Insert value in database
  if (rep != SQLITE_DONE) Abort(elvDBE);
  sqlite3_finalize(stm);                      // Close statement
  return;
}
//==============================================================================
//  Insert Elevation record
//==============================================================================
void SqlMGR::WriteElevationRecord(REGION_REC &reg)
{ if (reg.type != EL_UNIC)
  { reg.val = WriteElevationBlob(reg);
    delete [] reg.data;
    reg.data = 0;
  }
  WriteElevationRegion(reg);
  return;
}
//==============================================================================
//  Check for POD-TRN in Database
//==============================================================================
bool SqlMGR::FileInELV(char *fn)
{	char rq[1024];
	_snprintf(rq,1023,"SELECT rowid FROM FNM WHERE file LIKE '%%%s%%';*",fn);
	sqlite3_stmt *stm = CompileREQ(rq,elvDBE);
	bool in = (SQLITE_ROW == sqlite3_step(stm));
	sqlite3_finalize(stm);                      // Close statement
	return in;
}
//==============================================================================
//  Insert Elevation record
//==============================================================================
void SqlMGR::WriteElevationTRN(C_STile &sup,U_INT row)
{ int  rep = 0;
  char txn[TEX_NAME_DIM*16 + 2];
  char *rq =  "INSERT INTO TRN (qgt,sup,det,file,npod,spt,sub,user,water,nite,txn,nbe,elv) "  
              "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13);*";
  sqlite3_stmt *stm = CompileREQ(rq,elvDBE);
	//--- QGT Key -------------------------------------------
  rep = sqlite3_bind_int(stm, 1,sup.GetKey());
  if (rep != SQLITE_OK) Abort(elvDBE);
	//--- Supertile No --------------------------------------
  rep = sqlite3_bind_int(stm, 2,sup.GetNo());
  if (rep != SQLITE_OK) Abort(elvDBE);
	//--- Detail index --------------------------------------
  rep = sqlite3_bind_int(stm, 3, 0);
  if (rep != SQLITE_OK) Abort(elvDBE);
	//--- Origin file ---------------------------------------
  rep =  sqlite3_bind_int(stm, 4, row);
  if (rep != SQLITE_OK) Abort(elvDBE);
	//--- Pod name -----------------------------------------
	rep = sqlite3_bind_int(stm, 5, 0);				// Not used
  if (rep != SQLITE_OK) Abort(elvDBE);
	//--- Default or detail flag ----------------------------
	U_INT f0 = sup.GetTextureOrg();
  rep = sqlite3_bind_int(stm, 6,f0);
  if (rep != SQLITE_OK) Abort(elvDBE);
	//--- Subdivision level ---------------------------------
  rep = sqlite3_bind_int(stm, 7,(sup.side - 1));
  if (rep != SQLITE_OK) Abort(elvDBE);
	//--- User texture flags --------------------------------
	U_INT f1 = sup.GetTextureType(TC_TEXRAWTN);
  rep = sqlite3_bind_int(stm, 8,f1);
  if (rep != SQLITE_OK) Abort(elvDBE);
	//--- Water texture flags -------------------------------
	U_INT f2 = sup.GetTextureType(TC_TEXWATER);
  rep = sqlite3_bind_int(stm, 9,f2);
  if (rep != SQLITE_OK) Abort(elvDBE);
	//--- Night flags ---------------------------------------
	U_INT f3 = sup.GetNite();
  rep = sqlite3_bind_int(stm, 10,f3);
  if (rep != SQLITE_OK) Abort(elvDBE);
	//--- Get texture names ---------------------------------
	sup.PackTexName(txn);
  rep = sqlite3_bind_text(stm, 11,txn,-1,SQLITE_TRANSIENT);
	if (rep != SQLITE_OK) Abort(elvDBE);
  //----Number of elevations ------------------------------
	int	nbe = sup.GetSide() * sup.GetSide();	
  rep = sqlite3_bind_int(stm, 12,nbe);
  if (rep != SQLITE_OK) Abort(elvDBE);
	//--- Array of elevations -------------------------------
	int siz  = nbe * sizeof(float);
  rep = sqlite3_bind_blob(stm,13,sup.GetElvArray(), siz, SQLITE_TRANSIENT);

  //---Execute insert -------------------------------------------------------
  rep      = sqlite3_step(stm);               // Insert value in database
  if (rep != SQLITE_DONE) Abort(elvDBE);
  sqlite3_finalize(stm);                      // Close statement
  return;
}
//------------------------------------------------------------------------------
//	Patch detail tile texture: Restore generic texture construction
//------------------------------------------------------------------------------
void	SqlMGR::PatchDetailTRN(U_INT key,U_INT sno,U_INT det)
{	char rq[1024];  
  _snprintf(rq,1024,"UPDATE TRN SET det=%u WHERE (qgt=%d) AND (sup=%d);*",det,key,sno);
  sqlite3_stmt *stm = CompileREQ(rq,elvDBE);
	int rep = sqlite3_step(stm);
  if (SQLITE_ERROR == rep)	Abort(elvDBE);
	sqlite3_finalize(stm);                      // Close statement
	return;
}
//------------------------------------------------------------------------------
//	Read Patch detail tile texture: Restore generic texture construction
//------------------------------------------------------------------------------
U_INT	SqlMGR::ReadPatchDetail(U_INT key,U_INT sno)
{	U_INT P = 0;
	char rq[1024];  
  _snprintf(rq,1024,"SELECT det FROM TRN WHERE (qgt=%d) AND (sup=%d);*",key,sno);
  sqlite3_stmt *stm = CompileREQ(rq,elvDBE);
	if (SQLITE_ROW == sqlite3_step(stm)) P = sqlite3_column_int(stm, 0);
	sqlite3_finalize(stm);                      // Close statement
	return P;
}

//==============================================================================
//  Insert Elevation record
//	NOTE: parameter txn (?10) is skipped
//==============================================================================
void SqlMGR::WriteElevationDET(U_INT key,TRN_HDTL &hdl,int row)
{ int  rep = 0;  
  char *rq =  "INSERT INTO TILE (qgt,sup,det,file,flag,sub,nbe,elv) "  
              "VALUES(?1,?2,?3,?4,?5,?6,?7,?8);*";
  sqlite3_stmt *stm = CompileREQ(rq,elvDBE);
	//--- QGT key -------------------------------------
  rep = sqlite3_bind_int(stm, 1,key);
  if (rep != SQLITE_OK) Abort(elvDBE);
	//--- Super Tile number ----------------------------
  rep = sqlite3_bind_int(stm, 2,hdl.GetSupNo());
  if (rep != SQLITE_OK) Abort(elvDBE);
	//--- Detail Tile ----------------------------------
  rep = sqlite3_bind_int(stm, 3, hdl.GetTile());
  if (rep != SQLITE_OK) Abort(elvDBE);
	//--- Origin file -----------------------------------
  rep = sqlite3_bind_int(stm,4,row);
  if (rep != SQLITE_OK) Abort(elvDBE);
	//--- Detail elevation flag -------------------------
  rep = sqlite3_bind_int(stm, 5,ELV_DETAIL);
  if (rep != SQLITE_OK) Abort(elvDBE);
	//--- Subdivision level -----------------------------
  rep = sqlite3_bind_int(stm, 6,hdl.GetRes());
  if (rep != SQLITE_OK) Abort(elvDBE);
	//--- Number of elevations --------------------------
  rep = sqlite3_bind_int(stm, 7,hdl.GetArrayDim());
  if (rep != SQLITE_OK) Abort(elvDBE);
	//--- Array of elevations ---------------------------
	int siz  = hdl.GetArrayDim() * sizeof(int);
  rep = sqlite3_bind_blob(stm,8,hdl.GetElvArray(), siz, SQLITE_TRANSIENT);

  //---Execute insert -------------------------------------------------------
  rep      = sqlite3_step(stm);               // Insert value in database
  if (rep != SQLITE_DONE) Abort(elvDBE);
  sqlite3_finalize(stm);                      // Close statement
  return;
}
//==============================================================================
//  Begin a transaction on elevation
//==============================================================================
void SqlMGR::ELVtransaction()
{ char *rq = "BEGIN IMMEDIATE TRANSACTION;*";
  sqlite3_stmt *stm = CompileREQ(rq,elvDBE);
    //---Execute Transaction-------------------------------------------
  int rep  = sqlite3_step(stm);               // Start transaction
  if (rep != SQLITE_DONE) Abort(elvDBE);
  sqlite3_finalize(stm);                      // Close statement
  return;

}
//==============================================================================
//  End a transaction on elevation
//==============================================================================
void SqlMGR::ELVcommit()
{ char *rq = "COMMIT TRANSACTION;*";
  sqlite3_stmt *stm = CompileREQ(rq,elvDBE);
  //---Execute COMMIT-------------------------------------------
  int rep  = sqlite3_step(stm);               // Commit transaction
  if (rep != SQLITE_DONE) Abort(elvDBE);
  sqlite3_finalize(stm);                      // Close statement
  return;

}
//==============================================================================
//  Begin a transaction on COAST
//==============================================================================
void SqlMGR::SEAtransaction()
{ char *rq = "BEGIN IMMEDIATE TRANSACTION;*";
  sqlite3_stmt *stm = CompileREQ(rq,seaDBE);
    //---Execute Transaction-------------------------------------------
  int rep  = sqlite3_step(stm);               // Start transaction
  if (rep != SQLITE_DONE) Abort(seaDBE);
  sqlite3_finalize(stm);                      // Close statement
  return;

}
//==============================================================================
//  End a transaction on COAST
//==============================================================================
void SqlMGR::SEAcommit()
{ char *rq = "COMMIT TRANSACTION;*";
  sqlite3_stmt *stm = CompileREQ(rq,seaDBE);
  //---Execute COMMIT-------------------------------------------
  int rep  = sqlite3_step(stm);               // Commit transaction
  if (rep != SQLITE_DONE) Abort(seaDBE);
  sqlite3_finalize(stm);                      // Close statement
  return;
}
//==============================================================================
//  Insert COAST record
//==============================================================================
void SqlMGR::WriteCoastRec(COAST_REC &cst)
{ char *rq =  "INSERT INTO COAST (qgt,det,nbv,vtx) "  
              "VALUES(?1,?2,?3,?4);*";
  sqlite3_stmt *stm = CompileREQ(rq,seaDBE);
  int rep = 0;
  rep = sqlite3_bind_int(stm, 1,cst.qtk);
  if (rep != SQLITE_OK) Abort(seaDBE);
  rep = sqlite3_bind_int(stm, 2,cst.dtk);
  if (rep != SQLITE_OK) Abort(seaDBE);
  rep = sqlite3_bind_int(stm, 3,cst.nbv);
  if (rep != SQLITE_OK) Abort(seaDBE);
  rep = sqlite3_bind_blob(stm,4,cst.data, cst.dim, SQLITE_TRANSIENT);
  if (rep != SQLITE_OK) Abort(seaDBE);
  //-----Execute statement -----------------------------------------------
  rep      = sqlite3_step(stm);               // Insert value in database
  if (rep != SQLITE_DONE) Abort(seaDBE);
  sqlite3_finalize(stm);                      // Close statement
  return;
}
//==============================================================================
//  Insert Model record
//==============================================================================
void SqlMGR::Write3Dmodel(char *name,C3DPart *prt,double top, double bot)
{ char *rq =  "INSERT INTO MOD (name,type,tsp,ntex,ftop,fbot,lod,nvtx,nind,vtab,ntab,ttab,xtab) "  
              "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13);*";
  sqlite3_stmt *stm = CompileREQ(rq,modDBE);
  int nbv   = prt->GetNBVTX();
  int nbx   = prt->GetNbIND();
  int dimvx = nbv * sizeof(F3_VERTEX);
  int dimtx = nbv * sizeof(F2_COORD);
  int dimxx = nbx * sizeof(int);
  int type  = 0;
  int rep = 0;
  rep = sqlite3_bind_text(stm, 1,name, -1, SQLITE_TRANSIENT);
  if (rep != SQLITE_OK) Abort(modDBE);
  rep = sqlite3_bind_int (stm, 2, type);
  if (rep != SQLITE_OK) Abort(modDBE);
  rep = sqlite3_bind_int (stm, 3,prt->GetTSP());
  if (rep != SQLITE_OK) Abort(modDBE);
  rep = sqlite3_bind_text(stm, 4,prt->TextureName(),-1,SQLITE_TRANSIENT);
  if (rep != SQLITE_OK) Abort(modDBE);
  rep = sqlite3_bind_double(stm,5, top);					//double(prt->GetTop()));
  if (rep != SQLITE_OK) Abort(modDBE);
  rep = sqlite3_bind_double(stm,6, bot);					//double(prt->GetBot()));
  if (rep != SQLITE_OK) Abort(modDBE);
  rep = sqlite3_bind_int (stm, 7, prt->GetLOD());
  if (rep != SQLITE_OK) Abort(modDBE);
  rep = sqlite3_bind_int (stm, 8,nbv);
  if (rep != SQLITE_OK) Abort(modDBE);
  rep = sqlite3_bind_int (stm, 9,nbx);
  if (rep != SQLITE_OK) Abort(modDBE);
  rep = sqlite3_bind_blob(stm, 10,prt->GetVLIST(), dimvx, SQLITE_TRANSIENT);
  if (rep != SQLITE_OK) Abort(modDBE);
  rep = sqlite3_bind_blob(stm,11,prt->GetNLIST(), dimvx, SQLITE_TRANSIENT);
  if (rep != SQLITE_OK) Abort(modDBE);
  rep = sqlite3_bind_blob(stm,12,prt->GetTLIST(), dimtx, SQLITE_TRANSIENT);
  if (rep != SQLITE_OK) Abort(modDBE);
  rep = sqlite3_bind_blob(stm,13,prt->GetXLIST(), dimxx, SQLITE_TRANSIENT);
  if (rep != SQLITE_OK) Abort(modDBE);
  //-----Execute statement -----------------------------------------------
  rep      = sqlite3_step(stm);               // Insert value in database
  if (rep != SQLITE_DONE) Abort(modDBE);
  sqlite3_finalize(stm);                      // Close statement
  return;
}
//---------------------------------------------------------------------------------
//  Decode Model
//---------------------------------------------------------------------------------
int SqlMGR::DeleteM3DModel(char *fn)
{ char req[1024];
	int   nf = 0;                           // Number of faces
  _snprintf(req,1024,"SELECT * FROM mod WHERE name='%s';*",fn);
  sqlite3_stmt *stm = CompileREQ(req,modDBE);
  while (SQLITE_ROW == sqlite3_step(stm)) nf += RemoveM3DTexture(stm);
  //---Free statement --------------------------------------------------
  sqlite3_finalize(stm);
	RemoveM3DModel(fn);
  return 0;
}
//------------------------------------------------------------------------
//  Remove texture from table
//------------------------------------------------------------------------
int SqlMGR::RemoveM3DTexture(sqlite3_stmt *stm)
{ char req[1024];
	char *txn = (char*)sqlite3_column_text(stm,CLN_MOD_TXN);
  _snprintf(req,1024,"DELETE FROM texture WHERE xname='%s';*",txn);
  sqlite3_stmt *std = CompileREQ(req,modDBE);
	int rep = sqlite3_step(std);                // Execute Statement
  sqlite3_finalize(std);                      // Close statement
  return 0;
}
//------------------------------------------------------------------------
//  Remove texture from table
//------------------------------------------------------------------------
int SqlMGR::RemoveM3DModel(char *name)
{	char req[1024];
	_snprintf(req,1024,"DELETE FROM mod WHERE name='%s';*",name);
  sqlite3_stmt *stm = CompileREQ(req,modDBE);
	int rep = sqlite3_step(stm);                // Execute Statement
  sqlite3_finalize(stm);                      // Close statement
  return 0;
}
//---------------------------------------------------------------------------------
//  check for the model reference
//---------------------------------------------------------------------------------
bool SqlMGR::Check3DModel(char *name)
{ char req[1024];
	_snprintf(req,1024,"SELECT name FROM mod WHERE name='%s';*",name);
  sqlite3_stmt *stm = CompileREQ(req,modDBE);
  //-----Execute statement -----------------------------------------------
  int rep = sqlite3_step(stm);                  // Search database
  sqlite3_finalize(stm);                        // Close statement
  return (rep == SQLITE_ROW)?(true):(false);
}
//------------------------------------------------------------------------
//  Insert a M3D texture in table
//------------------------------------------------------------------------
int SqlMGR::WriteM3DTexture(TEXT_INFO &inf)
{ char req[1024];
	int dim = inf.wd * inf.ht * inf.bpp;
  strcpy(req,"INSERT INTO texture (xname,wd,ht,tex) VALUES(?1,?2,?3,?4);*");
  sqlite3_stmt *stm = CompileREQ(req,modDBE);
  int rep  = sqlite3_bind_text(stm, 1,inf.name, -1, SQLITE_TRANSIENT);
  if (rep != SQLITE_OK) Abort(modDBE);
  rep      = sqlite3_bind_int (stm, 2,inf.wd);
  if (rep != SQLITE_OK) Abort(modDBE);
  rep      = sqlite3_bind_int (stm, 3,inf.ht);
  if (rep != SQLITE_OK) Abort(modDBE);
  rep      = sqlite3_bind_blob(stm, 4,inf.mADR, dim, SQLITE_TRANSIENT);
  if (rep != SQLITE_OK) Abort(modDBE);
  rep      = sqlite3_step(stm);               // Insert value in database
  if (rep != SQLITE_DONE) Abort(modDBE);
  sqlite3_finalize(stm);                      // Close statement
  return 0;
}

//---------------------------------------------------------------------------------
//  check for the texture reference
//---------------------------------------------------------------------------------
bool SqlMGR::Check3DTexture(char *name)
{ char req[1024];
	_snprintf(req,1024,"SELECT xname FROM texture WHERE xname='%s';*",name);
  sqlite3_stmt *stm = CompileREQ(req,modDBE);
  //-----Execute statement -----------------------------------------------
  int rep = sqlite3_step(stm);                  // Search database
  sqlite3_finalize(stm);                        // Close statement
  return (rep == SQLITE_ROW)?(true):(false);
}

//==============================================================================
//  Insert Pavement Record for taxiways
//==============================================================================
void SqlMGR::WritePavement(CPaveRWY *pave, char * key)
{ char *rq =  "INSERT INTO Pave (aKey,Type,aUse,Nbvt,vTab) "  
              "VALUES(?1,?2,?3,?4,?5);*";
  //---- Compile query -----------------------------------------
  sqlite3_stmt *stm = CompileREQ(rq,txyDBE);
  int  rep = 0;
  rep = sqlite3_bind_text(stm, 1,key, -1, SQLITE_TRANSIENT);
  if (rep != SQLITE_OK) Abort(txyDBE);
  rep = sqlite3_bind_int (stm, 2,pave->GetType());
  if (rep != SQLITE_OK) Abort(txyDBE);
  rep = sqlite3_bind_int (stm, 3,pave->GetUsage());
  if (rep != SQLITE_OK) Abort(txyDBE);
  rep = sqlite3_bind_int (stm, 4,pave->GetNBVT());
  if (rep != SQLITE_OK) Abort(txyDBE);
  //----Blob part ----------------------------------------
  int   dim   = pave->GetNBVT() * sizeof(TC_VTAB);
  void *data  = pave->GetVTAB();
  rep = sqlite3_bind_blob(stm, 5,data, dim, SQLITE_TRANSIENT);
  if (rep != SQLITE_OK) Abort(txyDBE);
  rep      = sqlite3_step(stm);               // Insert value in database
  if (rep != SQLITE_DONE) Abort(txyDBE);
  sqlite3_finalize(stm);                      // Close statement
  return;
}
//==============================================================================
//  Insert Light Record for taxiways
//==============================================================================
void SqlMGR::WriteTaxiLigth(CBaseLITE *lite, char * key)
{ char *rq =  "INSERT INTO Light (aKey,Type,Txtr,sCol,sPm1,sPm2,Nbs,vTab) "
              "VALUES(?1,?2,?3,?4,?5,?6,?7,?8);*";
  //---- Compile query -----------------------------------------
  sqlite3_stmt *stm = CompileREQ(rq,txyDBE);
  int  rep = 0;
  rep = sqlite3_bind_text(stm, 1,key, -1, SQLITE_TRANSIENT);
  if (rep != SQLITE_OK) Abort(txyDBE);
  rep = sqlite3_bind_int (stm, 2,lite->GetType());
  if (rep != SQLITE_OK) Abort(txyDBE);
  rep = sqlite3_bind_int (stm, 3,lite->GetTextureNb());
  if (rep != SQLITE_OK) Abort(txyDBE);
  rep = sqlite3_bind_int (stm, 4,lite->GetColor1());
  if (rep != SQLITE_OK) Abort(txyDBE);
  rep = sqlite3_bind_int (stm, 5,0);
  if (rep != SQLITE_OK) Abort(txyDBE);
  rep = sqlite3_bind_int (stm, 6,0);
  if (rep != SQLITE_OK) Abort(txyDBE);
  rep = sqlite3_bind_int (stm, 7,lite->GetNbSpot());
  if (rep != SQLITE_OK) Abort(txyDBE);
  //----Blob part ----------------------------------------
  int   dim   = lite->GetNbSpot() * sizeof(TC_VTAB);
  void *data  = lite->GetVTAB();
  rep = sqlite3_bind_blob(stm, 8,data, dim, SQLITE_TRANSIENT);
  if (rep != SQLITE_OK) Abort(txyDBE);
  rep      = sqlite3_step(stm);               // Insert value in database
  if (rep != SQLITE_DONE) Abort(txyDBE);
  sqlite3_finalize(stm);                      // Close statement
  return;
}
//------------------------------------------------------------------------------
//  Decode Pavement Record for taxiways  from TXY Database
//------------------------------------------------------------------------------
int SqlMGR::DecodePAVE(CAptObject *apo)
{ char req[1024];
	int rec = 0;
  _snprintf(req,1024,"SELECT * FROM Pave WHERE akey='%s';*",apo->GetKey());
  sqlite3_stmt *stm = CompileREQ(req,txyDBE);
  //----Execute select -------------------------------------------
  while (SQLITE_ROW == sqlite3_step(stm))
  {int nbv =  sqlite3_column_int(stm, CLN_PAV_NBV);       // Number of vertices
   CPaveRWY *pave = new CPaveRWY(nbv);                    // A new pavement structure
   int type = sqlite3_column_int(stm, CLN_PAV_TYP);       // Pavement type
   int ause = sqlite3_column_int(stm, CLN_PAV_USE);       // Airport usage
   pave->SetType(type);
   pave->SetUsage(ause);
   pave->SetMode(GL_TRIANGLES);
   //---Fill vertex table ---------------------------------------
   TC_VTAB  *dst = pave->GetVTAB();
   TC_VTAB  *src = (TC_VTAB*) sqlite3_column_blob(stm,CLN_PAV_TAB);
   for (int k=0; k < nbv; k++)
      {  *dst = *src++;
          apo->ComputeElevation(dst++);
      }
   //---Send pavement to airport --------------------------------
   apo->OnePavement(pave,nbv);
   rec++;
   }
  //---Free statement --------------------------------------------------
  sqlite3_finalize(stm);
  return rec;
}
//------------------------------------------------------------------------------
//  Decode Light Record for taxiways  from TXY Database
//------------------------------------------------------------------------------
int  SqlMGR::DecodeLITE(CAptObject *apo)
{ char req[1024];
	int rec = 0;
  _snprintf(req,1024,"SELECT * FROM Light WHERE akey='%s';*",apo->GetKey());
  sqlite3_stmt *stm = CompileREQ(req,txyDBE);
    //----Execute select -------------------------------------------
  while (SQLITE_ROW == sqlite3_step(stm))
  {int nbs    = sqlite3_column_int(stm, CLN_LIT_NBS);       // Number of spots
   int type   = sqlite3_column_int(stm, CLN_LIT_TYP);       // Light Type 
   int texn   = sqlite3_column_int(stm, CLN_LIT_TEX);       // Texture number
   int coln   = sqlite3_column_int(stm, CLN_LIT_COL);       // Color number
 //  double pm1 = sqlite3_column_double(stm, CLN_LIT_PM1);  // Parameter 1 (ignore for now)
 //  double pm2 = sqlite3_column_double(stm, CLN_LIT_PM2);  // Parameter 2 (ignore for now)
   CBaseLITE *lite = new CBaseLITE((U_CHAR)type,nbs);       // A new Light structure
   lite->SetColor1(coln);                                   // Set color (OMNI type)
   lite->SetTextureNb(texn);                                // Set Texture
   //---Fill vertex table ---------------------------------------
   TC_VTAB  *dst = lite->GetVTAB();
   TC_VTAB  *src = (TC_VTAB*) sqlite3_column_blob(stm,CLN_LIT_TAB);
   for (int k=0; k < nbs; k++)
      {  *dst = *src++;
          apo->ComputeElevation(dst++);
      }
   //---Send Light to airport --------------------------------
   apo->OneTaxiLight(lite);
   rec++;
  }
  //---Free statement --------------------------------------------------
  sqlite3_finalize(stm);
  return rec;
}
//------------------------------------------------------------------------------
//  Check for Taxiway in Database
//------------------------------------------------------------------------------
bool SqlMGR::FileInTXY(char *akey)
{	char rq[1024];
	_snprintf(rq,1023,"SELECT * FROM Pave WHERE akey='%s';*",akey);
	sqlite3_stmt *stm = CompileREQ(rq,txyDBE);
	bool in = (SQLITE_ROW == sqlite3_step(stm));
	sqlite3_finalize(stm);                      // Close statement
	return in;
}

//==============================================================================
//  TEXTURE ROUTINES
//==============================================================================
//  Insert BLOB into detail elevation.  return the rowid of record
//------------------------------------------------------------------------
int SqlMGR::WriteGenTexture(TEXT_INFO &inf,char *tab)
{ char req[1024];
	int dim  = inf.wd * inf.ht * inf.bpp;
  _snprintf(req,1024,"INSERT INTO %s (name,wd,ht,tex) VALUES(?1,?2,?3,?4);*",tab);
  sqlite3_stmt *stm = CompileREQ(req,texDBE);
  int rep  = sqlite3_bind_text(stm, 1,inf.path, -1, SQLITE_TRANSIENT);
  if (rep != SQLITE_OK) Abort(texDBE);
  rep      = sqlite3_bind_int (stm, 2, inf.wd);
  if (rep != SQLITE_OK) Abort(texDBE);
  rep      = sqlite3_bind_int (stm, 3, inf.ht);
  if (rep != SQLITE_OK) Abort(texDBE);
  rep      = sqlite3_bind_blob(stm, 4,inf.mADR, dim, SQLITE_TRANSIENT);
  if (rep != SQLITE_OK) Abort(texDBE);
  rep      = sqlite3_step(stm);               // Insert value in database
  if (rep != SQLITE_DONE) Abort(texDBE);
  sqlite3_finalize(stm);                      // Close statement
  int seq  = sqlite3_last_insert_rowid(texDBE.sqlOB);   // Last ROWID
  return seq;
}
//------------------------------------------------------------------------
//  Insert a texture in table
//------------------------------------------------------------------------
int SqlMGR::WriteAnyTexture(TEXT_INFO &inf)
{ char req[1024];
	int dim = inf.wd * inf.ht * inf.bpp;
  strcpy(req,"INSERT INTO texture (name,wd,ht,tex) VALUES(?1,?2,?3,?4);*");
  sqlite3_stmt *stm = CompileREQ(req,texDBE);
  int rep  = sqlite3_bind_text(stm, 1,inf.path, -1, SQLITE_TRANSIENT);
  if (rep != SQLITE_OK) Abort(texDBE);
  rep      = sqlite3_bind_int (stm, 2, inf.wd);
  if (rep != SQLITE_OK) Abort(texDBE);
  rep      = sqlite3_bind_int (stm, 3, inf.ht);
  if (rep != SQLITE_OK) Abort(texDBE);
  rep      = sqlite3_bind_blob(stm, 4, inf.mADR, dim, SQLITE_TRANSIENT);
  if (rep != SQLITE_OK) Abort(texDBE);
  rep      = sqlite3_step(stm);               // Insert value in database
  if (rep != SQLITE_DONE) Abort(texDBE);
  sqlite3_finalize(stm);                      // Close statement
  return 0;
}

//------------------------------------------------------------------------
//  End a transaction on texture
//------------------------------------------------------------------------
void SqlMGR::TEXcommit()
{ char *rq = "COMMIT TRANSACTION;*";
  sqlite3_stmt *stm = CompileREQ(rq,texDBE);
  //---Execute COMMIT-------------------------------------------
  int rep  = sqlite3_step(stm);               // Commit transaction
  if (rep != SQLITE_DONE) Abort(texDBE);
  sqlite3_finalize(stm);                      // Close statement
  return;

}
//------------------------------------------------------------------------
//  Decode a Generic Texture
//------------------------------------------------------------------------
GLubyte *SqlMGR::GetGenTexture(TEXT_INFO &txd)
{ char *name = txd.path;
	char *dot = strrchr(name,'.');
  char *typ = dot - 1;
  char *res = typ - 1;
  char *tab = 0;
	if (dot == 0)		gtfo("GetGenTexture bad name");
  char *txn = strrchr(name,'/') + 1;
  if (*res == '4')  {tab = "DAY128";}
  if (*res == '5')  {tab = "DAY256";}
  if (*typ == 'N')  {tab = "NIT128";}
  char req[1024];
  char *tex = 0;
  *dot = 0;
  _snprintf(req,1024,"SELECT * FROM %s WHERE name='%s';*",tab,txn);
  *dot = '.';
  sqlite3_stmt *stm = CompileREQ(req,texDBE);
  if (SQLITE_ROW == sqlite3_step(stm))
  { //---- Decode the texture and return it ----------------------------
    int   dim =  sqlite3_column_bytes(stm,CLN_TXT_IMG);
    char *src =  (char*)sqlite3_column_blob (stm,CLN_TEX_IMG);
    tex = new char[dim];
    //---Copy texture to destination -----------------------------------
		memcpy(tex,src,dim);
		//--- Update txd parameters ----------------------------------------
		int wd    = sqlite3_column_int (stm,CLN_TXT_WID);
    txd.wd    = wd;
    txd.ht    = wd;
    txd.mADR  = (GLubyte*)tex;
	}
  //---Free statement --------------------------------------------------
  sqlite3_finalize(stm);
  return (GLubyte*)tex;
}
//------------------------------------------------------------------------
//  Get any other texture
//------------------------------------------------------------------------
GLubyte *SqlMGR::GetAnyTexture(TEXT_INFO &inf)
{ char  req[1024];
  inf.mADR = 0;
  _snprintf(req,1024,"SELECT * FROM texture WHERE name='%s';*",inf.name);
  sqlite3_stmt *stm = CompileREQ(req,texDBE);
  if (SQLITE_ROW != sqlite3_step(stm)) {
    sqlite3_finalize(stm); // lc 052310 +
    return 0;
  }
  //----Store width and height for further query -----------------------
  inf.wd  = sqlite3_column_int(stm,CLN_TXT_WID);
  inf.ht  = sqlite3_column_int(stm,CLN_TXT_HTR);
  //--------------------------------------------------------------------
  int   dim =  sqlite3_column_bytes(stm,CLN_TXT_IMG);
  char *src =  (char*)sqlite3_column_blob (stm,CLN_TXT_IMG);
  char *tex = new char[dim];
	memcpy(tex,src,dim);
  //---Free statement --------------------------------------------------
  inf.mADR = (GLubyte*)tex;
  sqlite3_finalize(stm);
  return inf.mADR;
}
//------------------------------------------------------------------------
//  Decode texture line
//------------------------------------------------------------------------
void SqlMGR::DecodeLinTexture(sqlite3_stmt *stm, CTgxLine *lin)
{ int   typ =        sqlite3_column_int (stm,CLN_LST_TYP); 
  char *nit = (char*)sqlite3_column_text(stm,CLN_LST_NIT);
  char *dsc = (char*)sqlite3_column_text(stm,CLN_LST_DSC);
  lin->SetType(typ);
  lin->SetNite(*nit);
  lin->SetLabel(typ,dsc);
  return;
}
//------------------------------------------------------------------------
//  Get all texture list
//------------------------------------------------------------------------
void SqlMGR::GetTerraList(tgxFunCB *fun)
{ char req[1024];
  _snprintf(req,1024,"SELECT * FROM TLIST;*");
  sqlite3_stmt *stm = CompileREQ(req,texDBE);
  while (SQLITE_ROW == sqlite3_step(stm))
    { CTgxLine *lin = new CTgxLine;
      DecodeLinTexture(stm,lin);
      if (fun)  fun(lin);
    }
  //-----Close request ---------------------------------------------------
  sqlite3_finalize(stm);
  return;
}
//------------------------------------------------------------------------
//  Get all texture list
//------------------------------------------------------------------------
void SqlMGR::GetTerraSQL(TCacheMGR *tcm)
{ char req[1024];
  _snprintf(req,1024,"SELECT * FROM TLIST;*");
  sqlite3_stmt *stm = CompileREQ(req,texDBE);
  while (SQLITE_ROW == sqlite3_step(stm))
    { CTgxLine *lin = new CTgxLine;
      DecodeLinTexture(stm,lin);
      tcm->OneTerraLine(lin);
    }
    //-----Close request ---------------------------------------------------
    sqlite3_finalize(stm);
    return;
}
//=================================================================================
//  Write a World Object model
//=================================================================================
void SqlMGR::WriteWOBJ(U_INT qgt,CWobj *obj,int row)
{ char req[1024];
  strcpy(req,"INSERT INTO OBJ (qgt,type,xk,yk,kind,flag,mday,nday,lon,lat,xori,yori,zori,fobj,nozb,nozu,pm1,pm2,name,desc,scn)" 
    "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16,?17,?18,?19,?20,?21);*");
  sqlite3_stmt *stm = CompileREQ(req,objDBE);
  //---Extract all value from this object ----------------------
  U_INT xk = obj->GetIntLongitude();
  U_INT yk = obj->GetIntLatitude();
  SPosition *pos = obj->ObjPosition();
  SVector   *ori = obj->ObjOrientation();
  //---Write the record ----------------------------------------
  int rep  = sqlite3_bind_int (stm, 1, qgt);                // QGT Key
  if (rep != SQLITE_OK) Abort(objDBE);
	int typ  = obj->GetType();
	rep			 = sqlite3_bind_int (stm, 2, typ);                 // object type
	if (rep != SQLITE_OK) Abort(objDBE);
  rep      = sqlite3_bind_int (stm, 3, xk);                 // integer longitude 
  if (rep != SQLITE_OK) Abort(objDBE);
  rep      = sqlite3_bind_int (stm, 4, yk);                 // integer latitude 
  if (rep != SQLITE_OK) Abort(objDBE);
  rep      = sqlite3_bind_int (stm, 5, obj->GetKind());     // Object kind 
  if (rep != SQLITE_OK) Abort(objDBE);
  rep      = sqlite3_bind_int (stm, 6, obj->GetFlag());     // Object flag
  if (rep != SQLITE_OK) Abort(objDBE);
  rep      = sqlite3_bind_text(stm, 7, obj->DayName(), -1, SQLITE_TRANSIENT);
  if (rep != SQLITE_OK) Abort(objDBE);
  rep      = sqlite3_bind_text(stm, 8, obj->NitName(), -1, SQLITE_TRANSIENT);
  if (rep != SQLITE_OK) Abort(objDBE);
  //------World coordinates --------------------------------------------
  rep      = sqlite3_bind_double(stm, 9, pos->lon);         // Longitude
  if (rep != SQLITE_OK) Abort(objDBE);
  rep      = sqlite3_bind_double(stm, 10, pos->lat);         // Latitude
  if (rep != SQLITE_OK) Abort(objDBE);
  //------Object orientation -------------------------------------------
  rep      = sqlite3_bind_double(stm,11, ori->x);           // X (pitch)
  if (rep != SQLITE_OK) Abort(objDBE);
  rep      = sqlite3_bind_double(stm,12, ori->y);           // Y (bank)
  if (rep != SQLITE_OK) Abort(objDBE);
  rep      = sqlite3_bind_double(stm,13, ori->z);           // Y (head)
  if (rep != SQLITE_OK) Abort(objDBE);
  //-----Mother file index -------------------------------------------
  rep      = sqlite3_bind_int(stm,  14, row);
  if (rep != SQLITE_OK) Abort(objDBE);
  //-----ATTRIBUTES -----------------------------------------------------
  rep      = sqlite3_bind_int (stm,  15, obj->GetZB());     // Object flag
  if (rep != SQLITE_OK) Abort(objDBE);
  rep      = sqlite3_bind_int (stm,  16, obj->GetZU());     // Object flag
  if (rep != SQLITE_OK) Abort(objDBE);
  //----PARAMETERS--------------------------------------------------------
  rep      = sqlite3_bind_int (stm,  17, obj->GetPM1());     // Parameter 1
  if (rep != SQLITE_OK) Abort(objDBE);
  rep      = sqlite3_bind_int (stm,  18, obj->GetPM2());     // Parameter 2
  if (rep != SQLITE_OK) Abort(objDBE);
  //----Object name and description --------------------------------------
  rep      = sqlite3_bind_text(stm,  19, obj->GetName(), -1, SQLITE_TRANSIENT);
  if (rep != SQLITE_OK) Abort(objDBE);
  rep      = sqlite3_bind_text(stm,  20, obj->GetDescription(), -1, SQLITE_TRANSIENT);
  if (rep != SQLITE_OK) Abort(objDBE);
	//--- Object scene name index ------------------------------------------
	rep			 =  sqlite3_bind_int (stm, 21, obj->GetScene());     // Scene index
	if (rep != SQLITE_OK) Abort(objDBE);
  //----End of statement -------------------------------------------------
  rep      = sqlite3_step(stm);               // Insert value in database
  if (rep != SQLITE_DONE) Abort(objDBE);
  sqlite3_finalize(stm);                      // Close statement

  WriteLightsFrom(obj);
  return;
}
//---------------------------------------------------------------------------------
//  Write a Light
//---------------------------------------------------------------------------------
void SqlMGR::Write3DLight(CWobj *obj,C3DLight *lit)
{ char req[1024];
  strcpy(req,"INSERT INTO Light (xk,yk,mod,mode,color,texture,radius,freq,ontime,dx,dy,dz)" 
    "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12);*");
  sqlite3_stmt *stm = CompileREQ(req,objDBE);
  //---Extract all value from this object ---------------------------
  U_INT xk = obj->GetIntLongitude();
  U_INT yk = obj->GetIntLatitude();
  SPosition *ofs = lit->ObjPosition();
  //-----------------------------------------------------------------
  int rep  = sqlite3_bind_int (stm, 1, xk);                       // integer longitude 
  if (rep != SQLITE_OK) Abort(objDBE);
  rep      = sqlite3_bind_int (stm, 2, yk);                       // integer latitude 
  if (rep != SQLITE_OK) Abort(objDBE);
  rep      = sqlite3_bind_text(stm, 3, obj->DayName(), -1, SQLITE_TRANSIENT);
  if (rep != SQLITE_OK) Abort(objDBE);
  rep      = sqlite3_bind_int (stm, 4, lit->GetMode());           // integer latitude 
  if (rep != SQLITE_OK) Abort(objDBE);
  rep      = sqlite3_bind_int (stm, 5, lit->GetColor());          // integer latitude 
  if (rep != SQLITE_OK) Abort(objDBE);
  rep      = sqlite3_bind_int (stm, 6, lit->GetNTex());           // integer latitude 
  if (rep != SQLITE_OK) Abort(objDBE);
  rep      = sqlite3_bind_double(stm,7, lit->GetRadius());        // Halo radius)
  if (rep != SQLITE_OK) Abort(objDBE);
  rep      = sqlite3_bind_double(stm,8, lit->GetFreq());          // Frequence)
  if (rep != SQLITE_OK) Abort(objDBE);
  rep      = sqlite3_bind_double(stm,9, lit->GetOntm());          // On time
  if (rep != SQLITE_OK) Abort(objDBE);
  //----Light offset --------------------------------------------------
  rep      = sqlite3_bind_double(stm,10, ofs->lon);                // dx
  if (rep != SQLITE_OK) Abort(objDBE);
  rep      = sqlite3_bind_double(stm,11, ofs->lat);                // dy
  if (rep != SQLITE_OK) Abort(objDBE);
  rep      = sqlite3_bind_double(stm,12, ofs->alt);                // dy
  if (rep != SQLITE_OK) Abort(objDBE);
  //----End of statement -------------------------------------------------
  rep      = sqlite3_step(stm);               // Insert value in database
  if (rep != SQLITE_DONE) Abort(objDBE);
  sqlite3_finalize(stm);                      // Close statement
  return;
}
//---------------------------------------------------------------------------------
//  Write all light from this object
//---------------------------------------------------------------------------------
void SqlMGR::WriteLightsFrom(CWobj *obj)
{ C3DLight *lit = obj->PopALight();
  while (lit)
  { Write3DLight(obj,lit);
    lit = obj->PopALight();
  }
  return;
}
//---------------------------------------------------------------------------------
//  Update the nozb flag
//---------------------------------------------------------------------------------
void SqlMGR::UpdateOBJzb(CWobj *obj)
{	char req[1024];
	int zb = obj->GetZB();
	char *mod = obj->DayName();
  _snprintf(req,1024,"UPDATE obj SET nozb = %d WHERE mday = '%s';*",zb,mod);
  sqlite3_stmt *stm = CompileREQ(req,objDBE);
	int rep = sqlite3_step(stm);
  if (SQLITE_ERROR == rep)	Abort(objDBE);
	sqlite3_finalize(stm);                      // Close statement
	return;;
}
//---------------------------------------------------------------------------------
//  Update the nozb flag
//---------------------------------------------------------------------------------
void SqlMGR::UpdateOBJzu(CWobj *obj)
{	char req[1024];
	int zb = obj->GetZB();
	char *mod = obj->DayName();
  _snprintf(req,1024,"UPDATE obj SET nozu = %d WHERE mday = '%s';*",zb,mod);
  sqlite3_stmt *stm = CompileREQ(req,objDBE);
	int rep = sqlite3_step(stm);
  if (SQLITE_ERROR == rep)	Abort(objDBE);
	sqlite3_finalize(stm);                      // Close statement
	return;;
}
//==============================================================================
//  Check for POD-OBJ in Database
//==============================================================================
bool SqlMGR::FileInOBJ(char *fn)
{	char rq[1024];
	_snprintf(rq,1023,"SELECT * from FNM where file = '%s';*",fn);
	sqlite3_stmt *stm = CompileREQ(rq,objDBE);
	bool in = (SQLITE_ROW == sqlite3_step(stm));
	sqlite3_finalize(stm);                      // Close statement
	return in;
}
//=================================================================================
//  Read a set of World Object model
//=================================================================================
int SqlMGR::ReadWOBJ(C_QGT *qgt)
{ U_INT key = qgt->FullKey();
  char req[1024];
  _snprintf(req,1024,"SELECT * FROM OBJ where qgt='%d';*",key);
  sqlite3_stmt *stm = CompileREQ(req,objDBE);
	int cnto		= 0;
  while (SQLITE_ROW == sqlite3_step(stm))
    { CWobj *obj = 0;
      int kind =  sqlite3_column_int (stm,CLN_OBJ_KND);
      if (kind == 'mobj')  obj = new CWobj(kind);
      if (kind == 'nvor')  obj = new CWvor(kind);
      if (kind == 'wdsk')  obj = new CWobj(kind);
      if (kind == 'gvew')  obj = new CGvew(kind);
      if (kind == 'wsok')  obj = new CWsok(kind);
      if (kind == 'atna')  obj = new CWobj(kind);
      if (kind == 'slit')  obj = new CWobj(kind);
      if (0 == obj)   continue;
      //---Process this object ---------------------------------
      int xk = sqlite3_column_int (stm,CLN_OBJ_XKP);
      int yk = sqlite3_column_int (stm,CLN_OBJ_YKP);
      DecodeWOBJ(stm,obj);
      ReadOBJLite(obj,xk,yk);
      C3Dworld *w3d = qgt->Get3DW();
      w3d->AddToWOBJ(obj);
			cnto++;
    }
  //---Free statement ------------------------------------------
  sqlite3_finalize(stm);
  return cnto;
}
//---------------------------------------------------------------------------------
//  Select al objects from the same file name
//---------------------------------------------------------------------------------
int  SqlMGR::SearchWOBJ(char *fn)
{ int nb = 0;
	char req[1024];
  _snprintf(req,1024,"SELECT qgt FROM OBJ where fobj LIKE '%%%s%%';*",fn);
	sqlite3_stmt *stm = CompileREQ(req,objDBE);
	nb = (SQLITE_ROW == sqlite3_step(stm))?(1):(0);
	sqlite3_finalize(stm);
	return nb;
}
//---------------------------------------------------------------------------------
//  Select a POD file name
//---------------------------------------------------------------------------------
bool  SqlMGR::SearchPODinOBJ(char *pn)
{ int nb = 0;
	char req[1024];
  _snprintf(req,1024,"SELECT file FROM FNM where file LIKE '%%%s%%';*",pn);
	sqlite3_stmt *stm = CompileREQ(req,objDBE);
	nb = (SQLITE_ROW == sqlite3_step(stm))?(1):(0);
	sqlite3_finalize(stm);
	return (nb != 0);
}

//---------------------------------------------------------------------------------
//  Decode the current object
//---------------------------------------------------------------------------------
void SqlMGR::DecodeWOBJ(sqlite3_stmt *stm,CWobj *obj)
{ int flg = sqlite3_column_int (stm,CLN_OBJ_FLG);
  obj->SetFlag(flg);
  char *day = (char*)sqlite3_column_text(stm,CLN_OBJ_DAY);
  obj->SetDayRef(day);
  char *nit = (char*)sqlite3_column_text(stm,CLN_OBJ_NIT);
  if (*nit) obj->SetNitRef(nit);
	//---Type ----------------------------------------------------
	int	t			= sqlite3_column_int (stm,CLN_OBJ_TYP);
	obj->SetType(t);
  //---Get position --------------------------------------------
  SPosition *pos = obj->ObjPosition();
  pos->lon	= sqlite3_column_double(stm, CLN_OBJ_LON);
  pos->lat	= sqlite3_column_double(stm, CLN_OBJ_LAT);
  //---Get Orientation -----------------------------------------
	SVector v;
	v.x		= sqlite3_column_double(stm, CLN_OBJ_ORX); 
  v.y		= sqlite3_column_double(stm, CLN_OBJ_ORY);
  v.z		= sqlite3_column_double(stm, CLN_OBJ_ORZ);
	obj->SetOrientation(v);
  //---Other flags ---------------------------------------------
  int zb		= sqlite3_column_int (stm,CLN_OBJ_NZB);
  obj->SetNOZB(zb);
  int zu		= sqlite3_column_int (stm,CLN_OBJ_NZU);
  obj->SetNOZU(zu);
  //--- Parameters ---------------------------------------------
  int pm1		= sqlite3_column_int (stm,CLN_OBJ_PM1);
  obj->SetPM1(pm1);
  int pm2		= sqlite3_column_int (stm,CLN_OBJ_PM2);
  obj->SetPM2(pm2);
  //----Object name and description ----------------------------
  char *nam = (char*)sqlite3_column_text(stm,CLN_OBJ_NAM);
  if (nam)  obj->SetObjName(nam);
  char *dsc = (char*)sqlite3_column_text(stm,CLN_OBJ_DSC);
  if (dsc) obj->SetObjDesc(dsc);
	int scn = sqlite3_column_int(stm,CLN_OBJ_SCN);
  obj->SetScene(scn);
  //---Mother file ---------------------------------------------
  int fob = sqlite3_column_int(stm,CLN_OBJ_FOB);
	ReadOBJFile(obj,fob);
  return;
}
//---------------------------------------------------------------------------------
//  Read object lights
//---------------------------------------------------------------------------------
void SqlMGR::ReadOBJLite(CWobj *obj,int xk, int yk)
{ //---Extract parameters -------------------------------------
  char *mod = obj->DayName();
  char req[1024];
  _snprintf(req,1024,"SELECT * FROM Light where xk='%d' and yk='%d' and mod='%s';*",xk,yk,mod);
  sqlite3_stmt *stm = CompileREQ(req,objDBE);
  while (SQLITE_ROW == sqlite3_step(stm))
  { Decode3DLight(stm,obj);
  }
  //---Free statement ------------------------------------------
  sqlite3_finalize(stm);
  return;
}
//---------------------------------------------------------------------------------
//  Read object texture
//---------------------------------------------------------------------------------
void SqlMGR::ReadOBJFile(CWobj *obj,int row)
{ //---Extract parameters -------------------------------------
  char req[1024];
  _snprintf(req,1024,"SELECT * FROM fnm where rowid=%d;*",row);
  sqlite3_stmt *stm = CompileREQ(req,objDBE);
  if (SQLITE_ROW == sqlite3_step(stm))
  { char *fob = (char*)sqlite3_column_text(stm,0);
	  if (fob)	obj->SetFileName(fob);
  }
  //---Free statement ------------------------------------------
  sqlite3_finalize(stm);
  return;
}

//---------------------------------------------------------------------------------
//  Decode the current light
//---------------------------------------------------------------------------------
void SqlMGR::Decode3DLight(sqlite3_stmt *stm,CWobj *obj)
{ C3DLight *lit = new C3DLight(obj,TC_LMOD_OMNI);
  //----Set Type -----------------------------------------------
  int mod =  sqlite3_column_int (stm,CLN_LTO_MOD);
  lit->SetMode(mod);
  int col =  sqlite3_column_int (stm,CLN_LTO_COL);
  lit->SetColor(col);
  int txu =  sqlite3_column_int (stm,CLN_LTO_NTX);
  lit->SetTexture(txu);
  float rad = sqlite3_column_double(stm, CLN_LTO_RAD);
  lit->SetRadius(rad);
  float frq = sqlite3_column_double(stm, CLN_LTO_FRQ);
  lit->SetFreq(frq);
  float ont = sqlite3_column_double(stm, CLN_LTO_ONT);
  lit->SetOntm(ont);
  //----Set Light offset ---------------------------------------
  SPosition *ofs = lit->ObjPosition();
  ofs->lon  = sqlite3_column_double(stm, CLN_LTO_XOF); 
  ofs->lat  = sqlite3_column_double(stm, CLN_LTO_YOF);
  ofs->alt  = sqlite3_column_double(stm, CLN_LTO_ZOF);
  //----Add the light -------------------------------------------
  obj->PushLight(lit);
  return;
}
//=================================================================================
//	TRN FILES MANAGEMENT
//=================================================================================
//===================================================================================
//  Decode all TRN elevations for a given QGT
//===================================================================================
int SqlMGR::GetTRNElevations(C_QGT *qgt)
{	if (!UseElvDB())		return 0;
  
	char req[1024];
	U_INT key = qgt->FullKey();
	//count			= 0;
	this->qgt	= qgt;
	_snprintf(req,1024,"SELECT * FROM TRN WHERE qgt= %d;*",key);
	stm = CompileREQ(req,elvDBE);
	sup->qKey = key;
	sup->qgt	= qgt;
	char comp	= globals->comp & qgt->UseCompTexture();
	while (SQLITE_ROW == sqlite3_step(stm))	DecodeTRNrow(comp,key);
	//---Free statement ------------------------------------------
  sqlite3_finalize(stm);
	//return count;
	return 0;
}
//---------------------------------------------------------------------
//	Decode TRN Table elevation
//	TRN elevation gives an elevation array for the SuperTile mesh
//		There are 4 * 4 Detail Tiles in a SuperTile
//		The mesh redefines default elevation for each vertex in the Sup mesh
//---------------------------------------------------------------------
int SqlMGR::DecodeTRNrow(char comp,U_INT key)
{ //--- Separate key components ---------------------
	U_INT	qx	= ((key >> 16) << 5);			// Base X indice
	U_INT qz  = ((key & 0xFFFF) << 5);	// Base Z indice
	//--- Decode the Supertile record -----------------
	sup->No			= sqlite3_column_int(stm,1);
  int  sub		= sqlite3_column_int(stm,6);
	int  nbe		= sqlite3_column_int(stm,11);
	//-------------------------------------------------
	sup->side		= sub + 1;
	sup->Dim    = (sup->side)*(sup->side);
  float *src	= (float*)sqlite3_column_blob (stm,12);
	float	*dst	= *sup->elev;
	for (int k=0; k<sup->Dim; k++) *dst++ = *src++;
	//--- Compute Base Detail Tile -------------------
	U_INT sz		= (sup->No >> 3);						// SUP(Z)
	U_INT sx    = (sup->No & 0x07);					// SUP(X)
	U_INT tx    = (sx << 2);								// DET(X)
	U_INT	tz		= (sz << 2);								// DET(Z)
	//--- Compute Supertile SW corner key ------------
	U_INT dx    = qx | tx;									// Base key
	U_INT dz    = qz | tz;						
	//--- Set default elevation in SuperTile ---------
  qgt->SetElvFromTRN(tx,tz,*sup->elev);
	//--- Generate TextureDef list -------------------
	CTextureDef *txd  = qgt->GetTexList(sup->No);
	sup->SetTList(txd,comp);
	//--- Extract names and flags --------------------
	char *name  = (char*)sqlite3_column_text(stm,10);		// Texture name
	U_INT pt    = (U_INT)sqlite3_column_int(stm,  2);		// Patche
	U_INT fg    = (U_INT)sqlite3_column_int(stm,  5);		// Flag
	U_INT us    = (U_INT)sqlite3_column_int(stm,  7);   // User texture
	U_INT	wt		= (U_INT)sqlite3_column_int(stm,  8);		// Water texture
	U_INT	nt		= (U_INT)sqlite3_column_int(stm,  9);		// Night texture
	U_INT	tokn	= 0x00000001;
	//--- Precompute raw type ------------------------
	rawtype = (comp)?(TC_TEXCMPRS):(TC_TEXRAWTN);
	//--- TextureDef Reconstruction ------------------
	for (U_INT k=0; k<TC_TEXSUPERNBR; k++,txd++)
	{	name	+= txd->CopyName(name);
		txd->Name[8] = colInSUP[k];
		txd->Name[9] = rowInSUP[k];
		if (fg & tokn)  txd->SetFlag (TC_USRTEX);
		if (us & tokn)	txd->TypTX = (rawtype);
		if (wt & tokn)	txd->TypTX = (TC_TEXWATER);
		if (nt & tokn)	txd->SetFlag (TC_NITTEX);
		if (pt & tokn)  ResetDetailTRN(txd);
		//--- Next token for next texture --------------
		tokn   = (tokn << 1);
		//---  Make detail tile key for compression ----
		U_INT px = dx | (k & 0x03);
		U_INT pz = dz | (k >> 2);
		txd->sKey	= (px << 16) | pz;
	}
	//--- End of process -----------------------------
	qgt->IndElevation();
	return 0;
}
//---------------------------------------------------------------------
//	NOTE: Column det is used as a patch indicator to void the
//				specific texture.  When the bit is set, the corresponding
//				detail tile is processed as a generic tile
//---------------------------------------------------------------------
void SqlMGR::ResetDetailTRN(CTextureDef *txd)
{ if (txd->TypTX != rawtype)				return;
	//---  This is either a raw or compressed texture -----------
  txd->Name[0] = 0;											// Force generic name assignment
	txd->TypTX	 = 0;											// No type yet
	txd->Raz(TC_USRTEX);
	return;
}
//===================================================================================
//  Decode TILE TABLE elevations for a given QGT
//	Each row defines a detailled tile with a subdivision level with a
//	corresponding elevation array
//===================================================================================
int SqlMGR::GetTILElevations(C_QGT *qgt)
{	char req[1024];
	U_INT key = qgt->FullKey();
	//count			= 0;
	this->qgt	= qgt;
	_snprintf(req,1024,"SELECT * FROM TILE WHERE qgt= %d;*",key);
	stm = CompileREQ(req,elvDBE);
	sup->qKey = key;
	sup->qgt	= qgt;
	while (SQLITE_ROW == sqlite3_step(stm))	DecodeDETrow();
  //---Free statement ------------------------------------------
  sqlite3_finalize(stm);
	//return count;
	return 0;
}
//---------------------------------------------------------------------
//	Decode DET elevation
//---------------------------------------------------------------------
int SqlMGR::DecodeDETrow()
{ int       sub	= sqlite3_column_int(stm,CLN_TIL_SUB);
	TRN_HDTL  hdt(sub,0);
	int       det = sqlite3_column_int(stm,CLN_TIL_DET);
	hdt.SetTile(det);
	int			  nbe	= sqlite3_column_int(stm,CLN_TIL_NBE); 
	int      *src = (int*)sqlite3_column_blob (stm,CLN_TIL_ELV);
	int      *dst = hdt.GetElvArray();
	for (int k=0; k<nbe; k++) *dst++ = *src++;
	//--- Set Super Tile No -----------------------------
	int				sno = sqlite3_column_int(stm,CLN_TIL_SUP);
	//---------------------------------------------------
	hdt.SetSup(sno);
	qgt->DivideHDTL(&hdt);
	return 0;
}
//===================================================================================
//	Read ELEVATION PATCHE FOR A QGT
//===================================================================================
void SqlMGR::ReadPatches(C_QGT *qgt)
{ char req[1024];
  U_INT key = qgt->FullKey();
	_snprintf(req,1024,"SELECT * FROM patch WHERE qgt= %d;*",key);
	stm = CompileREQ(req,elvDBE);
	while (SQLITE_ROW == sqlite3_step(stm))
	{	PATCH_ELV	*p = new PATCH_ELV();
		p->inx		= 0;
		p->qgt		= qgt;
		p->key		= qgt->FullKey();
		p->dno		= sqlite3_column_int(stm,1);		// Detail Tile No
		p->sno		= sqlite3_column_int(stm,2);		// Supertile No
		p->nbp		= sqlite3_column_int(stm,3);		// number of patches
		int	dim		= p->nbp * sizeof(ELV_ITEM);		// Blob size in bytes
		char *src	= (char *)sqlite3_column_blob(stm, 4); // Elevations
		memcpy(p->tab,src,dim);
		p->dir		= PATCH_WRITE;
		qgt->ApplyPatches(p);
	}
  //---Free statement ------------------------------------------
  sqlite3_finalize(stm);
	return;
}
//===================================================================================
//	SAVE PATCHE into ELEVATION DATABASE
//	A patche for the detail tile is saved under the QGT key
//===================================================================================
int SqlMGR::WritePatche(PATCH_ELV &p)
{ //--- then insert the new one ----------------------------------------
	char *rq = "INSERT OR REPLACE INTO patch (qgt,sno,dno,nbp,elev) VALUES(?1,?2,?3,?4,?5);*";
	int rep	 = 0;
  sqlite3_stmt *stm = CompileREQ(rq,elvDBE);
	//--- insert QGT key ------------------------
	sqlite3_bind_int(stm, 1,p.key);				// Param 1
	//--- Insert SuperTile No -------------------
	sqlite3_bind_int(stm, 2,p.sno);				// Param 2
	//--- insert DET Key ------------------------
	sqlite3_bind_int(stm, 3,p.dno);			// Param 3
	//--- Insert number of entries --------------
	sqlite3_bind_int(stm, 4,p.nbp);				// Param 4
	//--- Insert elevations in blob -------------
	int dim  = p.nbp * sizeof(ELV_ITEM);	// Param 5
  sqlite3_bind_blob(stm, 5,p.tab, dim, SQLITE_TRANSIENT);
	//--- Execute statement ---------------------
	rep      = sqlite3_step(stm);               // Insert value in database
  if (rep != SQLITE_DONE) Abort(elvDBE);
  //---Free statement ------------------------------------------
  sqlite3_finalize(stm);
	return 0;
}
//===================================================================================
//	SAVE TRN TEXTURE
//	
//===================================================================================
int SqlMGR::WriteTRNtexture(TEXT_INFO &txd,char *tab)
{ SQL_DB &db = dtxDBE;
	char req[1024];
  _snprintf(req,1024,"INSERT INTO %s (key,qgt,type,name,file,mip,width,height,size,reso,data) "
					"VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11);*",tab);
	sqlite3_stmt *stm = CompileREQ(req,db);
	//---Byte size of texture ----------------------------------------------
	U_INT dim = txd.dim * sizeof(U_INT);
	//---Key ---------------------------------------------------------------
  int  rep = sqlite3_bind_int(stm, 1,txd.key);
  if (rep != SQLITE_OK)   Abort(db);
	//---QGT ---------------------------------------------------------------
  rep = sqlite3_bind_int(stm, 2,0);				// Is not used
  if (rep != SQLITE_OK)   Abort(db);
	//---Compression type --------------------------------------------------
  rep = sqlite3_bind_int(stm, 3,txd.type);
  if (rep != SQLITE_OK)   Abort(db);
	//---Texture name ------------------------------------------------------
	rep = sqlite3_bind_text(stm,4,txd.name,8,SQLITE_TRANSIENT);
  if (rep != SQLITE_OK) Abort(db);
	//---Index in File name table (FNM) ------------------------------------
  rep = sqlite3_bind_int(stm, 5,txd.Dir);
  if (rep != SQLITE_OK)   Abort(db);
	//---Mip level encoded in the texture  ---------------------------------
  rep = sqlite3_bind_int(stm, 6,txd.mip);
  if (rep != SQLITE_OK)   Abort(db);
	//---Uncompressed texture width  ---------------------------------------
  rep = sqlite3_bind_int(stm, 7,txd.wd);
  if (rep != SQLITE_OK)   Abort(db);
	//---Uncompressed texture height  ---------------------------------------
  rep = sqlite3_bind_int(stm,8,txd.ht);
  if (rep != SQLITE_OK)   Abort(db);
	//--- byte size of compressed texture  ----------------------------------
  rep = sqlite3_bind_int(stm,9,dim);
  if (rep != SQLITE_OK)   Abort(db);
	//--- Texture resolution  ----------------------------------------------
  rep = sqlite3_bind_int(stm,10,txd.res);
  if (rep != SQLITE_OK)   Abort(db);
	//--- pixel Blob --------------------------------------------------------
  rep = sqlite3_bind_blob(stm,11,txd.mADR, dim, SQLITE_TRANSIENT);
  if (rep != SQLITE_OK)   Abort(db);
	//--- Commit in database ------------------------------------------------
	rep      = sqlite3_step(stm);               // Insert value in database
	//--- End of record -----------------------------------------------------
	sqlite3_finalize(stm);                      // Close statement
	return 1;
}

//---------------------------------------------------------------------------------
//  check for the texture reference
//---------------------------------------------------------------------------------
bool SqlMGR::TRNTextureInTable(U_INT key,char *tab)
{ char req[1024];
	_snprintf(req,1024,"SELECT key FROM %s WHERE key = %d;*",tab,key);
  sqlite3_stmt *stm = CompileREQ(req,dtxDBE);
  //-----Execute statement -----------------------------------------------
  int rep = sqlite3_step(stm);                  // Search database
  sqlite3_finalize(stm);												// Close statement
	return (rep == SQLITE_ROW); 
}
//---------------------------------------------------------------------------------
//  Check if QGT match the database content
//---------------------------------------------------------------------------------
bool SqlMGR::QGTnotInArea(U_INT qx,U_INT qz)
{	//--- Compute global tile indices --------------------------
	U_INT gx = (qx >> 1) >> 2;		// X  QGT->GTx->AREAx
	U_INT gz = (qz >> 1) >> 2;		// Z  QGT->GTz->AREAz												
	return (gx != glx) || (gz != glz);
}
//=================================================================================
//
//  SQL THREAD Access methods
//
//=================================================================================
//=================================================================================
SqlTHREAD::SqlTHREAD(char tn)
{ sqlTYP    = SQL_THR;
  Init();
	pthread_mutex_lock(&globals->mux);
	//---Open Generic database -----------------------------------------
  if (tn == 1)	Open(genDBE);
  //---Open Taxiway database -----------------------------------------
  //Open(txyDBE);
  //---Open Elevation database ---------------------------------------
  if (tn == 1)	Open(elvDBE);
  //---Open 3D model database ----------------------------------------
  if (tn == 0)	Open(modDBE);
  //---Open Sea database ---------------------------------------------
  if (tn == 1)	Open(seaDBE);
  //---Open texture database -----------------------------------------
  if (tn == 0)	Open(texDBE);
	pthread_mutex_unlock(&globals->mux);
	go = true;
}
//-----------------------------------------------------------------------------
//  Close database
//-----------------------------------------------------------------------------
SqlTHREAD::~SqlTHREAD()
{ TRACE("Close SQL THREAD");
	if (sup) delete sup;
	sup	= 0;
}
//==============================================================================
//  COAST DATABASE
//==============================================================================
void SqlTHREAD::DecodeCST(sqlite3_stmt *stm,COAST_REC &cst)
{ cst.dtk   = sqlite3_column_int(stm,  CLN_CST_DET);
  cst.nbv   = sqlite3_column_int(stm,  CLN_CST_NBV);
  cst.dim   = sqlite3_column_bytes(stm,CLN_CST_VTX);
  cst.data  = (char*)sqlite3_column_blob(stm,CLN_CST_VTX);
  CopyData(cst);
  return;
}
//---------------------------------------------------------------------------
//  Copy data into vertex table
//---------------------------------------------------------------------------
void SqlTHREAD::CopyData(COAST_REC &rec)
{ //-- compute a size in integer ----------------------
	int   wdm = (rec.dim / sizeof(int)) + 1;
	int  *buf	= new int[wdm];
  char *dst = (char*)buf;
  char *src = rec.data;
  rec.data = dst;
	memcpy(dst,src,rec.dim);
  return;
}
//---------------------------------------------------------------------------
//  Get coast data for the whole QGT
//---------------------------------------------------------------------------
void SqlTHREAD::ReadCoast(COAST_REC &rec,C_CDT *cst)
{ char  req[1024];
  _snprintf(req,1024,"SELECT * FROM coast WHERE qgt = %d;*",rec.qtk);
  sqlite3_stmt *stm = CompileREQ(req,seaDBE);
  //---Execute select --------------------------------------------
  while (SQLITE_ROW == sqlite3_step(stm))
  { DecodeCST(stm,rec);
    cst->AddCoast(rec.dtk,rec.data);
    rec.data = 0;
  }
  //---Free statement --------------------------------------------
  sqlite3_finalize(stm);
  return;
}
//---------------------------------------------------------------------------------
//  check for the 3Dmoddel reference
//---------------------------------------------------------------------------------
bool SqlTHREAD::CheckM3DModel(char *name)
{ char req[1024];
  _snprintf(req,1024,"SELECT name FROM mod WHERE name='%s';*",name);
  sqlite3_stmt *stm = CompileREQ(req,modDBE);
  //-----Execute statement -----------------------------------------------
  int rep = sqlite3_step(stm);                  // Search database
  sqlite3_finalize(stm);                    // Close statement
  return (rep == SQLITE_ROW)?(true):(false);
}
//---------------------------------------------------------------------------------
//  Decode 3DPart
//  NOTE: Type is ignored for now
//---------------------------------------------------------------------------------
int SqlTHREAD::DecodeM3DdayPart(sqlite3_stmt *stm,C3Dmodel *modl,EXT_3D &E)
{ int   lod =        sqlite3_column_int (stm,CLN_MOD_LOD);
	//--- Eliminate non 3D parts ---------------------------
  if (lod > 3)			 return 0;
	int   tsp =        sqlite3_column_int (stm,CLN_MOD_TSP);
  char *txn = (char*)sqlite3_column_text(stm,CLN_MOD_TXN);
  int   nbv =        sqlite3_column_int (stm,CLN_MOD_NVT);
  int   nbx =        sqlite3_column_int (stm,CLN_MOD_NIX);
	//--- Search part for same texture ---------------------

	C3DPart *prt = modl->GetPartFor(FOLDER_ART,txn,lod, nbx);

	//---- Build this part --------------------------------
  float top =  float(sqlite3_column_double(stm, CLN_MOD_TOP));
  modl->SaveTop(top);
  float bot =  float(sqlite3_column_double(stm, CLN_MOD_BOT));
  modl->SaveBot(bot);
  int   dim =        sqlite3_column_bytes(stm,CLN_MOD_VTX);
  F3_VERTEX *V = (F3_VERTEX*)sqlite3_column_blob (stm,CLN_MOD_VTX);
  F3_VERTEX *N = (F3_VERTEX*)sqlite3_column_blob (stm,CLN_MOD_NTB);
  F2_COORD  *T = (F2_COORD*)sqlite3_column_blob (stm,CLN_MOD_TTB);
  int       *X = (int*)sqlite3_column_blob (stm,CLN_MOD_ITB);
  prt->SQLstrip(nbx,V,N,T,X,E);
  //----return number of faces -------------------------------
  int nbf = (nbx / 3);
  return nbf;
}
//---------------------------------------------------------------------------------
//  Decode Modele texture
//---------------------------------------------------------------------------------
int SqlTHREAD::GetM3DTexture(TEXT_INFO *inf)
{ char req[1024];
  char *name = inf->path+4;
  _snprintf(req,1024,"SELECT * FROM texture WHERE xname='%s';*",name);
  sqlite3_stmt *stm = CompileREQ(req,modDBE);

  if (SQLITE_ROW == sqlite3_step(stm))
  {
    //---- Decode the texture and return it ----------------------------
    int   dim		=      sqlite3_column_bytes(stm,CLN_NTX_TEX);
    char *src	= (char*)sqlite3_column_blob (stm,CLN_NTX_TEX);
    char *tex = new char[dim];
		memcpy(tex,src,dim);
		//------------------------------------------------------------------
    inf->mADR = (GLubyte*)tex;
    inf->wd   =  sqlite3_column_int (stm,CLN_NTX_WID);
    inf->ht   =  sqlite3_column_int (stm,CLN_NTX_HTR);
		inf->dim	=  dim;
  }
  //---Free statement --------------------------------------------------
  sqlite3_finalize(stm);
  return 0;
}
//---------------------------------------------------------------------------------
//  Decode a 3D Model
//---------------------------------------------------------------------------------
int SqlTHREAD::GetM3Dmodel(C3Dmodel *modl)
{ char *fn = modl->GetFileName();
  char req[1024];
  int   nf = 0;                           // Number of faces
	EXT_3D ext;															// Model extension
  _snprintf(req,1024,"SELECT * FROM mod WHERE name='%s';*",fn);
  sqlite3_stmt *stm = CompileREQ(req,modDBE);
  while (SQLITE_ROW == sqlite3_step(stm)) nf += DecodeM3DdayPart(stm,modl,ext);
  if (nf)	modl->SetState(M3D_LOADED);
	modl->SetExtend(ext);
  //---Free statement --------------------------------------------------
  sqlite3_finalize(stm);
  return nf;
}
//===================================================================================
//  Decode a Generic terrain Texture
//===================================================================================
GLubyte *SqlTHREAD::GetSQLGenTexture(TEXT_INFO &txd)
{ if (!UseTexDB())     return 0;
  char *name = txd.path;
  char *dot  = strrchr(txd.path,'.');
  char *typ  = dot - 1;
  char *res  = typ - 1;
  char *tab  = 0;
  char *txn  = strrchr(name,'/') + 1;
  if (*res == '4')  {tab = "DAY128";}
  if (*res == '5')  {tab = "DAY256";}
  if (*typ == 'N')  {tab = "NIT128";}
  char req[1024];
  *dot = 0;
  _snprintf(req,1024,"SELECT * FROM %s WHERE name='%s';*",tab,txn);
  *dot = '.';
  sqlite3_stmt *stm = CompileREQ(req,texDBE);
  if (SQLITE_ROW == sqlite3_step(stm))
  {  //---- Decode the texture and return it ------------------------------
    int   dim = sqlite3_column_bytes(stm,CLN_NTX_TEX);
    U_INT *src = (U_INT*)sqlite3_column_blob (stm,CLN_TEX_IMG);
    char *tex = new char[dim];
    //---- Transfer pixels without alpha chanel (land texture) -----------
    U_INT *dst = (U_INT*)tex;
		int    nbw = dim >> 2;			// Word count
    for (int k=0; k<nbw; k++)    *dst++ = (*src++) & (0x00FFFFFF);
    int wd    = sqlite3_column_int (stm,CLN_NTX_WID);
    txd.wd    = wd;
    txd.ht    = wd;
    txd.mADR  = (GLubyte*)tex;
  }
  //---Free statement --------------------------------------------------
  sqlite3_finalize(stm);
  return txd.mADR;
}
//---------------------------------------------------------------------------------
//	Read compressed texture 
//	NOTE:  Only the first thread instance has the Compressed database open
//---------------------------------------------------------------------------------
int SqlTHREAD::ReadaTRNtexture (TEXT_INFO &txd,char *tab,SQL_DB *db)
{	char req[1024];
	_snprintf(req,1024,"SELECT * FROM %s WHERE key=%d;*",tab,txd.key);
  sqlite3_stmt *stm = CompileREQ(req,*db);
	char *src;
	txd.mADR	= 0;
	txd.dim		= 0;
  if (SQLITE_ROW == sqlite3_step(stm))
  { //--- decode type -------------------------------------------------
		txd.type   = sqlite3_column_int(stm, 2);
		//--- Decode name -------------------------------------------------
		src		= (char*)sqlite3_column_text(stm,3);
		strncpy(txd.name,src,8);
		//--- Decode mip level --------------------------------------------
		txd.mip   = sqlite3_column_int(stm, 5);
		//--- Decode widht & height ---------------------------------------
		txd.wd		= sqlite3_column_int(stm, 6);
		txd.ht		= sqlite3_column_int(stm, 7);
		//--- Decode resolution ---- ---------------------------------------
		txd.res		= sqlite3_column_int(stm, 9);
    //---- Decode the texture and return it ----------------------------
    int   dim = sqlite3_column_int(stm,8);
    src = (char*)sqlite3_column_blob (stm,10);
		//--- allocate a bufer and transfert into --------------------------
    char *tex = new char[dim];
		memcpy(tex,src,dim);
    //------------------------------------------------------------------
    txd.mADR = (GLubyte*)tex;
		txd.dim	 =  dim;
  }
  //---Free statement --------------------------------------------------
  sqlite3_finalize(stm);
	return 1;
}


//=============END OF FILE ==========================================================
