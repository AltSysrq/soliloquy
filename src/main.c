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

#include "main.slc"

static void doit(void) {
  printf("%s\n", $s_greeting);
}

int main(void) {
  object en = object_new(NULL), de = object_new(NULL);
  within_context(en, implant($s_greeting));
  within_context(de, implant($s_greeting));
  within_context(en, $s_greeting = "Hello, world!");
  within_context(de, $s_greeting = "Hallo, Welt!");

  within_context(en, doit());
  within_context(de, doit());
  return 0;
}
