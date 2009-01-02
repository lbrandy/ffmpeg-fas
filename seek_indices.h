/*****************************************************************************
 * Copyright 2008. Pittsburgh Pattern Recognition, Inc.
 * 
 * This file is part of the Frame Accurate Seeking extension library to 
 * ffmpeg (ffmpeg-fas).
 * 
 * ffmpeg-fas is free software; you can redistribute it and/or modify it 
 * under the terms of the GNU Lesser General Public License as published by 
 * the Free Software Foundation; either version 3 of the License, or (at your 
 * option) any later version.
 *
 * The ffmpeg-fas library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with the ffmpeg-fas library.  If not, see <http://www.gnu.org/licenses/>.
 *
 ******************************************************************************/

#ifndef FAS_SEEK_INDICES_H 
#define FAS_SEEK_INDICES_H

#include <stdint.h>
#include <stdio.h>

/* If C++ then we need to __extern "C". Compiler defines __cplusplus */
#ifdef __cplusplus
#define __extern extern "C"
#else
#define __extern extern
#endif


/**********************************************************************
 * Seek Table Types
 **********************************************************************/

typedef enum
{
  seek_no_error,
  seek_unknown_error,
  seek_bad_argument,
  seek_malloc_failed,
} seek_error_type;

typedef enum
{
  seek_false = 0,
  seek_true  = 1
} seek_boolean_type;

typedef struct
{
  int     display_index;
  int64_t first_packet_dts;
  int64_t last_packet_dts;
} seek_entry_type;

typedef struct
{
  seek_entry_type *array;
  seek_boolean_type completed;
  int num_frames;               // total number of frames
  int num_entries;              // ie, number of seek-points (keyframes)
  int allocated_size;
} seek_table_type;



/**********************************************************************
 * Seek Table Functions
 **********************************************************************/


__extern seek_table_type seek_init_table    (int initial_size);
__extern void            seek_release_table (seek_table_type *table);

__extern seek_table_type seek_copy_table (seek_table_type source);
__extern int             compare_seek_tables(seek_table_type t1, seek_table_type t2);

__extern seek_error_type seek_append_table_entry (seek_table_type *table, seek_entry_type entry);

__extern seek_error_type seek_get_nearest_entry (seek_table_type *table, seek_entry_type *entry, int display_index, int offset);

__extern seek_error_type seek_show_table (seek_table_type table);          /* human readable */
__extern seek_error_type seek_show_raw_table (FILE *file, seek_table_type table);

__extern seek_table_type read_table_file(char *name);                      /* read raw file */

#endif 

/**** End of File *****************************************************/
