//==========================================================================================
//
// Part of Fly! Legacy project
//
// Copyright 2005 Chris Wallace
// Copyright 2012 Jean Sabatier
// Fly! Legacy is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation; either version 2 of the License, or
//   (at your option) any later version.
//
// Fly! Legacy is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
//   along with Fly! Legacy; if not, write to the Free Software
//    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//	Primary data used for building are comming from http://www.openstreetmap.org/ with
//	copyright "� les contributeurs d�OpenStreetMap, CC BY-SA "
//
//	"osm2xp par Benjamin Blanchet" pour la generation FlyLegacy
//
//==========================================================================================
#include "../include/WinCity.h"
#include "../Include/Triangulator.h"
#include "../Include/RoofModels.h"
#include "../Include/Model3D.h"
#include "../Include/TerrainTexture.h"
#include "../Include/FuiOption.h"
#include "../Include/OSMobjects.h"
#include "../Include/SqlMGR.h"
#include "../Include/ScenerySet.h"
#include <vector>
using namespace std;
//==========================================================================================
//  Window to display building sketch
//==========================================================================================
CFuiSketch::CFuiSketch(Tag idn, const char *filename)
:CFuiWindow(idn,filename,220,500,0)
{ title = 1;
  close = 1;
  zoom  = 0;
  mini  = 0;
	//--- white color -------------------------------
	U_INT wit = MakeRGBA(255,255,255,255);
	//--- Set title ---------------------------------
	strcpy(text,"CITY EDITOR");
	gBOX  = new CFuiGroupBox(10,8,200,50,this);
  AddChild('gbox',gBOX,"");
	//--- Label --------------------------------------
  aLAB  = new CFuiLabel   ( 4, 4,56, 20, this);
  gBOX->AddChild('labl',aLAB,"Options:",wit);
	//---Fill option ---------------------------------
  fOPT  = new CFuiCheckBox(60, 4,160,20,this);
  gBOX->AddChild('fopt',fOPT,"Fill mode",wit);
	//--- Other 3D objects ---------------------------
  vOPT  = new CFuiCheckBox(60,24,160,20,this);
  gBOX->AddChild('vopt',vOPT,"View scenery objects",wit);
	vOPT->SetState(1);

	//--- Second group box ------------------------
	Box2 = new CFuiGroupBox(10,66,200,100,this);
	AddChild('box2',Box2,"");
	//--- Terrain button---------------------------
	vTer	= "Hide Terrain";
	vTER = new CFuiButton( 106, 46,  90, 20,this);
	Box2->AddChild('vter', vTER,"View Terrain");
	//--- NEXT button---------------------------
	nBUT	= new CFuiButton(  4, 72,  90, 20,this);
	Box2->AddChild('nbut', nBUT,"Next");
	//--- View All button --------------------------
	vALL	= new CFuiButton(106, 72,  90, 20,this);
	Box2->AddChild('vall', vALL,"Load all");
	//-----------------------------------------------
	nBAT	= new CFuiLabel ( 4,   4, 190, 20, this);
	Box2->AddChild('nbat', nBAT, "Building",wit);
	//-----------------------------------------------
	nTAG  = new CFuiLabel ( 4,  30, 190, 20, this);
	Box2->AddChild('ntag', nTAG, "Tag",wit);

	//----Create the style List box -----------------
  sWIN  = new CFuiList  (10,174, 200,140,this);
  sWIN->SetVScroll();
  AddChild('styl',sWIN,"Styles",FUI_TRANSPARENT,wit);

	//--- Create a group box 3 -------------------
	Box3	= new CFuiGroupBox(10, 330, 200, 56, this);
	AddChild('box3',Box3,"", FUI_TRANSPARENT,wit);
	//--- Change style ---------------------------
	nSTY	= new CFuiButton(  4, 4, 90, 20,this);
	Box3->AddChild('nsty', nSTY,"Change Style");
	//--- Delete object ---------------------------
	dOBJ	= new CFuiButton(106, 4, 90, 20,this);
	Box3->AddChild('delt', dOBJ,"Remove object");
	//--- Restore object ---------------------------
	gOBJ	= new CFuiButton(106, 30, 90, 20,this);
	Box3->AddChild('gobj', gOBJ,"Restore object");

	//--- Create a group box 4 -------------------
	Box4	= new CFuiGroupBox(10, 400, 200, 36, this);
	AddChild('box4',Box4,"", FUI_TRANSPARENT,wit);
	//----Create Replace button ---------------------	
	rOBJ	= new CFuiButton(4,  4,  90, 20,this);
	Box4->AddChild('robj', rOBJ,"Replace building");
	//----Create Rotation button --------------------	
	lROT	= new CFuiButton(106,4,  42, 20,this);
	Box4->AddChild('lrot', lROT,"left");
	lROT->SetRepeat(0.1F);
	//----Create Rotation button --------------------	
	rROT	= new CFuiButton(152,4,  42, 20,this);
	Box4->AddChild('rrot', rROT,"right");
	rROT->SetRepeat(0.1F);
	//-----------------------------------------------
	bTRY  = new CFuiButton( 10, 446, 80, 20, this);
	AddChild('btry',bTRY,"FLY IT",0);
	//-----------------------------------------------
	bSAV  = new CFuiButton(100, 446, 110, 20, this);
	AddChild('bsav',bSAV,"Save in Database",0);
	//-----------------------------------------------	
	ReadFinished();
	//--- Open triangulation ------------------------
	trn	= new CBuilder(&ses);
	globals->trn = trn;
	//-----------------------------------------------
	ctx.prof	= PROF_SKETCH;
	ctx.mode	= 0;
  rcam			= globals->ccm->SetRabbitCamera(ctx,RABBIT_S_AND_C);
	rcam->SetTracker(trn, this);
	rcam->SetRange(100);
	rcam->MoveTo(10,3000);
	//--- Set transparent mode ----------------------
	SetTransparentMode();
	//-----------------------------------------------
	oneD = DegToRad(double(1));
	//------------------------------------------------
	SetOptions(TRITOR_ALL);
	ses.SetTrace(1);
	tera	= 0;
	scny	= 1;
	wait  = 1;
	edit	= 0;
	wfil	= 1;
	FP		= 0;
	sqlp	= 0;
	//--- delete aircraft for more memory ------------
	//SAFE_DELETE( globals->pln);
	//--- Set Initial state --------------------------
	globals->Disp.ExecON  (PRIO_ABSOLUTE);	// Allow terrain TimeSlice
	globals->Disp.ExecOFF (PRIO_TERRAIN);		// Nothing exe after terrain
	globals->Disp.DrawON  (PRIO_ABSOLUTE);	// Drawing
	globals->Disp.ExecLOK (PRIO_WEATHER);		// No weather
	//-------------------------------------------------
	globals->fui->SetBigFont();
	DrawNoticeToUser("INITIALIZATION",1);
	//---- Marker 1 -----------------------------------
	char *ds = new char[32];
	strcpy(ds,"*START CityEDIT*");
	//---Collect OFE file -----------------------------
	FPM.close = 0;
	FPM.sbdir = 1;
	FPM.userp = CITY_FILE_OFE;
	FPM.text  = "SELECT A FILE";
	FPM.dir		= "OpenStreet";
	FPM.pat		= "*.ofe";
	STREETLOG("START CITY EDITOR");
	*sname = 0;
	*dbase = 0;
	 CreateFileBox(&FPM);
	 State = SKETCH_PAUSE;

};
//-----------------------------------------------------------------------
//	destroy this
//-----------------------------------------------------------------------
CFuiSketch::~CFuiSketch()
{	//--- Close File ------------------------------------
	if (FP)	fclose(FP);
	//--- Restore State ---------------------------------
	globals->Disp.DrawON (PRIO_TERRAIN);		// Allow Terrain TimeSlice
	globals->Disp.ExecON (PRIO_TERRAIN);		// Allow Terrain Draww
	globals->Disp.ExecULK(PRIO_WEATHER);
	//--- Back to position ------------------------------
	SAFE_DELETE(globals->trn);

  if (sqlp)	globals->sqm->CloseOSMbase(sqlp);
	sqlp = 0;
	//---- Marker 2 -------------------------------------
	char *ds = Dupplicate("*END CityEDIT*",32);
	STREETLOG("END CITY EDITOR");
	globals->ccm->RestoreCamera(ctx);
}
//----------------------------------------------------------------------
//	Override check profile
//----------------------------------------------------------------------
bool CFuiSketch::CheckProfile(char a)
{	return true;	}
//-----------------------------------------------------------------------
//	A file is selected (virtual called from CFuiFileBox)
//-----------------------------------------------------------------------
void CFuiSketch::FileSelected(FILE_SEARCH *fpm)
{ switch (fpm->userp)	{
		//--- Openstreet file selected ---------
		case CITY_FILE_OFE:
			State = SKETCH_WSEL;
			strncpy(spath,fpm->sdir,FNAM_MAX);
			strncpy(sname,fpm->sfil,FNAM_MAX);
			_snprintf(fnam,FNAM_MAX,"%s/%s",spath,sname);
			break;
		case CITY_FILE_MOD:
			repPM.obr = Dupplicate(fpm->sfil,FNAM_MAX);
			State = SKETCH_ROBJ;
			break;
		case CITY_FILE_DBA:
			_snprintf(dbase,FNAM_MAX,"%s/%s",fpm->sdir,fpm->sfil);
			dial	= '_yes';
			break;
	}

	return;
 }
//-----------------------------------------------------------------------
//	Set Option
//-----------------------------------------------------------------------
void CFuiSketch::SetOptions(U_INT q)
{	char p = 0;
	optD.Toggle(q);	
	trn->repD(optD.GetAll());
	p = optD.Has(TRITOR_DRAW_FILL);
	fOPT->SetState(p);
	return;
}
//-------------------------------------------------------------------
//	Parse a new building 
//-------------------------------------------------------------------
bool CFuiSketch::ParseBuilding()
{	char txt[128];
  fsetpos( FP, &fpos); 
	char *ch = ReadTheFile(FP,txt);
	_strupr(txt);
	confp.Reset();					// Building by default
 *tagn = *valn = 0;				// Clear tag-value
	repPM.Clear();					// Clear replacement
	//--- Check for a building number -------------------------
	int nf = sscanf(ch,"START %d ID=%d ",&seqNo, &ident);
	if (nf != 2)		return false;
	trn->SetIdent(ident); 
	return ParseFile();
}
//-------------------------------------------------------------------
//	Parse vertice list 
//-------------------------------------------------------------------
bool CFuiSketch::ParseFile()
{	char txt[256];
	bool go = true;
	trn->StartOBJ();
	while (go)
	{	fgetpos (FP,&fpos);
		char *ch = ReadTheFile(FP,txt);
		if (ParseStyle(txt))							continue;
		if (ParseReplace(txt))						continue;
		//--- continue with upper cases ------------
		_strupr(ch);
		if (ParseTAG(txt))								continue;				// Tag directive
		if (ParseVTX(txt))								continue;				// Vertices
		if (ParseHOL(txt))								continue;				// Hole directive
		//--- End of vertices --------------------------
		return true;
	}
	return false;
}
//-------------------------------------------------------------------
//	Parse Style
//-------------------------------------------------------------------
bool CFuiSketch::ParseStyle(char *txt)
{ char  nsty[64];
	U_INT rofm = 0;
	U_INT rftx = 0;
	U_INT flnb = 0;
	int nf = sscanf_s(txt," Style %63s rofm = %d rftx = %d flNbr = %d",nsty,63, &rofm, &rftx, &flnb);
	if (nf != 4)		return false;
	trn->ForceStyle(nsty,rofm,rftx,flnb);
	return true;
}
//-------------------------------------------------------------------
//	Parse vertex list
//-------------------------------------------------------------------
bool CFuiSketch::ParseVTX(char *txt)
{	int nv = 0;
  int rd = 0;
	double x;
	double y;
	bool go		= true;
	char *src = txt;
	while (go)
	{	int nf = sscanf(src," V ( %lf , %lf ) %n",&y,&x,&rd);
		if (nf != 2)	return (nv != 0);
		if (confp.otype) trn->AddVertex(confp.zned,x,y);
		src += rd;
		nv++;
	}
	return true;
}
//-------------------------------------------------------------------
//	Parse Hole directive
//-------------------------------------------------------------------
bool CFuiSketch::ParseHOL(char *txt)
{	if (strncmp(txt,"HOLE",4) != 0)			return false;
	if (confp.otype)	trn->NewHole();
	return true;
}
//-------------------------------------------------------------------
//	Parse Tag directive
//-------------------------------------------------------------------
bool CFuiSketch::ParseTAG(char *txt)
{	int nf = sscanf(txt,"TAG ( %32[^ =)] = %32[^ )] ) ",tagn, valn);
	if (nf != 2)		return false;
	GetOSMconfig(tagn,valn,confp);
	return true;
}
//-------------------------------------------------------------------
//	Parse replace directive
//-------------------------------------------------------------------
bool CFuiSketch::ParseReplace(char *txt)
{ char objn[PATH_MAX];
	int nf = sscanf(txt,"Replace ( Z = %lf ) with %s",&orien,objn);
	if (nf != 2)  return false;
	repPM.otype	= confp.otype;
	repPM.dir		= GetOSMfolder(confp.otype);
	repPM.obr		= Dupplicate(objn,FNAM_MAX);
	return true;
}
//-------------------------------------------------------------------
//	Abort
//-------------------------------------------------------------------
U_INT CFuiSketch::Abort(char *erm, char *fn)
{	char txt[128];
	_snprintf(txt,127,"%s %s", erm, fn);
	STREETLOG("%s %s",erm,fn);
	CreateDialogBox("ERROR",txt);
	return SKETCH_ABORT;
}
//-------------------------------------------------------------------
//	Parse Area
//-------------------------------------------------------------------
bool CFuiSketch::ParseArea()
{	char txt[128];
	char *ed = " AREA SW [ %lf , %lf ] NE [ %lf , %lf ] ";
	char *ch = ReadTheFile(FP,txt);
	_strupr(ch);
	double y1;
	double x1;
	double y2;
	double x2;
	int nf = sscanf(ch, ed , &SW.lat, &SW.lon, &NE.lat , &NE.lon);
	if (nf != 4)	return false;
	//--- Compute mid position ----------------------------
	fgetpos( FP, &fpos);
	y1 = FN_ARCS_FROM_DEGRE(SW.lat);
	x1 = FN_ARCS_FROM_DEGRE(SW.lon);
	y2 = FN_ARCS_FROM_DEGRE(NE.lat);
	x2 = FN_ARCS_FROM_DEGRE(NE.lon);
	rpos.lon = AddLongitude(x1,x2) * 0.5;
	rpos.lat = (y1 + y2) * 0.5;
	return true;
}
//-------------------------------------------------------------------
//	Compute reference position
//-------------------------------------------------------------------
void CFuiSketch::BuildDBname()
{ _snprintf(dbase,FNAM_MAX,"OpenStreet/Databases/%s",sname);
	char *dot = strrchr(dbase,'.');
	if (0 == dot)	return;
	strcpy(dot,".db");
	return;
}
//-------------------------------------------------------------------
//	Compute reference position
//-------------------------------------------------------------------
U_INT CFuiSketch::GotoReferencePosition()
{	char *er1 = "Cant open file ";
	char *er2 = "Invalid file ";
	//--- build a proposed database name if no one ----------------
	if (*dbase == 0)	BuildDBname();
	rpos.lon	= 0;
	rpos.lat	= 0;
	rpos.alt	= 0;
	count			= 0;
	FP  = fopen(fnam,"rb");
  if (0 == FP)			return Abort(er1,fnam);
	fpos	= 0;
	if (!ParseArea())	return Abort(er2,fnam);
	//--- Go to reference position --------
	cntw	= 0;
	geop  = rpos;
	rcam->GoToPosition(rpos);			// Teleport
	globals->Disp.ExecON (PRIO_ABSOLUTE);		// Allow Terrain
	globals->Disp.ExecOFF(PRIO_TERRAIN);		// Stop after terrain
	rcam->SetRange(300);
	nStat = SKETCH_OPEN;
	return SKETCH_WAIT;
}
//-------------------------------------------------------------------
//	Edit building
//-------------------------------------------------------------------
int CFuiSketch::EditBuilding()
{	char txt[128];
	int er  = trn->EditPrm(txt);									// osmB
	nBAT->SetText(txt);
	//--------------------------------------------
	trn->EditTag(txt);														// osmB
	nTAG->SetText(txt);
	//--- Get object position --------------------
	trn->ActualPosition(geop);										// osmB
	return er;
}
//-------------------------------------------------------------------
//	Check for replacement
//	Return true if object is really replaced
//-------------------------------------------------------------------
bool CFuiSketch::AutoReplace()
{ repPM.Clear();
	repPM.otype = confp.otype;
	if (!ses.GetReplacement(repPM))		return false;
	char rep = trn->ReplaceOBJ(&repPM,1);
	return (1 ==rep);								
}
//-------------------------------------------------------------------
//	Build Object
//-------------------------------------------------------------------
bool CFuiSketch::BuildObject()
{	if (0 == confp.otype)						return false;
	trn->BuildOBJ(&confp);
	int	rep = 0;
	//--- Replace directive -----------------
	if (repPM.obr)
	{	repPM.sinA = sin(orien);
		repPM.cosA = cos(orien);
		rep = trn->ReplaceOBJ(&repPM,0);
	}
	//--- Auto replace ----------------------
	else				rep = AutoReplace();
	//--- Check for deletion ----------------
	bool dlt =  (0 == rep) && (confp.prop & OSM_PROP_SKIP);
	if (dlt)	{	trn->RemoveOBJ();	return false;	}
	//---------------------------------------
	int er = EditBuilding();
	rcam->GoToPosition(geop);			// Teleport
	if (0 == er)	rpos = geop;
	return true;
}
//-----------------------------------------------------------------------
//	View terrain action
//-----------------------------------------------------------------------
U_INT CFuiSketch::TerrainView()
{	trn->ModeGroups();
	rcam->GoToPosition(geop);			// Teleport
	globals->Disp.ExecON (PRIO_ABSOLUTE);		// Allow Terrain
	globals->Disp.ExecOFF(PRIO_TERRAIN);		// Stop after terrain
	globals->Disp.DrawON (PRIO_TERRAIN);		// Draw Terrain
	tera  = 1;
	globals->fui->CaptureMouse(this);
	vTER->SetText(vTer);
	nStat = SKETCH_PAUSE;
	return SKETCH_WAIT;
}
//-----------------------------------------------------------------------
//	Hide terrain action
//-----------------------------------------------------------------------
U_INT CFuiSketch::TerrainHide()
{	trn->ModeSingle();
	globals->Disp.ExecOFF (PRIO_ABSOLUTE);				// No Terrain
	globals->Disp.DrawOFF (PRIO_TERRAIN);					// No Terrain
	tera  = 0;
	vTER->SetText("View Terrain");								// View or Edit
	globals->fui->CaptureMouse(0);								// Stop Capture
	return SKETCH_PAUSE;													// just pause till next event
}
//-----------------------------------------------------------------------
//	Wait terrain for ready to set the terrain altitude
//-----------------------------------------------------------------------
U_INT CFuiSketch::TerrainWait()
{	char txt[128];
	//--- warning in first step ---------------------------
	if (wait)
	{	_snprintf(txt,128,"TELEPORTING TO DESTINATION (%06u).  PLEASE WAIT",cntw++);
		DrawNoticeToUser(txt,1);
	}
	if (!globals->tcm->TerrainStable(0))		return SKETCH_WAIT;
	//--- get terrain elevation ------------------------
	rpos.alt = globals->tcm->GetGroundAltitude();
	geop	= rpos;
	globals->geop.alt = rpos.alt;
	wfil	= 0;
	wait	= 0;
	return nStat;
}
//-----------------------------------------------------------------------
//	Terrain at reference position
//-----------------------------------------------------------------------
U_INT CFuiSketch::HereWeAre()
{	rcam->SetAngle(30,15);
	globals->Disp.ExecOFF (PRIO_ABSOLUTE);		// No Terrain
	globals->Disp.DrawOFF (PRIO_TERRAIN);		  // No Terrain
	trn->ModeSingle();
	eofl	= 0;
	return SKETCH_NEXT;
}
//-----------------------------------------------------------------------
//	Show style on box
//-----------------------------------------------------------------------
void CFuiSketch::ShowStyle()
{	//--- show style on list box ---------
	D2_Style *sty = trn->ActualStyle();
	if (0 == sty)		return;
	U_INT     ns	= sty->GetSlotSeq();
	sBOX.GoToItem(ns);
	return;
}
//-----------------------------------------------------------------------
//	Get next building from OFE file
//-----------------------------------------------------------------------
U_INT	CFuiSketch::OneBuilding()
{	bool go = true;
	while (go)
	{	if (!ParseBuilding())	return SKETCH_ENDL;
		//--- build object -----------------
		if (BuildObject())		break;
	}
	//--- show style on list box ---------
	ShowStyle();
	return SKETCH_PAUSE;
}
//-----------------------------------------------------------------------
//	Start Load
//-----------------------------------------------------------------------
U_INT CFuiSketch::StartLoad()
{	if (eofl)			return SKETCH_PAUSE;
	if (tera)			TerrainHide();
	rcam->SetAngle(30,15);
	return SKETCH_LOAD;
}
//-----------------------------------------------------------------------
//	Read next building
//-----------------------------------------------------------------------
U_INT CFuiSketch::ReadNext()
{ char txt[128];
	if (eofl)							return SKETCH_PAUSE;
	if (!ParseBuilding())	return SKETCH_ENDL;
	//--- build object ---------------------
	if (!BuildObject())		return SKETCH_LOAD;
	time	= 0;
	//---------------------------------------
	U_INT	nobj = trn->ActualStamp();
	_snprintf(txt,127,"Load Building %05d",nobj);
	DrawNoticeToUser(txt,1);
	return SKETCH_SHOW;
}
//-----------------------------------------------------------------------
//	Show building before next
//-----------------------------------------------------------------------
U_INT	CFuiSketch::ShowBuilding()
{	time--;
	if (time >= 0)	return SKETCH_SHOW;
	return SKETCH_LOAD;
}
//-----------------------------------------------------------------------
//	End of file is reached
//	Enter in edit mode
//-----------------------------------------------------------------------
U_INT CFuiSketch::EndLoad()
{	char txt[128];
	U_INT nbo = ses.GetNbObject();
	_snprintf(txt,127,"%05d Objects",nbo);
	nBAT->SetText(txt);
	trn->EndOBJ();
	fclose(FP);
	FP		= 0;
	eofl	= 1;
	edit  = 1;
	TerrainView();
	rcam->GoToPosition(rpos);
	rcam->MoveTo(10,1800);
	globals->fui->CaptureMouse(this);
	vTer	= "Zoom Building";
	vTER->SetText(vTer);
	//--- change ident for 'load all' button -----------------------------
	vALL->SetId('null');					// Becomes a null button
	vALL->SetText("End of File");
	//--- inhibit next button --------------------------------------
	nBUT->SetId('null');
	DrawNoticeToUser("GOING to TERRAIN NOW......",1);
	return SKETCH_PAUSE;
}
//-----------------------------------------------------------------------
//	Edit error code
//-----------------------------------------------------------------------
bool CFuiSketch::EditError()
{ char txt[128];
	int		error = trn->ActualError();
	int		stamp	= trn->ActualStamp();
	if (0 == error)		return false;
	_snprintf(txt,128,"Build %d Error %d",stamp,error);
	nBAT->SetText(txt);
	trn->ClearError();
	return true;
}
//-----------------------------------------------------------------------
//	Change the current style
//-----------------------------------------------------------------------
void CFuiSketch::ChangeStyle()
{	if ((tera) && (0 == trn->ActualFocus()))	return;
  EditError();
	D2_Style *sty = (D2_Style*)sBOX.GetSelectedSlot();
	trn->ModifyStyle(sty);
	EditError();
	return;
}
//-----------------------------------------------------------------------
//	Remove current building
//	NOTE:  We must be in edit mode
//-----------------------------------------------------------------------
void	CFuiSketch::RemoveObject()
{	if (NoSelection())	return;
	trn->RemoveOBJ();
	return;
}
//-----------------------------------------------------------------------
//	Replace current building
//	NOTE:  We must be in edit mode
//-----------------------------------------------------------------------
void CFuiSketch::ReplaceBuilding()
{	if (NoSelection())	return;
	trn->ReplaceOBJ(&repPM,1);
	State = SKETCH_PAUSE;
	return;
}
//-----------------------------------------------------------------------
//	Restore last deleted building
//	NOTE:  We must be in edit mode
//-----------------------------------------------------------------------
void	CFuiSketch::RestoreObject()
{	trn->RestoreOBJ();
	return;
}
//-----------------------------------------------------------------------
//	Rotate the building
//-----------------------------------------------------------------------
void CFuiSketch::RotateBuilding(double rad)
{	if (NoSelection())	return;
	trn->RotateOBJ(rad);
	return;
}
//-----------------------------------------------------------------------
//  Click detected outside any window
//	-Start picking mode for the camera
//	return true only if a hit was detected. If not
//	return false so that the mouse event can be passed on to the next
//	object in scene
//-----------------------------------------------------------------------
bool CFuiSketch::MouseCapture(int mx, int my, EMouseButton bt)
{	RegisterFocus(0);										// Free textedit
	bool		hit = rcam->PickObject(mx, my);						
	return	hit;		
}
//-----------------------------------------------------------------------
//	One object selected
//-----------------------------------------------------------------------
void CFuiSketch::OnePicking(U_INT No)
{	trn->SelectOBJ(No);
	EditBuilding();
	ShowStyle();
	return;
}
//-----------------------------------------------------------------------
//	Check if selected
//-----------------------------------------------------------------------
bool CFuiSketch::NoSelection()
{	char sel = trn->ActualFocus();	
	if (0 == tera)	return false;
	return (sel == 0);
}
//-----------------------------------------------------------------------
//	Fly Over the city
//-----------------------------------------------------------------------
void CFuiSketch::FlyOver()
 { Write(); 
	 globals->Disp.DrawOFF (PRIO_TERRAIN);		// No Terrain
	 ses.UpdateCache();
	 Close();
	 return;
}
//-----------------------------------------------------------------------
//	Collect databases to save
//-----------------------------------------------------------------------
bool CFuiSketch::CollectBases()
{	FPM.userp	= CITY_FILE_DBA;
	FPM.close = 1;
	FPM.sbdir = 0;
	FPM.text  = "Save in Existing database?";
	FPM.dir   = "OpenStreet/Databases";
	FPM.pat		= "*.DB";
	return CreateFileBox(&FPM);
}
//---------------------------------------------------------------------
// Save database Step 0
//---------------------------------------------------------------------
U_INT CFuiSketch::SaveStepA()
{ dial	= 0;
	if (CollectBases()) return SKETCH_SAVEA;
	CloseModal();
	return SaveStepD();
}
//---------------------------------------------------------------------
// Check if existing database is selected
//---------------------------------------------------------------------
U_INT CFuiSketch::SaveStepB()
{	if (dial != '_yes') return SaveStepD();
	return OpenDatabase(0);
}
//---------------------------------------------------------------------
// Save database Step 1
//---------------------------------------------------------------------
U_INT CFuiSketch::SaveStepD()
{	BuildDBname();
	if (ses.IsEmpty())		return State;
	//--- Save the file first ---------------------------
	Write();
	//--- Ask confirmation ------------------------------
	char txt[512];
	_snprintf(txt,380,"Create database %s ?", dbase);
	dial = 0;
	CreateDialogBox("PLEASE CONFIRM",txt,1);
	statP	= SKETCH_SAVED; 
	return  SKETCH_PAUSE;
}
//---------------------------------------------------------------------
//  Check for answer 
//---------------------------------------------------------------------
U_INT CFuiSketch::SaveCheck()
{	if (dial != '_yes')	return SKETCH_PAUSE;
	return OpenDatabase(ScriptCreateOSM);
}
//---------------------------------------------------------------------
//  Open database for saving 
//---------------------------------------------------------------------
U_INT CFuiSketch::OpenDatabase(char **script)
{//--- Init for database saving -------------------
	TerrainHide();
	Stamp	= ses.GetStamp();			// Total objects
	objno	= 1;
	qKey	= 0;
	count = 0;
	sqlp = globals->sqm->OpenSQLbase(dbase,script);
	int ok = sqlp->use;
	return (ok)?(SKETCH_SAVEE):(SKETCH_SAVEF);
}
//---------------------------------------------------------------------
//  Save one building at a time
//---------------------------------------------------------------------
U_INT CFuiSketch::SaveObject()
{	char txt[128];
	//--- OK for saving databaes -------------------
	while (objno <= Stamp)
		{	OSM_Object *obj = ses.GetObjectOSM(objno++);
			if (0 == obj)						continue;
			if (!obj->IsValid())		continue;
			//---- Process QGT key first -----------
			U_INT okey = obj->GetKey();
		  if (qKey != okey) globals->sqm->UpdateOSMqgt(*sqlp,okey);
			qKey	= okey;
			count++;
			//--- Get a strip of translated vertices --
			GN_VTAB *tab = obj->StripToSupertile();
			if (0 == tab)					continue;
			globals->sqm->UpdateOSMobj(*sqlp,obj,tab);
			delete [] tab;
			//--- Display message ---------------------
			_snprintf(txt,127," Saving object %05d. DONT CLOSE EDITOR",count);
			DrawNoticeToUser(txt,2);
			//--- Check for any error -----------------
			char ok = sqlp->use;
			return (ok)?(SKETCH_SAVEE):(SKETCH_SAVEF);
		}
	return SKETCH_SAVEF;
}
//---------------------------------------------------------------------
// Save database Step 2:  Open or create database
//---------------------------------------------------------------------
U_INT CFuiSketch::SaveEnd()
{ char *erm = "There was database error. Check OpenStreet.log";
	char *okm = "DATABASE is now UPDATED ";
	char *msg;
	
	int OK = sqlp->use;
	globals->sqm->CloseOSMbase(sqlp);
	sqlp = 0;
	msg = (OK)?(okm):(erm);
	//------------------------------------------------
	STREETLOG("Saved  %05d Buildings", count);
	//--- Advise user --------------------------------
	globals->fui->DialogError(msg,"CITY EDITOR");
	return SKETCH_PAUSE;
}

//-----------------------------------------------------------------------
//	Time slice
//-----------------------------------------------------------------------
void CFuiSketch::TimeSlice()
{	TCacheMGR *tcm = 0;
	char *erm = "No file to edit";
	switch (State)	{
		//--- Wait file selection --------------
		case SKETCH_WSEL:
			  if (0 == *sname)	State = Abort(erm," Editor will close");
			  else							State = BuildStyList();
				return;
		//--- Create reference position --------
		case SKETCH_REFER:
				State = GotoReferencePosition();
				return;
		//--- Open again the file ---------------
		case SKETCH_OPEN:
				State = HereWeAre();
				return;
		//--- Read next building ----------------
		case SKETCH_NEXT:
				State = OneBuilding();
				return;
		//--- Pausing --------------------------
		case SKETCH_PAUSE:
				return;
		//--- Waiting on terrain ---------------
		case SKETCH_WAIT:
				State = TerrainWait();
				return;
		//---LOAD State ------------------------
		case SKETCH_LOAD:
				State = ReadNext();
				return;
		//--- SHOW state -----------------------
		case SKETCH_SHOW:
				State = ShowBuilding();
				return;
		//--- Replace model --------------------
		case SKETCH_ROBJ:
				ReplaceBuilding();
				return;
		//--- END of file ----------------------
		case SKETCH_ENDL:
				State = EndLoad();
				return;
		//--- END of seession ----------------------
		case SKETCH_ABORT:
				Close();
				return;
		case SKETCH_SAVEA:
				State = SaveStepB();
				return;
		//--- Check save answer --------------------
		case SKETCH_SAVED:
				State = SaveCheck();
				return;
		//--- Save one building --------------------
		case SKETCH_SAVEE:
				State = SaveObject();
				return;
		//--- Save Database Step 1----------------
		case SKETCH_SAVEF:
				State = SaveEnd();
				return;
	}
	return;
}

//-----------------------------------------------------------------------
//	Collect all models for the actual object type
//-----------------------------------------------------------------------
void CFuiSketch::CollectModels()
{	if (NoSelection())	return;
	repPM.otype	= confp.otype;
	repPM.dir	= GetOSMfolder(confp.otype);
	FPM.userp	= CITY_FILE_MOD;
	FPM.close = 1;
	FPM.sbdir = 0;
	FPM.text  = "Select a Model";
	FPM.dir   = GetOSMdirectory(confp.otype);
	FPM.pat		= "*.OBJ";
	CreateFileBox(&FPM);
	return;
}
//-----------------------------------------------------------------------
//	No File session
//-----------------------------------------------------------------------
int CFuiSketch::NoSession()
{	char txt[256];
	_snprintf(txt,255,"Error with file %s", spath);
	STREETLOG("%s",txt);
  CreateDialogBox("ATTENTION PLEASE",txt);
	return SKETCH_ABORT;
}
//-----------------------------------------------------------------------
//	Collect all Style
//-----------------------------------------------------------------------
int CFuiSketch::BuildStyList()
{	sBOX.SetParameters(this,'styl',(LIST_DONT_FREE+LIST_HAS_TITLE));
  styTT.SetSlotName("Style collection");
	//--- Look for session parameters --------
	bool ok = ses.ReadParameters(spath);
	if (!ok)		return NoSession();
	//----Warning ---------------------------
	ses.SetTRN(trn);
	sBOX.AddSlot(&styTT);
	ses.FillStyles(&sBOX);
	sBOX.Display();
	//--- Lock terrain now ------------------
	globals->Disp.DrawOFF (PRIO_TERRAIN);
	rcam->StopMove();
	return SKETCH_REFER;
}
//-----------------------------------------------------------------------
//	Swap scenery mode
//-----------------------------------------------------------------------
void CFuiSketch::Swap3D()
{	scny ^= 1;
	int md = (scny)?(-1):(+1);
	globals->noOBJ += md;
	return;
}
//-----------------------------------------------------------------------
//	Draw the sketch
//-----------------------------------------------------------------------
void	CFuiSketch::Draw()
{	CFuiWindow::Draw();
	//--------------------------------------------------
	return;
}
//---------------------------------------------------------------------
//  Write the file
//---------------------------------------------------------------------
void CFuiSketch::Write()
{	char txt[128];
	char *ed = "Area SW[%.2lf, %.2lf] NE[%.2lf, %.2lf] \n";
	if (0 == edit)			return;
	if (ses.IsEmpty())	return;
	FILE *fp = fopen(fnam,"w");
	if (0 == fp)				return;
	//--- Edit area --------------------------------
	_snprintf(txt,127,ed,SW.lat, SW.lon, NE.lat, NE.lon);
	fputs(txt,fp);
	ses.Write(fp);
	fclose(fp);
	return;
}
//---------------------------------------------------------------------
//  On positive dialog
//---------------------------------------------------------------------
void CFuiSketch::OnDialog(Tag rep)
{	if (rep != '_yes')	return;
	State = statP;
	dial  = rep;
	return;
}
//---------------------------------------------------------------------
//  Intercept events
//---------------------------------------------------------------------
void CFuiSketch::NotifyChildEvent(Tag idm,Tag itm,EFuiEvents evn)
{	switch (idm)  {
    case 'sysb':
			if (evn == EVENT_CLOSEWINDOW)	Write();
      SystemHandler(evn);
      return;
		//--- Model collection ------------------
		case 'modl':
      SystemHandler(evn);
      return;
		//--- Fill mode option -----------------
		case 'fopt':
			SetOptions(TRITOR_DRAW_FILL);
			return;
		//--- Scenery object -------------------
		case 'vopt':
			Swap3D();
			return;
		//--- View terrain ---------------------
		case 'vter':
			if (wfil)	return;
			if (tera)	State = TerrainHide();
			else			State = TerrainView();
			return;
		//--- Next Object ----------------------
		case 'nbut':
			if (wfil)	return;
			State = (State == SKETCH_WAIT)?(SKETCH_OPEN): (SKETCH_NEXT);
			return;
		//--- Load all ------------------------
		case	'vall':
			if (wfil)	return;
			State = StartLoad();
			return;
		//--- Style list -----------------------
		case 'styl':
			sBOX.VScrollHandler((U_INT)itm,evn);
			return;
		//--- New style ------------------------
		case 'nsty':
			if (wfil)	return;
			ChangeStyle();
			return;
		//--- Delete building ------------------
		case 'delt':
			RemoveObject();
			return;
		//--- Restore building ------------------
		case 'gobj':
			RestoreObject();
			return;
		//--- Replace building ------------------
		case 'robj':
			CollectModels();
			return;
		//--- Rotate left ------------------------
		case 'lrot':
			RotateBuilding(-oneD);
			return;
		//--- Rotate right -----------------------
		case 'rrot':
			RotateBuilding(+oneD);
			return;
		//--- Update cache -----------------------
		case 'btry':
			FlyOver();
			return;
		//--- Update database --------------------
		case 'bsav':
			if (0 == edit)	return;
			//State = SaveStepD();
			State = SaveStepA();
			return;
		//--- Dialogue button --------------------
		case 'dial':
			OnDialog(itm);
			return;
	}
	return;

}
//===================================================================================
//
//	Start OpenStreet session
//
//	NOTE:   The default group is for internal purpose and is never selected
//					from user parameter
//===================================================================================
D2_Session::D2_Session()
{	tr			= 0;
	trn			= 0;
	Stamp		= 1;
	roof = new CRoofFLAT('fixe');
	//--- Assign a default roof texture ------------------
	char *gnm = "$default$";
	rtex = new D2_TParam();
	//--- Allocate the default group ---------------------
	D2_Group *grp = new D2_Group(gnm,this);
	grp->SetTexName("DontRemove.jpg");
	grp->LoadTexture();
	grpQ[gnm] = grp;
	//--- Allocate others items --------------------------
}
//-----------------------------------------------------------------
//	Destroy resources
//-----------------------------------------------------------------
D2_Session::~D2_Session()
{	grpQ.clear();
	for (rp = repQ.begin(); rp != repQ.end(); rp++) delete (*rp).second;
	repQ.clear();
	for (rl = litQ.begin(); rl != litQ.end(); rl++)	delete (*rl).second;
	litQ.clear();
	delete roof;
	delete rtex;
}
//-----------------------------------------------------------------
//	Stop Session 
//-----------------------------------------------------------------
bool D2_Session::Stop01(char *ln)
{	STREETLOG("Invalid statement %s",ln);
	if (ln) exit(-1);
	return false;
}
//-----------------------------------------------------------------
//	Read session parameters 
//-----------------------------------------------------------------
bool	D2_Session::ReadParameters(char *dir)
{	char path[128];
	_snprintf(path,128,"%s/session.txt",dir);
	FILE  *f  = fopen(path,"r");
  if (0 == f)  return false;
	bool ok = ParseTheSession(f);
	fclose(f);
	return ok;
}
//-----------------------------------------------------------------
//	Read session Name
//	NOTE: When all style parameters are OK we may compute the
//				style ratio inside of each group. 
//-----------------------------------------------------------------
bool D2_Session::ParseTheSession(FILE *f)
{	char *line = ReadTheFile(f,buf);
	int nf = sscanf_s(line,"Session %63s", name,63);
	name[63] = 0;
	if (nf!= 1)				return false;
	//--- Decode all parameters ----------------------
	bool go = true;
	while (go)
	{	fpos = ftell(f);
	  line = ReadTheFile(f,buf);
		if (ParseReplace(f,line))		continue;
		if (ParseGroups(f,line))		continue;
		if (ParseStyles(f,line))		continue;
		break;
	}
	//--- Should be the end? ------------------------
	char *ok = strstr(buf,"FIN");
	if (ok)		return true;
	//-----------------------------------------------
	STREETLOG("Error arround %s in Session file",buf);
	return false;
}

//-----------------------------------------------------------------
//	Parse replacement list 
//-----------------------------------------------------------------
bool D2_Session::ParseReplace(FILE *f, char *line)
{	char txt[128];
	char tag[128];
	char val[128];
  char obj[128];
	strncpy(txt,line,127);
	int nf	= sscanf_s(txt," Replace ( %63[^ ),] , %63[^ ),] ) with %63[^ ]",tag,63,val,63,obj,63);
	if (nf != 3)		return false;
	//--- Insert an entry in replacement list -----------------
	OSM_REP *rpm = GetOSMreplacement(tag, val, obj);
	if (0 == rpm)	return false;
	//--- Add one replacement ---------------------------------
	std::pair <U_INT,OSM_REP*> p(rpm->otype,rpm);
	repQ.insert(p);
	return true;
}
//-----------------------------------------------------------------
//	Read a group 
//-----------------------------------------------------------------
bool D2_Session::ParseGroups(FILE *f, char *line)
{	char gnm[64];
	int nf	=	sscanf_s(line," Group %63s",gnm,63);
	    nf += sscanf_s(line," GROUP %63s",gnm,63);
	gnm[63]  = 0;
	if (nf != 1)		return false;
	//--- Allocate a new group -----------------
	D2_Group *gp = new D2_Group(gnm,this);
	grpQ[gnm] = gp;
	gp->Parse(f,this);
	groupQ.PutLast(gp);
	return true;
}
//-----------------------------------------------------------------
//	Read a Style 
//-----------------------------------------------------------------
bool D2_Session::ParseStyles(FILE *f, char *line)
{ char snm[64];
	char gnm[64];
	int nf	= sscanf_s(line," Style %63[^ (] ( %64[^)]s )",snm, 63, gnm, 63);
	    nf += sscanf_s(line," STYLE %63[^ (] ( %64[^)]s )",snm, 63, gnm, 63);
	snm[63] = 0;
	gnm[63]	= 0;
	if (nf != 2)		return false;
	//--- Have a new style -----------------------
	return AddStyle(f,snm,gnm);
}

//-----------------------------------------------------------------
//	Add a style  
//-----------------------------------------------------------------
bool D2_Session::AddStyle(FILE *f,char *sn,char *gn)
{	//--- Find group ----------------------------
	rg = grpQ.find(gn);
	if (rg == grpQ.end())	return Stop01(sn);
	D2_Group *gp = (*rg).second;
	D2_Style *sy = new D2_Style(sn,gp);
	//--- Decode Style parameters ---------------
	bool go = true;
	while (go)	
	{	fpos = ftell(f);	
		char * line = ReadTheFile(f,buf);
		_strupr(line);
		if (sy->DecodeStyle(line))	continue;
		//-- have a new style for the group -----
		gp->AddStyle(sy);
		//--- Exit now --------------------------
		fseek(f,fpos,SEEK_SET);
		if (sy->IsOK())				return true;
		go = false;
	}
	return false;
}
//------------------------------------------------------------------
//	Abort for bad parameters
//------------------------------------------------------------------
void D2_Session::Abort(char *msgr)
{	STREETLOG("%s",msgr);
	gtfo(msgr);
}
//-----------------------------------------------------------------
//	Force a Style 
//-----------------------------------------------------------------
D2_Style *D2_Session::GetStyle(char *nsty)
{	for (rg = grpQ.begin(); rg != grpQ.end(); rg++)
	{	D2_Group *grp		= (*rg).second;
		D2_Style *sty   = grp->GetStyle(nsty);
		if (0 == sty)	continue;
		return sty;
	}
	return 0;
}
//------------------------------------------------------------------
//	Select a roof texture from style
//------------------------------------------------------------------
D2_TParam *D2_Session::GetRoofTexture(D2_Style *sty)
{	D2_TParam *rft = sty->GetRoofTexture();
	if (0 == rft)	rft = rtex;					// Assign default
	rft->SetStyle(sty);
	return rft;
}
//------------------------------------------------------------------
//	Get a replacement for object type
//	NOTE:		When we replace the original object (otype) with an
//					object from the model catalog, 
//					we dont change the original property?
//------------------------------------------------------------------
bool D2_Session::GetReplacement(OSM_REP &rpm)
{	U_INT otype = rpm.otype;
	int nbr = repQ.count(otype);
	if (0 == nbr)		return false;
	//--- Get range -------------------------------------
	pair<multimap<U_INT,OSM_REP*>::iterator, 
		   multimap<U_INT,OSM_REP *>::iterator> 
			 R = repQ.equal_range(otype);
	int  k = RandomNumber(nbr);
	//--- search the kth element ------------------------

	for (rp = R.first; rp != R.second; rp++)
	{	if (k-- > 0)	continue;
		OSM_REP *rpp = (*rp).second;	
		rpm.dir  = rpp->dir;
		rpm.obr  =  Dupplicate(rpp->obr,FNAM_MAX);
		return true;
	}
	return false;
}
//------------------------------------------------------------------
//	Select a style 
//------------------------------------------------------------------
void	D2_Session::GetaStyle(D2_BPM *p)
{	int val = -1;
	grp			= 0;
	D2_Group *gp = 0;
	if (p->style)				return;
	for (gp = groupQ.GetFirst(); gp != 0; gp = gp->Next())
	{	int vg = gp->ValueGroup(p);
		if (vg <= val)		continue;
		grp = gp;				// Save group
		val	= vg;				// Save value
	}
	//--- Do we have one group -------------
	if (0 == grp) Abort("No Group found");
	//TRACE("SELECTED GROUP %s",grp->GetName());
	if (grp->ReachQuota()) 	groupQ.SwitchToLast(grp);
	sty = grp->GetOneStyle();
	grp->GenFloorNbr();			// Generate number of floors
	//--- check parameters -----------------
	if (0 == sty) Abort("No Style found");
	//--- Save parameters -------------------
	p->style = sty;
	sty->AssignBPM(p);
	return;
}
//-----------------------------------------------------------------
//	Add a building if not already in list
//-----------------------------------------------------------------
void D2_Session::AddLight(OSM_Object *L)
{	U_INT No = L->GetStamp();
	std::map<U_INT,OSM_Object*>::iterator rp = litQ.find(No);
	if (rp != litQ.end())		return;
	litQ[No]	= L;
	return;
}

//------------------------------------------------------------------
//	Return building 
//------------------------------------------------------------------
OSM_Object *D2_Session::GetObjectOSM(U_INT No)
{	//--- search in lights -------------------------
	rl = litQ.find(No);
	if (rl != litQ.end())		return (*rl).second;
	//--- search in groups -------------------------
	for (rg = grpQ.begin(); rg != grpQ.end(); rg++)
	{	D2_Group   *grp = (*rg).second;
		OSM_Object *bld = (*rg).second->FindBuilding(No);
		if (bld)	return bld;
	}
	return 0;
}
//------------------------------------------------------------------
//	Fill box with styles
//------------------------------------------------------------------
void D2_Session::FillStyles(CListBox *box)
{	Queue <D2_Style > *Q;
	D2_Style *sty = 0;
	for (rg = grpQ.begin(); rg != grpQ.end(); rg++)
	{	D2_Group *grp = (*rg).second;
		Q	= grp->GetStyleQ();
		for (sty = Q->GetFirst(); sty != 0; sty = sty->GetNext())
		{	box->AddSlot(sty);	}
	}
	return;
}
//------------------------------------------------------------------
//	Draw everything
//------------------------------------------------------------------
void D2_Session::Draw()
{	//--- Draw all buildings --------------------
	DebDrawOSMbuilding();							// Drawing environment
	for (rg = grpQ.begin(); rg != grpQ.end(); rg++)
	{	D2_Group *gpp = (*rg).second;
		gpp->DrawBuilding();	
	}
	EndDrawOSMbuilding();
	//--- Draw all lights ------------------------
	GLfloat col[4] = {1,1,0.5,1};
	DebDrawOSMlight(lightOSM,alphaOSM);
	for (rl = litQ.begin(); rl != litQ.end(); rl++)	(*rl).second->Draw();
	EndDrawOSMlight();
	return;
}
//------------------------------------------------------------------
//	Write all buildings
//------------------------------------------------------------------
void D2_Session::Write(FILE *fp)
{	U_INT bno = 1;
	U_INT wrt = 0;
	OSM_Object *bld = 0;

	for (bno = 1; bno <= Stamp; bno++)
	{	bld = GetObjectOSM(bno);
		if (0 == bld)	continue;
		bld->Write(fp);
		wrt++;
	}
	fputs("END\n",fp);
	STREETLOG("Imported %05d Buildings", (Stamp - 1));
	STREETLOG("Updated  %05d Buildings", wrt);
	return;
}
//------------------------------------------------------------------
//	Store all buildings
//------------------------------------------------------------------
void D2_Session::UpdateCache()
{	OSM_Object *obj;
	for (U_INT k=1; k <= Stamp; k++)
	{	obj = GetObjectOSM(k);
		bool ok = (0 != obj) && (obj->IsValid());
		if	(ok) globals->tcm->AddToPack(obj);
	}
	return;
}
//============================END OF FILE ================================================================================