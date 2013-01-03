#ifndef HAVE_DEFINED_LIST_HUNG
#define HAVE_DEFINED_LIST_HUNG

typedef struct list_HUNG {
  CTYPE car;
  const struct list_HUNG * cdr;
} const* list_HUNG;

static inline list_HUNG cons_HUNG
(CTYPE car, list_HUNG cdr) {
  struct list_HUNG* this = new(struct list_HUNG);
  this->car = car;
  this->cdr = cdr;
  return this;
}

static inline list_HUNG find_HUNG
(list_HUNG haystack, CTYPE needle) {
  while (haystack) {
    if (haystack->car == needle)
      return haystack;
    else
      haystack = haystack->cdr;
  }
  return NULL;
}

static inline list_HUNG find_where_HUNG
(list_HUNG haystack, bool (*test)(CTYPE)) {
  while (haystack) {
    if (test(haystack->car))
      return haystack;
    else
      haystack = haystack->cdr;
  }
  return NULL;
}

static inline void* fold_HUNG
(list_HUNG haystack, void* (*f)(CTYPE, void*)) {
  void* ret = NULL;
  while (haystack) {
    ret = f(haystack->car, ret);
    haystack = haystack->cdr;
  }
  return ret;
}

#endif /* HAVE_DEFINED_LIST_HUNG */
