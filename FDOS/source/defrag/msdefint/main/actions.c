/*    
   actions.c - routines that call the modules through the module gate.

   Copyright (C) 2000 Imre Leber

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

#include <stdio.h>

#include "fte.h"

#include "actions.h"
#include "chkargs.h"

#include "..\screen\screen.h"
#include "..\dialog\sortbox.h"
#include "..\dialog\methods.h"
#include "..\dialog\msgbxs.h"
#include "..\dialog\seldrvbx.h"
#include "..\dialog\recoment.h"

#include "..\..\modlgate\defrpars.h"
#include "..\..\modlgate\modlgate.h"
#include "..\..\modlgate\custerr.h"
#include "..\..\misc\bool.h"

#include "..\logman\logman.h"

#include "..\screen\scrmask.h"

#include "..\ovlhost\ovlimpl.h"

#include "actaspct.h"

#include "..\mouse\mouse.h"

#include "reboot.h"

#include "..\..\engine\header\fteerr.h"

#define METHODBORDER 80

static int ProcessStopped;

char* OptimizationMethods[] = {"No defragmentation",
                               "Full optimization",
                               "Unfragment files only",
                               "Files first",
                               "Directories first",
                               "Directories with files",
                               "Crunch only",
                               "Quick Try",
                               "Complete quick try"};
                
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
                
#ifndef NDEBUG
   
#define LOGFILE "c:\\defrag.log"
                
static void WriteErrorLog(void)
{
    FILE* fptr;
    BOOL retVal;
    char* file;
    int line;
    
    fptr = fopen(LOGFILE, "wt");
    if (fptr)
    {
   retVal = GetFirstFTEError(&line, &file);
   while (retVal)
   {
       if (fprintf(fptr, "Error in %s at line %d\n", file, line) == EOF)
      break;       
       
       retVal = GetNextFTEError(&line, &file);
   }  
   
   fclose(fptr);
    }    
}
                
                
#endif                
                
                
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

int BeginOptimization(void)
{
     char* buttons[] = {"Ok"};

     CROSSCUT_BEGIN_OPTIMIZATION    


     /* First check the method on FAT32 */ 
     if (InspectFatLabelSize() == 32)
     {
        if ((GetOptimizationMethod() != QUICK_TRY) &&
            (GetOptimizationMethod() != COMPLETE_QUICK_TRY))
        {
           ErrorBox("Invalid method on FAT32", 1, buttons);     
           return 1;
        }
     }

     LockMouse(1, 1, 80, 25);
     
     StartCounting();
     
     ProcessStopped = FALSE;

     if (!SortDirectories())
     {
#ifndef NDEBUG
   WriteErrorLog(); 
#endif
   UnLockMouse();
   
   ErrorBox(GetErrorMessage(), 1, buttons);  
    
   if ((GetFTEerror() == 0) && (GetCustomError() == HANDLENOTINITIALISED))
      return 1;
   
   return 0;
     }

     if (!ProcessStopped && !OptimizeDisk())
     {
#ifndef NDEBUG
   WriteErrorLog(); 
#endif
    
   UnLockMouse();
   ErrorBox(GetErrorMessage(), 1, buttons);
    
   if ((GetFTEerror() == 0) && (GetCustomError() == HANDLENOTINITIALISED))
      return 1;    
   
   
   return 0;     
     }

     UnLockMouse();
     return 0;
}

void SelectSortOptions (void)
{
     struct SortDialogStruct options;

     if (GetSortOptions(&options))
        SetSortOptions(options.SortCriterium, options.SortOrder);

     CROSSCUT_SET_OPTIONS          
}                                   

void SelectOptimizationMethod(void)
{
     int method = 0;
     BOOL bForFAT32;
    
     if ((GetOptimizationDrive() != -1) &&
         (InspectFatLabelSize() == 32))
     {
        bForFAT32 = TRUE;
     }
     else
     {
        bForFAT32 = FALSE; /* Also when no disk selected yet, assume not FAT12 or FAT16 */
     }

     if (AskOptimizationMethod(&method, bForFAT32))
     {
        DrawMethod (OptimizationMethods[method]);
        SetOptimizationMethod(method);
     }

     CROSSCUT_SET_METHOD           
}

int SelectDrive(void)
{
     char drive;
     char* buttons[] = {"Ok"};

     if ((drive = ShowDriveSelectionBox()) != 0)
     {
        CROSSCUT_SET_DRIVE           

        if (!SetOptimizationDrive(drive))
        {
#ifndef NDEBUG
           WriteErrorLog(); 
#endif
       
           ErrorBox(GetErrorMessage(), 1, buttons);    
           return FALSE;
        }
        
        DrawCurrentDrive(drive);
        return QueryDisk();
     }

     return FALSE;
}

int QueryDisk(void) 
{
   int   factor, goon = FALSE;
   char* buttons[] = {"Ok"};

   CROSSCUT_QUERY_BEFORE
   LockMouse(1, 1, 80, 25);

   if (CheckDiskIntegrity())
   {
      factor = ScanDrive();
      UnLockMouse();
            
      if (factor == 255)
      {
         CROSSCUT_QUERY_ERROR
#ifndef NDEBUG
   WriteErrorLog(); 
#endif
     
         ErrorBox(GetErrorMessage(), 1, buttons);
      }
      else
      {
         CROSSCUT_QUERY_OK
         
         if (!IsMethodEntered())
         {
            if (InspectFatLabelSize() == 32)
            {
                if (factor < METHODBORDER)
                {
                    goon = RecommendMethod(factor, 
                                            GetOptimizationDrive(), 
                                            OptimizationMethods[QUICK_TRY]);
                    DrawMethod (OptimizationMethods[COMPLETE_QUICK_TRY]);
                    SetOptimizationMethod(COMPLETE_QUICK_TRY);                              
                }
                else if (factor < 100)
                {
                    goon = RecommendMethod(factor, 
                                            GetOptimizationDrive(), 
                                            OptimizationMethods[QUICK_TRY]);
                    SetOptimizationMethod(QUICK_TRY);
                    DrawMethod (OptimizationMethods[QUICK_TRY]);
                }
                else
                {
                    if (IsRebootRequested())
                    {
                        DOSWipeScreen();
                        ColdReboot();     
                    }
                    else if (MustAutomaticallyExit())
                        goon = TRUE;     
                    else        
                        InformUser("Disk not fragmented.");


                    SetOptimizationMethod(QUICK_TRY);
                }
            }
            else
            {
                if (factor < METHODBORDER)
                {
                    goon = RecommendMethod(factor, 
                                            GetOptimizationDrive(), 
                                            OptimizationMethods[FULL_OPTIMIZATION]);
                    DrawMethod (OptimizationMethods[FULL_OPTIMIZATION]);
                    SetOptimizationMethod(FULL_OPTIMIZATION);
                }
                else if (factor < 100)
                {
                    goon = RecommendMethod(factor, 
                                            GetOptimizationDrive(), 
                                            OptimizationMethods[UNFRAGMENT_FILES]);
                    SetOptimizationMethod(UNFRAGMENT_FILES);
                    DrawMethod (OptimizationMethods[UNFRAGMENT_FILES]);
                }
                else
                {
                    if (IsRebootRequested())
                    {
                        DOSWipeScreen();
                        ColdReboot();     
                    }
                    else if (MustAutomaticallyExit())
                        goon = TRUE;     
                    else        
                        InformUser("Disk not fragmented.");
                }
            }
         }
         else
         {
            goon = TRUE;
            DrawMethod (OptimizationMethods[GetOptimizationMethod()]);
         }
      }
   }
   else
   {
      CROSSCUT_QUERY_ERROR

      UnLockMouse();
#ifndef NDEBUG
   WriteErrorLog(); 
#endif
         
      ErrorBox(GetErrorMessage(), 1, buttons);
      SetOptimizationDrive('0');
      DrawCurrentDrive('?');
      DrawBlockSize(0);
   }

   return goon;
}

void UpdateDriveMap(void)
{
   LockMouse(1, 1, 80, 25);

   ScanDrive();

   UnLockMouse();
}

void StopDefragmentationProcess(void)
{
   ProcessStopped = TRUE;
}
