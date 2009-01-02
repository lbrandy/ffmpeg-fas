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

#ifndef FAS_TEST_SUPPORT
#define FAS_TEST_SUPPORT

#include <stdio.h>
#include <stdlib.h>

#define fail(x) { fprintf(stderr, "fail : "); fprintf(stderr, x); exit(EXIT_FAILURE); }
#define success() { fprintf(stderr, "success\n");  exit(EXIT_SUCCESS); }

/* 
 * static void pgm_save(ppt_raw_image_type *image, char *filename)
 * {
 *     FILE *f;
 *     int i;
 * 
 *     f=fopen(filename,"w");
 *     fprintf(f,"P5\n%d\n%d\n%d\n", image->width, image->height, 255);
 * 
 *     for(i=0; i<image->height; i++) {
 *       fwrite(image->data + i * image->bytes_per_line, 1, image->width, f);
 *     }
 * 
 *     fclose(f);
 * }
 */

static void ppm_save(fas_raw_image_type *image, char *filename)
{
    FILE *f;
    int i;

    if (image->color_space != FAS_RGB24) {
      return;
    }

    f=fopen(filename,"wb");
    fprintf(f,"P6\n%d %d\n%d\n", image->width, image->height, 255);

    for(i=0; i<image->height; i++) {
      fwrite(image->data + i * image->bytes_per_line, 1, image->width * 3, f);
    }

    fclose(f);
}

#endif
