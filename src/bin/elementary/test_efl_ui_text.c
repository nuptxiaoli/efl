#ifdef HAVE_CONFIG_H
# include "elementary_config.h"
#endif
#define EO_BETA_API
#include <Elementary.h>

static void
_apply_style(Eo *obj, size_t start_pos, size_t end_pos, const char *style)
{
   Efl_Canvas_Text_Cursor *start, *end;

   start = efl_canvas_text_cursor_get(obj);
   end = efl_ui_text_cursor_new(obj);

   efl_canvas_text_cursor_position_set(start, start_pos);
   efl_canvas_text_cursor_position_set(end, end_pos);

   efl_canvas_text_annotation_insert(obj, start, end, style);
}

static void
_create_label(Eo *win, Eo *bx, const char *text,
      size_t style_start, size_t style_end, const char *style)
{
   Eo *en;
   en = efl_add(EFL_UI_TEXT_CLASS, win);
   printf("Added Efl.Ui.Text object\n");
   efl_text_set(en, text);
   _apply_style(en, style_start, style_end, style);
   efl_ui_text_interactive_editable_set(en, EINA_FALSE);
   efl_canvas_text_style_set(en, NULL, "DEFAULT='align=center font=Sans font_size=10 color=#fff wrap=word'");

   evas_object_size_hint_weight_set(en, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(en, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(en);
   elm_box_pack_end(bx, en);
}

void
test_efl_ui_text_label(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Evas_Object *win, *bx;

   win = elm_win_util_standard_add("label", "Label");
   elm_win_autodel_set(win, EINA_TRUE);

   bx = elm_box_add(win);
   evas_object_size_hint_weight_set(bx, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_win_resize_object_add(win, bx);
   evas_object_show(bx);

   _create_label(win, bx,
         "This is a small label",
   //     012345678901234567890
          0, 21,
          "font_size=12 font_weight=bold");
   _create_label(win, bx,
         "This is a small label",
   //     012345678901234567890
          0, 21,
          "font_size=12 font_weight=bold");
//   elm_object_focus_set(en, EINA_TRUE);

   evas_object_resize(win, 480, 320);
   evas_object_show(win);
}
