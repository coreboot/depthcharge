// SPDX-License-Identifier: GPL-2.0

#ifndef BASE_STRING_UTILS_H_
#define BASE_STRING_UTILS_H_

/*
 * Create new string which is copy of *src but with value for *key replaced with
 * *insert. If there is no *key in the *src, just append concat of *key & *insert.
 *
 * Caller is responsible for freeing memory allocated for the returned string.
 */
char *set_key_value_in_separated_list(const char *src, const char *key,
				      const char *value, const char *separator);

#endif  /* BASE_STRING_UTILS_H_*/
