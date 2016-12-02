/**
 * Ecore Evas example illustrating how to set a new cursor image for a pointer device.
 *
 * @verbatim
 * gcc -o ecore_evas_cursor_example ecore_evas_cursor_example.c `pkg-config --libs --cflags evas ecore ecore-evas`
 * @endverbatim
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <Ecore.h>
#include <Ecore_Evas.h>
#include <Evas.h>

#include <stdlib.h>
#include <string.h>
#include <time.h>

#define W (200)
#define H (200)
#define TIMEOUT (1.0)

static void
_delete_request_cb(Ecore_Evas *ee EINA_UNUSED)
{
   ecore_main_loop_quit();
}

static void
_resize_cb(Ecore_Evas *ee)
{
   int w, h;

   ecore_evas_geometry_get(ee, NULL, NULL, &w, &h);
   evas_object_resize(ecore_evas_data_get(ee, "bg"), w, h);
}

static Eina_Bool
_mouse_pos_print(void *data)
{
   Efl_Input_Device *pointer;
   const Eina_List *devs, *l;

   devs = evas_device_list(ecore_evas_get(data), NULL);

   EINA_LIST_FOREACH(devs, l, pointer)
     {
        Evas_Coord x, y;
        Efl_Input_Device *seat;

        if (efl_input_device_type_get(pointer) != EFL_INPUT_DEVICE_CLASS_MOUSE)
          continue;
        ecore_evas_pointer_device_xy_get(data, pointer, &x, &y);
        seat = efl_input_device_seat_get(pointer);
        if (!seat)
          {
             fprintf(stderr, "Could not fetch the seat from mouse '%s'\n",
                     efl_input_device_name_get(pointer));
             continue;
          }
        printf("Mouse from seat '%s' is at (%d, %d)\n",
               efl_input_device_name_get(seat), x, y);
     }
   return EINA_TRUE;
}

static Eina_Bool
_cursor_set(Ecore_Evas *ee, Efl_Input_Device *pointer)
{
   Evas_Object *obj;

   obj = evas_object_rectangle_add(ecore_evas_get(ee));
   if (!obj) return EINA_FALSE;
   evas_object_color_set(obj, rand() % 256, rand() % 256, rand() % 256, 255);
   evas_object_resize(obj, 30, 30);
   evas_object_show(obj);
   ecore_evas_object_cursor_device_set(ee, pointer, obj, 0, 10, 10);
   return EINA_TRUE;
}

static void
_device_added(void *data, const Efl_Event *event)
{
   Efl_Input_Device *pointer = event->info;
   Efl_Input_Device *seat;

   if (efl_input_device_type_get(pointer) != EFL_INPUT_DEVICE_CLASS_MOUSE)
     return;
   seat = efl_input_device_seat_get(pointer);
   if (!seat)
     {
        fprintf(stderr, "Could not fetch the seat from pointer '%s'\n",
                efl_input_device_name_get(pointer));
        return;
     }
   printf("Setting cursor image at seat '%s'\n", efl_input_device_name_get(seat));

   if (!_cursor_set(data, pointer))
     fprintf(stderr, "Could not set the cursor for the pointer\n");
}

int
main(int argc EINA_UNUSED, char *argv[] EINA_UNUSED)
{
   const Eina_List *devs, *l;
   Efl_Input_Device *pointer;
   Ecore_Evas *ee;
   Evas_Object *bg;
   Evas *e;
   Ecore_Timer *t;
   const char *driver;

   srand(time(NULL));
   if (!ecore_evas_init())
     {
        fprintf(stderr, "Could not init the Ecore Evas\n");
        return EXIT_FAILURE;
     }

   ee = ecore_evas_new(NULL, 0, 0, W, H, NULL);
   if (!ee)
     {
        fprintf(stderr, "Could not create the Ecore Evas\n");
        goto err_ee;
     }

   driver = ecore_evas_engine_name_get(ee);
   printf("Using driver %s\n", driver);
   if ((!strcmp(driver, "fb")  || !strcmp(driver, "software_x11")) &&
       !ecore_evas_vnc_start(ee, "localhost", -1, NULL, NULL, NULL))
     {
        fprintf(stderr, "Could not init the VNC server\n");
        goto err_ee;
     }

   e = ecore_evas_get(ee);

   bg = evas_object_rectangle_add(e);
   if (!bg)
     {
        fprintf(stderr, "Coult not create the background\n");
        goto err_bg;
     }

   evas_object_resize(bg, W, H);
   evas_object_move(bg, 0, 0);
   evas_object_color_set(bg, 255, 255, 255, 255);
   evas_object_show(bg);
   ecore_evas_data_set(ee, "bg", bg);

   devs = evas_device_list(e, NULL);

   EINA_LIST_FOREACH(devs, l, pointer)
     {
        if (efl_input_device_type_get(pointer) == EFL_INPUT_DEVICE_CLASS_MOUSE &&
            !_cursor_set(ee, pointer))
          {
             fprintf(stderr, "Could not set the cursor image\n");
             goto err_ee;
          }
     }

   t = ecore_timer_add(TIMEOUT, _mouse_pos_print, ee);

   if (!t)
     {
        fprintf(stderr, "Coult not create the timer\n");
        goto err_ee;
     }

   efl_event_callback_add(e, EFL_CANVAS_EVENT_DEVICE_ADDED,
                          _device_added, ee);
   ecore_evas_callback_resize_set(ee, _resize_cb);
   ecore_evas_callback_delete_request_set(ee, _delete_request_cb);

   ecore_evas_show(ee);
   ecore_main_loop_begin();
   ecore_evas_free(ee);
   ecore_timer_del(t);
   ecore_evas_shutdown();
   return EXIT_SUCCESS;

 err_bg:
   ecore_evas_free(ee);
 err_ee:
   ecore_evas_shutdown();
   return EXIT_FAILURE;
}
