/***************************************************************************
 * This program is free software; you can redistribute it and/or modify it *
 * under the terms of the GNU General Public License version 2 as          *
 * published by the Free Software Foundation.                              *
 *                                                                         *
 * This program is distributed in the hope that it will be useful, but     *
 * WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU        *
 * General Public License for more details.                                *
 *                                                                         *
 * You should have received a copy of the GNU General Public License along *
 * with this program; if not, write to the Free Software Foundation, Inc., *
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.                *
 ***************************************************************************/

#define __INCLUDE_FILES_H 1

#include "misc.h"

#ifdef __PACIFIC__

#undef getcwd

char *_getdcwd(int drive, char *dest, int len)
{
	sprintf(dest, "%c:", drive + 'A');
	strcat(dest, getcwd((char *)drive + 'A'));
	return dest;
}

char *getcurrentworkingdirectory(char *dest, int len)
{
	sprintf(dest, "%c:", _getdrive()-1+'A');
	strcat(dest, getcwd(NULL));
	return dest;
}

#endif
