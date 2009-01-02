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

#include "ffmpeg_fas.h"
#include "seek_indices.h"
#include "test_support.h"
#include <stdio.h>

int main (int argc, char **argv)
{
  fas_error_type video_error;
  fas_context_ref_type context, seek_context;
  
  if (argc < 3) {
    fprintf (stderr, "usage: %s <video_file> <seek_table>\n", argv[0]);
    fail("arguments\n");
  }

  seek_table_type table = read_table_file(argv[2]);
  if (table.num_entries == 0)
    fail("bad table\n");

  fas_initialize (FAS_TRUE, FAS_RGB24);
  
  video_error = fas_open_video (&context, argv[1]);
  if (video_error != FAS_SUCCESS)    fail("fail on open\n");
  
  video_error = fas_put_seek_table(context, table);
  if (video_error != FAS_SUCCESS)    fail("fail on put_seek_table\n");

  video_error = fas_open_video (&seek_context, argv[1]);
  if (video_error != FAS_SUCCESS)    fail("fail on open\n");
  
  int i;
  for(i=0;i<table.num_entries;i++)
    {
      //      int frame_index = table.array[table.num_entries - i - 1].display_index;
      int frame_index = table.array[i].display_index;
      if (FAS_SUCCESS != fas_seek_to_frame(context, frame_index))
	fail("failed on seek");

      fas_raw_image_type image_buffer;

      if (FAS_SUCCESS != fas_get_frame (context, &image_buffer))
	fail("failed on rgb image\n");  

      char filename[50];
      sprintf(filename, "frame_%04d.ppm", frame_index);
      
      fprintf(stderr, "Writing %s (seek_table_value=%d frame_index=%d)\n", filename, frame_index, fas_get_frame_index(context));
      ppm_save(&image_buffer, filename);
      
      fas_free_frame (image_buffer);
    }
  
  success();
}

