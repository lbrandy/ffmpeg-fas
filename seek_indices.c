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

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "seek_indices.h"
#include "private_errors.h"

/**** Defines *****************************************************************/

#define    DEFAULT_INITIAL_SIZE    100

static seek_error_type private_show_error (const char *message, seek_error_type error);
static seek_error_type private_resize_table (seek_table_type *table, int new_size);


/*
 * seek_init_table
 */

int compare_seek_tables(seek_table_type t1, seek_table_type t2)
{
  int i;

  //  printf("n_entries=(%d %d)\n", t1.num_entries, t2.num_entries);

  if (t1.num_entries != t2.num_entries)    
    return 0;

  if (t1.completed != t2.completed)
    return 0;
  
  if (t1.num_frames != t2.num_frames)
    return 0;
  
  for (i=0;i<t1.num_entries;i++)
    {  
      //                 printf("(%d %d) (%lld %lld) (%lld %lld)\n", 
      //      	     t1.array[i].display_index, t2.array[i].display_index,
      //      	     t1.array[i].decode_time, t2.array[i].decode_time,
      //      	     t1.array[i].display_time, t2.array[i].display_time);
      if ((t1.array[i].display_index != t2.array[i].display_index)  ||
	  (t1.array[i].last_packet_dts   != t2.array[i].last_packet_dts)    ||
	  (t1.array[i].first_packet_dts  != t2.array[i].first_packet_dts))
	return 0;
    }

  return 1;       
}

seek_table_type seek_init_table (int initial_size)
{
  seek_table_type table;

  if (initial_size < 0)
    initial_size = DEFAULT_INITIAL_SIZE;

  table.num_entries    = 0;
  table.num_frames     = -1;
  table.completed      = seek_false;

  table.array = (seek_entry_type *)malloc (initial_size * sizeof(seek_entry_type));
  
  if (NULL == table.array)
    table.allocated_size = 0;
  else
    table.allocated_size = initial_size;
  
  return table;
}

/*
 * seek_release_table
 */

void seek_release_table (seek_table_type *table)
{
  table->num_entries = 0;
  table->num_frames = -1;
  table->completed = seek_false;

  if (NULL == table || NULL == table->array) 
    return; 

  free (table->array);
  return;
}

/*
 * seek_copy_table
 */

seek_table_type seek_copy_table (seek_table_type source)
{
  seek_table_type dest;
  dest.num_entries = source.num_entries;
  dest.num_frames  = source.num_frames;
  dest.completed   = source.completed;

  if (NULL == source.array) 
    {
      dest.array = NULL;
      dest.allocated_size = 0;
      return dest;
    }

  dest.array = (seek_entry_type *)malloc (source.num_entries * sizeof(seek_entry_type));
  
  if (NULL == dest.array)
    {
      dest.array = NULL;
      dest.allocated_size = 0;
      return dest;
    }
  
  dest.allocated_size = source.num_entries;
  
  int i;
  for (i=0;i<source.num_entries;i++)
    dest.array[i] = source.array[i];
  
  return dest;
}

seek_error_type seek_append_table_entry (seek_table_type *table, seek_entry_type entry)
{

  if (NULL == table || NULL == table->array) 
    return private_show_error("null or invalid seek table", seek_bad_argument);

  if (table->num_entries != 0)
    if (table->array[table->num_entries - 1].display_index >= entry.display_index)
	return seek_no_error;

  if (table->num_entries == table->allocated_size)
    {
      seek_error_type error = private_resize_table (table, table->num_entries * 2);
      if (error != seek_no_error)
	return private_show_error ("unable to resize seek table", error);
    }

  table->array[table->num_entries] = entry;
  table->num_entries++;

  return seek_no_error;
}

/*
 * seek_get_nearest_entry
 */

seek_error_type seek_get_nearest_entry (seek_table_type *table, seek_entry_type *entry, int display_index, int offset)
{
  /* using offset>0 returns a modified seek_entry that sets the 'time-to-seek' to be $offset keyframes in the past. 
   */

  if (NULL == table || NULL == table->array || table->num_entries <= 0) {
    return private_show_error ("NULL or invalid seek table", seek_bad_argument);
  }

  if (NULL == entry) {
    return private_show_error ("NULL entry buffer (for return)", seek_bad_argument);
  }

  if (display_index < table->array[0].display_index)
    return private_show_error ("tried to seek to frame index before first frame", seek_bad_argument);

  int i;
  for (i=0; i < table->num_entries; i++)
    if (table->array[i].display_index > display_index)
      break;

  i = i-1;

  if (i<offset)   /* target was lower than first element (including offset) */
    return private_show_error ("target index out of table range (too small)", seek_bad_argument);
  
  *entry = table->array[i];
  (*entry).first_packet_dts = table->array[i-offset].first_packet_dts;

  return seek_no_error;
}


/* read raw file */
seek_table_type read_table_file(char *name)
{
  seek_table_type ans = { NULL, (seek_boolean_type) 0, (seek_boolean_type) 0 };
  
  FILE *table_file = fopen(name, "r");
  if (table_file == NULL)    
    return ans;
  
  int completed_flag;
  fscanf(table_file, "%d %d %d\n", &ans.num_frames, &ans.num_entries, &completed_flag);

  if (completed_flag == 1)
    ans.completed = seek_true;
  else
    ans.completed = seek_false;
  
  ans.allocated_size = ans.num_entries;
  ans.array = (seek_entry_type*) malloc (ans.allocated_size * sizeof(seek_entry_type));

  int i;
  for (i=0;i<ans.num_entries;i++)
    fscanf(table_file, "%d %lld %lld\n", &(ans.array[i].display_index), &(ans.array[i].first_packet_dts), &(ans.array[i].last_packet_dts));

  fclose(table_file);
  return ans;
}

seek_error_type seek_show_raw_table (FILE* file, seek_table_type table)
{
  seek_entry_type *entry;
  int index;

  if (NULL == table.array || table.num_entries <= 0)
    return private_show_error ("NULL or invalid seek table", seek_bad_argument);
  
  int completed_flag = 0;
  if (table.completed == seek_true)
    completed_flag = 1;
  
  fprintf(file, "%d %d %d\n", table.num_frames, table.num_entries, completed_flag);
  for (index = 0; index < table.num_entries; index++)
  {
    entry = &(table.array[index]);

    fprintf (file, "%d %lld %lld\n", entry->display_index, entry->first_packet_dts, entry->last_packet_dts);
  }
  return seek_no_error;
}

seek_error_type seek_show_table (seek_table_type table)
{
  seek_entry_type *entry;
  int index;

  if (NULL == table.array || table.num_entries <= 0) {
    return private_show_error ("NULL or invalid seek table", seek_bad_argument);
  }
  
  int completed_flag = 0;
  if (table.completed == seek_true)
    completed_flag = 1;

  fprintf (stderr, "--- Seek Table Dump ---\n");
  fprintf (stderr, "n_frames: %d   n_entries: %d   completed: %d\n",table.num_frames, table.num_entries, completed_flag);
  for (index = 0; index < table.num_entries; index++)
  {
    entry = &(table.array[index]);

    fprintf (stderr, "  %04d --> %08lld (%08lld)\n", entry->display_index, entry->first_packet_dts, entry->last_packet_dts);
  }

  fprintf (stderr, "-----------------------\n");

  return seek_no_error;
}

/*
 * private_show_error
 */

static seek_error_type private_show_error (const char *message, seek_error_type error)
{
  if (SHOW_ERROR_MESSAGES)
    fprintf (stderr, " ===> seek_indices: %s\n", message);

  return error;
}

/*
 * private_resize_table
 */

static seek_error_type private_resize_table (seek_table_type *table, int new_size)
{
  seek_entry_type *new_array = NULL;

  if (table == NULL || new_size < 0) {
    return private_show_error ("invalid argument for private_resize_table()", seek_malloc_failed);
  }

  new_array = (seek_entry_type *)malloc (sizeof (seek_entry_type) * new_size);
  if (NULL == new_array) {
    return private_show_error ("unable to allocate more space for table", seek_malloc_failed);
  }

  memcpy (new_array, table->array, table->allocated_size * sizeof (seek_entry_type));
  free (table->array);

  table->allocated_size = new_size;
  table->array          = new_array;
    
  return seek_no_error;
}
