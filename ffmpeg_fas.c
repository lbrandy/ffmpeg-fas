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

#if defined( _WIN32 ) && defined( STATIC_DLL )
extern "C" 
{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
int img_convert(AVPicture *dst, int dst_pix_fmt, const AVPicture *src, int src_pix_fmt, int src_width, int src_height);
}
#else
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#endif /*  _WIN32 && STATIC_DLL */

#include "seek_indices.h"
#include "private_errors.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define FIRST_FRAME_INDEX     0
#define NUM_POSSIBLE_ERRORS   9

enum PixelFormat	fmt;

/**** Private Types ***********************************************************/

typedef struct fas_context_struct {
  fas_boolean_type is_video_active;
  fas_boolean_type is_frame_available;

  int current_frame_index;

  seek_table_type seek_table;

  /* ffmpeg */
  AVFormatContext  *format_context;
  AVCodecContext   *codec_context;
  int               stream_idx;

  AVFrame          *frame_buffer;      // internal buffer

  AVFrame          *rgb_frame_buffer;  // extra AVFrames (for color conversion)
  uint8_t          *rgb_buffer;        // actual data buffer for rgb_frame_buffer (needs to be freed seperately)
  fas_boolean_type  rgb_already_converted; // color_convert(frame_buffer) == rgb_frame_buffer (so we don't need to repeat conversions)

  AVFrame          *gray8_frame_buffer;
  uint8_t          *gray8_buffer;
  fas_boolean_type  gray8_already_converted;

  int64_t          current_dts;         // decoding timestamp of the most recently parsed packet
  int64_t          previous_dts;        // for previous packet (always use previous packet for seek_table (workaround))
  int64_t          keyframe_packet_dts; // dts of most recent keyframe packet
  int64_t          first_dts;           // for very first packet (needed in seek, for first keyframe)

} fas_context_type;

static char* invalid_error_code = "not a valid error code";
static char *gbl_error_strings[NUM_POSSIBLE_ERRORS] =
{
  "fas_success",
  "fas_failure",
  "fas_invalid_argument",
  "fas_out_of_memory",
  "fas_unsupported_format",
  "fas_unsupported_codec",
  "fas_no_more_frames",
  "fas_decoding_error",
  "fas_seek_error"
};

char* fas_error_message(fas_error_type error)
{
  if ((error < 0) || (error >= NUM_POSSIBLE_ERRORS))
    return invalid_error_code;

  return gbl_error_strings[error];
}

static void             private_show_warning (const char *message);
static fas_error_type   private_show_error (const char *message, fas_error_type error);
static fas_error_type   private_convert_to_rgb (fas_context_ref_type ctx);
static fas_error_type   private_seek_to_nearest_key (fas_context_ref_type context, int target_index, int offset);
fas_error_type          private_complete_seek_table (fas_context_ref_type context);


void fas_set_logging (fas_boolean_type logging)
{
  if (logging == FAS_TRUE)
    {
      SHOW_ERROR_MESSAGES = 1;
      SHOW_WARNING_MESSAGES = 1;
#ifndef _WIN32
      av_log_level = AV_LOG_INFO;
#endif
    }
  else 
    {
      SHOW_ERROR_MESSAGES = 0;
      SHOW_WARNING_MESSAGES = 0;
#ifndef _WIN32
      av_log_level = AV_LOG_QUIET;
#endif
    }  
}

/* Set the output image colorspace */
void fas_set_format(fas_color_space_type format)
{
	switch (format)
	{
	case FAS_GRAY8:
		fmt = PIX_FMT_GRAY8;
		break;
	case FAS_ARGB32:
		fmt = PIX_FMT_RGB32_1;
		break;
	case FAS_ABGR32:
		fmt = PIX_FMT_BGR32_1;
		break;
	case FAS_YUV420P:
		fmt = PIX_FMT_YUV420P;
		break;
	case FAS_YUYV422:
		fmt = PIX_FMT_YUYV422;
		break;
	case FAS_UYVY422:
		fmt = PIX_FMT_UYVY422;
		break;
	case FAS_YUV422P:
		fmt = PIX_FMT_YUV422P;
		break;
	case FAS_YUV444P:
		fmt = PIX_FMT_YUV444P;
		break;
	case FAS_RGB24:
		fmt = PIX_FMT_RGB24;
		break;
	case FAS_BGR24:
		fmt = PIX_FMT_BGR24;
		break;
	default:
		fmt = PIX_FMT_RGB24;
		break;
	}
}


void fas_initialize (fas_boolean_type logging, fas_color_space_type format)
{
  fas_set_logging(logging);
  fas_set_format(format);
  av_register_all();
  
  return;
}

/* fas_open_video */

fas_error_type fas_open_video (fas_context_ref_type *context_ptr, char *file_path)
{
  if (NULL == context_ptr)
    return private_show_error ("NULL context pointer provided", FAS_INVALID_ARGUMENT);

  //  seek_error_type      seek_error;
  fas_context_ref_type fas_context;

  *context_ptr = NULL; // set returned context to NULL in case of error
  
  fas_context = (fas_context_ref_type)malloc (sizeof (fas_context_type));
  memset(fas_context, 0, sizeof(fas_context_type));
  
  if (NULL == fas_context)
    return private_show_error ("unable to allocate buffer", FAS_OUT_OF_MEMORY);

  fas_context->is_video_active        = FAS_TRUE;
  fas_context->is_frame_available     = FAS_TRUE;
  fas_context->current_frame_index    = FIRST_FRAME_INDEX - 1;
  fas_context->current_dts            = AV_NOPTS_VALUE;
  fas_context->previous_dts           = AV_NOPTS_VALUE;
  fas_context->keyframe_packet_dts    = AV_NOPTS_VALUE;
  fas_context->first_dts              = AV_NOPTS_VALUE;

  fas_context->seek_table = seek_init_table (-1); /* default starting size */ 

  if (av_open_input_file ( &(fas_context->format_context), file_path, NULL, 0, NULL ) != 0)
    {
      fas_close_video(fas_context);
      return private_show_error ("failure to open file", FAS_UNSUPPORTED_FORMAT);
    }

  if (av_find_stream_info (fas_context->format_context) < 0)
    {
      fas_close_video(fas_context);
      return private_show_error ("could not extract stream information", FAS_UNSUPPORTED_FORMAT);
    }

  if (SHOW_WARNING_MESSAGES)
    dump_format(fas_context->format_context, 0, file_path, 0);

  int stream_idx;
  for (stream_idx = 0; stream_idx < fas_context->format_context->nb_streams; stream_idx++) 
    {
      if (fas_context->format_context->streams[stream_idx]->codec->codec_type == CODEC_TYPE_VIDEO)
	{
	  fas_context->stream_idx = stream_idx;
	  fas_context->codec_context  = fas_context->format_context->streams[stream_idx]->codec;
	  break;
	}
    }
  
  if (fas_context->codec_context == 0)
    {
      fas_close_video(fas_context);
      return private_show_error ("failure to find a video stream", FAS_UNSUPPORTED_FORMAT);
    }

  AVCodec *codec = avcodec_find_decoder (fas_context->codec_context->codec_id);

  if (!codec)
    {
      fas_context->codec_context = 0;
      fas_close_video(fas_context);
      return private_show_error("failed to find correct video codec", FAS_UNSUPPORTED_CODEC);
    }
  
  if (avcodec_open (fas_context->codec_context, codec) < 0)
    {
      fas_context->codec_context = 0;
      fas_close_video(fas_context);
      return private_show_error ("failed to open codec", FAS_UNSUPPORTED_CODEC);
    }
  
  fas_context->frame_buffer     = avcodec_alloc_frame ();
  if (fas_context->frame_buffer == NULL)
    {
      fas_close_video(fas_context);
      return private_show_error ("failed to allocate frame buffer", FAS_OUT_OF_MEMORY);
    }
  
  fas_context->rgb_frame_buffer = avcodec_alloc_frame ();
  if (fas_context->rgb_frame_buffer == NULL)
    {
      fas_close_video(fas_context);
      return private_show_error ("failed to allocate rgb frame buffer", FAS_OUT_OF_MEMORY);
    }
  
  fas_context->gray8_frame_buffer = avcodec_alloc_frame ();
  if (fas_context->gray8_frame_buffer == NULL)
    {
      fas_close_video(fas_context);
      return private_show_error ("failed to allocate gray8 frame buffer", FAS_OUT_OF_MEMORY);
    }
  
  fas_context->rgb_buffer = 0;
  fas_context->gray8_buffer = 0;
  fas_context->rgb_already_converted = FAS_FALSE;
  fas_context->gray8_already_converted = FAS_FALSE;

  *context_ptr = fas_context; 


  if (FAS_SUCCESS != fas_step_forward(*context_ptr))
    return private_show_error ("failure decoding first frame", FAS_NO_MORE_FRAMES);

  if (!fas_frame_available(*context_ptr))
    return private_show_error ("couldn't find a first frame (no valid frames in video stream)", FAS_NO_MORE_FRAMES);



  return FAS_SUCCESS;
}

/* fas_close_video */
fas_error_type fas_close_video (fas_context_ref_type context)
{
  if (NULL == context) 
    return private_show_error ("NULL context provided for fas_close_video()", FAS_INVALID_ARGUMENT);
  
  if (!(context->is_video_active)) 
    {
      private_show_warning ("Redundant attempt to close an inactive video");
      return FAS_SUCCESS;
    }
  
  if (context->codec_context)
    if (avcodec_find_decoder (context->codec_context->codec_id))
      avcodec_close(context->codec_context);

  if (context->format_context)
    av_close_input_file (context->format_context);

  if (context->rgb_frame_buffer)
    av_free (context->rgb_frame_buffer);

  if (context->gray8_frame_buffer)
    av_free (context->gray8_frame_buffer);

  if (context->rgb_buffer)
    av_free(context->rgb_buffer);
  
  if (context->gray8_buffer)
    av_free(context->gray8_buffer);
  
  if (context->frame_buffer)
    av_free (context->frame_buffer);
    
  seek_release_table (&(context->seek_table)); 
  
  context->is_video_active = FAS_FALSE;
  
  free (context);

  return FAS_SUCCESS;
}


/* fas_step_forward */
fas_error_type fas_step_forward (fas_context_ref_type context)
{
  if ((NULL == context) || (FAS_TRUE != context->is_video_active)) {
    return private_show_error ("invalid or unopened context", FAS_INVALID_ARGUMENT);
  }
  
  if (!context->is_frame_available)
    {
      private_show_warning ("tried to advance after end of frames");
      return FAS_SUCCESS;
    }

  context->current_frame_index++;

  AVPacket packet;
  while (FAS_TRUE)
    {
      if (av_read_frame(context->format_context, &packet) < 0)
	{
	  /* finished */      
	  context->is_frame_available = FAS_FALSE;
	  context->seek_table.completed = seek_true;
	  return FAS_SUCCESS;
	}
      
      int frameFinished;
      if (packet.stream_index == context->stream_idx)
	{
	  context->previous_dts = context->current_dts;
	  context->current_dts = packet.dts;
	  
	  /* seek support: set first_dts */
	  if (context->first_dts == AV_NOPTS_VALUE)
	    context->first_dts = packet.dts;
	  
	  /* seek support: set key-packet info to previous packet's dts, when possible */
	  /* note this -1 approach to setting the packet is a workaround for a common failure. setting 
	     to 0 would work just incur a huge penalty in videos that needed -1. Might be worth testing.
	  */
	  if (packet.flags & PKT_FLAG_KEY)
	    {
	      //fprintf(stderr, "Packet: (F:%d %lld %lld)\n", context->current_frame_index, packet.pts, packet.dts);
	      
	      if (context->previous_dts == AV_NOPTS_VALUE)
		context->keyframe_packet_dts = packet.dts;
	      else
		context->keyframe_packet_dts = context->previous_dts;
	    }
	  
	  avcodec_decode_video(context->codec_context, context->frame_buffer, &frameFinished,
			       packet.data, packet.size);	
	  
	  if (frameFinished)
	    {
	      /* seek support: (try to) add entry to seek_table */
	      if (context->frame_buffer->key_frame)
		{
		  //		fprintf(stderr, "Frame : (PXX F%d: %lld %lld)\n", context->current_frame_index, packet.pts, packet.dts);
		  
		  seek_entry_type entry;
		  entry.display_index = context->current_frame_index;
		  entry.first_packet_dts = context->keyframe_packet_dts;
		  entry.last_packet_dts = packet.dts;
		  
		  if (fas_get_frame_index(context) == FIRST_FRAME_INDEX)
		    entry.first_packet_dts = context->first_dts;
		  
		  seek_append_table_entry(&context->seek_table, entry);	     
		}
	      
	      if (context->current_frame_index - FIRST_FRAME_INDEX + 1 > context->seek_table.num_frames)
		context->seek_table.num_frames = context->current_frame_index - FIRST_FRAME_INDEX + 1;
	      
	      break;
	    }
	}
      
      av_free_packet(&packet);
    }
  
  context->rgb_already_converted = FAS_FALSE;
  context->gray8_already_converted = FAS_FALSE;
  av_free_packet(&packet);
  return FAS_SUCCESS;
}

/* fas_get_frame_index */

int fas_get_frame_index (fas_context_ref_type context)
{
  if (NULL == context)
    return private_show_error ("NULL context provided for fas_get_frame_index()", FAS_INVALID_ARGUMENT);

  if (FAS_TRUE != context->is_video_active) 
    return private_show_error ("No video is open for fas_get_frame_index()", FAS_INVALID_ARGUMENT);
  
  return context->current_frame_index;
}


/* fas_get_frame */

fas_error_type fas_get_frame (fas_context_ref_type context, fas_raw_image_type *image_ptr)
{
  int buffer_size;
  fas_error_type fas_error;

  if (NULL == context || FAS_FALSE == context->is_video_active)
    return private_show_error ("null context or inactive video", FAS_INVALID_ARGUMENT);

  if (NULL == image_ptr)
    return private_show_error ("null image_ptr on get_frame", FAS_INVALID_ARGUMENT);
  
  if (!fas_frame_available(context))
    return private_show_error ("no frame available for extraction", FAS_NO_MORE_FRAMES);

  memset (image_ptr, 0, sizeof (fas_raw_image_type));

  switch (fmt)
  {
  case PIX_FMT_RGB24:
	  image_ptr->bytes_per_line = context->codec_context->width * 3;
	  image_ptr->color_space    = FAS_RGB24;
	  break;
  case PIX_FMT_BGR24:
	  image_ptr->bytes_per_line = context->codec_context->width * 3;
	  image_ptr->color_space    = FAS_BGR24;
	  break;
  case PIX_FMT_ARGB:
	  image_ptr->bytes_per_line = context->codec_context->width * 4;
	  image_ptr->color_space    = FAS_ARGB32;
	  break;
  case PIX_FMT_ABGR:
	  image_ptr->bytes_per_line = context->codec_context->width * 4;
	  image_ptr->color_space    = FAS_ABGR32;
	  break;
  case PIX_FMT_YUV420P:
	  image_ptr->bytes_per_line = (context->codec_context->width * 3) >> 1;
	  image_ptr->color_space    = FAS_YUV420P;
	  break;
  case PIX_FMT_YUYV422:
	  image_ptr->bytes_per_line = context->codec_context->width * 2;
	  image_ptr->color_space    = FAS_YUYV422;
	  break;
  case PIX_FMT_UYVY422:
	  image_ptr->bytes_per_line = context->codec_context->width * 2;
	  image_ptr->color_space    = FAS_UYVY422;
	  break;
  case PIX_FMT_YUV422P:
	  image_ptr->bytes_per_line = context->codec_context->width * 2;
	  image_ptr->color_space    = FAS_YUV422P;
	  break;
  case PIX_FMT_YUV444P:
	  image_ptr->bytes_per_line = context->codec_context->width * 3;
	  image_ptr->color_space    = FAS_YUV444P;
	  break;
  }

  buffer_size = image_ptr->bytes_per_line * context->codec_context->height;

  image_ptr->data = (unsigned char *)malloc (buffer_size);
  if (NULL == image_ptr->data)
    return private_show_error ("unable to allocate space for RGB image", FAS_OUT_OF_MEMORY);

  image_ptr->width          = context->codec_context->width;
  image_ptr->height         = context->codec_context->height;

  
  fas_error = private_convert_to_rgb(context);

  int j;
  unsigned char *from;
  unsigned char *to;
  for (j=0;j<context->codec_context->height; j++)
    {
      from = context->rgb_frame_buffer->data[0] + j*context->rgb_frame_buffer->linesize[0];
      to = image_ptr->data + j*image_ptr->bytes_per_line;
      
      memcpy(to, from, image_ptr->bytes_per_line);
    }

  if (FAS_SUCCESS != fas_error)
    return private_show_error ("unable to convert image to RGB", FAS_FAILURE);

  return FAS_SUCCESS;
}

/* fas_free_frame */

void fas_free_frame (fas_raw_image_type image)
{
  if (NULL == image.data) 
    return;
  
  free (image.data);
  
  return;
}

/* fas_get_seek_table */ 
seek_table_type fas_get_seek_table (fas_context_ref_type context)
{
  seek_table_type null_table;

  null_table.array = NULL;
  null_table.completed = seek_false;
  null_table.num_frames = -1;
  null_table.num_entries = 0;
  null_table.allocated_size = 0;

  if (NULL == context || FAS_FALSE == context->is_video_active)
    return null_table;
    
  return context->seek_table;
}

/* fas_put_seek_table */  
fas_error_type fas_put_seek_table (fas_context_ref_type context, seek_table_type table)
{
  if (NULL == context || FAS_FALSE == context->is_video_active)
    return private_show_error ("null context or inactive video", FAS_INVALID_ARGUMENT);
  
  seek_release_table (&context->seek_table);
  context->seek_table = seek_copy_table(table);
  
  return FAS_SUCCESS;
}
 
/* private_complete_seek_table */
fas_error_type private_complete_seek_table (fas_context_ref_type context)
{
  if ((NULL == context) || (FAS_FALSE == context->is_video_active))
    return private_show_error ("invalid or unopened context", FAS_INVALID_ARGUMENT);

  if (context->seek_table.completed)
    return FAS_SUCCESS;

  fas_error_type fas_error = fas_seek_to_nearest_key (context, context->seek_table.num_frames + FIRST_FRAME_INDEX - 1);
  if (FAS_SUCCESS != fas_error)
    return private_show_error("failed when trying to complete seek table (1) (first frame not labeled keyframe?)", fas_error);

  while (fas_frame_available(context))
    {
      //      printf("%d\n", context->seek_table.num_frames);
      fas_step_forward(context);
    }

  if (!context->seek_table.completed)
    return private_show_error("failed when trying to complete seek table (2)", FAS_SEEK_ERROR);

  return FAS_SUCCESS;
}

/* fas_seek_to_frame */
fas_error_type fas_seek_to_frame (fas_context_ref_type context, int target_index)
{

  fas_error_type fas_error;

  if ((NULL == context) || (FAS_FALSE == context->is_video_active))
    return private_show_error ("invalid or unopened context", FAS_INVALID_ARGUMENT);

  //  printf("seeking to %d (from %d)!\n", target_index, context->current_frame_index);
  if (target_index == context->current_frame_index)
    return FAS_SUCCESS;

  fas_error = fas_seek_to_nearest_key (context, target_index); 

  if (fas_error != FAS_SUCCESS)
    return private_show_error ("error advancing to key frame before seek", fas_error);

  if (fas_get_frame_index(context) > target_index)
    return private_show_error ("error advancing to key frame before seek (index isn't right)", fas_error);
 
  while (fas_get_frame_index(context) < target_index)
    {
      if (fas_frame_available(context))
	fas_step_forward(context);
      else
	return private_show_error ("error advancing to request frame (probably out of range)", FAS_SEEK_ERROR);
    }


  return FAS_SUCCESS;
}

/* fas_seek_to_nearest_key */

fas_error_type fas_seek_to_nearest_key (fas_context_ref_type context, int target_index)
{
  return private_seek_to_nearest_key(context, target_index,0);
}

/* private_seek_to_nearest_key */

fas_error_type private_seek_to_nearest_key (fas_context_ref_type context, int target_index, int offset)
{
  if ((NULL == context) || (FAS_TRUE != context->is_video_active))
    return private_show_error ("invalid or unopened context", FAS_INVALID_ARGUMENT);

  //  printf("HERE: from: %d to: %d offset: %d\n", context->current_frame_index, target_index, offset);
  fas_error_type fas_error;
  seek_entry_type seek_entry;
  seek_error_type seek_error = seek_get_nearest_entry (&(context->seek_table), &seek_entry, target_index, offset);
  
  if (seek_error != seek_no_error)
    return private_show_error ("error while searching seek table", FAS_SEEK_ERROR);

  if (seek_entry.display_index == context->current_frame_index)
    return FAS_SUCCESS;

  //  printf("HERE: from: %d to: %d (%d) offset: %d\n", context->current_frame_index, target_index, seek_entry.display_index, offset);
  //  printf("trying to seek to %d (%lld->%lld)\n", seek_entry.display_index, seek_entry.first_packet_dts, seek_entry.last_packet_dts);

  // if something goes terribly wrong, return bad current_frame_index
  context->current_frame_index = -2;
  context->is_frame_available = FAS_TRUE;

  int flags = 0;
  if (seek_entry.first_packet_dts <= context->current_dts)
    flags = AVSEEK_FLAG_BACKWARD;
  
  //  printf("av_seek_frame: %lld\n", seek_entry.first_packet_dts);
  if (av_seek_frame(context->format_context, context->stream_idx, seek_entry.first_packet_dts, flags) < 0)
    return private_show_error("seek to keyframe failed", FAS_SEEK_ERROR);
  

  avcodec_flush_buffers (context->codec_context);
  
  fas_error = fas_step_forward (context);  

  if (fas_error != FAS_SUCCESS || !context->is_frame_available)  
    {
      // something bad has happened, try previous keyframe
      private_show_warning("processing of seeked keyframe failed, trying previous keyframe");
      return private_seek_to_nearest_key(context, target_index, offset + 1);
    }
  
  while (context->current_dts < seek_entry.last_packet_dts)
    {
      //printf("frame-times: current: %lld target: %lld is_key: %d\n", context->current_dts, seek_entry.last_packet_dts, context->frame_buffer->key_frame);
      fas_error = fas_step_forward(context);
      if (fas_error != FAS_SUCCESS) 
	return private_show_error ("unable to process up to target frame (fas_seek_to_frame)", fas_error);     
    }
    
  //  printf("keyframe vitals: %d looking_for: %lld at: %lld\n", seek_entry.display_index, seek_entry.last_packet_dts, context->current_dts);
  if (context->current_dts != seek_entry.last_packet_dts)
    {      
      /* seek to last key-frame, but look for this one */
      private_show_warning("missed keyframe, trying previous keyframe");
      return private_seek_to_nearest_key(context, target_index, offset + 1);
    }

  /* Ideally, we could just check if the frame decoded is of the correct time stamp... but... we need several ugly workarounds:
     
     1) Some videos have bad keyframes that don't get decoded properly. In this cases, we need to go back a keyframe.
     
     2) Other times, none of the frames are labeled keyframes. In these cases, we need to allow seeking to frame 0 
     even when it's not labeled as a keyframe. Messy set of conditions.
  */
  
  if ((!context->frame_buffer->key_frame) && (seek_entry.display_index != 0))
    {
      private_show_warning("found keyframe, but not labeled as keyframe, so trying previous keyframe.");
      /* seek & look for previous keyframe */
      /* REMOVE FROM TABLE?                */
      return private_seek_to_nearest_key(context, seek_entry.display_index - 1, 0);
    }

  context->current_frame_index = seek_entry.display_index;

  return FAS_SUCCESS;
}

/* fas_get_frame_count */

int fas_get_frame_count_fast (fas_context_ref_type context)
{
  
  if (NULL == context || FAS_FALSE == context->is_video_active) 
    {
      private_show_error ("NULL or invalid context", FAS_INVALID_ARGUMENT);
      return -1;
    }
  
  if (context->seek_table.completed == seek_true)
    return context->seek_table.num_frames;
  
  return -1;
}

int fas_get_frame_count (fas_context_ref_type context)
{
  int fast = fas_get_frame_count_fast(context);
  if (fast >= 0)
    return fast;

  int current_frame = fas_get_frame_index(context);

  fas_error_type fas_error;

  fas_error = private_complete_seek_table(context); 
  if (FAS_SUCCESS != fas_error)
    {
      private_show_error("failed in get_frame_count trying to complete the seek table", fas_error);
      return -1;
    }

  //  seek_show_raw_table(stderr, context->seek_table);

  fas_error = fas_seek_to_frame(context, current_frame);
  if (FAS_SUCCESS != fas_error)
    {
      private_show_error("failed in get_frame_count when trying to seek back to original location", fas_error);
      return -1;
    }
  
  fast = fas_get_frame_count_fast(context);
  if (fast < 0)
    private_show_warning("get_frame_count failed");
  
  return fast;
}

/* fas_frame_available */

fas_boolean_type fas_frame_available (fas_context_ref_type context)
{
  if (NULL == context) 
    {
      private_show_error ("NULL context provided for fas_get_frame_index()", FAS_INVALID_ARGUMENT);
      return FAS_FALSE;
    }

  if (!context->is_video_active)
    return FAS_FALSE;

  return context->is_frame_available;
}


/* private_show_error */

static fas_error_type private_show_error (const char *message, fas_error_type error)
{
  if (SHOW_ERROR_MESSAGES)
    fprintf (stderr, " ===> ffmpeg_fas: %s\n", message);
  return error;
}

static void private_show_warning (const char *message) 
{
  if (SHOW_WARNING_MESSAGES)
    fprintf (stderr, " ---- ffmpeg_fas: %s\n", message);
  return;
}


/* private_convert_to_rgb */

fas_error_type private_convert_to_rgb (fas_context_ref_type ctx)
{
  if (ctx->rgb_already_converted)
    return FAS_SUCCESS;

  if (ctx->rgb_buffer == 0)
    {
      int numBytes = avpicture_get_size(fmt, ctx->codec_context->width,
					ctx->codec_context->height);
      ctx->rgb_buffer = (uint8_t *) av_malloc(numBytes*sizeof(uint8_t));
      avpicture_fill((AVPicture *) ctx->rgb_frame_buffer, ctx->rgb_buffer, fmt,
		     ctx->codec_context->width, ctx->codec_context->height);
    }

  if (img_convert((AVPicture *) ctx->rgb_frame_buffer, fmt, (AVPicture *) ctx->frame_buffer, 
		  ctx->codec_context->pix_fmt,
		  ctx->codec_context->width, ctx->codec_context->height) < 0)
    private_show_error("error converting to rgb", FAS_DECODING_ERROR);

  ctx->rgb_already_converted = FAS_TRUE;

  return FAS_SUCCESS;
}


/* private_convert_to_gray8 */

fas_error_type private_convert_to_gray8 (fas_context_ref_type ctx)
{
  if (ctx->gray8_already_converted)
    return FAS_SUCCESS;

  if (ctx->gray8_buffer == 0)
    {
      int numBytes = avpicture_get_size(PIX_FMT_GRAY8, ctx->codec_context->width,
					ctx->codec_context->height);
      ctx->gray8_buffer = (uint8_t *) av_malloc(numBytes*sizeof(uint8_t));
      avpicture_fill((AVPicture *) ctx->gray8_frame_buffer, ctx->gray8_buffer, PIX_FMT_GRAY8,
		     ctx->codec_context->width, ctx->codec_context->height);
    }

  if (img_convert((AVPicture *) ctx->gray8_frame_buffer, PIX_FMT_GRAY8, (AVPicture *) ctx->frame_buffer, 
		  ctx->codec_context->pix_fmt,
		  ctx->codec_context->width, ctx->codec_context->height) < 0)
    private_show_error("error converting to gray8", FAS_DECODING_ERROR);

  ctx->gray8_already_converted = FAS_TRUE;

  return FAS_SUCCESS;
}

int fas_get_current_width(fas_context_ref_type context)
{
  return context->codec_context->width;
}

int fas_get_current_height(fas_context_ref_type context)
{
  return context->codec_context->height;
}

unsigned long long fas_get_frame_duration(fas_context_ref_type context)
{
    if (context->format_context->streams[context->stream_idx]->time_base.den != context->format_context->streams[context->stream_idx]->r_frame_rate.num 
		|| context->format_context->streams[context->stream_idx]->time_base.num != context->format_context->streams[context->stream_idx]->r_frame_rate.den)
	{
		double frac = (double)(context->format_context->streams[context->stream_idx]->r_frame_rate.den) / (double)(context->format_context->streams[context->stream_idx]->r_frame_rate.num);
		return (unsigned long long)(frac*10000000.);
	}
	else
	{
		return (unsigned long long)(((double)(context->format_context->streams[context->stream_idx]->time_base.num))
				/((double)(context->format_context->streams[context->stream_idx]->time_base.den))*10000000.);
	}
}

fas_error_type fas_fill_gray8_ptr(fas_context_ref_type context, unsigned char *y)
{
  /* this conversion also seems to screw up sometimes -- pal8 -> gray8? legodragon.avi */
  if (private_convert_to_gray8(context) != FAS_SUCCESS)
    return FAS_FAILURE;

  int width = context->codec_context->width;
  int height = context->codec_context->height;
  int i;
  for (i=0;i < height; i++)
    memcpy(y + width * i, context->gray8_frame_buffer->data[0] + context->gray8_frame_buffer->linesize[0] * i, width);
  
  return FAS_SUCCESS;
}

fas_error_type  fas_fill_420p_ptrs (fas_context_ref_type context, unsigned char *y, unsigned char *u, unsigned char *v)
{
  AVFrame *p = context->frame_buffer;
  
  /* 411p to 420p conversion fails!? ... so i left this -ldb */
  if (context->codec_context->pix_fmt != PIX_FMT_YUV420P)
    return FAS_FAILURE;

  int width = context->codec_context->width;
  int height = context->codec_context->height;
  int i;
  for (i=0;i < height / 2; i++)
    {
      memcpy(y + width * (2*i)    , p->data[0] + p->linesize[0] * (2*i)    , width);
      memcpy(y + width * (2*i + 1), p->data[0] + p->linesize[0] * (2*i + 1), width);
      memcpy(u + width / 2 * i, p->data[1] + p->linesize[1] * i, width / 2);
      memcpy(v + width / 2 * i, p->data[2] + p->linesize[2] * i, width / 2);
    }
  
  return FAS_SUCCESS;
}
