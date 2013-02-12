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

#if !defined(DEBUG) && !defined(NDEBUG)
#define NDEBUG
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#ifdef HAVE_GC_GC_H
#include <gc/gc.h>
#else
#include <gc.h>
#endif

#include "qstring.h"

#define SYMBOL_CONSTRUCTION_PRIORITY 108
#define DOMAIN_CONSTRUCTION_PRIORITY 116
#define ROOT_OBJECT_EVISCERATION_PRIORITY 132
#define SYMBOL_ROOT_IMPLANTATION_PRIORITY 133
#define ADVICE_INSTALLATION_PRIORITY 164
#define STATIC_INITIALISATION_PRIORITY 228
#define TEST_EXECUTION_PRIORITY 356

#define __GLUE(x,y) x##y
#define _GLUE(x,y) __GLUE(x,y)

/**
 * Returns the length of an automatic array.
 */
#define lenof(x) (sizeof(x)/sizeof(x[0]))

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
 * Defines a function whose body is run at static initialisation time.
 */
#define ATSINIT ATSTART(ANONYMOUS, STATIC_INITIALISATION_PRIORITY)

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
 * Allocates the given number of wchar_ts with gcalloc().
 */
static inline wchar_t* wcalloc(size_t cnt) __attribute__((malloc));
static inline wchar_t* wcalloc(size_t cnt) {
  return gcalloc(cnt*sizeof(wchar_t));
}

/**
 * Allocates the given number of qchars with gcalloc().
 */
static inline qchar* qcalloc(size_t cnt) __attribute__((malloc));
static inline qchar* qcalloc(size_t cnt) {
  return gcalloc(cnt*sizeof(qchar));
}

#ifndef HAVE_WMEMCPY
static inline wchar_t* wmemcpy(wchar_t* dst, const wchar_t* src,
                               size_t n) {
  memcpy(dst, src, sizeof(wchar_t)*n);
  return dst;
}
#endif

#ifndef HAVE_WMEMMOVE
static inline wchar_t* wmemmove(wchar_t* dst, const wchar_t* src,
                                size_t n) {
  memmove(dst, src, sizeof(wchar_t)*n);
  return dst;
}
#endif

static inline qchar* qmemcpy(qchar* dst, const qchar* src, size_t n) {
  memcpy(dst, src, sizeof(qchar)*n);
  return dst;
}

static inline qchar* qmemmove(qchar* dst, const qchar* src, size_t n) {
  memmove(dst, src, sizeof(qchar)*n);
  return dst;
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

/**
 * Converts the given narrow string into a wide string, by allocating the wide
 * string with gcalloc().
 *
 * If an invalid encoding is encountered, the string is interpreted as
 * ISO8859-1 instead of using the current locale (ie, the narrow characters are
 * simply bitcast up to wchar_ts).
 */
mwstring cstrtowstr(string);

/**
 * Converts the given wide string into a narrow string, by allocating the
 * narrow string with gcalloc().
 *
 * If, for some reason, the given wstring is not encodable according to the
 * current locale, ISO-8859-1 will be used as fallback, with poorly-defined
 * results for characters outside of Latin-1.
 */
mstring wstrtocstr(wstring);

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
 * completes. The result of evaluating _body_ is returned.
 */
#define within_context(obj,body)                    \
  ({                                                \
    object _wc_object                               \
    __attribute__((cleanup(reembowel_if_not_null))) \
    = (obj);                                        \
    if (_wc_object)                                 \
      object_eviscerate(_wc_object);                \
    body;                                           \
  })

/**
 * $$(obj) { ... } works like within_context, but does not cause the entire
 * body to appear to be on the same line to the compiler. It is therefore
 * preferable to longer blocks. It is also safe to return or otherwise jump out
 * of this structure, unlike within_context. Unlike within_context, it is not
 * an expression.
 */
#define $$(obj)                                                    \
  for (object _GLUE(_$$_,__LINE__)                                 \
                __attribute__((cleanup(reembowel_if_not_null)))    \
                = (obj), _GLUE(_$$_ctl_,__LINE__) = NULL;          \
       control$$(_GLUE(_$$_,__LINE__), &_GLUE(_$$_ctl_,__LINE__)); \
    )

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
 * Like add_hook, but the hook is only executed if the bool is true when the
 * hook is considered for execution.
 */
void add_hook_cond(struct hook_point*, unsigned priority, const bool* when,
                   identity id, identity class,
                   void (*fun)(void),
                   hook_constraint_function);

/**
 * Combination of both add_hook_obj and add_hook_cond.
 */
void add_hook_obj_cond(struct hook_point*, unsigned priority, const bool* when,
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

#define _ADVISE_ID(id,hook,priority)                 \
  static void _GLUE(advice,__LINE__)(void);          \
  ATSTART(ANONYMOUS, ADVICE_INSTALLATION_PRIORITY) { \
    add_hook(&hook, priority,                        \
             id,                                     \
             NULL, _GLUE(advice,__LINE__), NULL);    \
  }                                                  \
  static void _GLUE(advice,__LINE__)(void)

/**
 * Like advise, but takes an explicit ID.
 */
#define advise_id(id,hook) _ADVISE_ID(id, hook, HOOK_MAIN)
/** Like advise_id, but runs at BEFORE priority. */
#define advise_id_before(id, hook) _ADVISE_ID(id, hook, HOOK_BEFORE)
/** Like advise_id, but runs at AFTER priority. */
#define advise_id_after(id, hook) _ADVISE_ID(id, hook, HOOK_AFTER)

/**
 * Begins definining a mode. A mode is a set of hooks which are active based on
 * a $y symbol. The $y symbol, upon construction of a class, is copied from the
 * default control symbol.
 *
 * defmode() must be used before any of the modeadv* macros, and can only be
 * used once per compilation unit. It defines static global variables named
 * mode_id and mode_control. It is legal for multiple compilation units to use
 * defmode() for the same parameters; in general, they all should be the same.
 *
 * Conventions:
 * - id is named $u_NAME
 * - control is named $y_CLASS_NAME
 * - control_default is named $y_CLASS_NAME_default
 *
 * @param on_class The $c symbol to hook onto to potentially activate the mode.
 * @param id A $u symbol identifying this mode.
 */
#define defmode(on_class, id, control, control_default) \
  static const identity mode_id = (identity)&id;        \
  static const bool* const mode_control = &control;     \
  advise_id_after(id, _GLUE(on_class,$hook))            \
  { control = control_default; }

#define _MODEADV(class,hook,priority)                   \
  static void _GLUE(modeadv,__LINE__)(void);            \
  ATSTART(ANONYMOUS, ADVICE_INSTALLATION_PRIORITY) {    \
    add_hook_cond(&hook, priority, mode_control,        \
                  mode_id, class,                       \
                  _GLUE(modeadv,__LINE__), NULL);       \
  }                                                     \
  static void _GLUE(modeadv,__LINE__)(void)

/**
 * Defines advice on the given hook. It will only be run when the control
 * variable specified in defmode() is true. class is the class of the hook, and
 * is unrelated to any $c symbol (eg, it might be something like
 * "$u_output_formatting").
 */
#define mode_adv(class, hook) _MODEADV(class, hook, HOOK_MAIN)
/** Like mode_adv, but runs at HOOK_BEFORE priority. */
#define mode_adv_before(class, hook) _MODEADV(class, hook, HOOK_BEFORE)
/** Like mode_adv, but runs at HOOK_AFTER priority. */
#define mode_adv_after(class, hook) _MODEADV(class, hook, HOOK_AFTER)

/**
 * Defines advice to run before a class's superconstructor.
 */
#define advise_before_superconstructor(hook)            \
  static void _GLUE(supercon,__LINE__)(void);           \
  ATSTART(ANONYMOUS, ADVICE_INSTALLATION_PRIORITY) {    \
    add_hook(&hook, HOOK_BEFORE,                        \
             (identity)_GLUE(supercon,__LINE__), NULL,  \
             _GLUE(supercon,__LINE__),                  \
             constraint_before_superconstructor);       \
  }                                                     \
  static void _GLUE(supercon,__LINE__)(void)

/**
 * Usage:
 *   defun($h_some_hook) { (* body *) }
 *
 * Binds an anonymous function to the given MAIN part of the given hook point,
 * with the id and class $u_main. Ie, it defines the primary implementation of
 * the given function.
 */
#define defun(hook)                                                     \
  static void _GLUE(hook,$main)(void);                                  \
  ATSTART(_GLUE(hook,$main_setup), ADVICE_INSTALLATION_PRIORITY) {      \
    add_hook(&hook, HOOK_MAIN,                                          \
             $u_main, $u_main, _GLUE(hook,$main),                       \
             NULL);                                                     \
  }                                                                     \
  static void _GLUE(hook,$main)(void)

/**
 * Usage: deftest(name) { (* body *) }
 *
 * If DEBUG is defined, runs the given test function after static
 * initialisation time, but before main runs. Otherwise, has no effect.
 */
#ifdef DEBUG
#define deftest(name)                                           \
  static  void test_##name(void)                                \
    __attribute__((constructor(TEST_EXECUTION_PRIORITY)));      \
  static void test_##name(void)
#else
// inline so that the function isn't even generated if not referenced (which it
// won't be)
#define deftest(name) \
  static inline void test_##name(void)
#endif

/**
 * Expands into an anonymous function which takes the given arguments and
 * returns the given expression. If you need multiple statements within expr,
 * enclose it in ({...}), and place the return value as the last statement
 * (without the "return" keyword).
 *
 * Note that, while this does form a closure, the function will be invalid once
 * the containing function returns.
 *
 * Example:
 *   find_where_i(list_of_integers, lambda((int i), i > 5))
 */
#define lambda(args, expr)                              \
  ({typeof(({STRIP_PARENS(args); expr;}))               \
  _GLUE(_lambda_,ANONYMOUS) args {                      \
      return expr;                                      \
    }                                                   \
    _GLUE(_lambda_,ANONYMOUS); })

/**
 * Like lambda, but the expression is prefixed with "(bool)!!" so that the
 * function returns a normalised boolean value.
 */
#define lambdab(args, expr) lambda(args, (bool)!!(expr))

/**
 * Like lambda, but the inner function returns void.
 *
 * Example:
 *   each_i(list_of_integers, lambdav((int i), printf("%d\n", i)));
 */
#define lambdav(args, expr)                     \
  ({void _GLUE(_lambdav_,ANONYMOUS) args {      \
      expr;                                     \
    }                                           \
    _GLUE(_lambdav_,ANONYMOUS); })

/**
 * Establishes a "dynamic" binding for the given variable, which should be
 * global. _var_ will take on the value of _expr_ until the containing scope
 * exits, at which point its former value will be restored.
 */
#define let(var,expr)                                                   \
  typeof(var) _GLUE(var,_GLUE($backup,__LINE__)) = (var);               \
  void _GLUE(var,_GLUE($restore,__LINE__))(char*c) {                    \
    var = _GLUE(var,_GLUE($backup,__LINE__));                           \
  }                                                                     \
  char _GLUE(var,_GLUE($trigger,__LINE__))                              \
  __attribute__((unused,cleanup(_GLUE(var,_GLUE($restore,__LINE__))))); \
  var = (expr);

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
    implant(_GLUE(parent,$this));                               \
    _GLUE(parent,$this) = _GLUE(child,$this);                   \
    _GLUE(parent,$function)();                                  \
  }                                                             \
  ATSTART(_GLUE(ANONYMOUS,_sc), ADVICE_INSTALLATION_PRIORITY) { \
    add_hook(&_GLUE(child,$hook),                               \
             HOOK_BEFORE, _GLUE(parent,$identity),              \
             $u_superconstructor, _GLUE(ANONYMOUS,_supercon),   \
             NULL);                                             \
  }

/**
 * Begins a transaction. A transaction functions as an error-handling structure
 * similar to exception handling, except that rollback is automatically
 * performed, so most code running within a transaction does not need to worry
 * about error handling. (This is in contrast to exceptions, where it is easy
 * to not worry about error handling, and even easier to leave the program in
 * an inconsistent state when something is thrown.)
 *
 * A transaction is associated with a rollback-exit function, the only
 * parameter to tx_start(). This function MUST NOT RETURN. It is called after
 * rollback completes.
 *
 * Make sure you commit, at latest, before exiting the context in which you
 * began the context. If rollback occurs outside of the context where the
 * transaction started, the program will explode.
 */
void tx_start(void (*)(void));
/**
 * Commits the current transaction. This may be called in any context; it
 * merely discards the information for the transaction. However, you generally
 * want to commit in the same context where you start the transaction for
 * reasons explained in the documentation for tx_start.
 */
void tx_commit(void);
/**
 * Rolls the current transaction back. This will invoke rollback handlers in
 * the reverse order they were pushed, then revert the state of the world to
 * what it was when the corresponding tx_start() was called, then exits to the
 * exit function given to tx_start.
 *
 * This does not return.
 */
void tx_rollback(void) __attribute__((noreturn));

/**
 * Convenience for setting $v_rollback_type to the given identity,
 * $s_rollback_reason to either strerror(errno) (if chk==-1) or otherwise (if
 * chk!=-1), then calling tx_rollback().
 */
void tx_rollback_merrno(identity, int chk, string otherwise)
__attribute__((noreturn));

/**
 * Like tx_rollback_merrno(), but always uses strerror(errno).
 */
static inline void tx_rollback_errno(identity id) __attribute__((noreturn));
static inline void tx_rollback_errno(identity id) {
  tx_rollback_merrno(id, -1, NULL);
}

/**
 * Pushes a rollback handler for the current transaction. If rollback occurs
 * while this is pushed, the given function will be called before rollback.
 *
 * Some precautions must be taken in the handler function. The context in which
 * it is called is undefined; if you need access to any symbols, you should
 * save references to the objects you need in local scope in conjunction with
 * nested functions.
 *
 * It is safe to push/pop new contexts during the execution of the handler.
 *
 * At the time the handler is called, it has already been popped.
 *
 * Normally, the handler should return. It MAY instead not return, though
 * typically you still need to continue rollback by calling tx_rollback() at
 * some point if you do so, since you have no idea what context you're running
 * in.
 */
void tx_push_handler(void (*)(void));

/**
 * Pops the handler pushed to the rollback handler stack for the current
 * transaction by the corresponding call to tx_push_handler().
 */
void tx_pop_handler(void);

/**
 * Propagates the current value of the given symbol within the current context
 * through all transactions.
 *
 * This should be used for values which reflect properties of the outside
 * world, such as details within files.
 */
#define tx_write_through(sym) tx_write_through_impl(&_GLUE(sym,$base))

/**
 * Aborts the currently-executing hook ponit. This function does not return,
 * nor does it gracefully unwind the stack. ONLY USE THIS IF YOU KNOW WHAT YOU
 * ARE DOING.
 * In particular:
 * - The current context, if any, will NOT be exited (ie, you can't call this
 *   within a $$ or within_context block).
 * - let() bindings will NOT revert.
 * - The hook that invokes this MUST NOT have a bound object.
 *
 * The effects of calling this in inappropriate circumstances are undefined.
 */
void hook_abort(void) __attribute__((noreturn));

/**
 * Invokes all hooks bound to the given hook point. The hook point is not
 * sensitive to concurrent modifications; it is copied before any hooks are
 * executed.
 *
 * If it is NULL, no action is taken.
 *
 * The execution of a hook can be (gracelessly) terminated by calling
 * hook_abort. There is almost never a good reason to do this.
 */
void invoke_hook(struct hook_point*);

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
  const bool* when;
  object context;
  hook_constraint_function constraints;
  identity id, class;

  struct hook_point_entry* next;
};

#define HOOK_BEFORE_EVERYTHING 0
#define HOOK_BEFORE 1
#define HOOK_MAIN 2
#define HOOK_AFTER 3
struct hook_point {
  struct hook_point_entry* entries[4];
};

enum implantation_type { ImplantSingle, ImplantDomain };

struct symbol_domain {
  struct symbol_header* member;
  enum implantation_type implant_type;
  struct symbol_domain* next;
};

void object_eviscerate(object);
void object_reembowel(void);
static inline void reembowel_if_not_null(object* that) {
  if (*that)
    object_reembowel();
}
void object_implant(struct symbol_header*, enum implantation_type);
void object_get_implanted_value(void* dst, object,
                                struct symbol_header* sym);

void add_symbol_to_domain(struct symbol_header*, struct symbol_domain**,
                          enum implantation_type);

hook_constraint constraint_after_superconstructor(
  identity, identity, identity, identity);
hook_constraint constraint_before_superconstructor(
  identity, identity, identity, identity);

static inline bool control$$(object obj, object* ctl) {
  if (!*ctl) {
    // *ctl is still NULL, this is the first time through the $$ "loop"
    if (obj)
      object_eviscerate(obj);
    // Point it at anything that isn't NULL
    *ctl = (object)ctl;
    return true;
  } else {
    // control$$ has run for this pair already, terminate loop
    return false;
  }
}

void tx_write_through_impl(struct symbol_header*);

#define SIZEALIGN(x) (((x)+sizeof(void*)-1)/sizeof(void*)*sizeof(void*))

#define _ID(...) __VA_ARGS__
#define STRIP_PARENS(x) _ID(_ID x)

#define ANONYMOUS_SLC _GLUE(_anon_slc_,__LINE__)
#endif /* COMMON_H_ */
