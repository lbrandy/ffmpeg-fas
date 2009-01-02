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

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include <stdio.h>
#include <string.h>
#include "seek_indices.h"

/* This executable is used for creating experimental seek tables.
   The show_seek_table executable will show the seek-table as ffmpeg_fas
   currently creates them.
 */

int main (int argc, char **argv)
{
  av_log_level = AV_LOG_QUIET;

  if (argc < 2) {
    fprintf (stderr, "usage: %s <video_file>\n", argv[0]);
    return -1;
  }

  av_register_all();

  AVFormatContext *pFormatCtx;
  if (av_open_input_file(&pFormatCtx, argv[1], NULL, 0, NULL) !=0)
    return -1;

  if (av_find_stream_info(pFormatCtx)<0)
    return -1;
   
  unsigned int stream_id = -1;
  unsigned int i;
  
  for (i = 0; i < pFormatCtx->nb_streams; i++)
    if (pFormatCtx->streams[i]->codec->codec_type == CODEC_TYPE_VIDEO)
      {
	stream_id = i;
	break;
      }

  if (stream_id == -1)
    return -1;

  AVCodecContext *pCodecCtx = pFormatCtx->streams[stream_id]->codec;
  AVCodec *pCodec;
  pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
  if (pCodec==NULL)
    return -1;

  if (avcodec_open(pCodecCtx, pCodec)<0)
    return -1;


  //    printf("\n%s \n",argv[1]);
  
  AVPacket Packet;
  int count = 0; 
  int key_packets = 0;
  int non_key_packets = 0;

  int frame_count = 0;
  int key_frames = 0;
  int non_key_frames = 0;

  AVFrame *pFrame;
  pFrame=avcodec_alloc_frame();
  int frameFinished;

  seek_table_type table;
  table = seek_init_table (16);
  seek_entry_type entry;
  
  int64_t key_packet_dts;
  int64_t prev_packet_dts = AV_NOPTS_VALUE;
  int64_t first_packet_dts;  /* ensure first keyframe gets first packet */

  int is_first_packet = 1;
  int frames_have_label = 1;

  //  const char *format_name = pFormatCtx->iformat->name;
  //  const char *codec_name = pFormatCtx->streams[stream_id]->codec->codec->name;

  
  /* these avi formats do not have labeled keyframes (the packets are labeled only and the packets and keyframe align 1-to-1 */
  /* DISABLING THIS TYPE OF GENERATION (these videos will be unseekable) */
  //  fprintf(stderr, "format: (%s) codec: (%s)\n", format_name, codec_name);
/* 
 *   if (!strcmp(format_name, "avi"))
 *     if (!strcmp(codec_name, "aasc")        ||
 * 	!strcmp(codec_name, "camtasia")    ||
 * 	!strcmp(codec_name, "cinepak")     ||
 * 	!strcmp(codec_name, "cyuv")        ||
 * 	!strcmp(codec_name, "huffyuv")     ||
 * 	!strcmp(codec_name, "indeo2")      ||
 * 	!strcmp(codec_name, "indeo3")      ||
 * 	!strcmp(codec_name, "msrle")       ||
 * 	!strcmp(codec_name, "msvideo1")    ||
 * 	!strcmp(codec_name, "mszh")        ||
 * 	!strcmp(codec_name, "qpeg")        ||
 * 	!strcmp(codec_name, "truemotion1") ||
 * 	!strcmp(codec_name, "ultimotion")  ||
 * 	!strcmp(codec_name, "vp3")         ||
 * 	!strcmp(codec_name, "zlib"))
 *       frames_have_label = 0;
 */

  while (av_read_frame(pFormatCtx, &Packet) >= 0)
    {      
      if (Packet.stream_index == stream_id)
	{	  
	  //	  fprintf(stderr, "Packet: (P%d: %lld %lld %d)\n", count, Packet.pts, Packet.dts, Packet.flags);
	  if ((Packet.flags & PKT_FLAG_KEY) || is_first_packet )
	    {	      		
	      /* when keyframes overlap in the stream, that means multiple packets labeled 'keyframe' will arrive before 
		 the keyframe itself. this results in wrong assignments all around, but only the first one needs to be right.
		 for all the rest, the seek-code will rewind to the previous keyframe to get them right.
	       */
	      if (is_first_packet)
		{    
		  first_packet_dts = Packet.dts;
		  is_first_packet = 0;
		}

	      //	      fprintf(stderr, "Packet: (P%d: %lld %lld)\n", count, Packet.pts, Packet.dts);

	      /* first keyframe gets own dts, others get previous packet's dts.. this is workaround for some mpegs    */
	      /* sometimes you need the previous packet from the supposed keyframe packet to get a frame back that is */
	      /* actually a keyframe */

	      if (prev_packet_dts == AV_NOPTS_VALUE)
		key_packet_dts = Packet.dts;
	      else
		key_packet_dts = prev_packet_dts;

	      if (Packet.flags & PKT_FLAG_KEY)
		key_packets++;
	      else
		non_key_packets++;
	    }
	  else
	    non_key_packets++;


	  avcodec_decode_video(pCodecCtx, pFrame, &frameFinished, Packet.data, Packet.size);

	  if (frameFinished)
	    {

	      //	      fprintf(stderr, "Frame : (P%d F%d: %lld %lld L:%d)\n", count, frame_count, Packet.pts, Packet.dts, pFrame->key_frame);
	      if ((pFrame->key_frame && frames_have_label) || ((Packet.flags & PKT_FLAG_KEY) && !frames_have_label))
		{
		  key_frames++;
		  
		  entry.display_index = frame_count;
		  entry.first_packet_dts = key_packet_dts;
		  entry.last_packet_dts = Packet.dts;

		  /* ensure first keyframe gets first packet dts */
		  if (frame_count == 0)
		    entry.first_packet_dts = first_packet_dts;

		  seek_append_table_entry(&table, entry);

		  //		  fprintf(stderr, "Frame : (P%d F%d: %lld %lld)\n", count, frame_count, Packet.pts, Packet.dts);
		}
	      else
		non_key_frames++;
	      frame_count++;
	    }
	  
	  count++;			  
	  prev_packet_dts = Packet.dts;
	}

      av_free_packet(&Packet);
    }

  //  printf("\n");

  fprintf (stderr, "Packets: key: %d  nonkey: %d   total: %d\n", key_packets, non_key_packets, count);
  fprintf (stderr, "Frames : key: %d  nonkey: %d   total: %d\n", key_frames, non_key_frames, frame_count);

  table.completed = seek_true;
  table.num_frames = frame_count;

  seek_show_raw_table(stdout, table);

  return 1;
}
