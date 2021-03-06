/*
 * TestBed.h
 *
 * Part of Fly! Legacy project
 *
 * Copyright 2003-2005 Chris Wallace
 * Copyright 2007-2009 Jean Sabatier
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


#ifndef TESTBED_H
#define TESTBED_H

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "FlyLegacy.h"
#include "../Include/3dMath.h"
#include "../Include/TerrainCache.h"
#include "crnlib.h"
//===================================================================================
//	Class ground Quad to build a terrain tile for tests
//===================================================================================
class GroundQUAD	
{	
protected:
	TC_GTAB				Q[4];											// Ground QUAD
	U_INT					xOBJ;											// Texture object
	U_INT         zOBJ;											// Compressed
	CTextureDef		txd;
	U_SHORT				wd;
	U_SHORT				ht;
	//-----------------------------------------------------------
	U_CHAR				cmp;											// Comprssion format
	int						mip;											// Mip base level [0,n]
	//-----------------------------------------------------------
	double				cx;
	double				cy;
	double				cz;
	//--- METHODES ----------------------------------------------
public:
	GroundQUAD(int x,int y, int z,int d,char c);
 ~GroundQUAD();
	void		SetQuad(int x, int y, int dim);
	void		GetTextureFromPOD(char *n,U_INT qx,U_INT qz);
	void		GetTextureFromSQL(char *n,U_INT dx,U_INT dz);
	void		GetTexOBJ(TEXT_INFO &txd);
	//-----------------------------------------------------------
	bool		IncDistance();
	bool		DecDistance();
	//-----------------------------------------------------------
	void	Draw();
	//-----------------------------------------------------------
	void	SetCompression()	{cmp = 1;}
};

//===================================================================================
//  TEST BED is used to test important features that needs most of Legacy environment
//  (Textures, terrain, weather, etc) before including those features into the main
//  code.
//===================================================================================
class CTestBed {
private:
	TEXT_INFO tinf;
	//---Original context -----------------------------------------
  CAMERA_CTX       ctx;         // Original camera and situation
	//--- Rabbit camera -----------------------------------------
	CRabbitCamera   *rcam;
	int							vp[4];
	//--- State --------------------------------------------------
	U_CHAR	state;
	U_CHAR	hold;
	int			time;
	//--- Terrain quad -------------------------------------------
	GroundQUAD  *Q0;
	GroundQUAD  *Q1;
  //----  METHODS --------------------------------------------------
public:
  CTestBed();
  virtual ~CTestBed();
	void	Clean();
	//----------------------------------------------------------------
	U_INT DetailKEY(U_INT qx,U_INT qz,U_INT tx, U_INT tz);
  //----------------------------------------------------------------
	void   Keyboard(U_INT key,int mod);
	void   Special(U_INT key,int mdf);
	void   TimeSlice();
	void	Shoot();
	void   Draw();
};
//================END OF FILE ===========================================================
#endif // TESTBED_H
