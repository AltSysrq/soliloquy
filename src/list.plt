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

static inline void each_HUNG
(list_HUNG this, void (*f)(CTYPE)) {
  while (this) {
    f(this->car);
    this = this->cdr;
  }
}

static inline list_HUNG lrm_HUNG(list_HUNG this, CTYPE needle) {
  if (this == NULL) return NULL;
  if (this->car == needle)
    return this->cdr;

  return cons_HUNG(this->car, lrm_HUNG(this->cdr, needle));
}

static inline unsigned llen_HUNG(list_HUNG this) {
  unsigned len = 0;
  while (this) {
    ++len;
    this = this->cdr;
  }
  return len;
}

#endif /* HAVE_DEFINED_LIST_HUNG */
