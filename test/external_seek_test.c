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

/* test seek using external table */

#include "ffmpeg_fas.h"
#include "seek_indices.h"
#include "test_support.h"
#include <stdio.h>

#define TEST_SET_SIZE  1000
#define N_ITERATIONS   500

int compare_frames(fas_raw_image_type img1, fas_raw_image_type img2)
{
  //  printf("(%d %d) (%d %d) (%d %d) (%d %d)\n", img1.width, img2.width,
  // 	 img1.height, img2.height,
  // img1.bytes_per_line, img2.bytes_per_line,
  // img1.color_space, img2.color_space);
  
  if ((img1.width  != img2.width) ||
      (img1.height != img2.height) || 
      (img1.bytes_per_line != img2.bytes_per_line) || 
      (img1.color_space != img2.color_space))
    return 0;

  int i,j;
  int mask = 0;

  for (i=0;i<img1.height;i++)
    for(j=0;j<img1.bytes_per_line;j++)
      {
	//printf("%d ", (img1.data[i*img1.bytes_per_line+j] - img2.data[i*img1.bytes_per_line+j+j]));
	mask |= (img1.data[i*img1.bytes_per_line+j] - img2.data[i*img1.bytes_per_line+j]);
      }
  
  if (mask == 0)
    return 1;
  else
    return 0;
}

void do_random_test(fas_context_ref_type context, int start, int stop, int count)
{
  //  printf ("start: %d  stop: %d\n", start, stop );

  while (fas_get_frame_index(context) < start)
    if (FAS_SUCCESS != fas_step_forward(context))
      fail("failed on advancement\n");

  fas_raw_image_type *ref_frames = malloc( (stop - start + 1)* sizeof(fas_raw_image_type));
  
  int i;
  fas_error_type video_error;

  while (fas_get_frame_index(context) <= stop)
    {
      i = fas_get_frame_index(context) - start;

      video_error = fas_get_frame(context, &(ref_frames[i]));
      if (video_error != FAS_SUCCESS)    fail("fail on test(1)\n");

      video_error = fas_step_forward(context);
      if (video_error != FAS_SUCCESS)    fail("fail on test(2)\n");

    }

  int index = -1;
  int prev_index;

  for (i=0;i<count;i++)
    {
      int offset = random() % (stop - start + 1);
      prev_index = index;
      index = start + offset;

      video_error = fas_seek_to_frame(context, index);
      if (video_error != FAS_SUCCESS)    fail("fail on test(seek)\n");


      fas_raw_image_type test_frame;
      video_error = fas_get_frame(context, &test_frame);
      if (video_error != FAS_SUCCESS)    fail("fail on test(seek2)\n");

      //      printf("offset: %d / %d\n", offset, stop - start + 1);
      
      if (!compare_frames(test_frame, ref_frames[offset]))
	{	  
	  char buffer[70];
	  
	  sprintf(buffer, "fail-%d-test.ppm", index);
	  ppm_save(&test_frame, buffer);
	  sprintf(buffer, "fail-%d-ref.ppm", index);
	  ppm_save(&ref_frames[offset], buffer);
	  sprintf(buffer, "failed on compare after seeking (%d->%d)\n", prev_index, index);
	  
	  fail(buffer);
	}
      
      fas_free_frame(test_frame);
    }
  
  for (i=0;i<stop - start + 1;i++)
    free(ref_frames[i].data);
    
  free(ref_frames);
}

int main (int argc, char **argv)
{
  fas_error_type video_error;
  fas_context_ref_type context;
  
  if (argc < 3) {
    fprintf (stderr, "usage: %s <video_file> <seek_table>\n", argv[0]);
    fail("arguments\n");
  }

  fprintf(stderr, "%s : ", argv[1]);

  seek_table_type table = read_table_file(argv[2]);
  if (table.num_entries == 0)
    fail("bad table\n");
  
  fas_initialize (FAS_FALSE, FAS_RGB24);
  
  video_error = fas_open_video (&context, argv[1]);
  if (video_error != FAS_SUCCESS)    fail("fail on open\n");
  
  video_error = fas_put_seek_table(context, table);
  if (video_error != FAS_SUCCESS)    fail("fail on put_seek_table\n");
    
  if (fas_get_frame_count(context) < 0)
    fail("n_frames = -1\n");

  if (fas_get_frame_count(context) < TEST_SET_SIZE)
      do_random_test(context, 0, fas_get_frame_count(context) - 1, N_ITERATIONS);
  else if (fas_get_frame_count(context) < TEST_SET_SIZE * 2)
    {
      do_random_test(context, 0, fas_get_frame_count(context) / 2 - 1, N_ITERATIONS / 2);
      do_random_test(context, fas_get_frame_count(context) / 2 + 1, fas_get_frame_count(context) - 1 , N_ITERATIONS / 2);   
    }
  else
    {
      do_random_test(context, 0, TEST_SET_SIZE, N_ITERATIONS / 2);
      do_random_test(context, fas_get_frame_count(context) - TEST_SET_SIZE, fas_get_frame_count(context) - 1 , N_ITERATIONS / 2);   
    }

  seek_release_table(&table);
  fas_close_video(context);
  
  success();
}

