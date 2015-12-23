/**
 * This example is a testing example, first model (from the right side) is
 * original model, the second one is convex hull maden in evas-3d,
 * the third one is convex hull maden in blender.
 *
 * Press "Right" or "Left" to switch models, the result of test (vertex count)
 * will be printed in console window.
 *
 * @verbatim
 * gcc -o evas-3d-hull evas-3d-hull.c -g `pkg-config --libs --cflags efl evas ecore ecore-evas eo eina` -lm
 * @endverbatim
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#define PACKAGE_EXAMPLES_DIR "."
#define EFL_EO_API_SUPPORT
#define EFL_BETA_API_SUPPORT
#endif

#include <Eo.h>
#include <Evas.h>
#include <Ecore.h>
#include <Ecore_Evas.h>
#include "evas-common.h"

#define  WIDTH          1024
#define  HEIGHT         1024
#define  TESTS_COUNT    8
#define  SCALE_SPHERE   2.0
#define  SCALE_TORUS    2.7
#define  SCALE_HOME     0.7
#define  SCALE_SONIC    0.08
#define  SCALE_EAGLE    0.06

typedef struct _Scene_Data
{
   Eo *scene;
   Eo *root_node;
   Eo *camera_node;
   Eo *light_node;
   Eo *mesh_node;
   Eo *mesh_node_convex_hull;
   Eo *mesh_node_blender;

   Eo *camera;
   Eo *light;
   Eo *mesh_sphere;
   Eo *mesh_torus;
   Eo *mesh_cube;
   Eo *mesh_plain;
   Eo *mesh_column;
   Eo *mesh_home;
   Eo *mesh_sonic;
   Eo *mesh_eagle;
   Eo *mesh_sphere_ch;
   Eo *mesh_torus_ch;
   Eo *mesh_cube_ch;
   Eo *mesh_plain_ch;
   Eo *mesh_column_ch;
   Eo *mesh_home_ch;
   Eo *mesh_sonic_ch;
   Eo *mesh_eagle_ch;
   Eo *mesh_blender_sphere;
   Eo *mesh_blender_torus;
   Eo *mesh_blender_cube;
   Eo *mesh_blender_plain;
   Eo *mesh_blender_column;
   Eo *mesh_blender_home;
   Eo *mesh_blender_sonic;
   Eo *mesh_blender_eagle;
   Eo *material;
} Scene_Data;

int rr;

#define MODEL_MESH_INIT(name, model)                                              \
   data->mesh_##name = eo_add(EVAS_CANVAS3D_MESH_CLASS, evas);                          \
   eo_do(data->mesh_##name,                                                       \
         efl_file_set(model, NULL),                                               \
         evas_canvas3d_mesh_vertex_assembly_set(EVAS_CANVAS3D_VERTEX_ASSEMBLY_TRIANGLES),     \
         evas_canvas3d_mesh_shade_mode_set(EVAS_CANVAS3D_SHADE_MODE_DIFFUSE),                   \
         evas_canvas3d_mesh_frame_material_set(0, data->material));

#define CONVEX_HULL_MESH_INIT(name)                                                              \
   vert = eina_inarray_new(sizeof(float), 1); \
   ind = eina_inarray_new(sizeof(unsigned short int), 1);\
   eo_do(data->mesh_##name,                                                                      \
         evas_canvas3d_mesh_convex_hull_data_get(0, vert, ind));  \
   vertex = (float*) vert->members;\
   index = ind->members;\
   data->mesh_##name##_ch = eo_add(EVAS_CANVAS3D_MESH_CLASS, evas);                                    \
   eo_do(data->mesh_##name##_ch,                                                                 \
         evas_canvas3d_mesh_vertex_count_set((vert->len / 10)),                                         \
         evas_canvas3d_mesh_frame_add(0),                                                              \
         evas_canvas3d_mesh_frame_vertex_data_copy_set(0, EVAS_CANVAS3D_VERTEX_ATTRIB_POSITION,                     \
                                            10 * sizeof(float), &vertex[ 0]),                      \
         evas_canvas3d_mesh_frame_vertex_data_copy_set(0, EVAS_CANVAS3D_VERTEX_ATTRIB_NORMAL,                       \
                                            10 * sizeof(float), &vertex[ 3]),                      \
         evas_canvas3d_mesh_frame_vertex_data_copy_set(0, EVAS_CANVAS3D_VERTEX_ATTRIB_COLOR,                        \
                                            10 * sizeof(float), &vertex[ 6]),                      \
         evas_canvas3d_mesh_index_data_copy_set(EVAS_CANVAS3D_INDEX_FORMAT_UNSIGNED_SHORT,                   \
                                     ind->len, &index[0]),                                   \
         evas_canvas3d_mesh_vertex_assembly_set(EVAS_CANVAS3D_VERTEX_ASSEMBLY_TRIANGLES),                    \
         evas_canvas3d_mesh_shade_mode_set(EVAS_CANVAS3D_SHADE_MODE_VERTEX_COLOR),                           \
         evas_canvas3d_mesh_frame_material_set(0, data->material));                                    \
         free(vert);                                                                             \
         free(ind);

#define SWITCH_MESH(index, name, scale)                                           \
   case index:                                                                    \
     {                                                                            \
        eo_do(scene->mesh_node, list = evas_canvas3d_node_mesh_list_get());             \
        mesh = eina_list_nth(list, 0);                                            \
        eo_do(scene->mesh_node,                                                   \
              evas_canvas3d_node_mesh_del(mesh),                                        \
              evas_canvas3d_node_mesh_add(scene->mesh_##name),                          \
              evas_canvas3d_node_scale_set(scale, scale, scale));                       \
        eo_do(scene->mesh_node_convex_hull, list = evas_canvas3d_node_mesh_list_get()); \
        mesh = eina_list_nth(list, 0);                                            \
        eo_do(scene->mesh_node_convex_hull,                                       \
              evas_canvas3d_node_mesh_del(mesh),                                        \
              evas_canvas3d_node_mesh_add(scene->mesh_##name##_ch),                     \
              evas_canvas3d_node_scale_set(scale, scale, scale));                       \
        eo_do(scene->mesh_node_blender, list = evas_canvas3d_node_mesh_list_get());     \
        mesh = eina_list_nth(list, 0);                                            \
        eo_do(scene->mesh_node_blender,                                           \
              evas_canvas3d_node_mesh_del(mesh),                                        \
              evas_canvas3d_node_mesh_add(scene->mesh_blender_##name),                  \
              evas_canvas3d_node_scale_set(scale, scale, scale));                       \
        _print_result(scene->mesh_##name##_ch, scene->mesh_blender_##name);       \
        break;                                                                    \
     }

static const char *home_ch = PACKAGE_EXAMPLES_DIR EVAS_MODEL_FOLDER "/sweet_home_without_tex_coords.obj";
static const char *sonic_ch = PACKAGE_EXAMPLES_DIR EVAS_MODEL_FOLDER "/sonic.md2";

static const char *column_ch = PACKAGE_EXAMPLES_DIR EVAS_CONVEX_HULL_FOLDER "/column_test.ply";
static const char *plain_ch = PACKAGE_EXAMPLES_DIR EVAS_CONVEX_HULL_FOLDER "/plain.ply";
static const char *plain = PACKAGE_EXAMPLES_DIR EVAS_CONVEX_HULL_FOLDER "/plain_blender_ch.ply";
static const char *sphere = PACKAGE_EXAMPLES_DIR EVAS_CONVEX_HULL_FOLDER "/sphere_blender_ch.ply";
static const char *torus = PACKAGE_EXAMPLES_DIR EVAS_CONVEX_HULL_FOLDER "/torus_blender_ch.ply";
static const char *cube = PACKAGE_EXAMPLES_DIR EVAS_CONVEX_HULL_FOLDER "/cube_blender_ch.ply";
static const char *column = PACKAGE_EXAMPLES_DIR EVAS_CONVEX_HULL_FOLDER "/column_blender_ch.ply";
static const char *home = PACKAGE_EXAMPLES_DIR EVAS_CONVEX_HULL_FOLDER "/home_blender_ch.obj";
static const char *sonic = PACKAGE_EXAMPLES_DIR EVAS_CONVEX_HULL_FOLDER "/sonic_blender_ch.ply";
static const char *eagle = PACKAGE_EXAMPLES_DIR EVAS_CONVEX_HULL_FOLDER "/eagle_blender_ch.ply";

static Ecore_Evas *ecore_evas = NULL;
static Evas *evas = NULL;
static Eo *background = NULL;
static Eo *image = NULL;
static int next_model = 0;

static void
_print_result(Evas_Canvas3D_Mesh *mesh, Evas_Canvas3D_Mesh *convex_mesh)
{
   int v_count = 0;
   eo_do(mesh, v_count = evas_canvas3d_mesh_vertex_count_get());

   printf("Vertex count is %d for convex hull\n",
           v_count);

   eo_do(convex_mesh, v_count = evas_canvas3d_mesh_vertex_count_get());

   printf("Vertex count is %d for blender convex hull\n\n",
           v_count);

   return;
}

static void
_on_delete(Ecore_Evas *ee EINA_UNUSED)
{
   ecore_main_loop_quit();
}

static void
_on_canvas_resize(Ecore_Evas *ee)
{
   int w, h;

   ecore_evas_geometry_get(ee, NULL, NULL, &w, &h);
   eo_do(background, efl_gfx_size_set(w, h));
   eo_do(image, efl_gfx_size_set(w, h));
}

static Eina_Bool
_animate_scene(void *data)
{
   static float angle = 0.0f;
   Scene_Data *scene = (Scene_Data *)data;

   angle += 0.5;
   if (angle >= 360.0)
     angle = 0.0;

   eo_do(scene->mesh_node,
         evas_canvas3d_node_orientation_angle_axis_set(angle, 1.0, 1.0, 1.0));

   eo_do(scene->mesh_node_convex_hull,
         evas_canvas3d_node_orientation_angle_axis_set(angle, 1.0, 1.0, 1.0));

   eo_do(scene->mesh_node_blender,
         evas_canvas3d_node_orientation_angle_axis_set(angle, 1.0, 1.0, 1.0));

   return EINA_TRUE;
}

static void
_key_down(void *data,
         Evas *e EINA_UNUSED,
         Evas_Object *eo EINA_UNUSED,
         void *event_info)
{
   const Eina_List *list = NULL;
   Eo *mesh = NULL;
   Evas_Event_Key_Down *ev = event_info;
   Scene_Data *scene = (Scene_Data *)data;

   if (!strcmp(ev->key, "Right"))
     next_model++;
   else if (!strcmp(ev->key, "Left"))
     next_model--;

   if (next_model == -1)
     next_model = TESTS_COUNT - 1;
   else if (next_model == TESTS_COUNT)
     next_model = 0;

   switch (next_model)
     {
      SWITCH_MESH(0, sphere, SCALE_SPHERE)
      SWITCH_MESH(1, torus, SCALE_TORUS)
      SWITCH_MESH(2, cube, 1.0)
      SWITCH_MESH(3, plain, 1.0)
      SWITCH_MESH(4, column, 1.0)
      SWITCH_MESH(5, home, SCALE_HOME)
      SWITCH_MESH(6, sonic, SCALE_SONIC)
      SWITCH_MESH(7, eagle, SCALE_EAGLE)
      default:
        break;
     }
}

static void
_camera_setup(Scene_Data *data)
{
   data->camera = eo_add(EVAS_CANVAS3D_CAMERA_CLASS, evas);

   eo_do(data->camera,
         evas_canvas3d_camera_projection_perspective_set(60.0, 1.0, 2.0, 50.0));

   data->camera_node =
      eo_add(EVAS_CANVAS3D_NODE_CLASS, evas,
                    evas_canvas3d_node_constructor(EVAS_CANVAS3D_NODE_TYPE_CAMERA));
   eo_do(data->camera_node,
         evas_canvas3d_node_camera_set(data->camera),
         evas_canvas3d_node_position_set(0.0, 0.0, 10.0),
         evas_canvas3d_node_look_at_set(EVAS_CANVAS3D_SPACE_PARENT, 0.0, 0.0, 0.0,
                                  EVAS_CANVAS3D_SPACE_PARENT, 0.0, 1.0, 0.0));
   eo_do(data->root_node, evas_canvas3d_node_member_add(data->camera_node));
}

static void
_light_setup(Scene_Data *data)
{
   data->light = eo_add(EVAS_CANVAS3D_LIGHT_CLASS, evas);
   eo_do(data->light,
         evas_canvas3d_light_ambient_set(0.2, 0.2, 0.2, 1.0),
         evas_canvas3d_light_diffuse_set(1.0, 1.0, 1.0, 1.0),
         evas_canvas3d_light_specular_set(1.0, 1.0, 1.0, 1.0));

   data->light_node =
      eo_add(EVAS_CANVAS3D_NODE_CLASS, evas,
                    evas_canvas3d_node_constructor(EVAS_CANVAS3D_NODE_TYPE_LIGHT));
   eo_do(data->light_node,
         evas_canvas3d_node_light_set(data->light),
         evas_canvas3d_node_position_set(0.0, 0.0, 10.0),
         evas_canvas3d_node_look_at_set(EVAS_CANVAS3D_SPACE_PARENT, 0.0, 0.0, 0.0,
                                  EVAS_CANVAS3D_SPACE_PARENT, 0.0, 1.0, 0.0));
   eo_do(data->root_node, evas_canvas3d_node_member_add(data->light_node));
}

static void
_mesh_setup(Scene_Data *data)
{
   /*int vertex_ch_count = 0, index_ch_count = 0;
   float *vert = NULL;
   unsigned short int *ind = NULL;
   int count = 0;
   float *v = NULL;
   unsigned short int *indx = NULL;*/

   Eina_Inarray *vert, *ind;
   float *vertex;
   unsigned short int *index;
   int i = 0;
   Eo *primitive = NULL;
   /* Setup material. */

   data->material = eo_add(EVAS_CANVAS3D_MATERIAL_CLASS, evas);
   eo_do(data->material,
         evas_canvas3d_material_enable_set(EVAS_CANVAS3D_MATERIAL_ATTRIB_AMBIENT, EINA_TRUE),
         evas_canvas3d_material_enable_set(EVAS_CANVAS3D_MATERIAL_ATTRIB_DIFFUSE, EINA_TRUE),
         evas_canvas3d_material_enable_set(EVAS_CANVAS3D_MATERIAL_ATTRIB_SPECULAR, EINA_TRUE),

         evas_canvas3d_material_color_set(EVAS_CANVAS3D_MATERIAL_ATTRIB_AMBIENT, 0.2, 0.2, 0.2, 1.0),
         evas_canvas3d_material_color_set(EVAS_CANVAS3D_MATERIAL_ATTRIB_DIFFUSE, 0.8, 0.8, 0.8, 1.0),
         evas_canvas3d_material_color_set(EVAS_CANVAS3D_MATERIAL_ATTRIB_SPECULAR, 1.0, 1.0, 1.0, 1.0),
         evas_canvas3d_material_shininess_set(100.0));

   /* Setup mesh sphere */
   primitive = eo_add(EVAS_CANVAS3D_PRIMITIVE_CLASS, evas);
   eo_do(primitive,
         evas_canvas3d_primitive_form_set(EVAS_CANVAS3D_MESH_PRIMITIVE_SPHERE),
         evas_canvas3d_primitive_precision_set(10));
   data->mesh_sphere = eo_add(EVAS_CANVAS3D_MESH_CLASS, evas);
   eo_do(data->mesh_sphere, evas_canvas3d_mesh_from_primitive_set(0, primitive));
   eo_do(data->mesh_sphere,
         evas_canvas3d_mesh_vertex_assembly_set(EVAS_CANVAS3D_VERTEX_ASSEMBLY_TRIANGLES),

         evas_canvas3d_mesh_shade_mode_set(EVAS_CANVAS3D_SHADE_MODE_PHONG),

         evas_canvas3d_mesh_frame_material_set(0, data->material));

   /* Setup mesh torus */
   data->mesh_torus = eo_add(EVAS_CANVAS3D_MESH_CLASS, evas);
   eo_do(primitive,
         evas_canvas3d_primitive_form_set(EVAS_CANVAS3D_MESH_PRIMITIVE_TORUS),
         evas_canvas3d_primitive_precision_set(50));
   eo_do(data->mesh_torus, evas_canvas3d_mesh_from_primitive_set(0, primitive));
   eo_do(data->mesh_torus,
         evas_canvas3d_mesh_vertex_assembly_set(EVAS_CANVAS3D_VERTEX_ASSEMBLY_TRIANGLES),
         evas_canvas3d_mesh_shade_mode_set(EVAS_CANVAS3D_SHADE_MODE_PHONG),
         evas_canvas3d_mesh_frame_material_set(0, data->material));

   /* Setup mesh cube */
   data->mesh_cube = eo_add(EVAS_CANVAS3D_MESH_CLASS, evas);
   eo_do(primitive,
         evas_canvas3d_primitive_form_set(EVAS_CANVAS3D_MESH_PRIMITIVE_CUBE),
         evas_canvas3d_primitive_precision_set(50));
   eo_do(data->mesh_cube, evas_canvas3d_mesh_from_primitive_set(0, primitive));
   eo_do(data->mesh_cube,
         evas_canvas3d_mesh_vertex_assembly_set(EVAS_CANVAS3D_VERTEX_ASSEMBLY_TRIANGLES),
         evas_canvas3d_mesh_shade_mode_set(EVAS_CANVAS3D_SHADE_MODE_PHONG),
         evas_canvas3d_mesh_frame_material_set(0, data->material));

   MODEL_MESH_INIT(plain, plain_ch)
   MODEL_MESH_INIT(column, column_ch)
   MODEL_MESH_INIT(home, home_ch)
   MODEL_MESH_INIT(sonic, sonic_ch)
   MODEL_MESH_INIT(eagle, PACKAGE_EXAMPLES_DIR "/shooter/assets/models/eagle.md2")

   MODEL_MESH_INIT(blender_sphere, sphere)
   MODEL_MESH_INIT(blender_torus, torus)
   MODEL_MESH_INIT(blender_cube, cube)
   MODEL_MESH_INIT(blender_plain, plain)
   MODEL_MESH_INIT(blender_column, column)
   MODEL_MESH_INIT(blender_home, home)
   MODEL_MESH_INIT(blender_eagle, eagle)
   MODEL_MESH_INIT(blender_sonic, sonic)

   data->mesh_node =
      eo_add(EVAS_CANVAS3D_NODE_CLASS, evas,
                    evas_canvas3d_node_constructor(EVAS_CANVAS3D_NODE_TYPE_MESH));
   eo_do(data->root_node, evas_canvas3d_node_member_add(data->mesh_node));
   eo_do(data->mesh_node,
         evas_canvas3d_node_mesh_add(data->mesh_sphere),
         evas_canvas3d_node_scale_set(SCALE_SPHERE, SCALE_SPHERE, SCALE_SPHERE),
         evas_canvas3d_node_position_set(3.0, 0.0, 0.0));

   CONVEX_HULL_MESH_INIT(sphere)
   CONVEX_HULL_MESH_INIT(torus)
   CONVEX_HULL_MESH_INIT(cube)
   CONVEX_HULL_MESH_INIT(plain)
   CONVEX_HULL_MESH_INIT(column)
   CONVEX_HULL_MESH_INIT(home)
   CONVEX_HULL_MESH_INIT(sonic)
   CONVEX_HULL_MESH_INIT(eagle)

   _print_result(data->mesh_sphere_ch, data->mesh_blender_sphere);

   data->mesh_node_convex_hull =
      eo_add(EVAS_CANVAS3D_NODE_CLASS, evas,
                    evas_canvas3d_node_constructor(EVAS_CANVAS3D_NODE_TYPE_MESH));
   eo_do(data->root_node, evas_canvas3d_node_member_add(data->mesh_node_convex_hull));
   eo_do(data->mesh_node_convex_hull,
         evas_canvas3d_node_position_set(0.0, 0.0, 0.0),
         evas_canvas3d_node_scale_set(SCALE_SPHERE, SCALE_SPHERE, SCALE_SPHERE),
         evas_canvas3d_node_mesh_add(data->mesh_sphere_ch));

   data->mesh_node_blender =
      eo_add(EVAS_CANVAS3D_NODE_CLASS, evas,
                    evas_canvas3d_node_constructor(EVAS_CANVAS3D_NODE_TYPE_MESH));
   eo_do(data->root_node, evas_canvas3d_node_member_add(data->mesh_node_blender));
   eo_do(data->mesh_node_blender,
         evas_canvas3d_node_position_set(-3.0, 0.0, 0.0),
         evas_canvas3d_node_scale_set(SCALE_SPHERE, SCALE_SPHERE, SCALE_SPHERE),
         evas_canvas3d_node_mesh_add(data->mesh_blender_sphere));
}

static void
_scene_setup(Scene_Data *data)
{
   data->scene = eo_add(EVAS_CANVAS3D_SCENE_CLASS, evas);
   eo_do(data->scene,
         evas_canvas3d_scene_size_set(WIDTH, HEIGHT);
         evas_canvas3d_scene_background_color_set(0.0, 0.0, 0.0, 0.0));

   data->root_node =
      eo_add(EVAS_CANVAS3D_NODE_CLASS, evas,
                    evas_canvas3d_node_constructor(EVAS_CANVAS3D_NODE_TYPE_NODE));

   _camera_setup(data);
   _light_setup(data);
   _mesh_setup(data);

   eo_do(data->scene,
         evas_canvas3d_scene_root_node_set(data->root_node),
         evas_canvas3d_scene_camera_node_set(data->camera_node));
}

int
main(void)
{
   Ecore_Animator *anim;
   Scene_Data data;

   //Unless Evas 3D supports Software renderer, we set gl backened forcely.
   setenv("ECORE_EVAS_ENGINE", "opengl_x11", 1);
   int stride_pos;

   if (!ecore_evas_init()) return 0;

   ecore_evas = ecore_evas_new(NULL, 10, 10, WIDTH, HEIGHT, NULL);
   if (!ecore_evas) return 0;

   ecore_evas_callback_delete_request_set(ecore_evas, _on_delete);
   ecore_evas_callback_resize_set(ecore_evas, _on_canvas_resize);
   ecore_evas_show(ecore_evas);

   evas = ecore_evas_get(ecore_evas);

   _scene_setup(&data);

   /* Add a background rectangle objects. */
   background = eo_add(EVAS_RECTANGLE_CLASS, evas);
   eo_do(background,
         efl_gfx_color_set(0, 0, 0, 255),
         efl_gfx_size_set(WIDTH, HEIGHT),
         efl_gfx_visible_set(EINA_TRUE));

   /* Add an image object for 3D scene rendering. */
   image = evas_object_image_filled_add(evas);
   eo_do(image,
         efl_gfx_size_set(WIDTH, HEIGHT),
         efl_gfx_visible_set(EINA_TRUE));

   evas_object_show(image),
   evas_object_focus_set(image, EINA_TRUE);

   /* Set the image object as render target for 3D scene. */
   eo_do(image, evas_obj_image_scene_set(data.scene));

   evas_object_event_callback_add(image, EVAS_CALLBACK_KEY_DOWN, _key_down, &data);

   /* Add animator. */
   ecore_animator_frametime_set(0.008);
   anim = ecore_animator_add(_animate_scene, &data);

   /* Enter main loop. */
   ecore_main_loop_begin();
   ecore_animator_del(anim);

   ecore_evas_free(ecore_evas);
   ecore_evas_shutdown();

   return 0;
}

