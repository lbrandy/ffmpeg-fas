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

#ifndef FAS_PRIVATE_ERROR_H
#define FAS_PRIVATE_ERROR_H

#if defined( _WIN32 ) && defined( STATIC_DLL )
static int  SHOW_ERROR_MESSAGES;
static int  SHOW_WARNING_MESSAGES;
#else
int  SHOW_ERROR_MESSAGES;
int  SHOW_WARNING_MESSAGES;
#endif /* _WIN32 && STATIC_DLL */
#endif
