
// .h : include file
//

#if !defined(__MY_MAIN_HEADER__)
#define __MY_MAIN_HEADER__

#include <stdio.h> // FILE
#include <string.h> // strcpy
#include <time.h> // time


// add lib from main application (exe)
#pragma message ("loading FlyLegacy.lib")
#pragma comment (lib,    "./../../sdk/FlyLegacy.lib")

#define	FOR_EXPORT
#include "./../../sdk/mydll.h"

/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/
//  import API functions from main application (exe)
//  sdkintf.h

    APIFunctionRegistry	gRegistry;

    extern "C" void __declspec (dllimport)  GetSDKAPI (APIFunctionRegistry &api);

/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/

/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/
//  Definitions

    #define DLL_NAME "TEST_SUBS1"
    const long DLL_TYPE_ID = 'TST1';

    void DLLInstantiate     (const long type,
                             const long id,
                             SDLLObject **object);
    void DLLDestroyObject   (SDLLObject *object);
    void DLLTimeSlice       (SDLLObject *refObject,
                             const float deltaTime);
    int  DLLRead            (SDLLObject *object,
                             SStream *stream,
                             unsigned int tag);
/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/


/////////////////////////////////////////////////////////////
//
//	DLL Setup (Init & Kill)
//
//	These functions MUST be implemented, or the DLL
//	will totally fail!!!!
//
/////////////////////////////////////////////////////////////

DLL_INTERFACE int	DLLInit(DLLFunctionRegistry *dll, SDLLCopyright *copyright, SDLLRegisterTypeList **types)
{
	//
	//	populate copyright info
	//
  time_t dateTime;
  time (&dateTime);
  char buff [256];
  sprintf (buff, "1.00, %s", ctime (&dateTime));
  buff[strlen (buff) - 1] = '\0';

	strcpy(copyright->product, DLL_NAME);
	strcpy(copyright->company, "ROTW (c)");
	strcpy(copyright->programmer, "laurentC");
	strcpy(copyright->dateTimeVersion, buff);
	strcpy(copyright->email, "no@email.com");
	copyright->internalVersion = 100;

	//
	//	return function pointers of available DLL functions
	//
  dll->DLLInstantiate     = DLLInstantiate;
  dll->DLLDestroyObject   = DLLDestroyObject;
  dll->DLLTimeSlice       = DLLTimeSlice;
	dll->DLLRead            = DLLRead;
	//
	//	get dsk api from application
	//
  GetSDKAPI (gRegistry);

  //
  // initialize variables
  //


  //
  // define list of object types we can create
  //
  gRegistry.APIAddRegisteredType(types, TYPE_DLL_SYSTEM, DLL_TYPE_ID);

  //
  #ifdef _DEBUG	
  {	FILE *fp_debug;
	  if(!(fp_debug = fopen("__DDEBUG_dll.txt", "a")) == NULL)
	  {
		  fprintf(fp_debug, "==DLLInit %p %p %p %p %s\n", 
        types, types [0], types [1], types [2], DLL_NAME);
		  fclose(fp_debug); 
  }	}
  #endif

  //
  // result code
  //
	return(1);
}

void	DLLKill(void)
{
	//
	//	nothing mandatory to do here
	//	except internal cleanup
	//
  #ifdef _DEBUG	
  {	FILE *fp_debug;
	  if(!(fp_debug = fopen("__DDEBUG_dll.txt", "a")) == NULL)
	  {
		  fprintf(fp_debug, "==DLLKill %s\n", DLL_NAME);
		  fclose(fp_debug); 
  }	}
  #endif
}




#endif // __MY_MAIN_HEADER__