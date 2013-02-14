#ifndef KB_LAYOUT_XLATE_H_
#define KB_LAYOUT_XLATE_H_

/**
 * Translates the given wchar_t to the normal QWERTY value according to the
 * keyboard layout of the current terminal. If there is no conversion, returns
 * the character unmodified.
 */
wchar_t qwertify(wchar_t);

#endif /* KB_LAYOUT_XLATE_H_ */
