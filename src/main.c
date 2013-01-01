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

static void do_math(void) {
  printf("%d + %d = %d\n", $i_Math_a, $i_Math_b, $i_Math_a + $i_Math_b);
}

static void print_foo(void) {
  printf("foo\n");
}

static void print_bar(void) {
  printf("bar\n");
}

static void print_baz(void) {
  printf("baz\n");
}

static hook_constraint before_baz(identity a, identity b, identity id, identity c) {
  if (id == $$u_baz)
    return HookConstraintBefore;
  return HookConstraintNone;
}

static hook_constraint after_foo(identity a, identity b, identity id, identity c) {
  if (id == $$u_foo)
    return HookConstraintAfter;
  return HookConstraintNone;
}
static hook_constraint after_bar(identity a, identity b, identity id, identity c) {
  if (id == $$u_bar)
    return HookConstraintAfter;
  return HookConstraintNone;
}

int main(void) {
  object en = object_new(NULL), de = object_new(NULL);
  within_context(en, implant($s_greeting));
  within_context(de, implant($s_greeting));
  within_context(en, $s_greeting = "Hello, world!");
  within_context(de, $s_greeting = "Hallo, Welt!");

  within_context(en, doit());
  within_context(de, doit());

  within_context(en, {
      implant($h_test);
      add_hook(&$h_test, HOOK_MAIN, $$u_baz, $$u_generic, print_baz, NULL);
      add_hook(&$h_test, HOOK_MAIN, $$u_bar, $$u_generic, print_bar, after_foo);
      add_hook(&$h_test, HOOK_MAIN, $$u_foo, $$u_generic, print_foo, before_baz);
    });
  within_context(de, {
      implant($h_test);
      add_hook(&$h_test, HOOK_MAIN, $$u_foo, $$u_generic, print_foo, after_bar);
      add_hook(&$h_test, HOOK_MAIN, $$u_bar, $$u_generic, print_bar, NULL);
      add_hook(&$h_test, HOOK_MAIN, $$u_baz, $$u_generic, print_baz, after_foo);
    });

  printf("\n");
  within_context(en, $f_test());
  printf("\n");
  within_context(de, $f_test());

  within_context(en, {
      implant($d_Math);
      $i_Math_a = 5;
      $i_Math_b = 6;
    });
  within_context(de, {
      implant($d_Math);
      $i_Math_a = 0;
      $i_Math_b = -1;
    });
  within_context(en, do_math());
  within_context(de, do_math());

  $li_test = cons_i(4, cons_i(2, cons_i(0, cons_i(1, NULL))));
  return find_i($li_test, 0)->car;
}
