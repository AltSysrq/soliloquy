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

/*
  TITLE: Common functions and Silc support
  OVERVIEW: This is mostly implemantation details. You probably just want to
    read the header instead.

  SYMBOL: $o_root
    The root object into which all symbols are implanted. Its main purpose is
    to hold the values objects get when there is no current object.

  SYMBOL: $u_superconstructor
    Identifies hooks which call a class's superclass's constructor.

  SYMBOL: $u_fundamental_construction
    Identifies the hook which performs a class's fundamental construction.

  SYMBOL: $u_main
    Identifies the "primary" implementation of a function on a hook point. This
    is typically used by defun(), though sometimes may be added by other hooks.
 */

#include "common.slc"

#include <assert.h>
#include <errno.h>
#include <setjmp.h>

// Formerly known as $$ao_evisceration_stack
// (This comment necessary for the dynar_o template)
static dynar_o evisceration_stack;

char* gcstrdup(const char* str) {
  size_t len = strlen(str)+1;
  char* dst = gcalloc(len);
  memcpy(dst, str, len);
  return dst;
}

mwstring cstrtowstr(string str) {
  size_t sz = mbstowcs(NULL, str, 0);
  if (!~sz) {
    // Invalid byte string, fall back to ISO-8859-1
    sz = strlen(str);
    mwstring ret = gcalloc((sz+1)*sizeof(wchar_t));
    for (unsigned i = 0; i < sz; ++i)
      ret[i] = ((wchar_t)str[i]) & (wchar_t)0xFF;
    return ret;
  } else {
    mwstring ret = gcalloc((sz+1)*sizeof(wchar_t));
    mbstowcs(ret, str, sz);
    return ret;
  }
}

mstring wstrtocstr(wstring str) {
  size_t sz = wcstombs(NULL, str, 0);
  if (!~sz) {
    sz = wcslen(str);
    mstring ret = gcalloc(sz+1);
    for (unsigned i = 0; i < sz; ++i)
      ret[i] = str[i];
    return ret;
  } else {
    mstring ret = gcalloc(sz+1);
    wcstombs(ret, str, sz);
    return ret;
  }
}

/**
 * Objects are stored in two parts: A hashtable of entries, and a data section
 * storing the values of the implanted symbols at the time of the last
 * writeback.
 *
 * Each entry in the hashtable stores the location of the symbol (the key) and
 * the offset within the data. The size itself is stored within the symbol.
 */
struct object_implant_hashtable_entry {
  struct symbol_header* sym;
  unsigned offset;
};

struct object_implant_hashtable {
  unsigned num_entries, table_size;
  struct object_implant_hashtable_entry entries[];
};

struct object_t {
  object parent;
  struct object_implant_hashtable* implants;
  unsigned evisceration_count;
  /* When an object is touched by a transaction, it is cloned and the backup
   * stored in tx_backup. tx_id records the ID of the transaction that caused
   * this. If tx_backup is NULL, the object is currently untouched by
   * transactions.
   */
  unsigned tx_id;
  object tx_backup;
  /* Stored data and data bookkeeping */
  unsigned data_end, data_size;
  unsigned char* data;
};

static inline unsigned ptrhash(void* v) {
  unsigned long l = (unsigned long)v;
  // On typical platforms, we should expect the lower two to four bits to be
  // zero for all inputs. There will generally be virtually no entropy in the
  // upper bits, so using only the lower 4 bytes will be an acceptable amount
  // of entropy (and is the entire pointer on 32-bit platforms). We'll feed
  // this through the Jenkins hash function to get a better hash value.
  // Since alignment is guaranteed to be at least 4 bytes, chop the lower two
  // bits off for slightly more higher-half entropy on 64-bit platforms.
  unsigned input = (unsigned)(l>>2);
  unsigned hash = 0;

  hash += input & 0xFF;
  hash += hash << 10;
  hash ^= hash >> 6;
  input >>= 8;

  hash += input & 0xFF;
  hash += hash << 10;
  hash ^= hash >> 6;
  input >>= 8;

  hash += input & 0xFF;
  hash += hash << 10;
  hash ^= hash >> 6;
  input >>= 8;

  hash += input & 0xFF;
  hash += hash << 10;
  hash ^= hash >> 6;
  return hash;
}

static inline unsigned collision_increment(unsigned previnc) {
  // The developers of Python found this to be a good way of doing things, to
  // my recollection.
  return (previnc >> 5) + 1;
}

#define INIT_HASHTABLE_SIZE 8
#define INIT_DATA_SIZE (8*sizeof(void*))

static unsigned current_tx_id(void);

// Writes all currently-owned symbols back into the given object.
static void object_writeback(object this) {
  for (unsigned i = 0; i < this->implants->table_size; ++i) {
    if (this->implants->entries[i].sym &&
        this->implants->entries[i].sym->owner_stack &&
        this->implants->entries[i].sym->owner_stack->owner == this) {
      memcpy(this->data + this->implants->entries[i].offset,
             this->implants->entries[i].sym->payload,
             this->implants->entries[i].sym->size);
    }
  }
}

object object_new(object parent) {
  struct object_implant_hashtable* implants =
    gcalloc(sizeof(struct object_implant_hashtable) +
            INIT_HASHTABLE_SIZE*sizeof(struct object_implant_hashtable_entry));
  implants->num_entries = 0;
  implants->table_size = INIT_HASHTABLE_SIZE;
  object this = new(struct object_t);
  this->parent = parent;
  this->data = gcalloc(INIT_DATA_SIZE);
  this->data_end = 0;
  this->data_size = INIT_DATA_SIZE;
  this->implants = implants;
  this->evisceration_count = 0;
  this->tx_id = current_tx_id();
  return this;
}

object object_clone(object that) {
  object_writeback(that);

  object this = newdup(that);
  this->data = gcalloc(this->data_size);
  memcpy(this->data, that->data, this->data_size);
  this->implants = gcalloc(sizeof(struct object_implant_hashtable) +
                           that->implants->table_size*
                           sizeof(struct object_implant_hashtable_entry));
  memcpy(this->implants, that->implants,
         sizeof(struct object_implant_hashtable) +
         that->implants->table_size*
         sizeof(struct object_implant_hashtable_entry));
  // The clone is not currently on the stack, regardless of the state of the
  // original object
  this->evisceration_count = 0;
  // Similarly, it is not affected by the current transaction
  this->tx_id = current_tx_id();
  this->tx_backup = NULL;
  return this;
}

/**
 * Evisceration of an object is performed as follows:
 * - Eviscerate its parent, if present
 * - For each symbol implanted within this object:
 *   - If the symbol currently has an owner, write its current value back into
 *     the owner's data
 *   - Push the owner stack to indicate this object as its new owner
 *   - Write the object's value of the symbol into the symbol
 *
 * Re-embowelment is similar to the reverse of that:
 * - For each symbol implanted within this object:
 *   - Write the current value back into this object's data
 *   - Pop its owner stack
 *   - If it still has an owner, copy the new owner's value for the symbol into
 *     the symbol
 * - Re-embowel the parent, if present
 *
 * This set-up (versus simply creating backups of the symbols' values) has a
 * number of important properties:
 * - The current value of a symbol within the context of an object is always
 *   either the symbol's value iff the object is currently that symbol's value,
 *   or the symbol's value as stored within the object otherwise.
 * - An object may be eviscerated multiple times on the stack and will function
 *   as expected.
 * - In cases of multiple-evisceration, writes to a symbol in a lower
 *   evisceration frame will propagate to those symbols' values when the upper
 *   evisceration frame becomes visible.
 */
static void symbol_push_ownership(object,
                                  struct object_implant_hashtable_entry*);
static void symbol_pop_ownership(object this,
                                 struct object_implant_hashtable_entry*);
void object_eviscerate(object this) {
  if (this->parent) object_eviscerate(this->parent);

  ++this->evisceration_count;

  dynar_push_o(evisceration_stack, this);
  for (unsigned i = 0; i < this->implants->table_size; ++i)
    if (this->implants->entries[i].sym)
      symbol_push_ownership(this, &this->implants->entries[i]);
}

static void symbol_push_ownership(object this,
                                  struct object_implant_hashtable_entry* hte) {
  struct symbol_header* sym = hte->sym;

  // Write back into current owner and write new value unless the current
  // object is already the owner
  if (!sym->owner_stack || this != sym->owner_stack->owner) {
    if (sym->owner_stack)
      memcpy(sym->owner_stack->owner->data + sym->owner_stack->offset,
             sym->payload, sym->size);
    memcpy(sym->payload, this->data + hte->offset, sym->size);
  }

  // Push new stack entry
  struct symbol_owner_stack sos = {
    .owner = this,
    .offset = hte->offset,
    .next = sym->owner_stack,
  };
  sym->owner_stack = newdup(&sos);
}

void object_reembowel(void) {
  object this;
  do {
    this = dynar_pop_o(evisceration_stack);
    for (unsigned i = 0; i < this->implants->table_size; ++i)
      if (this->implants->entries[i].sym)
        symbol_pop_ownership(this, &this->implants->entries[i]);

    --this->evisceration_count;
  } while (this->parent);
}

static void symbol_pop_ownership(object this,
                                 struct object_implant_hashtable_entry* hte) {
  struct symbol_header* sym = hte->sym;

  // Pop stack entry
  sym->owner_stack = sym->owner_stack->next;

  // If the new owner is not this object, write back into this and restore new
  // owner's value
  if (!sym->owner_stack || this != sym->owner_stack->owner) {
    memcpy(this->data + hte->offset, sym->payload, sym->size);
    if (sym->owner_stack)
      memcpy(sym->payload,
             sym->owner_stack->owner->data + sym->owner_stack->offset,
             sym->size);
  }
}

object object_current(void) {
  return dynar_top_o(evisceration_stack);
}

static unsigned object_find_hashtable_entry(object,
                                            struct symbol_header*);
static void object_expand_hashtable(object);
static struct object_implant_hashtable*
clone_implants(struct object_implant_hashtable*);

void object_implant(struct symbol_header* sym,
                    enum implantation_type implant_type) {
  object this = object_current();
  if (this->tx_backup && this->implants == this->tx_backup->implants) {
    // The transaction backup must keep its implantation table, so create a new
    // one for the fork.
    this->implants = clone_implants(this->implants);
  }
  switch (implant_type) {
  case ImplantDomain: {
    struct symbol_domain* dom = *(struct symbol_domain**)sym->payload;
    while (dom) {
      object_implant(dom->member, dom->implant_type);
      dom = dom->next;
    }
  } break;

  case ImplantSingle: {
    // If the load factor is greater than 75%, double the hashtable size
    if (this->implants->num_entries * 4 >
        this->implants->table_size * 3)
      object_expand_hashtable(this);

    unsigned ix = object_find_hashtable_entry(this, sym);
    if (this->implants->entries[ix].sym)
      // Already implanted, nothing to do
      return;

    // New implant
    // First, add to hashtable
    unsigned offset = this->data_end;
    this->implants->entries[ix].sym = sym;
    this->implants->entries[ix].offset = offset;
    ++this->implants->num_entries;
    // Expand data section if needed
    if (offset + sym->size > this->data_size) {
      this->data_size *= 2;
      this->data = gcrealloc(this->data, this->data_size);
    }
    // Write value into data
    // We need to ensure that any following data are aligned properly so that
    // the GC can see pointers embedded within the data.
    this->data_end += SIZEALIGN(sym->size);
    memcpy(this->data + offset, sym->payload, sym->size);

    // Push ownership to the object
    symbol_push_ownership(this, &this->implants->entries[ix]);

    if (this->evisceration_count > 1) {
      /* In the case of multiple evisceration, we must also retroactively give
       * this object ownership within the multiple frames.
       *
       * In a consistent state of the world, the following properties hold:
       * - The ownership stack is a subsequence of the evisceration stack.
       * - If any instance of an object in the evisceration stack occurs in the
       *   ownership stack, all of them do.
       */
      unsigned estack = evisceration_stack->len-2;
      struct symbol_owner_stack* ostack = sym->owner_stack;
      /* To make the state consistent, examine pairs from the two stacks with
       * the following rules:
       * - If the next owner is the current evisceration context, move to the
       *   next owner.
       * - If the current evisceration context is this object, insert an
       *   ownership frame into the ownership stack after the current and move
       *   to it.
       */
      while (estack < evisceration_stack->len /* will wrap around */) {
        object that = evisceration_stack->v[estack--];
        if (ostack->next && that == ostack->next->owner) {
          assert(that != this);
          ostack = ostack->next;
        } else if (that == this) {
          struct symbol_owner_stack sos = {
            .owner = this,
            .next = ostack->next,
            .offset = this->implants->entries[ix].offset,
          };
          ostack = ostack->next = newdup(&sos);
        }
      }
    }
  } break;
  }
}

static struct object_implant_hashtable*
clone_implants(struct object_implant_hashtable* src) {
  size_t size =
    sizeof(struct object_implant_hashtable) +
    src->table_size * sizeof(struct object_implant_hashtable_entry);
  struct object_implant_hashtable* new = gcalloc(size);
  memcpy(new, src, size);
  return new;
}

static unsigned object_find_hashtable_entry(object this,
                                            struct symbol_header* sym) {
  unsigned hash = ptrhash(sym), incr = hash;
  unsigned ix = hash & (this->implants->table_size-1);

  // Use (nonlinear) probing to find a free entry
  while (this->implants->entries[ix].sym &&
         this->implants->entries[ix].sym != sym) {
    ix += incr;
    ix &= (this->implants->table_size - 1);
    incr = collision_increment(incr);
  }

  return ix;
}

static void object_expand_hashtable(object this) {
  /* Create new hashtable with double the size as the previous */
  struct object_implant_hashtable* old = this->implants;
  struct object_implant_hashtable* new =
    gcalloc(sizeof(struct object_implant_hashtable) +
            2 * old->table_size *
            sizeof(struct object_implant_hashtable_entry));
  this->implants = new;
  new->num_entries = old->num_entries;
  new->table_size = 2 * old->table_size;

  /* Insert each item from the old table into the new one. */
  for (unsigned i = 0; i < old->table_size; ++i) {
    if (old->entries[i].sym) {
      unsigned ix = object_find_hashtable_entry(this,
                                                old->entries[i].sym);
      new->entries[ix] = old->entries[i];
    }
  }
}

void object_get_implanted_value(void* dst, object this,
                                struct symbol_header* sym) {
  do {
    /* Simplest case: If the object currently owns the symbol, that is its
     * current value.
     */
    if (sym->owner_stack && sym->owner_stack->owner == this) {
      memcpy(dst, sym->payload, sym->size);
      return;
    }

    /* Next case: Symbol is implanted in the object. */
    unsigned ix = object_find_hashtable_entry(this, sym);
    if (this->implants->entries[ix].sym) {
      memcpy(dst, this->data + this->implants->entries[ix].offset,
             sym->size);
      return;
    }

    /* Fallback: Search the parent */
    this = this->parent;
  } while (this);

  /* Not in the given object or any direct or indirect parent.
   * Use the symbol's global value.
   */
  memcpy(dst, sym->payload, sym->size);
}

static void clone_hook_chain(struct hook_point_entry** base) {
  if (!*base) return;

  *base = newdup(*base);
  clone_hook_chain(&(*base)->next);
}

static void sort_hook_functions(struct hook_point_entry** base) {
  /* This is a rather naïve algorithm (O(n^3) worst-case), but it shouldn't be
   * an issue generally, since most hooks do not have interdependencies, and
   * even when they do, there are not THAT many of them.
   *
   * In cases of circular constraints, it may end up in an infinite loop.
   */
  restart_sort:
  while (*base) {
    // Ensure *base is not constrained to come after anything currently after
    // it
    for (struct hook_point_entry** other = &(*base)->next;
         *other; other = &(*other)->next) {
      hook_constraint base_to_other;
      base_to_other = HookConstraintNone;

      if ((*base)->constraints)
        base_to_other = (*base)->constraints((*base)->id, (*base)->class,
                                             (*other)->id, (*other)->class);
      // If the base constraints did not require anything, check the other point
      if (!base_to_other && (*other)->constraints) {
        switch ((*other)->constraints((*other)->id, (*other)->class,
                                      (*base)->id, (*base)->class)) {
        case HookConstraintNone: break;
        case HookConstraintBefore:
          base_to_other = HookConstraintAfter;
          break;
        case HookConstraintAfter:
          base_to_other = HookConstraintBefore;
          break;
        }
      }

      if (base_to_other == HookConstraintAfter) {
        // The head of the list needs to run after this item, so swap the two
        // and restart the sort from base.
        struct hook_point_entry* tmp = *base;
        *base = *other;
        *other = tmp;

        tmp = (*base)->next;
        (*base)->next = (*other)->next;
        (*other)->next = tmp;

        goto restart_sort;
      }
    }

    /* The head of this sublist can run before everything after it, so continue
     * the sort with the next item.
     */
    base = &(*base)->next;
  }
}

static void del_hook_impl(struct hook_point*,
                          unsigned, identity, object);
void add_hook_obj_cond(struct hook_point* point, unsigned priority,
                       const bool* when,
                       identity id, identity class,
                       void (*fun)(void), object context,
                       hook_constraint_function constraints) {
  // Copy the current chain in case we are currently sharing it
  clone_hook_chain(&point->entries[priority]);
  // Remove any existing
  del_hook_impl(point, priority, id, context);
  struct hook_point_entry hpe = {
    .fun = fun,
    .context = context,
    .constraints = constraints,
    .id = id,
    .class = class,
    .next = point->entries[priority],
    .when = when,
  };
  point->entries[priority] = newdup(&hpe);

  sort_hook_functions(&point->entries[priority]);
}

void add_hook(struct hook_point* point, unsigned priority,
              identity id, identity class,
              void (*fun)(void),
              hook_constraint_function constraints) {
  add_hook_obj_cond(point, priority, NULL, id, class, fun, NULL, constraints);
}

void add_hook_obj(struct hook_point* point, unsigned priority,
                  identity id, identity class,
                  void (*fun)(void), object this,
                  hook_constraint_function constraints) {
  add_hook_obj_cond(point, priority, NULL, id, class, fun, this, constraints);
}

void add_hook_cond(struct hook_point* point, unsigned priority,
                   const bool* when,
                   identity id, identity class,
                   void (*fun)(void),
                   hook_constraint_function constraints) {
  add_hook_obj_cond(point, priority, when, id, class, fun, NULL, constraints);
}

static void del_hook_impl(struct hook_point* point,
                          unsigned priority, identity id,
                          object context) {
  struct hook_point_entry** base = &point->entries[priority];
  while (*base) {
    if ((*base)->id == id && (*base)->context == context) {
      // Delete
      *base = (*base)->next;
      //No re-sorting is needed, since the current order remains valid even
      //with a link removed
      return;
    }

    base = &(*base)->next;
  }
}

void del_hook(struct hook_point* point,
              unsigned priority, identity id, object context) {
  // Copy the current chain in case we are currently sharing it
  clone_hook_chain(&point->entries[priority]);

  del_hook_impl(point, priority, id, context);
}

void del_hooks_of_id(struct hook_point* point, unsigned priority, identity id) {
  clone_hook_chain(&point->entries[priority]);

  struct hook_point_entry** base = &point->entries[priority];
  while (*base)
    if ((*base)->id == id)
      *base = (*base)->next;
    else
      base = &(*base)->next;
}

static sigjmp_buf hook_abort_point;
void hook_abort(void) {
  siglongjmp(hook_abort_point, 1);
}

static sigjmp_buf hook_continue_point;
void continue_hook_in_current_context(void) {
  siglongjmp(hook_continue_point, 1);
}

void invoke_hook(struct hook_point* ppoint) {
  if (!ppoint) return;

  // In case of an aborted hook or a continue_hook_in_current_context(), we
  // need to know how far to unwind the evisceration stack when finished.
  unsigned evisceration_stack_depth = evisceration_stack->len;

  sigjmp_buf old_abort_point, old_continue_point;
  // POSIX defines jmp_buf to be an array, so we don't explicitly reference the
  // jmp_buf in this call
  memcpy(old_abort_point, hook_abort_point, sizeof(hook_abort_point));
  memcpy(old_continue_point, hook_continue_point, sizeof(hook_continue_point));

  // Save this location; if it returns non-zero, the hook was aborted
  if (sigsetjmp(hook_abort_point, 1)) goto end;

  // Make a copy so that concurrent modifications do not interfere with this
  // invocation of the hook.
  struct hook_point point = *ppoint;

  // Loop through priorities and hooks attached thereto
  for (unsigned priority = 0;
       priority < sizeof(point.entries)/sizeof(point.entries[0]);
       ++priority) {
    for (struct hook_point_entry* curr = point.entries[priority];
         curr; curr = curr->next) {

      // Execute the hook if it has no condition, or the condition is true, AND
      // if sigsetjmp() returns 0 (ie, continue_hook_in_current_context() was
      // not called).
      if ((!curr->when || *curr->when) && !sigsetjmp(hook_continue_point, 1)) {
        within_context(curr->context, curr->fun());
      }
    }
  }

  end:
  // Restore old hooks
  memcpy(hook_abort_point, old_abort_point, sizeof(hook_abort_point));
  memcpy(hook_continue_point, old_continue_point, sizeof(hook_continue_point));

  // Restore evisceration stack if it is different from when we started
  while (evisceration_stack->len != evisceration_stack_depth)
    object_reembowel();
}

hook_constraint constraint_after_superconstructor(
  identity a, identity b, identity c, identity that_class
) {
  if (that_class == $u_superconstructor)
    return HookConstraintAfter;
  return HookConstraintNone;
}

hook_constraint constraint_before_superconstructor(
  identity a, identity b, identity c, identity that_class
) {
  if (that_class == $u_superconstructor)
    return HookConstraintBefore;
  return HookConstraintNone;
}

void add_symbol_to_domain(struct symbol_header* sym,
                          struct symbol_domain** dom,
                          enum implantation_type implant_type) {
  // Do nothing if already added
  for (struct symbol_domain* d = *dom; d; d = d->next)
    if (d->member == sym)
      return;

  struct symbol_domain entry = {
    .member = sym,
    .implant_type = implant_type,
    .next = *dom,
  };
  *dom = newdup(&entry);
}

ATSTART(eviscerate_root_object, ROOT_OBJECT_EVISCERATION_PRIORITY) {
  evisceration_stack = dynar_new_o();
  $o_root = object_new(NULL);
  object_eviscerate($o_root);
}

void on_each_o(list_o these, void (*f)(void)) {
  for (; these; these = these->cdr)
    within_context(these->car, f());
}

// So we get the list_o and list_p templates: $$lo_unused $$lp_unused

typedef struct transaction {
  //The unique identifier for this transaction
  unsigned id;
  //The length of the evisceration stack when the tx started
  unsigned evisceration_depth;
  //Objects which have been touched (forked) by this transaction.
  list_o objects_touched;
  //A list of void (*)(void) to invoke on (before) rollback
  list_p rollback_handlers;
  //Function to call to exit the transaction on rollback
  void (*exit_function)(void);

  struct transaction* next;
} transaction;

static unsigned next_tx_id;
static transaction* tx_current;

static unsigned current_tx_id() {
  if (!tx_current)
    return 0;
  else
    return tx_current->id;
}

static void tx_fork_object(object this) {
  if (!tx_current || this->tx_id == tx_current->id) return;

  object new = object_clone(this);
  // Populate data destroyed by object_clone
  new->evisceration_count = this->evisceration_count;
  new->tx_id = this->tx_id;
  new->tx_backup = this->tx_backup;

  this->tx_backup = new;
  this->tx_id = tx_current->id;

  lpush_o(tx_current->objects_touched, this);
}

void tx_start(void (*exit_function)(void)) {
  transaction tx = {
    .id = ++next_tx_id,
    .evisceration_depth = evisceration_stack->len,
    .objects_touched = NULL,
    .rollback_handlers = NULL,
    .exit_function = exit_function,
    .next = tx_current,
  };

  tx_current = newdup(&tx);

  // Touch all currently-eviscerated objects
  for (unsigned i = evisceration_stack->len-1;
       i < evisceration_stack->len; --i)
    tx_fork_object(evisceration_stack->v[i]);
}

void tx_commit(void) {
  // For each object touched, discard its backup and move it to the previous
  // tx_id
  for (list_o curr = tx_current->objects_touched; curr; curr = curr->cdr) {
    object this = curr->car;
    this->tx_id = this->tx_backup->tx_id;
    this->tx_backup = this->tx_backup->tx_backup;
  }

  tx_current = tx_current->next;
}

/*
  SYMBOL: $v_rollback_type
    Indicates the reason rollback occurred. This is automatically propagated
    across transaction boundaries by tx_rollback().

  SYMBOL: $s_rollback_reason
    Indicates the human-readable reason rollback occurred. This is
    automatically propagated across transaction boundaries by tx_rollback().
 */

void tx_rollback(void) {
  // Write reasons through
  tx_write_through($v_rollback_type);
  tx_write_through($s_rollback_reason);

  // Run rollback handlers
  while (tx_current->rollback_handlers) {
    void (*h)(void) = tx_current->rollback_handlers->car;
    tx_current->rollback_handlers = tx_current->rollback_handlers->cdr;
    h();
  }

  // Revert touched objects
  for (list_o curr = tx_current->objects_touched; curr; curr = curr->cdr) {
    object this = curr->car;
    memcpy(this, this->tx_backup, sizeof(*this));
  }

  //Revert all symbols to their pre-transaction values
  for (unsigned i = 0; i < $o_root->implants->table_size; ++i) {
    if ($o_root->implants->entries[i].sym &&
        $o_root->implants->entries[i].sym->owner_stack) {
      memcpy($o_root->implants->entries[i].sym->payload,
             $o_root->implants->entries[i].sym->owner_stack->owner->data +
               $o_root->implants->entries[i].sym->owner_stack->offset,
             $o_root->implants->entries[i].sym->size);
    }
  }

  //Restore evisceration stack
  dynar_o old_evisceration_stack = evisceration_stack;
  evisceration_stack = dynar_new_o();
  for (unsigned i = 0; i < tx_current->evisceration_depth; ++i) {
    // Eviscerate this object, unless it would be implicitly by the next object
    if (i == tx_current->evisceration_depth-1 ||
        !old_evisceration_stack->v[i+1]->parent)
      object_eviscerate(old_evisceration_stack->v[i]);
  }

  //Exit transaction
  void (*exit_function)(void) = tx_current->exit_function;
  tx_current = tx_current->next;
  exit_function();

  //Should never get here
  abort();
}

void tx_rollback_merrno(identity id, int chk, string otherwise) {
  $v_rollback_type = id;
  $s_rollback_reason = (chk == -1? strerror(errno) : otherwise);
  tx_rollback();
}

void tx_push_handler(void (*handler)(void)) {
  if (!tx_current) return;

  lpush_p(tx_current->rollback_handlers, handler);
}

void tx_pop_handler(void) {
  if (!tx_current) return;

  lpop_p(tx_current->rollback_handlers);
}

void tx_write_through_impl(struct symbol_header* sym) {
  if (!sym->owner_stack) return;

  // Copy into each version of the object.
  // If there exists a version of the object which does not have the symbol
  // implanted, this can be identified by the symbol's offset being beyond the
  // object's data end. If this is encountered, we stop, since all previous
  // versions will have the same property.
  for (object curr = sym->owner_stack->owner;
       curr && sym->owner_stack->offset < curr->data_end;
       curr = curr->tx_backup)
    memcpy(curr->data + sym->owner_stack->offset,
           sym->payload, sym->size);
}
