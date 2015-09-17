/*    
   Unfrag.c - unfragment files only.

   Copyright (C) 2003 Imre Leber

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

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "fte.h"
#include "DfrgDrvr.h"
#include "FllDfrag\flldfrg.h"
#include "..\dtstruct\ClMovMap.h"
#include "..\..\modlgate\expected.h"
#include "..\..\modlgate\custerr.h"
#include "defrmap.h"

/* Public functions */
static BOOL SelectFile(RDWRHandle handle, CLUSTER startfrom,
                       CLUSTER* clustertoplace);                         

/* Private functions */                  
static BOOL ConservativePlace(RDWRHandle handle, CLUSTER clustertoplace,
                              CLUSTER clustertobereplaced, CLUSTER* stop);                          
                           
static BOOL MarkNotFragmentedFiles(RDWRHandle handle);

struct Pipe
{
   CLUSTER cluster;
   CLUSTER clustertoplace;

   BOOL found;
};

BOOL UnfragmentFiles(RDWRHandle handle)
{
    BOOL result;

    if (CreateFastSelectMap(handle) != TRUE)
       RETURN_FTEERR(FALSE);

    if (CreateNotFragmentedMap(handle) != TRUE)
    {
       DestroyFastSelectMap();
       RETURN_FTEERR(FALSE);
    }

    if (!MarkNotFragmentedFiles(handle))
	RETURN_FTEERR(FALSE);

    result = DefragmentVolume(handle, SelectFile, ConservativePlace);

    DestroyFastSelectMap();
    DestroyNotFragmentedMap();

    return result;
}

static BOOL MarkNotFragmentedFiles(RDWRHandle handle)
{
    CLUSTER i, start = 0;
    unsigned long LabelsInFat;
    VirtualDirectoryEntry* NotFragmentedMap;
    BOOL NotFrag;
    
    NotFragmentedMap = GetNotFragmentedMap();
    
    assert(NotFragmentedMap);
    
    LabelsInFat = GetLabelsInFat(handle);
    if (!LabelsInFat)
    {
        SetCustomError(WRONG_LABELSINFAT);
	RETURN_FTEERR(FALSE);
    }

    for (i = 0; i < LabelsInFat; i++)
    {
	if (!GetVFSBitfieldBit(NotFragmentedMap, i, &NotFrag))
        {
            SetCustomError(VFS_GET_FAILED);
	    RETURN_FTEERR(FALSE);	
        }
	
	if (!start && NotFrag)
        {
           start = i;
        }
	else if (start && !NotFrag)
	{
	   DrawMoreOnDriveMap(start, OPTIMIZEDSYMBOL, i-start);
	   start = 0;
	}	
    }
    
    if (start)
    {
	DrawMoreOnDriveMap(start, OPTIMIZEDSYMBOL, LabelsInFat - start);	
    }    
    
    return TRUE;
}

static BOOL SelectFile(RDWRHandle handle, CLUSTER startfrom,
                       CLUSTER* clustertoplace)
{
    CLUSTER i;
    unsigned long LabelsInFat;
    VirtualDirectoryEntry* FastSelectMap;
        
    FastSelectMap = GetFastSelectMap();    
        
    LabelsInFat = GetLabelsInFat(handle);
    if (!LabelsInFat)
    {
        SetCustomError(WRONG_LABELSINFAT);
	RETURN_FTEERR(FAIL);
    }

    for (i = startfrom; i < LabelsInFat; i++)
    {
	BOOL value;

	if (!GetVFSBitfieldBit(FastSelectMap, i, &value))
        {
            SetCustomError(VFS_GET_FAILED);
	    RETURN_FTEERR(FALSE);
        }

        if (value)
        {
           *clustertoplace = i;
           return TRUE;
        }
    }
    
    RETURN_FTEERR(FALSE);
}

static CLUSTER GetNextContinousBlock(RDWRHandle handle,
                                     CLUSTER clustertoplace)
{
    CLUSTER i;
    unsigned long LabelsInFat;
    VirtualDirectoryEntry* NotFragmentedMap;
    
    NotFragmentedMap = GetNotFragmentedMap();
        
    LabelsInFat = GetLabelsInFat(handle);
    if (!LabelsInFat)
    {
        SetCustomError(WRONG_LABELSINFAT);
        return 0;
    }
    
    for (i = clustertoplace; i < LabelsInFat; i++)
    {
	BOOL value;

	if (!GetVFSBitfieldBit(NotFragmentedMap, i, &value))
        {
            SetCustomError(VFS_GET_FAILED);
	    return 0;
        }

        if (value)
        {
           return i;
        }
    }
    
    return 0;  /* No next continous block found */  
}

static unsigned long CalculateWorkingSpace(RDWRHandle handle, 
                                           CLUSTER clustertoplace)
{
    BOOL isMovable;
    CLUSTER i, label;
    unsigned long LabelsInFat, result = 0;
    VirtualDirectoryEntry* NotFragmentedMap;
    
    NotFragmentedMap = GetNotFragmentedMap();
        
    LabelsInFat = GetLabelsInFat(handle);
    if (!LabelsInFat) return 0;
    
    for (i = clustertoplace; i < LabelsInFat; i++)
    {
	BOOL value;

	if (!GetVFSBitfieldBit(NotFragmentedMap, i, &value))
        {
            SetCustomError(VFS_GET_FAILED);
	    return 0;
        }

        if (value)
        {
           return result;
        }

        /* Make sure not to count the bad and non movable clusters. */
        if (!GetNthCluster(handle, i, &label))
        {
            SetCustomError(CLUSTER_RETRIEVE_ERROR);
            return 0;
        }
           
        if (!FAT_BAD(label))
        {
           if (!IsClusterMovable(handle, i, &isMovable))
              return 0;
           if (isMovable)
              result++;
        }
    }
    
    return result;  /* No next continous block found */  
}

static CLUSTER SkipNonMovables(RDWRHandle handle, CLUSTER start)
{
    CLUSTER i;
    unsigned long LabelsInFat;
    VirtualDirectoryEntry* NotFragmentedMap;
    
    NotFragmentedMap = GetNotFragmentedMap();
        
    LabelsInFat = GetLabelsInFat(handle);
    if (!LabelsInFat)
    {
        SetCustomError(WRONG_LABELSINFAT);   
        return 0;
    }
    
    for (i = start; i < LabelsInFat; i++)
    {
	BOOL value;

        if (!GetVFSBitfieldBit(NotFragmentedMap, i, &value))
        {
            SetCustomError(VFS_GET_FAILED);
	    return 0;
        }
	
	if (!value)
	{
           return i;
        }
    }
    
    return 0;  /* No next continous block found */  
}

static BOOL ConservativePlace(RDWRHandle handle, CLUSTER clustertoplace,
                              CLUSTER clustertobereplaced, CLUSTER* stop)
{
    CLUSTER temp;
    unsigned long filelen, workingspace;
    CLUSTER FreeSpace, ContinousBlockStart;

    temp = SkipNonMovables(handle, clustertobereplaced);  
    
    if (temp != clustertobereplaced)
    {
       *stop = temp;
       return (temp == 0) ? FAIL : TRUE;
    }
    
    /* Calculate the size of the file to place, and the space available
       to the next non fragmented block. */
    filelen = CalculateFileSize(handle, clustertoplace);  
    if (filelen == FAIL)
    {
        SetCustomError(GET_FILESIZE_FAILED);
        RETURN_FTEERR(FAIL);
    }
            
    workingspace = CalculateWorkingSpace(handle, clustertobereplaced);
    if (!workingspace) 
        RETURN_FTEERR(FAIL);   

    /* If there is enough space to place the file completely, then move
       the complete file to this location. */
    if (filelen <= workingspace)
    {
       return FullDefragPlace(handle,
                              clustertoplace, clustertobereplaced, 
                              stop);
    }
    /* If there is not enough space, try putting the file in a free
       space on the disk that is large enough to contain the file. */
    else
    {
       switch (FindFirstFittingFreeSpace(handle, filelen, &FreeSpace))
       {
         case TRUE:
              if (FullDefragPlace(handle, 
                                   clustertoplace, FreeSpace, 
                                   stop) == FAIL)
              {
                RETURN_FTEERR(FAIL); 
              }
              
              MarkFileAsContinous(FreeSpace, filelen);
              *stop = clustertobereplaced;
              break;
                 
         case FAIL:
              SetCustomError(GET_FREESPACE_ERROR);
              RETURN_FTEERR(FAIL);
              
         case FALSE:
              /* Otherwise move the next continous block to the front,
                 and start again. */   
              ContinousBlockStart = GetNextContinousBlock(handle, 
                                                          clustertobereplaced); 
                                                          
              if (!ContinousBlockStart) 
                  RETURN_FTEERR(FAIL);
              
              /* Notice that the first cluster of that block is the start
                 of some file (since the file is continous). */
              if (ContinousBlockStart != clustertobereplaced)
              {
                 return FullDefragPlace(handle,
                                        ContinousBlockStart,
                                        clustertobereplaced,
                                        stop);
              }
              else
              {
                 *stop = SkipNonMovables(handle, clustertobereplaced);
                 
                 if ((*stop) == 0)
                    RETURN_FTEERR(FAIL);
              }
	      break;
	      
	 default:
	      assert(FALSE);  
       }
    }
    
    return TRUE;
}
         

 
