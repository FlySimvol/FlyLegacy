/*
 * Ui.cpp
 *
 * Part of Fly! Legacy project
 *
 * Copyright 2003 Chris Wallace
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
 *
 * This module implements the original prototype PUI user interface for Fly! Legacy.
 *   Over time, the functionality provided by PUI is being re-implemented using
 *   FUI.  This code will eventually be obsolete and removed.
 */

#include "../Include/Ui.h"
//=================================================================================



//---------------------------------------------------------------------------
// CSDKMenu
//   used to add user menu item at the very first init.
//
//  typedef struct SMenuData {
//
//    unsigned long menuID;
//    std::string str;
//    std::vector <unsigned long> itemID;
//    std::vector <std::string> items;
//    std::vector <puCallback *> _cb;
//
//  } SMenuData;
//---------------------------------------------------------------------------
sdkmenu::CSDKMenu::CSDKMenu (void)
{
  md_.clear ();
}

void sdkmenu::CSDKMenu::AddMenu (unsigned long _menuID, const char *label)
{
    #ifdef _DEBUG_ui	
    {   FILE *fp_debug;
	    if(!(fp_debug = fopen("__DDEBUG_ui.txt", "a")) == NULL)
	    {
		    int test = 0;
		    fprintf(fp_debug, "CSDKMenu::AddMenu\n");
		    fclose(fp_debug); 
	    }
    }
    #endif

  std::vector<SMenuData>::iterator i_md;
  for (i_md = md_.begin (); i_md != md_.end (); ++i_md) {

    if (i_md->menuID == _menuID) {
      return;
    }
  }

  SMenuData tmp;
  tmp.menuID = _menuID;
  tmp.str = label;

  if (md_.size () < sdkmenu::MAX_MAINMENU_ADDIN) // max 5 addin menu
    md_.push_back (tmp);

}

void sdkmenu::CSDKMenu::AddMenuItem  (unsigned long _menuID, unsigned long _itemID, const char *_label)
{
  std::vector<SMenuData>::iterator i_md;

  for (i_md = md_.begin (); i_md != md_.end (); ++i_md) {

    if (i_md->menuID == _menuID) {
		#ifdef _DEBUG_ui	
		{   
			FILE *fp_debug;
			if(!(fp_debug = fopen("__DDEBUG_ui.txt", "a")) == NULL)
			{
			  int test = 0;
			  fprintf(fp_debug, "CSDKMenu::AddMenuItem #%d\n", i_md->itemID.size ());
			  fclose(fp_debug); 
			}
		}
		#endif

		// 10 is the MAX_SDK_SUBMENU_NUM in menu.cpp init_user_menu()
		if (i_md->itemID.size () < sdkmenu::MAX_SDK_SUBMENU_NUM) {
			i_md->itemID.push_back (_itemID);
			i_md->items.push_back  (std::string (_label));
		}
	}
  }
}
