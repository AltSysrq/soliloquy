#ifndef HAVE_DEFINED_DYNAR_HUNG
#define HAVE_DEFINED_DYNAR_HUNG

typedef struct dynar_HUNG {
  size_t len, size;
  typeof(CTYPE)* v;
}* dynar_HUNG;

static inline typeof(CTYPE) dynar_push_HUNG(dynar_HUNG this,
                                            typeof(CTYPE) val) {
  if (this->len == this->size) {
    this->size *= 2;
    this->v = gcrealloc(this->v, this->size*sizeof(CTYPE));
  };
  this->v[this->len++] = val;
  return val;
}

static inline void dynar_expand_by_HUNG(dynar_HUNG this,
                                        size_t amt) {
  this->len += amt;
  if (this->size < this->len) {
    //Either double size, or expand to exactly fit, whichever is bigger
    this->size *= 2;
    if (this->size < this->len)
      this->size = this->len;

    this->v = gcrealloc(this->v, this->size * sizeof(CTYPE));
  }
}

static inline typeof(CTYPE) dynar_pop_HUNG(dynar_HUNG this) {
  typeof(CTYPE) val = this->v[--this->len];
  memset(&this->v[this->len], 0, sizeof(CTYPE));
  return val;
}

static inline dynar_HUNG dynar_new_HUNG(void) {
  dynar_HUNG this = new(struct dynar_HUNG);
  this->size = 4;
  this->v = gcalloc(this->size * sizeof(CTYPE));
  return this;
}

static inline dynar_HUNG dynar_clone_HUNG(dynar_HUNG that) {
  dynar_HUNG this = newdup(that);
  this->v = gcalloc(this->len * sizeof(CTYPE));
  memcpy(this->v, that->v, this->len * sizeof(CTYPE));
  return this;
}

static inline typeof(CTYPE) dynar_top_HUNG(dynar_HUNG this) {
  return this->v[this->len-1];
}

#endif /* HAVE_DEFINED_DYNAR_HUNG */
