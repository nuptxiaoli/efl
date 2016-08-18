/* EINA - EFL data type library
 * Copyright (C) 2016 ProFUSION embedded systems
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library;
 * if not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>

#include <Eina.h>

#include "eina_suite.h"

START_TEST(eina_test_slice_init)
{
   Eina_Rw_Slice a = EINA_SLICE_STR_LITERAL("hello world");
   char buf[1024] = "";
   Eina_Rw_Slice b = EINA_SLICE_ARRAY(buf);
   Eina_Rw_Slice c = EINA_SLICE_STR("hi there");
   EINA_RW_SLICE_DECLARE(d, 512);

   eina_init();

   fail_unless(a.len == sizeof("hello world") - 1);
   fail_unless(strcmp(a.mem, "hello world") == 0);

   fail_unless(b.len == sizeof(buf));

   fail_unless(c.len == strlen("hi there"));
   fail_unless(strcmp(c.mem, "hi there") == 0);

   fail_unless(d.len == 512);

   eina_shutdown();
}
END_TEST

START_TEST(eina_test_slice_ro)
{
   Eina_Slice slice = EINA_SLICE_STR_LITERAL("hi there");
   Eina_Slice a, needle;
   Eina_Rw_Slice dup;
   const void *p;
   char *str;

   dup = eina_slice_dup(slice);
   fail_unless(dup.len == slice.len);
   fail_unless(dup.mem != slice.mem);
   fail_unless(eina_slice_compare(eina_rw_slice_slice_get(dup), slice) == 0);
   free(dup.mem);

   str = eina_slice_strdup(slice);
   fail_unless(str != NULL);
   fail_unless(strcmp(str, "hi there") == 0);
   free(str);

   a = eina_slice_seek(slice, strlen("hi "), SEEK_SET);
   fail_unless(a.mem == slice.bytes + strlen("hi "));
   fail_unless(a.len == slice.len - strlen("hi "));

   a = eina_slice_seek(slice, 0, SEEK_SET);
   fail_unless(a.mem == slice.bytes + 0);
   fail_unless(a.len == slice.len);

   a = eina_slice_seek(slice, -1, SEEK_END);
   fail_unless(a.mem == slice.bytes + slice.len - 1);
   fail_unless(a.len == 1);

   a = eina_slice_seek(slice, 0, SEEK_END);
   fail_unless(a.mem == eina_slice_end_get(slice));
   fail_unless(a.len == 0);

   p = eina_slice_end_get(slice);
   fail_unless(p == slice.bytes + slice.len);

   slice = (Eina_Slice)EINA_SLICE_STR_LITERAL("HELLO WORLD");
   slice.len = strlen("hi there"); /* crop... */

   p = eina_slice_strchr(slice, ' ');
   fail_unless(p == slice.bytes + 5); /* 5 = index of ' ' in HELLO WORLD.. */

   p = eina_slice_strchr(slice, '!');
   fail_unless(p == NULL);

   needle = (Eina_Slice)EINA_SLICE_STR_LITERAL(" W");
   p = eina_slice_find(slice, needle);
   fail_unless(p == slice.bytes + 5); /* 5 = index of ' W' in HELLO WORLD..*/

   needle = (Eina_Slice)EINA_SLICE_STR_LITERAL("LO");
   p = eina_slice_find(slice, needle);
   fail_unless(p == slice.bytes + 3); /* 3 = index of 'LO' in HELLO WORLD..*/

   needle = (Eina_Slice)EINA_SLICE_STR_LITERAL("HELLO");
   p = eina_slice_find(slice, needle);
   fail_unless(p == slice.mem);

   needle = (Eina_Slice)EINA_SLICE_STR_LITERAL("WORLD"); /* would go out of boundaries */
   p = eina_slice_find(slice, needle);
   fail_unless(p == NULL);
}
END_TEST

START_TEST(eina_test_slice_rw)
{
   uint8_t buf[] = "hi there";
   Eina_Rw_Slice rw_slice = EINA_SLICE_ARRAY(buf);
   Eina_Slice ro_slice;
   Eina_Rw_Slice a;
   const void *p;
   char *str;

   buf[sizeof(buf) - 1] = 0xff;
   rw_slice.len--; /* do not account \0 (now 0xff) */
   ro_slice = eina_rw_slice_slice_get(rw_slice);

   fail_unless(rw_slice.len == ro_slice.len);
   fail_unless(rw_slice.mem == ro_slice.mem);

   a = eina_slice_dup(ro_slice);
   fail_unless(a.len == ro_slice.len);
   fail_unless(a.mem != ro_slice.mem);
   fail_unless(eina_rw_slice_compare(a, rw_slice) == 0);
   free(a.mem);

   str = eina_rw_slice_strdup(rw_slice);
   fail_unless(str != NULL);
   fail_unless(strcmp(str, "hi there") == 0);
   free(str);

   a = eina_rw_slice_seek(rw_slice, strlen("hi "), SEEK_SET);
   fail_unless(a.mem == rw_slice.bytes + strlen("hi "));
   fail_unless(a.len == rw_slice.len - strlen("hi "));

   a = eina_rw_slice_seek(rw_slice, 0, SEEK_SET);
   fail_unless(a.mem == rw_slice.bytes + 0);
   fail_unless(a.len == rw_slice.len);

   a = eina_rw_slice_seek(rw_slice, -1, SEEK_END);
   fail_unless(a.mem == rw_slice.bytes + rw_slice.len - 1);
   fail_unless(a.len == 1);

   a = eina_rw_slice_seek(rw_slice, 0, SEEK_END);
   fail_unless(a.mem == eina_rw_slice_end_get(rw_slice));
   fail_unless(a.len == 0);

   p = eina_rw_slice_end_get(rw_slice);
   fail_unless(p == rw_slice.bytes + rw_slice.len);

   ro_slice = (Eina_Slice)EINA_SLICE_STR_LITERAL("HELLO WORLD, big string to be cropped");
   a = eina_rw_slice_copy(rw_slice, ro_slice);

   fail_unless(a.mem == rw_slice.mem);
   fail_unless(a.len == rw_slice.len);
   fail_unless(strncmp(a.mem, "HELLO WO", a.len) == 0);

   p = eina_rw_slice_strchr(rw_slice, ' ');
   fail_unless(p == rw_slice.bytes + 5); /* 5 = index of ' ' in HELLO WORLD.. */

   p = eina_rw_slice_strchr(rw_slice, '!');
   fail_unless(p == NULL);

   ro_slice = (Eina_Slice)EINA_SLICE_STR_LITERAL(" W");
   p = eina_rw_slice_find(rw_slice, ro_slice);
   fail_unless(p == rw_slice.bytes + 5); /* 5 = index of ' W' in HELLO WORLD..*/

   ro_slice = (Eina_Slice)EINA_SLICE_STR_LITERAL("LO");
   p = eina_rw_slice_find(rw_slice, ro_slice);
   fail_unless(p == rw_slice.bytes + 3); /* 3 = index of 'LO' in HELLO WORLD..*/

   ro_slice = (Eina_Slice)EINA_SLICE_STR_LITERAL("HELLO");
   p = eina_rw_slice_find(rw_slice, ro_slice);
   fail_unless(p == rw_slice.mem);

   ro_slice = (Eina_Slice)EINA_SLICE_STR_LITERAL("WORLD"); /* would go out of boundaries */
   p = eina_rw_slice_find(rw_slice, ro_slice);
   fail_unless(p == NULL);

   fail_unless(buf[sizeof(buf) - 1] == 0xff);
}
END_TEST

START_TEST(eina_test_slice_print)
{
   char buf[] = "Hello World";
   Eina_Slice slice = {.len = strlen("Hello"), .mem = buf};
   char a[128], b[128];

   snprintf(a, sizeof(a), EINA_SLICE_FMT, EINA_SLICE_PRINT(slice));
   snprintf(b, sizeof(b), "%p+%zu", slice.mem, slice.len);
   fail_unless(strcmp(a, b) == 0);

   snprintf(a, sizeof(a), EINA_SLICE_STR_FMT, EINA_SLICE_STR_PRINT(slice));
   fail_unless(strcmp(a, "Hello") == 0);
}
END_TEST

void
eina_test_slice(TCase *tc)
{
   tcase_add_test(tc, eina_test_slice_init);
   tcase_add_test(tc, eina_test_slice_ro);
   tcase_add_test(tc, eina_test_slice_rw);
   tcase_add_test(tc, eina_test_slice_print);
}
