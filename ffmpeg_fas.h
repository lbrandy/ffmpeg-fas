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

#ifndef FFMPEG_FAS_H
#define FFMPEG_FAS_H

/* If C++ then we need to __extern "C". Compiler defines __cplusplus */
#ifdef __cplusplus
#define __extern extern "C"
#else
#define __extern extern
#endif

#include "seek_indices.h"


typedef enum
{
  FAS_GRAY8    = 1,
  FAS_RGB24    = 2,
  FAS_BGR24    = 3,
  FAS_ARGB32   = 4,
  FAS_ABGR32   = 5,
  FAS_YUV420P  = 6,
  FAS_YUYV422  = 7,
  FAS_UYVY422  = 8,
  FAS_YUV422P  = 9,
  FAS_YUV444P  = 10,
} fas_color_space_type;

typedef struct
{
  unsigned char *data;
  int width;
  int height;
  int bytes_per_line;
  fas_color_space_type color_space;
} fas_raw_image_type;


/**********************************************************************
 * Video IO Types
 **********************************************************************/

typedef struct fas_context_struct* fas_context_ref_type;

typedef enum
{
  FAS_SUCCESS,
  FAS_FAILURE,
  FAS_INVALID_ARGUMENT,
  FAS_OUT_OF_MEMORY,
  FAS_UNSUPPORTED_FORMAT,
  FAS_UNSUPPORTED_CODEC,
  FAS_NO_MORE_FRAMES,
  FAS_DECODING_ERROR,
  FAS_SEEK_ERROR,
} fas_error_type;

typedef enum
{
  FAS_FALSE = 0,
  FAS_TRUE  = 1
} fas_boolean_type;


__extern void             fas_initialize (fas_boolean_type logging, fas_color_space_type format);
__extern void             fas_set_format (fas_color_space_type format);

__extern fas_error_type   fas_open_video  (fas_context_ref_type *context_ptr, char *file_path);
__extern fas_error_type   fas_close_video (fas_context_ref_type context);

__extern char*            fas_error_message (fas_error_type error);

__extern fas_boolean_type fas_frame_available (fas_context_ref_type context);
__extern int              fas_get_frame_index (fas_context_ref_type context);
__extern fas_error_type   fas_step_forward    (fas_context_ref_type context);

__extern fas_error_type   fas_get_frame  (fas_context_ref_type context, fas_raw_image_type *image_ptr);
__extern void             fas_free_frame (fas_raw_image_type image);

__extern fas_error_type   fas_seek_to_nearest_key     (fas_context_ref_type context, int target_index);
__extern fas_error_type   fas_seek_to_frame           (fas_context_ref_type context, int target_index);

__extern int              fas_get_frame_count         (fas_context_ref_type context);
__extern int              fas_get_frame_count_fast    (fas_context_ref_type context);

__extern fas_error_type   fas_put_seek_table  (fas_context_ref_type context, seek_table_type table);
__extern seek_table_type  fas_get_seek_table  (fas_context_ref_type context);

/* will extract raw 420p if the video is in that format -- needs to be alloced ahead of time*/
__extern fas_error_type  fas_fill_420p_ptrs (fas_context_ref_type context, unsigned char *y, unsigned char *u, unsigned char *v);

/* will extract gray8 data from movie (will convert to ensure you get it) -- need to be alloc'ed ahead of time*/
__extern fas_error_type  fas_fill_gray8_ptr(fas_context_ref_type context, unsigned char *y);

__extern int  fas_get_current_width(fas_context_ref_type context);
__extern int  fas_get_current_height(fas_context_ref_type context);

__extern unsigned long long fas_get_frame_duration(fas_context_ref_type context);

#endif 
