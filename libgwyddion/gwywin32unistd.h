/*
 *  @(#) $Id$
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#ifndef __GWY_WIN32_UNISTD_H__
#define __GWY_WIN32_UNISTD_H__

/* To be includable unconditionally, but do no harm on sane systems. */
#ifdef _WIN32

#include <io.h>
#include <direct.h>
#include <process.h>

#define chmod(file, mode)     _chmod(file, mode)
#define getpid()              _getpid()

#ifndef GWY_DISABLE_DEPRECATED
#define mkdir(dir, mode)      _mkdir(dir)
#define unlink(file)          _unlink(file)
#endif  /* GWY_DISABLE_DEPRECATED */

#ifndef S_ISREG
#define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
#endif
#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#endif

#ifndef S_IRUSR
#define S_IRUSR _S_IREAD
#endif
#ifndef S_IWUSR
#define S_IWUSR _S_IWRITE
#endif
#ifndef S_IXUSR
#define S_IXUSR _S_IEXEC
#endif

#ifndef _O_BINARY
#define _O_BINARY 0
#endif
#ifndef _O_TEMPORARY
#define _O_TEMPORARY 0
#endif

#ifndef W_OK
#define W_OK 2
#endif

#endif /* _WIN32 */

#endif /* __GWY_WIN32_UNISTD_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

