`#'ifndef HAVE_DEFINED_DYNAR_`'HUNG
`#'define HAVE_DEFINED_DYNAR_`'HUNG

typedef struct dynar_`'HUNG {
  size_t `len', `size';
  typeof(CTYPE)* v;
}* dynar_`'HUNG;

static inline typeof(CTYPE) dynar_push_`'HUNG`'(dynar_`'HUNG this,
                                                typeof(CTYPE) val) {
  if (this->`len' == this->`size') {
    this->v = gcrealloc(this->v, this->`size');
    this->`size' *= 2;
  };
  this->v[this->`len'++] = val;
  return val;
}

static inline typeof(CTYPE) dynar_pop_`'HUNG`'(dynar_`'HUNG this) {
  typeof(CTYPE) val = this->v[--this->`len'];
  memset(this->v[this->`len'], 0, sizeof(CTYPE));
  return val;
}

static inline dynar_`'HUNG dynar_new_`'HUNG`'(void) {
  dynar_`'HUNG this = new(struct dynar_`'HUNG);
  this->size = 4;
  this->v = gcalloc(this->size * sizeof(CTYPE));
  return this;
}

static inline dynar_`'HUNG dynar_clone_`'HUNG`'(dynar_`'HUNG that) {
  dynar_`'HUNG this = newdup(that);
  this->v = gcalloc(this->`len' * sizeof(CTYPE));
  memcpy(this->v, that->v, this->`len' * sizeof(CTYPE));
  return this;
}

static inline typeof(CTYPE) dynar_top_`'HUNG`'(dynar_`'HUNG this) {
  return this->v[this->`len'-1];
}

`#'endif /* HAVE_DEFINED_DYNAR_`'HUNG */
