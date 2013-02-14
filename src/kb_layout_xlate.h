/*
  Copyright ⓒ 2013 Jason Lingle

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

#ifndef KB_LAYOUT_XLATE_H_
#define KB_LAYOUT_XLATE_H_

/**
 * Translates the given wchar_t to the normal QWERTY value according to the
 * keyboard layout of the current terminal. If there is no conversion, returns
 * the character unmodified.
 */
wchar_t qwertify(wchar_t);

#endif /* KB_LAYOUT_XLATE_H_ */
