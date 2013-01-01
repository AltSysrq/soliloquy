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

#define SYMBOL_CONSTRUCTION_PRIORITY 108
#define DOMAIN_CONSTRUCTION_PRIORITY 116
#define STATIC_INITIALISATION_PRIORITY 132

#define __GLUE(x,y) x##y
#define _GLUE(x,y) __GLUE(x,y)

/**
 * Causes a global variable to be initialised before main() is run, but after
 * symbols have been constructed.
 *
 * Usage:
 *
 * qualifiers type STATIC_INIT(name, expression)
 */
#define STATIC_INIT(name, value)                \
  name; STATIC_INIT_TO(name, value)
#define STATIC_INIT_TO(name, value)                    \
  ATSTART(_GLUE(name##_init, __LINE__),                \
          STATIC_INITIALISATION_PRIORITY)              \
  { name = value; }

/**
 * Defines a function with the provided name which is run when the program
 * starts, with the given priority, where lower priority functions are
 * exectued first.
 */
#define ATSTART(name,priority) \
  static void name(void) __attribute__((constructor(priority))); \
  static void name(void)

/**
 * Results in a symbol unique to the given line of the given compilation
 * unit. This is not guaranteed to be unique across compilation units, so it
 * must be used only with local or static scope.
 */
#define ANONYMOUS _GLUE(_anon_,__LINE__)

/// MEMORY
/**
 * Allocates memory of the given size, which will be automatically
 * garbage-collected when it is no longer needed.
 *
 * The memory returned is zero-initialised.
 *
 * If allocation fails, the program aborts.
 */
static inline void* gcalloc(size_t) __attribute__((malloc));
static inline void* gcalloc(size_t size) {
  void* ret = GC_MALLOC(size);
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
struct undefined_struct;
typedef struct undefined_struct* identity;

/// OBJECTS/CONTEXTS
/**
 * Creates a new object with no implants. If a parent is specified, any
 * activations of the object will implicitly eviscerate the parent first.
 */
object object_new(object parent);
/**
 * Clones the given object, yielding an identical one with the same implants
 * and parent. This is a shallow copy.
 */
object object_clone(object);
/**
 * Returns the object which represents the current context.
 */
object object_current(void);

/**
 * Executes _body_ within the context of _obj_, restoring context when _body_
 * completes. This is not an expression; any return value of _body_ is
 * discarded.
 */
#define within_context(obj,body) \
  do { object_eviscerate(obj); body; object_reembowel(); } while(0)
/**
 * Implants the given symbol, or domain of symbols, into the current context.
 *
 * The effect of using this macro to implant something which is not a symbol is
 * undefined.
 */
#define implant(sym) \
  object_implant(&_GLUE(sym,_base), _GLUE(sym, _implantation_type))

/**
 * Returns the value of _sym_ within the context of _obj_ without needing to go
 * through the trouble of eviscerating _obj_. If _sym_ is not implanted in
 * _obj_, its value is taken from the nearest context above _obj_ in the
 * activation stack if _obj_ is activated, by searching _obj_'s parents if it
 * is not, and falling back to the current context if that fails as well.
 */
#define $(obj,sym) ({                                                   \
  typeof(sym) _GLUE(_ret_, __LINE__);                                   \
  object_get_implanted_value(_GLUE(_ret_, __LINE__), obj,               \
                             &_GLUE(sym,_base));                        \
  _GLUE(_ret_, __LINE__);})

///////////////////////////////////////////////////////////////////////////////
/// Mostly internal details below. You need not concern yourself with these.///
///////////////////////////////////////////////////////////////////////////////

struct symbol_owner_stack {
  object owner;
  unsigned offset;
  struct symbol_owner_stack* next;
};

/// All physical first-class symbols MUST begin with this structure.
/// Silc uses preprocessor macros to hide this detail.
struct symbol_header {
  unsigned size;
  struct symbol_owner_stack* owner_stack;
  // This is a pointer to the real contents of the symbol. This will actually
  // point to the first byte after this header and any following alignment
  // padding. (It is due to the padding that we need to use an extra pointer,
  // since there is no portable way to determine where the data would be, since
  // different symbol payload types may have differing alignment.)
  void* payload;
};

struct hook_point_entry {
  hook_function fun;
  struct hook_point_entry* next;
};

struct hook_point {
  struct hook_point_entry* entries[16];
};

enum implantation_type { ImplantSingle, ImplantDomain };

struct symbol_domain {
  struct symbol_header* member;
  enum implantation_type implant_type;
  struct symbol_domain* next;
};

void object_eviscerate(object);
void object_reembowel(void);
void object_implant(struct symbol_header*, \
                    enum implantation_type);
void object_get_implanted_value(void* dst, object,
                                struct symbol_header* sym);

#endif /* COMMON_H_ */
