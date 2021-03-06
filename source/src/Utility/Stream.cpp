/*
 * Stream.cpp
 *
 * Part of Fly! Legacy project
 *
 * Copyright 2003-2006 Chris Wallace
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
 */

/*! \file Stream.cpp
 *  \brief Implements API functions for access to stream files
 *
 * This module contains the API for reading and writing to stream files.
 *   A stream file is a special type of text file consisting of a nested
 *   series of data tags (4-character strings enclosed by <> brackets) and
 *   data values (of various types...string, int, float, vector, etc.).
 *   These stream files are used to encode many of the internal objects
 *   within the application.
 *
 * Each line consists of leading whitespace (which is ignored) followed by
 *   either a data tag (<xxxx>), data value or comment (preceded by //)
 *   Comments can also follow data tags (but not data values)
 *
 *       // This comment is OK
 *       <dtag>   // This comment is OK
 *       123.45   // Invalid, comments cannot follow data values
 *
 * Nested sub-objects use the special tags <bgno> and <endo> as delimiters.
 *   It is up to the application to know how to interpret the various data tags
 *   values and sub-objects in the stream that it is reading.
 *
 */

#include "../Include/Globals.h"
#include "../Include/FlyLegacy.h"
#include "../Include/Utility.h"
#include "../Include/Endian.h"
#include "../Include/TerrainTexture.h"
#include "../Include/NewGauge.h"
//==================================================================================
int Lines = 0;
//==================================================================================
struct MSG_HW_TYPE {
	char *idn;									// Identifier
	U_INT hwd;									// Hardware type
};
//==================================================================================
//	List of hardware type
//==================================================================================
MSG_HW_TYPE hwdTAB[] = {
			 {"GAUGE"				,HW_GAUGE},
			 {"SWITCH"			,HW_SWITCH},
			 {"LIGHT"				,HW_LIGHT},
			 {"STATE"				,HW_STATE},
			 {"BUS"					,HW_BUS},
			 {"FUSE"				,HW_FUSE},
			 {"OTHER"				,HW_OTHER},
			 {"CIRCUIT"			,HW_CIRCUIT},
			 {"RADIO"				,HW_RADIO},
			 {"FLAP"				,HW_FLAP},
			 {"HILIFT"			,HW_HILIFT},
			 {"BATTERY"			,HW_BATTERY},
			 {"ALTERNATOR"	,HW_ALTERNATOR},
			 {"ANNUNCIATOR"	,HW_ANNUNCIATOR},
			 {"GENERATOR"		,HW_GENERATOR},
			 {"CONTACTOR"		,HW_CONTACTOR},
			 {"SOUNDFX"			,HW_SOUNDFX},
			 {"FLASHER"			,HW_FLASHER},
			 {"INVERTER"		,HW_INVERTER},
			 {"UNITLESS"		,HW_UNITLESS},
			 {"UNBENT"			,HW_UNBENT},
			 {"SCALE"				,HW_SCALE},
			 {0,0},
};
//==================================================================================
//	Locate hardware type
//==================================================================================
U_INT GetHardwareType(char *hwd)
{	char go = 0;
	while (go >= 0)
	{	char *idn = hwdTAB[go].idn;
		if (0 == idn)	return 0;
		if (strcmp(idn,hwd)) {go++; continue;}
		return hwdTAB[go].hwd;
	}
	return 0;
}
//==========================================================================
//  Stream object
//==========================================================================
//--------------------------------------------------------------
//  Decode the file name
//---------------------------------------------------------------
void CStreamObject::DecodeStripName(SStream *str, char *fn, TEXT_DEFN &txd)
{ char txt[128];
  int  nbf;
  ReadString(txt,128,str);
  if (2 == sscanf(txt," %d , %s", &nbf,fn))    {txd.nf = nbf; return;}
  txd.nf  = 1;
  if (1 == sscanf(txt," %s",fn))                return;
  gtfo("Invalid texture name %s", txt);
  return;
}

//--------------------------------------------------------------
//  Read parameters for a strip of vertical textures
//---------------------------------------------------------------
void CStreamObject::ReadStrip(SStream *str,TEXT_DEFN &txd)
{	char *erm = "Texture %s does not contain integral frame number";
  TEXT_INFO txf;                // Texture info;
	txf.azp = 0;
  DecodeStripName(str,txf.name,txd);
  //--- Read the texture ----------------------
  CArtParser img(TX_HIGHTR);
  _snprintf(txf.path,TC_TEXTURE_NAME_DIM,"ART/%s",txf.name);
  img.GetAnyTexture(txf);
  txd.Copy(txf);
  //---- Adjust texture height ----------------
  int nf  = txd.nf;
  txd.ht  = txf.ht / nf;
  if (0  != (txf.ht % nf)) gtfo(erm,txf.name);
  txd.dm  = txd.wd * txd.ht;
  return;
}
//--------------------------------------------------------------
//  Decode mono strip the file name
//---------------------------------------------------------------
void CStreamObject::DecodeMonoName(SStream *str,char *fn,int *px,int *py)
{ char txt[128];
  int x,y;
  ReadString(txt,64,str);
  int nf = sscanf(txt,"%d , %d ,%s",&x, &y, fn);
  if (3 == nf) { *px = x; *py = y; return;}
  strncpy(fn,txt,64);
  return;
}
//--------------------------------------------------------------
//  Read parameters for a mono strip texture
//  with optional origin
//---------------------------------------------------------------
void CStreamObject::ReadMonoStrip(SStream *str,TEXT_DEFN &txd,int *px, int *py)
{	TEXT_INFO txf;                // Texture info;
  txd.nf = 1;
  DecodeMonoName(str,txf.name,px,py);
  //--- Read the texture ----------------------
  CArtParser img(TX_HIGHTR);
  txf.azp   = 0;
  _snprintf(txf.path,TC_TEXTURE_NAME_DIM,"ART/%s",txf.name);
  img.GetAnyTexture(txf);
  txd.Copy(txf);
  //---- Adjust texture parameters ------------
  txd.dm  = txd.wd * txd.ht;
  return;
}
//-----------------------------------------------------------
//  Read texture parameters
//-----------------------------------------------------------
void CStreamObject::ReadBMAP(SStream *str,TEXT_DEFN &txd)
{ char fn[128];
	ReadString(fn,128,str);
	TEXT_INFO txf;  // Texture info;
	txf.azp = 0;
  CArtParser img(TX_HIGHTR);
  strncpy(txf.name,fn,TC_TEXTURE_NAME_NAM);
  _snprintf(txf.path,TC_TEXTURE_NAME_DIM,"ART/%s",fn);
  img.GetAnyTexture(txf);
  txd.Copy(txf);
	return;
}

//----------------------------------------------------------------------
//  If the position is specified, the needle uses its own Quad
//----------------------------------------------------------------------
void  CStreamObject::ReadCADR(SStream *str,CGauge *mgg,TC_VTAB *qd)
{ char txt[128];
  int px = 0;
  int py = 0;
  int wd = 0;
  int ht = 0;
  ReadString(txt,128,str);
  int nf = sscanf(txt,"%d , %d , %d , %d",&px,&py,&wd,&ht);
  //---- Check for px,py,wd,ht --------------------------
  if (4 != nf) gtfo("Invalide <cadr>");
  px += mgg->GetXPos();
  py += mgg->GetYPos();
  //---- Build the needle quad ------------------------
  mgg->BuildQuad(qd,px,py,wd,ht);
  return;
}
//----------------------------------------------------------------------
//  Decode size statement
//----------------------------------------------------------------------
void CStreamObject::ReadSIZE(SStream *str,int *px,int *py, int *wd, int *ht)
{ char txt[128];
  ReadString(txt,128,str);
  int n1 = sscanf(txt,"%d , %d , %d, %d",px,py,wd,ht);
  if (4 == n1)    return;
  int n2 = sscanf(txt,"%d ",px);
  ReadInt(py,str);
  ReadInt(wd,str);
  ReadInt(ht,str);
  return;
}
//----------------------------------------------------------------------
//  Decode size statement
//----------------------------------------------------------------------
void CStreamObject::ReadSIZE(SStream *str,short *px,short *py, short *wd, short *ht)
{ char txt[128];
  ReadString(txt,128,str);
  int n1 = sscanf(txt,"%d , %d , %d, %d",px,py,wd,ht);
  if (4 == n1)    return;
  int n2 = sscanf(txt,"%d ",px);
  ReadShort(py,str);
  ReadShort(wd,str);
  ReadShort(ht,str);
  return;
}
//----------------------------------------------------------------------
//  Decode size statement
//----------------------------------------------------------------------
void CStreamObject::ReadSIZE(SStream *str,S_PIXEL *t, short *wd, short *ht)
{ char txt[128];
  int  px,py;
  ReadString(txt,128,str);
  int n1 = sscanf(txt,"%d , %d , %d, %d",&px,&py,wd,ht);
  if (4 != n1)
	{ int n2 = sscanf(txt,"%d ",&px);
		ReadInt(&py,str);
		ReadShort(wd,str);
		ReadShort(ht,str);
	}
	//--- Build the gauge tour -----------------------
	t[NW_PIX].x = px;		// NE corner
	t[NW_PIX].y = py;
	//--- SW corner -------------------
	t[SW_PIX].x = px;
	t[SW_PIX].y = py + *ht;
	//--- SE corner -------------------
	t[SE_PIX].x = px + *wd;
	t[SE_PIX].y = py + *ht;
	//--- NE corner -------------------
	t[NE_PIX].x = px + *wd;
	t[NE_PIX].y = py;
  return;
}

//==================================================================================
//  CStream file implementation
//==================================================================================
CStreamFile::CStreamFile (void)
{
  readable  = writeable = false;
  podfile   = NULL;
  f         = NULL;
  indent    = 0;
  nBytes    = 0;
}
//------------------------------------------------------------------------
//  Open for reading
//------------------------------------------------------------------------
CStreamFile::CStreamFile(char *fn,PFS *pfs,CStreamObject *obj)
{	if (OpenRead (fn,pfs))	ReadFrom(obj);
	Close();
}
//------------------------------------------------------------------------
//  Open for reading
//------------------------------------------------------------------------
int CStreamFile::OpenRead (char *filename, PFS *pfs)
{ line		= 0;
	tot			= 0;						// Total blocks
  podfile = popen (pfs, filename);
  if (podfile != NULL) {
    // File was opened from the POD filesystem
    readable = true;
  } else 
	{ // Attempt to open from normal (non-POD) filesystem
    f = fopen (filename, "rb");
    if (f != NULL) readable = true;
  }
  //---------Init for first reading ------------------------
  nBytes		= 1;
  strcpy(buf," ");                // trigger pump
  rpos			= buf;
	lpos			= buf + 1;
	return (readable)?(1):(0);
}
//------------------------------------------------------------------------
//    Fill the read buffer with a chunk of charater from the file
//    NOTE: This actually work only for PODFILE
//------------------------------------------------------------------------
bool CStreamFile::Refill()
{	if (0 == podfile) return false;
	FILE *file = podfile->pFile;
	switch (podfile->source) {
    case PODFILE_SOURCE_POD:
      // Reads to different files within the same POD may be interspersed,
      //   so the read pointer must be reset to the next expected byte for
      //   this pod file.
			//_lock_file(file);
      fseek (file, podfile->pos, SEEK_SET);
      nBytes        = fread (buf, 1, 256, file);
      //podfile->pos  = ftell (podfile->pFile);
			podfile->pos += nBytes;
      rpos          = buf;
			lpos					= buf + nBytes;
//			buf[nBytes]		= 0;
			tot++;
			//_lock_file(file);
			//--- DEBUG ONLY ----------------------
			//if (nBytes < 256)
			//int a = 0;
			//-------------------------------------
      return true;

    case PODFILE_SOURCE_DISK:
      nBytes				= fread (buf, 1, 256, file);
      rpos					= buf;
			lpos					= buf + nBytes;
//			buf[nBytes]		= 0;
      return true;
    }

  return false;
}
//------------------------------------------------------------------------
//  Parse error
//------------------------------------------------------------------------
bool CStreamFile::ParseError()
{ buf[255] = 0;
  gtfo ("Error in file: %s near %s", podfile->fullFilename,buf);
  return false;
}
//------------------------------------------------------------------------
//  check for valid character
//  -Skip space, tab, CR, LF,
//------------------------------------------------------------------------
bool CStreamFile::IsValid(char car)
{ if (' '   == car) return false;
  if ('\t'  == car) return false;
  if ('\n'  == car) return false;
  if ('\r'  == car) return false;
  return true;
}
//------------------------------------------------------------------------
//  One character
//------------------------------------------------------------------------
char CStreamFile::OneCharacter()
{	char car = *rpos++;
	if (rpos == lpos)	Refill();
	return car;
}
//------------------------------------------------------------------------
//  Check for inline position
//------------------------------------------------------------------------
bool CStreamFile::SameLine()
{	char car = OneCharacter();
	return (car)?(car != 0x0A):false;
}	
//------------------------------------------------------------------------
//  By pass n characters
//------------------------------------------------------------------------
bool CStreamFile::NextCharacter(short nc)
{ while (nc-- != 0)	OneCharacter();
  return true;
}
//------------------------------------------------------------------------
//  Return n characters in the string
//------------------------------------------------------------------------
bool  CStreamFile::GetChar(char *st,int nb)
{ while (nb-- > 0) *st++ = OneCharacter();
  return true;
}
//------------------------------------------------------------------------
//  Skip any comment
//------------------------------------------------------------------------
void CStreamFile::SkipLine()
{	while (SameLine())	continue;
	return;
}
//------------------------------------------------------------------------
//  Advance up to the next valid character
//  -Skip space, tab, CR, LF,
//	NOTE that when a character is invalid from the abose function it is
//	not zero. Thus for invalid character the call OneCharacter is always
//	true
//------------------------------------------------------------------------
char CStreamFile::NextToken(char dt)
{ while (!IsValid(*rpos) && OneCharacter()) continue;
	return (dt)?(OneCharacter()):(*rpos);
}
//------------------------------------------------------------------------
//  Decode tag
//------------------------------------------------------------------------
bool CStreamFile::GetTag()
{ int  dim  = 0;
	char car  = 0;
	for (int k=0; k < 8; k++)
	{	car = OneCharacter();
		if ('>' == car)	break;
    dtag[k] =  car;
  }
  if (car != '>')         return ParseError();
  SkipLine();                               // Skip > and 0
  tag = StringToTag(dtag);
  return true;
}
//------------------------------------------------------------------------
//  Find next tag
//------------------------------------------------------------------------
bool CStreamFile::NextTag()
{ while (NextToken(1) != '<') continue;
 // OneCharacter();
  return GetTag();
}
//------------------------------------------------------------------------
//  Find next string
//------------------------------------------------------------------------
bool CStreamFile::NextString()
{*string = 0;
  NextToken(0);
	int	k;
	for (k = 0; k < 127; k++)
	{	char car = OneCharacter();
		if (!IsValid(car))	break;
		string[k]	= car;
	}
	string[k]	= 0;
	SkipLine();
	return true;
}
//------------------------------------------------------------------------
//  Find next string without comment
//------------------------------------------------------------------------
bool	CStreamFile::TrueString()
{	while(NextString())
	{	if (strncmp(string,"//",2) != 0)	return true; 
	}
	return false;
}
//------------------------------------------------------------------------
//  Get a double number
//------------------------------------------------------------------------
void CStreamFile::GetINT(int &nd)
{	if (!TrueString())	return;
	nd = int(atoi (string));
}

//------------------------------------------------------------------------
//  Get a double number
//------------------------------------------------------------------------
void CStreamFile::GetUINT(U_INT &nd)
{	if (!TrueString())	return;
	nd = U_INT(atoi (string));
}
//------------------------------------------------------------------------
//  Get a float number
//------------------------------------------------------------------------
void CStreamFile::GetFloat(float &nd)
{	if (!TrueString())	return;
	nd = float(atof (string));
}
//------------------------------------------------------------------------
//  Get a String
//------------------------------------------------------------------------
void CStreamFile::GetString(char *dst,int sz)
{	if (!TrueString())	return;
	strncpy(dst,string,sz);
}

//------------------------------------------------------------------------
//  Get a double number
//------------------------------------------------------------------------
void CStreamFile::GetDouble(double &nd)
{	if (!TrueString())	return;
	nd = double(atof (string));
}
//------------------------------------------------------------------------
//  Read a double number
//------------------------------------------------------------------------
void CStreamFile::ReadDouble(double &nd)
{ char  db[12];
  GetChar(db,sizeof(double));
  double d = *(double*)(db);
  nd   = LittleEndian (d);
  return;
}
//------------------------------------------------------------------------
//  Read a long number
//------------------------------------------------------------------------
void CStreamFile::ReadLong(long &nb)
{ char lg[12];
  GetChar(lg,sizeof(long));
  long nl = *(long*)(lg);
  nb   = LittleEndian (nl);
  return;
}
//------------------------------------------------------------------------
//  Read a  number
//------------------------------------------------------------------------
void CStreamFile::ReadINT(int &nb)
{ char lg[12];
  GetChar(lg,sizeof(int));
  int nl = *(int*)(lg);
  nb   = LittleEndian (nl);
  return;
}
//------------------------------------------------------------------------
//  Read a unsigned number
//------------------------------------------------------------------------
void CStreamFile::ReadUINT(U_INT &nb)
{ char lg[12];
  GetChar(lg,sizeof(int));
  U_INT nl = *(U_INT*)(lg);
  nb   = LittleEndian (nl);
  return;
}

//------------------------------------------------------------------------
//  Read a float number
//------------------------------------------------------------------------
void CStreamFile::ReadFloat(float &nb)
{ char fl[12];
  GetChar(fl,sizeof(float));
  float nf = *(float*)(fl);
  nb   = LittleEndian (nf);
  return;
}
//------------------------------------------------------------------------
//  Read from streamed object (copied from the normal ReadFrom)
//  
//------------------------------------------------------------------------
void  CStreamFile::ReadFrom(CStreamObject *object)
{
  // The first tag should be <bgno>.  Some default files (e.g. TRI40.PNL) have
  //   missing <bgno> tags so this code must be tolerant of that.
  bool openingBgno = true;

  // Start sending tags to the CStreamObject, up until (and not including)
  //   the <endo> corresponding to the <bgno> found above.  A count of
  //   nested sub-objects is kept to ensure that we don't quit prematurely
  int nestCount = 0;

  // Now continue to read tags until <endo>, sending them to CStreamObject::Read
 // while ( ! (((tag = snexttag (s, 256, stream)) == 'endo')  || tag == 0 && (nestCount == 0)) )
  while (NextTag() && (tag != 'endo'))
  { if (tag == 'bgno') {
      if (openingBgno) continue;        // This is the expected opening <bgno>, just ignore it
      // This is the start of a nested sub-object, increment nestCount
      nestCount++;
    }
    if (tag == 'endo') {
      // This is the end of a nested sub-object, so decrement nestCount.
      //   Note that if nestCount is already zero, then we wouldn't get here
      nestCount--;
    } 
    // Pass tag to the object and keep going
    int rep = object->Read (this, tag);
    // After the first tag has been read, we are no longer expecting an
    //   opening <bgno>
    openingBgno = false;
  }

  // Finally call CStreamObject::ReadFinished
  object->ReadFinished();
}
//========================================================================
//  Writing functions
//========================================================================
//------------------------------------------------------------------------
//  Open for writing
//------------------------------------------------------------------------
int CStreamFile::OpenWrite (char *filename)
{ f = fopen (filename, "w");
  if (f != NULL)  writeable = true;
	return (writeable)?(1):(0);
}
//------------------------------------------------------------------------
//  Close file
//------------------------------------------------------------------------
void CStreamFile::Close (void)
{
  if (IsReadable()) {
    if (podfile != NULL) {
      // Close podfile
      podfile = pclose (podfile);
    } else 
		if (f != NULL) {
      // Close normal file
      fclose (f);
      f = 0;
    }
    readable = false;
  } else 
	if (IsWriteable()) {
    // Close disk file
    fclose (f);
    f = 0;
    writeable = false;
  }
}
//------------------------------------------------------------------
//	Write a blank line
//------------------------------------------------------------------
void CStreamFile::WriteBlankLines (int nLines)
{ if (IsWriteable()) {for (int i=nLines; i > 0; i++) fprintf (f, "\n"); }
}
//------------------------------------------------------------------
//	Write a comment
//------------------------------------------------------------------
void CStreamFile::WriteComment (const char *comment)
{	if (IsWriteable())	fprintf (f, "// %s\n", comment);
}
//------------------------------------------------------------------
//	Write a Tag with indentation
//------------------------------------------------------------------
void CStreamFile::WriteTag (Tag tag, const char* comment = NULL)
{	if (!IsWriteable()) return;
  // Decrease indent level for <endo> tag
  if (tag == 'endo') {
      indent--;
      if (indent < 0) indent = 0;
    }
  // Write bracketed tag value
  char s[8];
  TagToString (s, tag);
  WriteIndent();
  fprintf (f, "<%s>", s);
  // Write optional comment following tag value
  if (comment)   fprintf (f, " %s", comment);
  fprintf (f, "\n");
  // Increase indent level for <bgno> tag
  if (tag == 'bgno')	indent++;
	
}
//------------------------------------------------------------------
//	Write a bgno tag with indentation
//------------------------------------------------------------------
void	CStreamFile::DebObject()
{	WriteTag('bgno', "========== BEGIN OBJECT ==========");
}
//------------------------------------------------------------------
//	Write a end tag with indentation
//------------------------------------------------------------------
void	CStreamFile::EndObject()
{	WriteTag('endo', "========== END OBJECT ============");
}
//------------------------------------------------------------------
//	Write a integer
//------------------------------------------------------------------
void CStreamFile::WriteInt(int val)
{
  if (IsWriteable()) {
    WriteIndent();
    fprintf (f, "%d\n", val);
  }
}

void CStreamFile::WriteUInt(U_INT val)
{
  if (IsWriteable()) {
    WriteIndent();
    fprintf (f, "%u\n", val);
  }
}

void CStreamFile::WriteFloat(float vf)
{
  if (IsWriteable()) {
    WriteIndent();
    fprintf (f, "%f\n", vf);
  }
}

void CStreamFile::WriteDouble(double *value)
{
  if (IsWriteable()) {
    WriteIndent();
    fprintf (f, "%lf\n", *value);
  }
}
//------------------------------------------------------------------
//	Write a string
//------------------------------------------------------------------
void CStreamFile::WriteString(const char *value)
{	if (!IsWriteable()) return;
  WriteIndent();
  fprintf (f, "%s\n", value);
}
//------------------------------------------------------------------
//	Write a Vector
//------------------------------------------------------------------
void CStreamFile::WriteVector(SVector *value)
{	if (!IsWriteable()) return;
  WriteIndent();
  fprintf (f, "%lf\n", value->x);
  WriteIndent();
  fprintf (f, "%lf\n", value->y);
  WriteIndent();
  fprintf (f, "%lf\n", value->z);
}
//------------------------------------------------------------------
//	Write Orientation
//	Change system coordianate 
//------------------------------------------------------------------
void CStreamFile::WriteOrientation(SVector &V)
{	if (!IsWriteable())	return;
	double tx = 0;
	double ty = -V.z;
	double tz =  V.y;
	WriteIndent();
	fprintf(f, "%.6lf, %.6lf, %.6lf\n",tx,ty,tz);
}
//------------------------------------------------------------------
//	Write a position
//------------------------------------------------------------------
void CStreamFile::WritePosition(SPosition *value)
{
  if (IsWriteable()) {
    WriteIndent();
    fprintf (f, "%.6f,", value->lat);
    fprintf (f, "%.6f,", value->lon);
    fprintf (f, "%.6f\n",value->alt);
  }
}
//------------------------------------------------------------------
//	Edit a position
//------------------------------------------------------------------
void	CStreamFile::EditPosition(SPosition &pos)
{	char edt[32];
	WriteTag('geop',"------GEO POSITION (lat,lon,alt)-------");
	EditLat2DMS(pos.lat, edt, 0);
	WriteString(edt);
	EditLon2DMS(pos.lon, edt, 0);
	WriteString(edt);
	WriteDouble(&pos.alt);
}
//------------------------------------------------------------------
//	write time
//------------------------------------------------------------------
void CStreamFile::WriteTime(SDateTime *value)
{	if (!IsWriteable()) return;
  WriteIndent();
  fprintf (f, "SDateTime not implemented yet\n");
}

void CStreamFile::WriteTimeDelta(SDateTimeDelta *value)
{
  if (IsWriteable()) {
    WriteIndent();
    fprintf (f, "SDateTimeDelta not implemented yet\n");
  }
}

void CStreamFile::WriteMessage(SMessage *message)
{
  if (IsWriteable()) {
    WriteIndent();
    fprintf (f, "SMessage not implemented yet\n");
  }
}
//---------------------------------------------------------
//	Write indentation
//--------------------------------------------------------
void CStreamFile::WriteIndent (void)
{
  for (int i=indent; i>0; --i) fprintf (f, "\t");
}

//==========================================================================
// Local function that reads the next line from the stream file, discards
//   comment lines, and returns a pointer to the start of the first
//   non-whitespace character in the line.  The complete line is copied
//   in to the buffer pointed to by s.  If EOF has been reached, the function
//   returns NULL.  Use peof() to verify EOF
//===========================================================================
static char* sgets (char* s, int maxLength, SStream *stream)
{
  char* p = NULL;

  // TODO replace stream->stream with CStreamFileReadable call
  CStreamFile* sf = (CStreamFile*)stream->stream;
  if (sf->IsReadable()) {
    if (sf->podfile) {
      // Read next tag from POD file
      while (pgets(s, maxLength, sf->podfile) != NULL) {
        // Skip leading whitespace
        sf->IncLine();
        char *test = s;
        while ((*test == ' ') || (*test == '\t')) test++;

        // If not a comment, then go ahead and parse it
        if (strncmp (test, "//", 2) != 0) {
          p = test;
          break;
        }
      }
    } else if (sf->f != NULL) {
      // Read next tag from normal file
      while (fgets(s, maxLength, sf->f) != NULL) {
        // Skip leading whitespace
        sf->IncLine();
        char *test = s;
        while ((*test == ' ') || (*test == '\t')) test++;

        // If not a comment, then go ahead and parse it
        if (strncmp (test, "//", 2) != 0) {
          p = test;
          break;
        }
      }
    }
  }

  return p;
}

//------------------------------------------------------------------------
// Local function to read the next tag value.  Tags are surrounded by
//  the marker characters < and >.
// JS: Not sure what <zero> stand for
//------------------------------------------------------------------------
Tag    snexttag (char* s, int maxLength, SStream *stream)
{
  Tag tag = 0;

  char *p;
  bool gotTag = false;
  while (!gotTag) {
    p = sgets (s, maxLength, stream);
    if (p == NULL) {
      // Error or EOF encountered
      break;
    } else {
      // Check for brackets
      if ((strlen(p) >= 6) && (p[0] == '<') && (p[5] == '>')) {
        char tagstring[5];
        strncpy (tagstring, (p+1), 4);
        tag = StringToTag (tagstring);
        return tag;
      }
    }
  }
  return tag;
}

//===========================================================================
// OpenStream - Open a stream file for reading
//
// The function returns 1 if the stream was successfully opened, 0 otherwise
//==========================================================================
int OpenRStream(char *fn,SStream &s)
{ if (0 == fn)	return 0;
	strncpy (s.filename, fn,(PATH_MAX-1));
  strcpy  (s.mode, "rb");
	return OpenStream (&globals->pfs, &s);
}
//===========================================================================
// OpenStream - Open a binary stream file for reading
//
// The function returns 1 if the stream was successfully opened, 0 otherwise
//==========================================================================
int OpenBStream(char *fn,SStream &s)
{ if (0 == fn)	return 0;
	strncpy (s.filename, fn,(PATH_MAX-1));
  strcpy  (s.mode, "rb");
	return OpenStream (&globals->pfs, &s);
}

//===========================================================================
// OpenStream - Open a stream file for reading from WORLD DIRECTORY
//
// The function returns 1 if the stream was successfully opened, 0 otherwise
//==========================================================================
int OpenRStream(char *pn,char *fn,SStream &s)
{ _snprintf(s.filename,PATH_MAX-1,"%s/%s",pn,fn);
  strncpy (s.mode, "r",3);
	return OpenStream (&globals->pfs, &s);
}

//==========================================================================
// OpenStream - Open a stream file for reading
//
// The function returns 1 if the stream was successfully opened, 0 otherwise
//==========================================================================
int   OpenStream(PFS *pfs, SStream *stream)
{
  int rc = 0;

  // Check mode attribute of SStream data structure to determine if this is
  //   a readable or writeable stream.  Mixed-mode "rw" and append mode "a"
  //   streams are not supported.

  if (*stream->mode == 'r') {
    // Open readable stream
    CStreamFile* sf = new CStreamFile;
    sf->OpenRead (stream->filename, pfs);
    if (sf->IsReadable()) {
      // OK
      stream->stream = sf;
      return 1;
    } else {
      // Error opening stream file for reading
      WARNINGLOG ("OpenStream : Could not open %s in read mode", stream->filename);
      delete sf;
      return 0;
    }
  } else if (*stream->mode == 'w') {
    // Open writeable stream
    CStreamFile* sf = new CStreamFile;
    sf->OpenWrite (stream->filename);
    if (sf->IsWriteable()) {
      // OK
      stream->stream = sf;
      return 1;
    } else {
      // Error opening stream file for writing
      WARNINGLOG ("OpenStream : Could not open %s in write mode", stream->filename);
      delete sf;
      rc = 0;
    }
  }

  return rc;
}
//====================================================================================
// CloseStream - Close a stream file
//
// This function closes the specified stream file.  No further read/write operations
//   can be performed on the SStream object.
//=======================================================================================
void  CloseStream(SStream *stream)
{
  CStreamFile* sf = (CStreamFile*)stream->stream;
  sf->Close ();
  delete sf;
  stream->stream = NULL;
}

//===================================================================================
// ReadFrom - Initialize a CStreamObject from the contents of a stream file
//
// Example (CAircraft is descended from CStreamObject):
//   CAircraft *pAircraft = new CAircraft();
//   SStream *pStream = new SStream;
//   strcpy (pStream->filename, "World/Flyhawk.nfo");
//   strcpy (pStream->mode, "r");
//   if (OpenStream (pStream)) {
//     // Stream successfully opened
//     ReadFrom (pAircraft, pStream);
//     CloseStream (pStream);
//   }
//=============================================================================
void  ReadFrom(CStreamObject *object, SStream *stream)
{
  // The first tag should be <bgno>.  Some default files (e.g. TRI40.PNL) have
  //   missing <bgno> tags so this code must be tolerant of that.
  //  JS: Some aircraft has a tag <zero> before <bgno>
  char s[256];
  Tag tag;
  bool openingBgno = true;

  // Start sending tags to the CStreamObject, up until (and not including)
  //   the <endo> corresponding to the <bgno> found above.  A count of
  //   nested sub-objects is kept to ensure that we don't quit prematurely
  int nestCount = 0;

  // Now continue to read tags until <endo>, sending them to CStreamObject::Read
  while ( ! (((tag = snexttag (s, 256, stream)) == 'endo')  || tag == 0 && (nestCount == 0)) )
  { if (tag == 'bgno') {
      if (openingBgno) 
      { // This is the expected opening <bgno>, just ignore it
        openingBgno = false;
        continue; 
      }
      // This is the start of a nested sub-object, increment nestCount
      nestCount++;
      object->Read (stream, tag);
      continue;
    }
    if (tag == 'endo') {
      // This is the end of a nested sub-object, so decrement nestCount.
      //   Note that if nestCount is already zero, then we wouldn't get here
      nestCount--;
      object->Read (stream, tag);
      continue;
    }
    // Normal tag, pass it to the object
		//--- Check result -----------------------
		Tag ret = object->Read (stream, tag);
		if (ret == TAG_EXIT)		return;
		if (ret == TAG_STOP)		break;
    // Process next statement ----------------
    
  }

  // Finally call CStreamObject::ReadFinished
  object->ReadFinished();
	return;
}


//
// SkipObject - Skip over an object bracketed by <bgno> <endo> tags
//
// This function is called when it is desired to skip over a sub-object
//   (and all of its nested sub-sub-objects) in a stream file.  The first
//   tag read should be <bgno> which marks the beginning of the object to
//   be skipped.  From this point on, all tags and values are read and
//   discarded until the <endo> tag corresponding to the beginning tag
//   is read.
//
void  SkipObject(SStream *stream)
{
  //-- skip every thing to next bgno tag -------------------------------------
  char s[256];
  Tag tag;
  while ((tag = snexttag (s, 256, stream)) != 'bgno') {}

  // Continue reading tags up to the <endo> corresponding to the <bgno> found above.
  //   A count ofnested sub-objects is kept to ensure that we don't quit prematurely
  int nestCount = 0;

  // Now continue to read tags until <endo>, sending them to CStreamObject::Read
  bool keepGoing = true;
  while (keepGoing) {
    tag = snexttag (s, 256, stream);
    if (tag == 'bgno') {
      // This is the start of a nested sub-object, so increment nestCount
      nestCount++;
    } else if (tag == 'endo') {
      // This is the end of either a nested sub-object (if nestCount > 0) or
      //   the end of the object we're skipping (if nestCount == 0)
      if (nestCount > 0) {
        nestCount--;
      } else {
        keepGoing = false;
      }
    }
  }
}

//
// AdvanceToTag - Skip forward until the specified tag is found
//
bool AdvanceToTag(unsigned int tag, SStream *stream)
{
  bool rc = false;
  
  char s[80];
  Tag nextTag;
  
  do
  {
    nextTag = snexttag (s, 80, stream);
    if (nextTag == tag)
      rc = true;
  }  while ((nextTag != 0) && (nextTag != 'endo') && !rc);
  
  return rc;
}

//
// ReadInt - Read a (signed) integer value from the stream
//
bool  ReadInt (int *value, SStream *stream)
{
  // Read up to next token
  char s[256];
  char *p = sgets (s, 256, stream);
  if (p) *value = atoi (p);
  return (p != 0);
}
//
// ReadInt - Read a (signed) short value from the stream
//
void  ReadShort (short *value, SStream *stream)
{
  // Read up to next token
  char s[256];
  char *p = sgets (s, 256, stream);
  *value = (short)atoi (p);
}
//
// ReadUInt - Read an unsigned integer value from the stream
//
bool  ReadUInt (unsigned int *value, SStream *stream)
{
  // Read up to next token
  char s[256];
  char *p = sgets (s, 256, stream);

  // Parse data value
  if (p) *value = (unsigned int)(atoi(p));
  return (p != 0);
}

//
// ReadFloat - Read a single-precision float value value from the stream
//
void  ReadFloat (float *value, SStream *stream)
{
  // Read up to next token
  char s[256];
  ReadString (s, 256, stream);
  *value = (float)(atof (s));
}

//
// ReadDouble - Read a double-precision float value value from the stream
//
void  ReadDouble (double *value, SStream *stream)
{
  // Read up to next token
  char s[256];
  ReadString (s, 256, stream);
  *value = atof (s);
}

//==========================================================================
// ReadString - Read a newline-terminated character string from the stream
//=========================================================================
void  ReadString (char *value, int maxLength, SStream *stream)
{
  // Read up to next token
  char s[256];
  char *p = sgets (s, 256, stream);

  // If empty line, copy empty string to return value
  if ((strncmp (p, "\r\n", 2) == 0)) {
    strcpy (value, "");
  } else {
    TrimTrailingWhitespace (p);

    if ((int)strlen(p) >= maxLength) {
      // String was longer than supplied buffer
      WARNINGLOG ("ReadString : Line truncated (len=%d max=%d)\n%s", strlen(p), maxLength, p);
      strncpy (value, p, maxLength-1);
    } else {   strcpy (value, p);    }
  }
}

//
// ReadVector - Read a three-component single-precision float vecotr from the stream
//
void  ReadVector (SVector *value, SStream *stream)
{
  // Read up to next token
  char s[256];
  char *p = sgets (s, 256, stream);

  // Parse the data value
  float x, y, z;
  sscanf (p, "%f, %f, %f", &x, &y, &z);
  value->x = (double)x;
  value->y = (double)y;
  value->z = (double)z;
}

//
// ReadLatLon - Read a geographical latitude/longitude position from a stream
//
// Format is:
//    33 56 24.6 N          (N 33 deg 56 min 24.6 sec latitude)
//    118 23 56.5 W         (W 118 deg, 23 min, 56.5 sec longitude)
//
void  ReadLatLon (SPosition *value, SStream *stream)
{
  // Declare locals for parsing of position strings
  char s[256];
  int deg, min;
  float sec;
  char  hemi;

  // First line is latitude
  ReadString (s, 256, stream);
  double lat = 0;
  if (sscanf (s, "%d %d %f %c", &deg, &min, &sec, &hemi) == 4) {
    switch (hemi) {
    case 'N':
    case 'n':
      lat = (deg * 3600.0) + (min * 60.0) + sec;
      break;

    case 'S':
    case 's':
      lat = -((deg * 3600.0) + (min * 60.0) + sec);
      break;

    default:
      gtfo ("ReadLatLon : Invalid latitude hemisphere <%c>", hemi);
    }
  } else {
    gtfo ("ReadLatLon : Malformed latitude string <%s>", s);
  }
  value->lat = lat;

  // Second line is longitude
  ReadString (s, 256, stream);
  double lon = 0;
  if (sscanf (s, "%d %d %f %c", &deg, &min, &sec, &hemi) == 4) {
    switch (hemi) {
    case 'E':
    case 'e':
      lon = (deg * 3600.0) + (min * 60.0) + sec;
      break;

    case 'W':
    case 'w':
      lon = (360.0 * 3600.0) - ((deg * 3600.0) + (min * 60.0) + sec);
      break;

    default:
      gtfo ("ReadPosition : Invalid longitude hemisphere <%c>", hemi);
    }
  } else {
    gtfo ("ReadPosition : Malformed longitude string <%s>", s);
  }
  value->lon = lon;

  // Set altitude to zero
  value->alt = 0;
}


//
// ReadPosition - Read a geographical position from the stream
//
// Format is:
//    33 56'24.6405"N        (N 33 deg 56 min 24.6405 sec latitude)
//    118 23'56.5173"W       (W 118 deg, 23 min, 56.5173 sec longitude)
//    94.248616728049114     (94 feet above MSL altitude)
//
void  ReadPosition (SPosition *value, SStream *stream)
{
  // Declare locals for parsing of position strings
  char s[256];
  int deg, min;
  float sec;
  char  hemi;

  // First line is latitude
  ReadString (s, 256, stream);
  double lat = 0;
  if (sscanf (s, "%d %d' %f\" %c", &deg, &min, &sec, &hemi) == 4) {
    switch (hemi) {
    case 'N':
    case 'n':
      lat = (deg * 3600.0) + (min * 60.0) + sec;
      break;

    case 'S':
    case 's':
      lat = -((deg * 3600.0) + (min * 60.0) + sec);
      break;

    default:
      gtfo ("ReadPosition : Invalid latitude hemisphere <%c>", hemi);
    }
  } else {
    gtfo ("ReadPosition : Malformed latitude string <%s>", s);
  }
  value->lat = lat;

  // Second line is longitude
  ReadString (s, 256, stream);
  double lon = 0;
  if (sscanf (s, "%d %d' %f\" %c", &deg, &min, &sec, &hemi) == 4) {
    switch (hemi) {
    case 'E':
    case 'e':
      lon = (deg * 3600.0) + (min * 60.0) + sec;
      break;

    case 'W':
    case 'w':
      lon = (360.0 * 3600.0) - ((deg * 3600.0) + (min * 60.0) + sec);
      break;

    default:
      gtfo ("ReadPosition : Invalid longitude hemisphere <%c>", hemi);
    }
  } else {
    gtfo ("ReadPosition : Malformed longitude string <%s>", s);
  }
  value->lon = lon;

  // Third line is altitude
  ReadDouble (&value->alt, stream);
}

//
// ReadTime - Read a date/time value from the stream
//
void  ReadTime (SDateTime *value, SStream *stream)
{
}

//
// ReadTimeDelta - Read a time delta value from the stream
//
void  ReadTimeDelta (SDateTimeDelta *value, SStream *stream)
{
}
//==================================================================================
//  Read and decode User Tag
//  decode only what is needed
//  TODO make a lookup table
//==================================================================================
void ReadUserTag(SMessage *msg,SStream *st)
// Various members of the user union
{ char s[64];
  ReadString (s, 64, st);
  char dtag_string[16]  = {0};
  char param[16] = {0};
  int  int_v            =  0;
  if      (sscanf (s, "DATATAG,'%s'", dtag_string)  == 1) msg->user.u.datatag = StringToTag (dtag_string);
  else if (sscanf (s, "GROUP,'%s'",   param) == 1) msg->group = StringToTag (param);
  else if (sscanf (s, "HARDWARE,%s",  param) == 1) msg->user.u.hw = GetHardwareType(param);
  else if (sscanf (s, "ENGINE,%d", &int_v) == 1) msg->user.u.engine = int_v;
  else if (sscanf (s, "UNIT,%d",   &int_v) == 1) msg->user.u.unit   = int_v;
  else if (sscanf (s, "%d", &int_v) == 1) msg->user.u.datatag = int_v; // used in FLYHAWK01.PLN <shet> trim wheel
  return;
}
//==================================================================================
// ReadMessage - Read a message object from a stream
//  NOTE:  Message without datatag must be set to st8t by default for those
//         gauges that dont specify the tag
//===================================================================================
void  ReadMessage (SMessage *msg, SStream *stream)
{
  // Advance to <bgno> tag
  char s[256];
  Tag tag;
  msg->user.u.datatag = 'st8t';         // JS default tag
  msg->id             = MSG_GETDATA;    // JS Get by default
  msg->dataType       = TYPE_REAL;      // JS Real by default
 
  // The following kluge is in place because of a typo in a <mesg> object
  //   in the default panel TRI30.PNL which misspells <bgno> as <bgmo>
  while (((tag = snexttag (s, 256, stream)) != 'bgno') && (tag != 'bgmo')) {}

  // Now continue to read tags until <endo>, populating various fields of the
  //   SMessage struct
  bool keepGoing = true;
  while (keepGoing) {
    tag = snexttag (s, 256, stream);
    switch (tag) {
    case 'grou':
    case 'conn':
      // Group (unique ID) tag
      {
        Tag grou;
        ReadTag (&grou, stream);
        msg->group = grou;
				TagToString(msg->dst,grou);
      }
      break;

    case 'user':
    case 'usr1':
      ReadUserTag(msg,stream);
      break;

    case 'dtag':
      // Datatag member of user data
      {
        Tag dtag;
        ReadTag (&dtag, stream);
        msg->user.u.datatag = dtag;
      }
      break;

    case 'type':
      // Data type and initial value
      {
        char s[64];
        ReadString (s, 64, stream);
        /// \todo Parse data type tag // 
        int   type_int     = 0;
        double type_float  = 0.0;

        if (sscanf (s, "INT,%d", &type_int) == 1) {
          msg->dataType = TYPE_INT;
          msg->intData  = type_int;
        } else if (sscanf (s, "INT,%d", &type_int) == -1) {       // no actual int value
          msg->dataType = TYPE_INT;
          msg->intData  = 0;                                      // default int value
        } else if (sscanf (s, "FLOAT,%lf", &type_float) == 1) {
          msg->dataType = TYPE_REAL;
          msg->realData = type_float;
        } else if (sscanf (s, "REAL,%lf", &type_float) ==  1) {  // real value
          msg->dataType = TYPE_REAL;
          msg->realData = type_float;                                    // default
        } else {
          ; // ...
        }

      }
      break;

    case 'endo':
      // End of message
      keepGoing = false;
      break;

    default:
      // Unrecognized tag
      char s[8];
      TagToString (s, tag);
      WARNINGLOG ("ReadMessage : Unrecognized tag %s", s);
    }
  }
}
//===================================================================
// Trace message
//===================================================================
void SMessage::Trace()
	{	char T[8];
		char S[8];
		char Y[8];
		TagToString(S,sender);
		TagToString(T,user.u.datatag);
		TagToString(Y,dataType);
		char R = (receiver)?(1):(0);
		TRACE("MESSAGE R=%d from %s To %s tag=%s type=%s INT=%d REAL=%.4f",R,S,dst,T,Y,intData,realData);
	}
//===================================================================
// ReadTag - Read a four-character datatag value from the stream
//===================================================================
void  ReadTag (Tag *tag, SStream *stream)
{
  char s[64];
  ReadString (s, 64, stream);
  //---eliminate any delimiters ------------------
	char *t = s;			// Tag start
  if (*s == 0x027)  t++;
	//---Skip introduction -------------------------
	if (*s == '<')	  t++;
  // Pad string with spaces to four chars in length
  while (strlen(t) < 4)  strcat (t, " ");
  *tag = StringToTag (t);
}
//--------------------------------------------------------------------
//	JSDEV* Read a signed Tag (with optional ~ in front
//	Return 0 if not inverted
//	return 1 if inverted
//--------------------------------------------------------------------
int  ReadInvertedTag(Tag *tag, SStream*stream)
{	int	 invert = 0;
	char s[64];
	ReadString(s,64,stream);
	s[63]	= 0;
	char *t = s;			// Tag start
	if (s[0] == '~')	{	invert = 1;		t++;	}		// remove '~' 
	while (strlen(t) < 4) strcat(t," ");				// Pad with space
 *tag	= StringToTag(t);
	return invert;	}
//==================================================================================
//	SStream constructor
//==================================================================================
void StreamWarn(char *pn, char *fn)
{	WARNINGLOG("Can't open %s/%s",pn,fn);
	return;
}
//==================================================================================
//	SStream constructor for read
//==================================================================================
SStream::SStream(CStreamObject *O,char *fn)
{	ok = false;
  if (fn && *fn && OpenRStream (fn,*this)) 
	{	ReadFrom (O,this);
	  CStreamFile* sf = (CStreamFile*)stream;
		sf->Close ();
		delete sf;
		stream = 0;
		ok = true;
	}
	else StreamWarn("",fn);
}
//==================================================================================
//	SStream constructor
//==================================================================================
SStream::SStream(CStreamObject *O,char *pn, char *fn)
{	ok	= false;
	if (*fn && OpenRStream (pn,fn,*this)) 
  {	ReadFrom (O,this);
		CStreamFile* sf = (CStreamFile*)stream;
		sf->Close ();
		delete sf;
		stream = 0;
		ok	= true;
	}
	else StreamWarn(pn,fn);
}

//==================== END OF FILE =================================================================