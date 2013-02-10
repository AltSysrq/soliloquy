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

static inline bool any_HUNG
(list_HUNG haystack, bool (*f)(CTYPE)) {
  while (haystack) {
    if (f(haystack->car))
      return true;
    haystack = haystack->cdr;
  }
  return false;
}

static inline bool every_HUNG
(list_HUNG haystack, bool (*f)(CTYPE)) {
  while (haystack) {
    if (!f(haystack->car))
      return false;
    haystack = haystack->cdr;
  }

  return true;
}

static inline void each_HUNG
(list_HUNG this, void (*f)(CTYPE)) {
  while (this) {
    f(this->car);
    this = this->cdr;
  }
}

static inline list_HUNG map_HUNG(list_HUNG this,
                                 CTYPE (*fun)(CTYPE)) {
  if (!this) return this;
  struct list_HUNG* curr = new(struct list_HUNG);
  list_HUNG ret = curr;
  while (this) {
    curr->car = fun(this->car);
    this = this->cdr;
    if (this)
      curr = (struct list_HUNG*)(curr->cdr = new(struct list_HUNG));
  }
  return ret;
}

static inline list_HUNG lrm_HUNG(list_HUNG this, CTYPE needle) {
  if (this == NULL) return NULL;
  if (this->car == needle)
    return this->cdr;

  return cons_HUNG(this->car, lrm_HUNG(this->cdr, needle));
}

static inline list_HUNG lrmrev_HUNG(list_HUNG this, CTYPE needle) {
  list_HUNG that = NULL;
  while (this) {
    if (this->car != needle) {
      that = cons_HUNG(this->car, that);
    }

    this = this->cdr;
  }

  return that;
}

static inline unsigned llen_HUNG(list_HUNG this) {
  unsigned len = 0;
  while (this) {
    ++len;
    this = this->cdr;
  }
  return len;
}

static inline list_HUNG lmget_HUNG(list_HUNG this, CTYPE key) {
  while (this && this->car != key)
    this = this->cdr->cdr;

  if (this)
    return this->cdr;
  else
    return NULL;
}

static inline list_HUNG lmdel_HUNG(list_HUNG this, CTYPE key) {
  if (this == NULL) return NULL;
  if (this->car == key)
    return this->cdr->cdr;

  return
    cons_HUNG(
      this->car,
      cons_HUNG(
        this->cdr->car,
        lmdel_HUNG(this->cdr->cdr, key)));
}

static inline list_HUNG lmput_HUNG(list_HUNG this, CTYPE key, CTYPE value) {
  if (lmget_HUNG(this, key))
    this = lmdel_HUNG(this, key);

  return cons_HUNG(key, cons_HUNG(value, this));
}

static inline list_HUNG lrev_HUNG(list_HUNG this) {
  list_HUNG that = NULL;
  while (this) {
    that = cons_HUNG(this->car, that);
    this = this->cdr;
  }

  return that;
}

#define lpush_HUNG(lst,value) ({                \
  list_HUNG* _lv = &(lst);                      \
  *_lv = cons_HUNG((value), *_lv);              \
})
#define lpop_HUNG(lst) ({                       \
  list_HUNG* _lv = &(lst);                      \
  list_HUNG _l = *_lv;                          \
  *_lv = (*_lv)->cdr;                           \
  _l->car;                                      \
})

#endif /* HAVE_DEFINED_LIST_HUNG */
