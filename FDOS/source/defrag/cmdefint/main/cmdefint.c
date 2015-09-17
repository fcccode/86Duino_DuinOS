/*    
   Cmdefint.c - main module for command line interface.
   Copyright (C) 2000, 2002 Imre Leber

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   If you have any questions, comments, suggestions, or fixes please
   email me at:  imre.leber@worldonline.be
*/

#include <stdlib.h>
#include <stdio.h>
#include <dos.h>
#include <string.h>

#include "..\..\misc\bool.h"
#include "..\..\engine\header\fte.h"
#include "..\..\modlgate\modlgate.h"
#include "..\..\modlgate\custerr.h"
#include "..\..\modlgate\argvars.h"
#include "..\..\modlgate\defrpars.h"
#include "..\..\modlgate\fatsize.h"
#include "..\..\engine\header\FTE.h"
#include "..\..\modlgate\callback.h"
#include "..\..\misc\version.h"
#include "..\..\misc\misc.h"
#include "..\..\misc\reboot.h"

#include "..\ovlhost\ovlimpl.h"

#include "..\..\misc\drvtypes.h"

#include "chkargs.h"
#include "..\..\environ\checkos.h"

/* Error messages */              
static char* ErrorMessage[] = {"Undefined problem",
                               "Disk read error",
                               "Disk write error",
                               "Memory exhausted",
                               "Write on read only",
                               "Invalid bytes per sector",
                               "Not reserved",
                               "Insufficient handles",
                               "Disk corrupted"};
                
static char* CustomMessage[] = {"Undefined problem",
                                "Volume not initialised (choose a disk!)",
            "FAT32 not currently supported",
                                "Disk checking failed",
                                "Could not determine count of labels in FAT",   
                                "Wrong label in FAT",
                                "Could not read label from FAT",
                                "Could not draw drive map",
                                "Could not fill drive map",
                                "Could not get drive size",
                                "VFS allocation failed",
                                "VFS get failed",
                                "VFS set failed",
                                "Get fixed file failed",
                                "Get FAT type failed",
                                "Get FAT32 root cluster failed",
                                "Get FAT32 root directory failed",
                                "Could not determine unmovable clusters",
                                "Could not get directory count",
            "Could not read directory",
            "Could not write directory",
                                "Could not determine first cluster of directory",
                                "Could not update first cluster table",
                                "Could not get continous file data",
                                "Insufficient free disk space",
                                "Could not get free space",
                                "VFS relocation error",
                                "Relocation error (overlapping)",
                                "Could not get file size",
                                "Could not get free space",
                                "Could not get file cluster",
                                "Relocation error (sequence)",
                                "Could not determine if file fragmented"};                
                
#define NUM_MESSAGES sizeof(ErrorMessage) / sizeof(*ErrorMessage) 
//#define NUM_CUSTOMS  sizeof(CustomMessage) / sizeof(*CustomMessage)   
            
static void cdecl OnExit(void);
static int  cdecl OnCBreak(void);
static int  CheckOS(void);
static char* GetErrorMessage(void);

#define METHODBORDER 80

static struct CallBackStruct CallBacks;

int CMDefint(int argc, char *argv[])
{
    char switchchar = SwitchChar();
    int  factor;
    int  doOptimize = 1;

    /* Check parameters. */
    ParseCmdLineArguments(argc, argv, switchchar);

    /* Initialise and start the sector cache */
    InitSectorCache();
    StartSectorCache();

    /* Check file system integrity. */
    if (!CheckOS()) return 1;

    /* Show copyright on the screen. 
    printf("This program is free software. It comes with ABSOLUTELY NO WARANTIES.\n"
        "You are welcome to redistribute it under the terms of the\n" 
        "GNU General Public License, see http://www.GNU.org for details.\n\n");
    */
    atexit(OnExit);
    ctrlbrk(OnCBreak);

    CMDEFINT_GetCallbacks(&CallBacks);
    SetCallBacks(&CallBacks);

    if (!SetOptimizationDrive(GetParsedDrive()))
    {
        printf(GetErrorMessage()); 
        CloseSectorCache();     
        return 1;
    }

    if (!CheckDiskIntegrity())
    {
        printf(GetErrorMessage());
        CloseSectorCache();

        return 1;
    }
    else
    {
        if ((factor = ScanDrive()) == 255)
        {
            printf(GetErrorMessage());
            CloseSectorCache();
            return 1;
        }
        else
        {
            printf("%d%% of drive %c: is not fragmented.\n",
                factor,
                GetOptimizationDrive());

            if (!IsMethodEntered())
            {
                if (factor == 100)
                {
                    printf("No optimization necessary.\n");
                    doOptimize = 0;
                }
                else
                {
                    if (InspectFatLabelSize() == 32)
                    {                   
                        if (factor > METHODBORDER)
                        {
                            SetOptimizationMethod(QUICK_TRY);
                        }
                        else /* if (factor < METHODBORDER) */
                        {
                            SetOptimizationMethod(COMPLETE_QUICK_TRY);
                        }
                    }
                    else
                    {
                        if (factor > METHODBORDER)
                        {
                            SetOptimizationMethod(UNFRAGMENT_FILES);
                        }
                        else /* if (factor < METHODBORDER) */
                        {
                            SetOptimizationMethod(FULL_OPTIMIZATION);
                        }
                    }
                }
            }
        
            if (doOptimize)
            {
                switch (GetOptimizationMethod())
                {
                case FULL_OPTIMIZATION:
                    printf("Optimization method is full optimization.\n");
                    break;
                case UNFRAGMENT_FILES:
                    printf("Optimization method is unfragment files only.\n");
                    break;
                case FILES_FIRST:  
                    printf("Optimization method is files first.\n");
                    break;
                case DIRECTORIES_FIRST:
                    printf("Optimization method is directories first.\n");
                    break;
                case DIRECTORIES_FILES: 
                    printf("Optimization method is directories together with files.\n");
                    break;
                case CRUNCH_ONLY: 
                    printf("Optimization method is crunch only.\n");
                    break;
                case QUICK_TRY:     
                    printf("Optimization method is quick try.\n");
                    break;
                case COMPLETE_QUICK_TRY:
                    printf("Optimization method is complete quick try.\n");
                    break;
                }
            }

            if (!SortDirectories())
            {
                printf(GetErrorMessage());
                CloseSectorCache();
                return 1;
            }

            if (doOptimize)
            {
                if (!OptimizeDisk())
                {
                    printf(GetErrorMessage());
                    CloseSectorCache();
                    return 1;     
                }
            }

            if (IsRebootRequested())
            {
                printf("Rebooting the computer...");
                CloseSectorCache();
                ColdReboot();
            }
        }
    }

    CloseSectorCache();

    return 0;
}

static void cdecl OnExit(void)
{
}

static int cdecl OnCBreak(void)
{
    return 1;
}

static int CheckOS(void)
{
    char* msg;

    msg = CheckDefragEnvironment();
    if (msg)
    {
        printf("%s\n", msg);
        return FALSE;
    }

    return TRUE;
}

static char* GetErrorMessage(void)
{
    int error = GetFTEerror();         /* Check engine messages */    
    if (error > 0 && error < NUM_MESSAGES)
        return ErrorMessage[error];

    error = GetCustomError();       /* Check module messages */
    if (error >= 0 && error < NUM_CUSTOMS)
        return CustomMessage[error];     

    return "Internal problem";
}
