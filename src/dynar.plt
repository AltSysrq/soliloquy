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

static inline void dynar_contract_by_HUNG(dynar_HUNG this,
                                          size_t amt) {
  this->len -= amt;
  memset(this->v + this->len, 0, sizeof(CTYPE) * amt);
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

static inline void dynar_ins_HUNG(dynar_HUNG this, size_t off,
                                  const CTYPE* value, size_t cnt) {
  dynar_expand_by_HUNG(this, cnt);
  memmove(this->v + off + cnt, this->v + off,
          (this->len - off - cnt)*sizeof(CTYPE));
  memcpy(this->v + off, value, cnt*sizeof(CTYPE));
}

static inline void dynar_erase_HUNG(dynar_HUNG this, size_t off, size_t cnt) {
  memmove(this->v + off, this->v + off + cnt,
          (this->len - off - cnt)*sizeof(CTYPE));
  dynar_contract_by_HUNG(this, cnt);
}

#endif /* HAVE_DEFINED_DYNAR_HUNG */
