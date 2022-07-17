/*
 *  OpenSlide, a library for reading whole slide image files
 *
 *  Copyright (c) 2007-2015 Carnegie Mellon University
 *  Copyright (c) 2015 Benjamin Gilbert
 *  All rights reserved.
 *
 *  OpenSlide is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation, version 2.1.
 *
 *  OpenSlide is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with OpenSlide. If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include <config.h>

#define NO_POISON_FSEEKO
#include "openslide-private.h"

#include <stdio.h>
#include <string.h>
#include <glib.h>

#ifdef HAVE_FCNTL
#include <unistd.h>
#include <fcntl.h>
#endif

struct _openslide_file {
  FILE *fp;
};

#undef fopen
#undef fread
#undef fclose
#undef g_file_test

static FILE *do_fopen(const char *path, const char *mode, GError **err) {
  FILE *f;

#ifdef HAVE__WFOPEN
  wchar_t *path16 = (wchar_t *) g_utf8_to_utf16(path, -1, NULL, NULL, err);
  if (path16 == NULL) {
    g_prefix_error(err, "Couldn't open %s: ", path);
    return NULL;
  }
  wchar_t *mode16 = (wchar_t *) g_utf8_to_utf16(mode, -1, NULL, NULL, err);
  if (mode16 == NULL) {
    g_prefix_error(err, "Bad file mode %s: ", mode);
    g_free(path16);
    return NULL;
  }
  f = _wfopen(path16, mode16);
  if (f == NULL) {
    _openslide_io_error(err, "Couldn't open %s", path);
  }
  g_free(mode16);
  g_free(path16);
#else
  f = fopen(path, mode);
  if (f == NULL) {
    _openslide_io_error(err, "Couldn't open %s", path);
  }
#endif

  return f;
}

struct _openslide_file *_openslide_fopen(const char *path, GError **err)
{
  FILE *f = do_fopen(path, "rb" FOPEN_CLOEXEC_FLAG, err);
  if (f == NULL) {
    return NULL;
  }

  /* Unnecessary if FOPEN_CLOEXEC_FLAG is non-empty.  Not built on Windows. */
#ifdef HAVE_FCNTL
  if (!FOPEN_CLOEXEC_FLAG[0]) {
    int fd = fileno(f);
    if (fd == -1) {
      _openslide_io_error(err, "Couldn't fileno() %s", path);
      fclose(f);
      return NULL;
    }
    long flags = fcntl(fd, F_GETFD);
    if (flags == -1) {
      _openslide_io_error(err, "Couldn't F_GETFD %s", path);
      fclose(f);
      return NULL;
    }
    if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC)) {
      _openslide_io_error(err, "Couldn't F_SETFD %s", path);
      fclose(f);
      return NULL;
    }
  }
#endif

  struct _openslide_file *file = g_slice_new0(struct _openslide_file);
  file->fp = f;
  return file;
}

size_t _openslide_fread(struct _openslide_file *file, void *buf, size_t size) {
  char *bufp = buf;
  size_t total = 0;
  while (total < size) {
    size_t count = fread(bufp + total, 1, size - total, file->fp);
    if (count == 0) {
      return total;
    }
    total += count;
  }
  return total;
}

int _openslide_fseek(struct _openslide_file *file, off_t offset, int whence) {
  return fseeko(file->fp, offset, whence);
}

off_t _openslide_ftell(struct _openslide_file *file) {
  return ftello(file->fp);
}

void _openslide_fclose(struct _openslide_file *file) {
  fclose(file->fp);
  g_slice_free(struct _openslide_file, file);
}

bool _openslide_fexists(const char *path) {
  return g_file_test(path, G_FILE_TEST_EXISTS);
}
