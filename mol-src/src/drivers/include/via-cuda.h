/*
 *   via-cuda.h
 *   copyright (c)1999 Ben Martz
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2
 *   of the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#ifndef __H_MOL_VIA_CUDA__
#define __H_MOL_VIA_CUDA__

/* Exported to adb.c */
extern void adb_service_request(void);
extern void via_cuda_set_reply_buffer(char *buf, int len);
extern void via_cuda_reply(int len, ...);
extern int cuda_autopolls(void);

#endif /* __H_MOL_VIA_CUDA__ */
