`#'ifndef HAVE_DEFINED_LIST_`'HUNG
`#'define HAVE_DEFINED_LIST_`'HUNG

typedef struct list_`'HUNG {
  CTYPE car;
  const struct list_`'HUNG * cdr;
} const* list_`'HUNG;

static inline list_`'HUNG cons_`'HUNG
(CTYPE car, list_`'HUNG cdr) {
  struct list_`'HUNG* this = new(struct list_`'HUNG);
  this->car = car;
  this->cdr = cdr;
  return this;
}

static inline list_`'HUNG find_`'HUNG
(list_`'HUNG haystack, CTYPE needle) {
  while (haystack) {
    if (haystack->car == needle)
      return haystack;
    else
      haystack = haystack->cdr;
  }
  return NULL;
}

static inline list_`'HUNG find_where_`'HUNG
(list_`'HUNG haystack, bool (*test)(CTYPE)) {
  while (haystack) {
    if (test(haystack->car))
      return haystack;
    else
      haystack = haystack->cdr;
  }
  return NULL;
}

static inline void* fold_`'HUNG
(list_`'HUNG haystack, void* (*f)(CTYPE, void*)) {
  void* ret = NULL;
  while (haystack) {
    ret = f(haystack->car, ret);
    haystack = haystack->cdr;
  }
  return ret;
}

`#'endif /* HAVE_DEFINED_LIST_`'HUNG */
