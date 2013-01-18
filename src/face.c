/*
  Copyright ⓒ 2013

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
#include "face.slc"

#define INVALID_COLOUR 0xFF

static unsigned translate_colour(char name) {
  switch (name) {
  case 'k': return DARK_BLACK;
  case 'r': return DARK_RED;
  case 'y': return DARK_YELLOW;
  case 'g': return DARK_GREEN;
  case 'c': return DARK_CYAN;
  case 'b': return DARK_BLUE;
  case 'm': return DARK_MAGENTA;
  case 'w': return DARK_WHITE;
  case 'K': return BRIGHT_BLACK;
  case 'R': return BRIGHT_RED;
  case 'Y': return BRIGHT_YELLOW;
  case 'G': return BRIGHT_GREEN;
  case 'C': return BRIGHT_CYAN;
  case 'B': return BRIGHT_BLUE;
  case 'M': return BRIGHT_MAGENTA;
  case 'W': return BRIGHT_WHITE;
  default:
    fprintf(stderr, "WARN: Invalid colour: %c\n", name);
    return INVALID_COLOUR;
  }
}

#define INVALID_ALTERATION ((unsigned)-1)
static face parse_alteration(string* str, face* family) {
  switch (*(*str)++) {
  case 'f': {
    if (!**str) {
      fprintf(stderr, "WARN: Missing colour after f alteration\n");
      return INVALID_ALTERATION;
    }
    unsigned colour = translate_colour(*(*str)++);
    if (colour == INVALID_COLOUR)
      return INVALID_ALTERATION;
    colour ^= 0xE;
    *family = QC_FG;
    return colour << QC_FG_SHIFT;
  }

  case 'b': {
    if (!**str) {
      fprintf(stderr, "WARN: Missing colour after b alteration\n");
      return INVALID_ALTERATION;
    }
    unsigned colour = translate_colour(*(*str)++);
    if (colour == INVALID_COLOUR)
      return INVALID_ALTERATION;
    *family = QC_BG;
    return colour << QC_BG_SHIFT;
  }

  case 'B': *family = QC_ATTR; return QC_BOLD;
  case 'U': *family = QC_ATTR; return QC_ULIN;
  case 'I': *family = QC_ATTR; return QC_ITAL;
  case 'X': *family = QC_ATTR; return QC_RVID;
  default:
    fprintf(stderr, "WARN: Invalid alteration type: %c\n",
            *(*str - 1));
    return INVALID_ALTERATION;
  }
}

face mkface(string str) {
  return mkface_of(0, str);
}

face mkface_of(face f, string str) {
  while (*str) {
    face bits, family;
    char type = *str++;
    if (type == ' ' || type == '\t') continue;
    if (type != '+' &&
        type != '-' &&
        type != '!' &&
        type != '*' &&
        type != '=') {
      fprintf(stderr, "WARN: Invalid alteration type: %c\n",
              type);
      continue;
    }

    bits = parse_alteration(&str, &family);
    if (bits == INVALID_ALTERATION) continue;

    switch (type) {
    case '+':
      //Clear the bits with AND-NOT, then set with XOR
      f |= bits >> FACE_AND_SHIFT;
      f |= bits >> FACE_XOR_SHIFT;
      break;

    case '-':
      //Clear bits with AND-NOT
      f |= bits >> FACE_AND_SHIFT;
      //Reset XOR mask for these bits
      f &= ~(bits >> FACE_XOR_SHIFT);
      break;

    case '!':
      f |= bits >> FACE_XOR_SHIFT;
      //Reset AND-NOT mask for these bits
      f &= ~(bits >> FACE_AND_SHIFT);
      break;

    case '*':
      f |= family >> FACE_AND_SHIFT;
      f &= ~(family >> FACE_XOR_SHIFT);
      f |= bits >> FACE_XOR_SHIFT;
      break;

    case '=':
      //Reset appropriate bits
      f &= ~(bits >> FACE_AND_SHIFT);
      f &= ~(bits >> FACE_XOR_SHIFT);
      break;
    }
  }

  return f;
}

qchar apply_face(face f, qchar ch) {
  ch &= ~((f & FACE_AND_MASK) << FACE_AND_SHIFT) | QC_CHAR;
  ch ^=  ((f & FACE_XOR_MASK) << FACE_XOR_SHIFT);
  return ch;
}

mqstring apply_face_str(face f, mqstring str) {
  mqstring str_begin = str;
  while (*str) {
    *str = apply_face(f, *str);
    ++str;
  }

  return str_begin;
}

mqstring apply_face_arr(face f, mqstring str, size_t n) {
  for (size_t i = 0; i < n; ++i)
    str[i] = apply_face(f, str[i]);

  return str;
}
