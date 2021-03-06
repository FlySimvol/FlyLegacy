//============================================================================================
// WinScenery.cpp
//
// Part of Fly! Legacy project
//
// Copyright 2005 Chris Wallace
// Copyright 2007 Jean Sabatier
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
//===========================================================================================

#include "../Include/WinScenery.h"
//===========================================================================================
//	Display all credits 
//===========================================================================================
CWinOSM::CWinOSM(Tag idn, const char *filename)
:CFuiWindow(idn,filename,160,500,0)
{ //--- Init window-------------------------------
	SetTransparentMode();
  SetText("OSM tuning");
	InitOptions();
	CFuiWindow::ReadFinished();
}

//------------------------------------------------------------------------------------
//	Init options 
//------------------------------------------------------------------------------------
void CWinOSM::InitOptions()
{	CFuiCheckBox *box;
	U_INT	lin = 10;
	U_INT wit = MakeRGBA(255,255,255,255);
	for (U_INT k=1; k < OSM_MAX; k++)
	{	char *txt = GetOSMType(k);
		char  use = GetOSMUse(k);
		box	= new CFuiCheckBox(4,lin,150,20,this);
    AddChild(k,box,txt,FUI_TRANSPARENT,wit);
		box->SetState(use);
		lin +=20;
	}
}
//------------------------------------------------------------------------------------
//	Change options 
//------------------------------------------------------------------------------------
void	CWinOSM::NotifyChildEvent(Tag idm,Tag itm,EFuiEvents evn)
{	if (idm == 'sysb')	{  SystemHandler(evn); return;	}
	//--- Change option --------------------------------------
	char st = SwapOSMuse(idm);
	CFuiCheckBox *box = (CFuiCheckBox *)GetComponent(idm);
	if (box)	box->SetState(st);
	globals->tcm->FlushOSM();
	return;
}
//=========================END 0F FILE ====================================================
