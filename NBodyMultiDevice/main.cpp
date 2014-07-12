/* ************************************************************************* *\
                  INTEL CORPORATION PROPRIETARY INFORMATION
     This software is supplied under the terms of a license agreement or
     nondisclosure agreement with Intel Corporation and may not be copied
     or disclosed except in accordance with the terms of that agreement.
          Copyright (C) 2012 Intel Corporation. All Rights Reserved.
\* ************************************************************************* */

/// @file main.cpp
///       Defines the main entry point for the windows application.


#include "stdafx.h"
#include "fast_particles.h" ///< Definition of the application class
#include <tchar.h>

/// Entry point for the application. The main propose is to create
/// single application object and pass control to it.
int wmain(int argc, WCHAR *argv[])
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) || defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    CSample_NBody app; // create application object on the stack

    int numArgs;
    LPWSTR* pArgs= CommandLineToArgvW(GetCommandLine(), &numArgs);
    return app.Run();  // call the main application function and return exit code
}


// end of file
