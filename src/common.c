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

#include "common.slc"

const char* gcstrdup(const char* str) {
  size_t len = strlen(str)+1;
  char* dst = gcalloc(len);
  memcpy(dst, str, len);
  return dst;
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
  if (previnc == 1)
    return previnc;
  else
    return previnc >> 5;
}

#define INIT_HASHTABLE_SIZE 8
#define INIT_DATA_SIZE (8*sizeof(void*))

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
  return this;
}

object object_clone(object that) {
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
  return this;
}

STATIC_INIT_TO($ao_evisceration_stack, dynar_new_o())

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

  dynar_push_o($ao_evisceration_stack, this);
  for (unsigned i = 0; i < this->implants->table_size; ++i)
    if (this->implants->entries[i].sym)
      symbol_push_ownership(this, &this->implants->entries[i]);
}

static void symbol_push_ownership(object this,
                                  struct object_implant_hashtable_entry* hte) {
  struct symbol_header* sym = hte->sym;

  // Write back into current owner and write new value unless the current
  // object is already the owner
  if (sym->owner_stack && this != sym->owner_stack->owner) {
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
    this = dynar_pop_o($ao_evisceration_stack);
    for (unsigned i = 0; i < this->implants->table_size; ++i)
      if (this->implants->entries[i].sym)
        symbol_pop_ownership(this, &this->implants->entries[i]);
  } while (this->parent);
}

static void symbol_pop_ownership(object this,
                                 struct object_implant_hashtable_entry* hte) {
  struct symbol_header* sym = hte->sym;

  // Pop stack entry
  sym->owner_stack = sym->owner_stack->next;

  // If the new owner is not this object, write back into this and restore new
  // owner's value
  if (sym->owner_stack && this != sym->owner_stack->owner) {
    memcpy(this->data + hte->offset, sym->payload, sym->size);
    memcpy(sym->payload,
           sym->owner_stack->owner->data + sym->owner_stack->offset,
           sym->size);
  }
}

object object_current(void) {
  return dynar_top_o($ao_evisceration_stack);
}

static unsigned object_find_hashtable_entry(object,
                                            struct symbol_header*);
static void object_expand_hashtable(object);
void object_implant(struct symbol_header* sym,
                    enum implantation_type implant_type) {
  object this = object_current();
  switch (implant_type) {
  case ImplantDomain: {
    struct symbol_domain* dom = *(struct symbol_domain**)sym->payload;
    while (dom) {
      object_implant(dom->member, dom->implant_type);
      dom = dom->next;
    }
  } break;

  case ImplantSingle: {
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
    // If the load factor is greater than 75%, double the hashtable size
    if (this->implants->num_entries * 4 >
        this->implants->table_size * 3)
      object_expand_hashtable(this);
    // Expand data section if needed
    if (offset + sym->size > this->data_size) {
      this->data_size *= 2;
      this->data = gcrealloc(this->data, this->data_size);
    }
    // Write value into data
    this->data_end += sym->size;
    memcpy(this->data + offset, sym->payload, sym->size);

    // Push ownership to the object
    symbol_push_ownership(this, &this->implants->entries[ix]);
  } break;
  }
}

static unsigned object_find_hashtable_entry(object this,
                                            struct symbol_header* sym) {
  unsigned hash = ptrhash(sym), incr = hash;
  unsigned ix = hash & (this->implants->table_size-1);

  // Use (nonlinear) probing to find a free entry
  while (this->implants->entries[ix].sym) {
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
      new->entries[ix] = old->entries[ix];
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

#include "symbols.def"
