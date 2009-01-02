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

#include "seek_indices.h"
#include "ffmpeg_fas.h"
#include <stdio.h>

int main (int argc, char **argv)
{
  fas_error_type video_error;
  fas_context_ref_type context;
  fas_raw_image_type image_buffer;
  
  if (argc < 2) {
    fprintf (stderr, "usage: %s <video_file>\n", argv[0]);
    return -1;
  }

  fas_initialize (FAS_FALSE, FAS_RGB24);
  
  video_error = fas_open_video (&context, argv[1]);
  if (video_error != FAS_SUCCESS)
    return -1;

  while (fas_frame_available (context))
    {
      if (FAS_SUCCESS != fas_get_frame (context, &image_buffer))
	return -1;
      fas_free_frame (image_buffer);

      video_error = fas_step_forward (context);
    }

  seek_table_type table;
  table = fas_get_seek_table(context);

  seek_show_raw_table(stdout, table);
  
  return 1;
}

