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

/// MACRO UTILS
/// (substitution and gluing together)
#define _GLUE(a,b)  __GLUE(a,b)
#define __GLUE(a,b) ___GLUE(a##b)
#define ___GLUE(a)  ____GLUE(a)
#define ____GLUE(a) a

#define ATSTART __attribute__((constructor))

/// MEMORY

static inline void* gcalloc(size_t size) {
  void* ret = GC_ALLOC(size);
  if (!ret) {
    fprintf(stderr, "Out of memory");
    exit(255);
  }
  return ret;
}

#define new(type) gcalloc(sizeof(type))
#define newdup(pointer) \
  (memcpy(new(sizeof(*pointer)), pointer, sizeof(*pointer)))

/// GENERIC TYPES

typedef enum {
  Syval_String = 0,
  Syval_Sylist = 1,
  Syval_Sytab  = 2,
  Syval_Int    = 3,
  Syval_Symbol = 4,
  Syval_Dynfun = 5,
  Syval_Opaque = 6
} syval_type;

typedef const char* string;
typedef char* mstring;
struct sytab;
typedef struct sytab* sytab;
typedef sytab* symbol;
typedef sytab (*dynfun)(sytab);
struct cons_cell;
typedef struct cons_cell* sylist;
typedef void* opaque;

typedef struct {
  syval_type type;
  union {
    string as_str;
    sylist as_list;
    sytab as_table;
    int as_int;
    symbol as_symbol;
    dynfun as_dynfun;
    opaque as_opaque;
  } value;
} syval;

/// LISTS

struct cons_cell {
  syval car;
  sylist cdr;
};

inline sylist cons(syval car, sylist cdr) {
  struct cons_cell cell = { car, cdr };
  return newdup(&cell);
}

sylist sylist_create(sylist, ...);

#define LC(...) (sylist_create(NULL, __VA_ARGS__, NULL))
#define LP(head,...) (sylist_create((head), __VA_ARGS__, NULL))
//Usage: L$(sylist, type var, ...)
//type is one of: bool int signed unsigned string sylist sytab dynfun
//opaque(XXX); in the last case, XXX denotes the actual type of the variable.
//If var ends with a question mark, a bool named var_p will also be declared,
//which will indicate whether var has any meaningful value.
//
//Optionally, the last element may be a simple varible name, which will hold
//the tail of the sylist, if destructuring got that far.
#define L$(...) _GLUE(LIST_DESTRUCTURING, __LINE__)

/// TABLES

sytab sytab_new(void);
sytab sytab_create(symbol, syval, ...);
const syval* sytab_get(sytab, symbol);
const syval* sytab_iget(unsigned long, symbol);
void sytab_put(sytab, symbol, syval);
void sytab_iput(unsigned long, symbol, syval);
void sytab_putl(sytab, ...);
void sytab_rem(sytab, symbol);
void sytab_rem(sytab, unsigned long);
void sytab_merge(sytab, sytab);
sytab sytab_clone(sytab);

#define TC(...) (sytab_create(__VA_ARGS__, NULL))
#define TP(init,...) (sytab_putl(init, __VA_ARGS__, NULL))
//Usage: T$(sytab, type $symbol var, ...)
//type is one of: bool int signed unsigned string sylist sytab dynfun
//opaque(XXX); in the last case, XXX denotes the actual type of the variable.
//If var ends with a question mark, a bool named var_p will also be declared,
//which will indicate whether var has any meaningful value. var may also be
//omitted, in which case the name of the symbol is used for the variable, sans
//the dollar. The question syntax can still be used in this case.
#define T$(...) _GLUE(TABLE_DESTRUCTURING, __LINE__)

/// DYNAMIC FUNCTIONS

#define hfun(name) TODO
//Usage: defun(name, (destructuring)) { body }
//destructuring is the same as for T$. Additionally, a lone variable name will
//hold the raw arguments sytab.
#define defun(...) _GLUE(DEFINE_DYNAMIC_FUNCTION, __LINE__)
#define defadvice(...) _GLUE(DEFINE_ADVICE, __LINE__)

#endif /* COMMON_H_ */
