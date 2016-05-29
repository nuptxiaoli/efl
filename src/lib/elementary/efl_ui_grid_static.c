#include "efl_ui_grid_private.h"

#define MY_CLASS EFL_UI_GRID_STATIC_CLASS

typedef struct _Efl_Ui_Grid_Static_Data Efl_Ui_Grid_Static_Data;

struct _Efl_Ui_Grid_Static_Data
{
};

EOLIAN static void
_efl_ui_grid_static_evas_object_smart_add(Eo *obj, Efl_Ui_Grid_Static_Data *pd EINA_UNUSED)
{
   elm_widget_sub_object_parent_add(obj);

   evas_obj_smart_add(eo_super(obj, MY_CLASS));

   Efl_Ui_Grid_Data *gd = eo_data_scope_get(obj, EFL_UI_GRID_CLASS);
   gd->layout_engine = MY_CLASS;
}

EOLIAN static void
_efl_ui_grid_static_efl_pack_layout_layout_do(Eo_Class *klass EINA_UNUSED,
                                           void *_pd EINA_UNUSED,
                                           Eo *obj, const void *data EINA_UNUSED)
{
   Efl_Ui_Grid_Data *gd;
   Grid_Item *gi;
   Evas *e;
   Evas_Coord x, y, w, h;
   long long xl, yl, wl, hl, vwl, vhl;
   Eina_Bool mirror;

   gd = eo_data_scope_get(obj, EFL_UI_GRID_CLASS);
   if (!gd->items) return;

   e = evas_common_evas_get(obj);
   eo_event_freeze(e);

   efl_gfx_position_get(obj, &x, &y);
   efl_gfx_size_get(obj, &w, &h);
   xl = x;
   yl = y;
   wl = w;
   hl = h;
   mirror = elm_widget_mirrored_get(obj);
   vwl = gd->req_cols;
   vhl = gd->req_rows;

   EINA_INLIST_FOREACH(gd->items, gi)
     {
        long long x1, y1, x2, y2;

        if (!mirror)
          {
             x1 = xl + ((wl * (long long)gi->col) / vwl);
             x2 = xl + ((wl * (long long)(gi->col + gi->col_span)) / vwl);
          }
        else
          {
             x1 = xl + ((wl * (vwl - (long long)(gi->col + gi->col_span))) / vwl);
             x2 = xl + ((wl * (vwl - (long long)gi->col)) / vwl);
          }
        y1 = yl + ((hl * (long long)gi->row) / vhl);
        y2 = yl + ((hl * (long long)(gi->row + gi->row_span)) / vhl);
        efl_gfx_position_set(gi->object, x1, y1);
        efl_gfx_size_set(gi->object, x2 - x1, y2 - y1);
     }

   eo_event_thaw(e);
}

#include "efl_ui_grid_static.eo.c"
