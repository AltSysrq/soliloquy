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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "common.h"

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
 * Each entry in the hashtable stores the location of the symbol (the key), the
 * offset within the data, and the size of the datum.
 */
struct object_implant_hashtable_entry {
  void* sym;
  unsigned offset, size;
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
  unsigned char data[];
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
  object this = gcalloc(sizeof(struct object_t) + INIT_DATA_SIZE);
  this->parent = parent;
  this->data_end = 0;
  this->data_size = INIT_DATA_SIZE;
  this->implants = implants;
  this->evisceration_count = 0;
  return this;
}

object object_clone(object that) {
  object this = gcalloc(sizeof(struct object_t) + that->data_size);
  memcpy(this, that, sizeof(struct object_t) + that->data_size);
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
