//==============================================================================================
// Stream.h
//
// Part of Fly! Legacy project
//
//	Copyright 2003 Chris Wallace
//	copyright 2007 Jean Sabatier
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
//================================================================================================
#ifndef STREAM_H
#define STREAM_H
//================================================================================================
#include "../Include/FlyLegacy.h"
//============================================================================
// Local classes which encapsulate the state information about an open
//   stream file.  An instance of one of these classes is stored in the
//   'stream' member of the SStream data structure.
//-------------------------------------------------------------------------------------
//  JS: Add a low level read and decode functions for files like BGR and TSM
//      that does not separate TAG and item by a new line
//      Move the class definition in FlyLegacy.h
//============================================================================
class CStreamFile {
public:
  // Constructor
  CStreamFile (void);
	CStreamFile(char *fn,PFS *pfs,CStreamObject *obj);
  // Status methods
  bool      IsReadable (void)  { return readable; }
  bool      IsWriteable (void) { return writeable; }

  // Open/close methods
  int       OpenRead (char* filename, PFS *pfs);
  int       OpenWrite(char* filename);
  void      Close (void);

  // Low level Read methods--------------------------
  bool              Refill();
	bool							SameLine();
  bool              GetChar(char *st,int nb);
  bool              NextCharacter(short nc);
  bool              IsValid(char car);
  bool              NextTag();
  bool              GetTag();
  bool              ParseError();
	bool							NextString();
	bool							TrueString();
	//-----------------------------------------------
  char              NextToken(char dt);
	char							OneCharacter();
	void							SkipLine();
	//---------------------------------------------
  void              ReadFrom(CStreamObject *object);
  void              ReadDouble(double &nd);
  void              ReadLong(long &nb);
  void              ReadFloat(float &nb);
	void							ReadUINT(U_INT &nb);
	void							ReadINT(int &nb);
	void							GetINT(int &nd);
	void              GetFloat(float &nf);
	void							GetDouble(double &nd);
	void							GetUINT(U_INT &nb);
	void							GetString(char *dst,int sz);
  // Write methods------------------------------------
	void							DebObject();
	void							EndObject();
	void              EditPosition(SPosition &pos);
	void              WriteBlankLines (int i);
  void              WriteComment (const char *comment);
  void              WriteTag (Tag tag, const char *comment);
  void              WriteInt (int val);
	void							WriteUInt (U_INT val);
	//void              WriteFloat(float *value);
	void							WriteFloat(float vf);
  void              WriteDouble(double *value);
  void              WriteString(const char *value);
  void              WriteVector(SVector *value);
  void              WritePosition(SPosition *value);
	void							WriteOrientation(SVector &V);
	void              WriteTime(SDateTime *value);
  void              WriteTimeDelta(SDateTimeDelta *value);
  void              WriteMessage(SMessage *message);
  //---------------------------------------------------
  inline int        GetLine()   {return line;}
  inline void       IncLine()   {line++;}  
protected:
  void              WriteIndent (void);

public:
  PODFILE*  podfile;        // POD filesystem reference
  FILE*     f;              // Normal filesystem reference

protected:
  bool      readable;       // Whether this stream is readable
  bool      writeable;      // Whether this stream is writeable
  int       indent;
  int       line;           // Line number about
	U_INT	    tot;
	//----------String buffer -------------------------------------------------
	char			string[128];
	char			dtag[8];
  //----------For low level decoding ----------------------------------------
  char            buf[258];     // Read buffer
  int             nBytes;       // Number of bytes left
  char           *rpos;         // Character position in buffer
	char           *lpos;					// Last position
  Tag             tag;          // Decoded tag

};
//======================================================================
/*
 * Stream functions implemented in Stream.cpp
 */

int   OpenRStream(char *pn,char *fn, SStream &s);
int   OpenRStream(char *fn, SStream &s);
int   OpenStream(SStream *stream);
int   OpenStream(PFS *pfs, SStream *stream);
int		OpenBStream(char *fn,SStream &s);

void  CloseStream(SStream *stream);
void  ReadFrom(CStreamObject *object, SStream *stream);
void  SkipObject(SStream *stream);
bool  AdvanceToTag(unsigned int tag, SStream *stream);

bool  ReadInt (int *value, SStream *stream);
void  ReadShort (short *value, SStream *stream);
bool  ReadUInt (unsigned int *value, SStream *stream);
void  ReadFloat (float *value, SStream *stream);
void  ReadDouble (double *value, SStream *stream);
void  ReadString (char *value, int maxLength, SStream *stream);
void  ReadVector (SVector *value, SStream *stream);
void  ReadLatLon (SPosition *value, SStream *stream);
void  ReadPosition (SPosition *value, SStream *stream);
void  ReadTime (SDateTime *value, SStream *stream);
void  ReadTimeDelta (SDateTimeDelta *value, SStream *stream);
void  ReadMessage (SMessage *message, SStream *stream);
void  ReadTag (Tag *tag, SStream *stream);
int	  ReadInvertedTag (Tag *tag, SStream *stream);				// JSDEV* 
//=============================================================================
U_INT GetHardwareType(char *hwd);


//======================= END OF FILE ==============================================================
#endif // STREAM_H
