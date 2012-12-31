/*
  Copyright ⓒ 2012 Jason Lingle

  This file is part of Soliloquy.

  Soliloquy is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Soliloquy is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Soliloquy.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef COMMON_H_
#define COMMON_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gc.h>

/**
 * Defines a function with the provided name which is run when the program
 * starts, with the given priority, where heigher priority functions are
 * exectued first.
 */
#define ATSTART(name,priority) \
  static void name(void) __attribute__((constructor(priority)))

/// MEMORY
/**
 * Allocates memory of the given size, which will be automatically
 * garbage-collected when it is no longer needed.
 *
 * The memory returned is zero-initialised.
 *
 * If allocation fails, the program aborts.
 */
static inline void* gcalloc(size_t size) __attribute__((malloc)) {
  void* ret = GC_ALLOC(size);
  if (!ret) {
    fprintf(stderr, "Out of memory");
    exit(255);
  }
  return ret;
}

/**
 * Reällocates the given pointer to be of the requested new size, returning the
 * result. The behaviour is the same as realloc(), except that the memory will
 * be reclaimed automatically. The given pointer must have been retrieved from
 * gcalloc() or gcrealloc().
 *
 * If allocation fails, the program aborts.
 */
static inline void* gcrealloc(void* ptr, size_t size) {
  void* ret = GC_REALLOC(ptr, size);
  if (!ret) {
    fprintf(stderr, "Out of memory");
    exit(255);
  }
  return ret;
}

/**
 * Allocates a new instance of the given type.
 */
#define new(type) gcalloc(sizeof(type))
/**
 * Clones the given pointer, assuming its size is fixed and derivable from
 * sizeof(*pointer).
 */
#define newdup(pointer) \
  (memcpy(new(sizeof(*pointer)), pointer, sizeof(*pointer)))

/**
 * Like strdup(), but uses gcalloc().
 */
const char* gcstrdup(const char*) __attribute__((malloc));

/// STANDARD TYPES
typedef const char* string;
typedef void (*hook_function)(void);
struct object_t;
typedef struct object_t* object;

#endif /* COMMON_H_ */
