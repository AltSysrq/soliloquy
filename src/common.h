/*
  Copyright ⓒ 2012, 2013 Jason Lingle

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
#include <wchar.h>
#include <gc.h>

#include "qstring.h"

#define SYMBOL_CONSTRUCTION_PRIORITY 108
#define DOMAIN_CONSTRUCTION_PRIORITY 116
#define ROOT_OBJECT_EVISCERATION_PRIORITY 132
#define SYMBOL_ROOT_IMPLANTATION_PRIORITY 133
#define ADVICE_INSTALLATION_PRIORITY 164
#define STATIC_INITIALISATION_PRIORITY 228

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
  ATSTART(_GLUE(name##$static_init, __LINE__),         \
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
#define ATSTARTA(name,priority,linkage,attr)                            \
  linkage void name(void) __attribute__((constructor(priority))) attr;  \
  linkage void name(void)

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
  (memcpy(new(typeof(*pointer)), pointer, sizeof(typeof(*pointer))))

/**
 * Like strdup(), but uses gcalloc().
 */
const char* gcstrdup(const char*) __attribute__((malloc));

/// STANDARD TYPES
typedef char* mstring;
typedef const char* string;
typedef wchar_t* mwstring;
typedef const wchar_t* wstring;
typedef qchar* mqstring;
typedef const qchar* qstring;
typedef void (*hook_function)(void);
struct object_t;
typedef struct object_t* object;
struct undefined_struct;
typedef struct undefined_struct* identity;
struct hook_point;

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
  do {                                                                  \
    object _wc_object = (obj);                                          \
    if (_wc_object)                                                     \
      object_eviscerate(_wc_object);                                    \
    body;                                                               \
    if (_wc_object)                                                     \
      object_reembowel();                                               \
  } while(0)

/**
 * Implants the given symbol, or domain of symbols, into the current context.
 *
 * The effect of using this macro to implant something which is not a symbol is
 * undefined.
 */
#define implant(sym) \
  object_implant(&_GLUE(sym,$base), _GLUE(sym, $implantation_type))

/**
 * Returns the value of _sym_ within the context of _obj_ without needing to go
 * through the trouble of eviscerating _obj_. If _sym_ is not implanted in
 * _obj_, its parents are searched; failing that, the value in the current
 * context is used.
 */
#define $(obj,sym) ({                                                   \
  typeof(sym) _GLUE(_ret_, __LINE__);                                   \
  object_get_implanted_value(&_GLUE(_ret_, __LINE__), obj,              \
                             &_GLUE(sym,$base));                        \
  _GLUE(_ret_, __LINE__);})

typedef enum hook_constraint {
  HookConstraintNone = 0,
  HookConstraintBefore,
  HookConstraintAfter
} hook_constraint;

/**
 * Function pointer type to examine a pair of hook IDs and their classes and
 * indicate whether there is any ordering relationship between them, from the
 * perspective of this (ie, returning HookConstraintBefore indicates to run the
 * function identified by this_id before that identified by that_id).
 *
 * Given two hook constraint functions F and G, the following must hold:
 *   F(a,b,c,d) == None || G(c,d,a,b) == None ||
 *     F(a,b,c,d) != G(c,d,a,b)
 * That is, either must return that they don't care about ordering, or they
 * must indicate a consistent order with respect to each other.
 *
 * A NULL pointer of this type is considered equivalent to always returning
 * HookConstraintNone.
 */
typedef hook_constraint (*hook_constraint_function)(
  identity this_id, identity this_class,
  identity that_id, identity that_class
);

/**
 * Adds the given function to the given hook if it is not already present.
 *
 * Priority must be one of HOOK_BEFORE, HOOK_MAIN, and HOOK_AFTER, and
 * indicates which phase of the hook this function belongs to.
 *
 * id is used to identify this hook, and should be a global identity
 * symbol (eg, $$u_foo). class is used to classify the purpose of this
 * function, and should also be a global identity symbol.
 *
 * fun is the function to call when the hook is invoked.
 *
 * If constraints is non-NULL, it is consulted for each other function at the
 * same priority level to determine the order in which the hooked functions
 * must be run. This function may be consulted later when more functions are
 * hooked. The effects of circular constraints are undefined.
 */
void add_hook(struct hook_point*, unsigned priority,
              identity id, identity class,
              void (*fun)(void), hook_constraint_function);

/**
 * Like add_hook, but with an extra object argument. The hook function (but not
 * the constraint function) will be evaluated within the context of that object.
 */
void add_hook_obj(struct hook_point*, unsigned priority,
                  identity id, identity class,
                  void (*fun)(void), object,
                  hook_constraint_function);

/**
 * Deletes the given hook of the given priority from the given hook point, if
 * such a hook exists. Does nothing otherwise.
 *
 * Two hooks are the same if they have the same priority, identity, and
 * context.
 *
 * A hook added by add_hook has a NULL context.
 */
void del_hook(struct hook_point*, unsigned priority, identity, object context);

#define _ADVISE(hook,priority)                       \
  static void _GLUE(advice,__LINE__)(void);          \
  ATSTART(ANONYMOUS, ADVICE_INSTALLATION_PRIORITY) { \
    add_hook(&hook, priority,                        \
             (identity)_GLUE(advice,__LINE__),       \
             NULL, _GLUE(advice,__LINE__), NULL);    \
  }                                                  \
  static void _GLUE(advice,__LINE__)(void)

/**
 * Usage:
 *   advise($h_some_hook) { (* body *) }
 *
 * Binds an anonymous function to the named hook point. The ID of the hook is
 * the address of the function, and its class is NULL. When you use this,
 * ensure that your hook is self-contained so that noone would need to care
 * about ordering relationships with your hook, since it is impossible to refer
 * to it by id.
 */
#define advise(hook) _ADVISE(hook, HOOK_MAIN)
/** Like advise, but runs at BEFORE priority. */
#define advise_before(hook) _ADVISE(hook, HOOK_BEFORE)
/** Like advise, but runs at AFTER priority. */
#define advise_after(hook) _ADVISE(hook, HOOK_AFTER)

/**
 * Usage:
 *   defun($h_some_hook) { (* body *) }
 *
 * Binds an anonymous function to the given MAIN part of the given hook point,
 * with the id and class $u_main. Ie, it defines the primary implementation of
 * the given function.
 */
#define defun(hook)                                  \
  static void _GLUE(hook,$main)(void);               \
  ATSTART(ANONYMOUS, ADVICE_INSTALLATION_PRIORITY) { \
    add_hook(&hook, HOOK_MAIN,                       \
             $u_main, $u_main, _GLUE(hook,$main),    \
             NULL);                                  \
  }                                                  \
  static void _GLUE(hook,$main)(void)

/* While the below macro will cause add_symbol_to_domain to be run multiple
 * times for the same symbol if multiple compilation units use it (which will
 * happen, for example, with implicit domain membership of public symbols), it
 * won't be a problem since the extraa calls have no effect.
 */
/**
 * Adds the given symbol to the given symbol domain during static
 * initialisation.
 */
#define member_of_domain(sym,dom)                               \
  ATSTART(ANONYMOUS, DOMAIN_CONSTRUCTION_PRIORITY) {            \
    add_symbol_to_domain(&_GLUE(sym,$base),&dom,                \
                         _GLUE(sym,$implantation_type));        \
  }

/**
 * Declares that _child_ is a subclass of _parent_. This both makes _child_'s
 * domain a subdomain of _parent_'s, and also arranges to run the parent's
 * constructor BEFORE the child's. Multiple inheritance is supported, though
 * odd things could occur if there are symbol conflicts.
 *
 * Invoking this multiple times with the same pair of classes, in the same
 * compilation unit or different ones, has no additional effect.
 */
#define subclass(parent,child)                                  \
  member_of_domain(_GLUE(parent,$domain),_GLUE(child,$domain))  \
  static void _GLUE(ANONYMOUS,_supercon)(void) {                \
    _GLUE(parent,$this) = _GLUE(child,$this);                   \
    _GLUE(parent,$function)();                                  \
  }                                                             \
  ATSTART(_GLUE(ANONYMOUS,_sc), ADVICE_INSTALLATION_PRIORITY) { \
    add_hook(&_GLUE(child,$hook),                               \
             HOOK_BEFORE, _GLUE(parent,$identity),              \
             $u_superconstructor, _GLUE(ANONYMOUS,_supercon),   \
             NULL);                                             \
  }

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
  object context;
  hook_constraint_function constraints;
  identity id, class;

  struct hook_point_entry* next;
};

#define HOOK_BEFORE 0
#define HOOK_MAIN 1
#define HOOK_AFTER 2
struct hook_point {
  struct hook_point_entry* entries[3];
};

enum implantation_type { ImplantSingle, ImplantDomain };

struct symbol_domain {
  struct symbol_header* member;
  enum implantation_type implant_type;
  struct symbol_domain* next;
};

void object_eviscerate(object);
void object_reembowel(void);
void object_implant(struct symbol_header*, enum implantation_type);
void object_get_implanted_value(void* dst, object,
                                struct symbol_header* sym);

void invoke_hook(struct hook_point*);

void add_symbol_to_domain(struct symbol_header*, struct symbol_domain**,
                          enum implantation_type);

hook_constraint constraint_after_superconstructor(
  identity, identity, identity, identity);

#define ANONYMOUS_SLC _GLUE(_anon_slc_,__LINE__)
#endif /* COMMON_H_ */
