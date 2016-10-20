#include "elm_test_focus_common.h"
#include "focus_test_sub.eo.h"

typedef struct {

} Focus_Test_Sub_Data;

EOLIAN static Eina_List*
_focus_test_sub_efl_ui_focus_manager_sub_select_set(Eo *obj EINA_UNUSED, Focus_Test_Sub_Data *pd EINA_UNUSED, Eina_Iterator *objects)
{
   Eina_List *list = NULL;
   Efl_Ui_Focus_Object *o;

   EINA_ITERATOR_FOREACH(objects, o)
     {
        list = eina_list_append(list, o);
     }

   eina_iterator_free(objects);

   return list;
}

EOLIAN static void
_focus_test_sub_efl_ui_focus_object_geometry_get(Eo *obj EINA_UNUSED, Focus_Test_Sub_Data *pd EINA_UNUSED, Eina_Rectangle *rect EINA_UNUSED)
{
  rect->y = rect->x = 0;
  rect->w = rect->h = 20;
}

EOLIAN static Eina_Bool
_focus_test_sub_efl_ui_focus_object_focus_get(Eo *obj EINA_UNUSED, Focus_Test_Sub_Data *pd EINA_UNUSED)
{
   return EINA_FALSE;
}

EOLIAN static Efl_Ui_Focus_Manager*
_focus_test_sub_efl_ui_focus_user_manager_get(Eo *obj, Focus_Test_Sub_Data *pd EINA_UNUSED)
{
   return efl_parent_get(obj);
}

static Eina_List *registered;
static Eina_List *unregistered;

static Eina_Bool
_register(Eo *eo, void* data EINA_UNUSED, Efl_Ui_Focus_Object *child, Efl_Ui_Focus_Object *parent, Efl_Ui_Focus_Manager *manager)
{
   registered = eina_list_append(registered, child);
   printf("REGISTERED %p %s\n", child, efl_name_get(child));

   return efl_ui_focus_manager_register(efl_super(eo, EFL_OBJECT_OVERRIDE_CLASS) , child, parent, manager);
}

static void
_unregister(Eo *eo, void* data EINA_UNUSED, Efl_Ui_Focus_Object *child)
{
   unregistered = eina_list_append(unregistered, child);
   printf("UNREGISTERED %p %s\n", child, efl_name_get(child));

   efl_ui_focus_manager_unregister(efl_super(eo, EFL_OBJECT_OVERRIDE_CLASS) , child);
}

static Eina_Bool
_set_equal(Eina_List *a, Eina_List *b)
{
   Eina_List *n;
   void *d;

   if (eina_list_count(a) != eina_list_count(b)) return EINA_FALSE;

   EINA_LIST_FOREACH(a, n, d)
     {
        if (!eina_list_data_find(b, d)) return EINA_FALSE;
     }
   return EINA_TRUE;
}

#include "focus_test_sub.eo.c"

static void
_setup(Efl_Ui_Focus_Manager **m, Efl_Ui_Focus_Manager_Sub **sub, Efl_Ui_Focus_Object **r)
{

   TEST_OBJ_NEW(root, 10, 10, 10, 10);
   TEST_OBJ_NEW(root_manager, 0, 20, 20, 20);

   EFL_OPS_DEFINE(manager_tracker,
    EFL_OBJECT_OP_FUNC(efl_ui_focus_manager_register, _register),
    EFL_OBJECT_OP_FUNC(efl_ui_focus_manager_unregister, _unregister),
    );

   Efl_Ui_Focus_Manager *manager = efl_add(EFL_UI_FOCUS_MANAGER_CLASS, NULL,
    efl_ui_focus_manager_root_set(efl_added, root_manager)
   );
   //flush now all changes
   efl_event_callback_call(manager, EFL_UI_FOCUS_MANAGER_EVENT_PRE_FLUSH, NULL);
   registered = NULL;
   unregistered = NULL;

   efl_object_override(manager, &manager_tracker);

   Efl_Ui_Focus_Manager_Sub *subm = efl_add(FOCUS_TEST_SUB_CLASS, manager,
    efl_ui_focus_manager_sub_parent_set(efl_added, root_manager),
    efl_ui_focus_manager_root_set(efl_added, root)
   );

   efl_event_callback_call(manager, EFL_UI_FOCUS_MANAGER_EVENT_PRE_FLUSH, NULL);

   *sub = subm;
   *m = manager;
   *r = root;
}

START_TEST(correct_register)
{
   Eina_List *set1 = NULL;
   Efl_Ui_Focus_Object *root;
   Efl_Ui_Focus_Manager *manager, *sub;
   elm_init(0, NULL);

   _setup(&manager, &sub, &root);

   TEST_OBJ_NEW(child1, 0, 0, 10, 10);
   TEST_OBJ_NEW(child2, 10, 0, 10, 10);
   TEST_OBJ_NEW(child3, 0, 10, 10, 10);

   set1 = eina_list_append(set1, sub);
   set1 = eina_list_append(set1, root);
   set1 = eina_list_append(set1, child1);
   set1 = eina_list_append(set1, child2);
   set1 = eina_list_append(set1, child3);

   //test register stuff
   efl_ui_focus_manager_register(sub, child1, root, NULL);
   efl_ui_focus_manager_register(sub, child2, root, NULL);
   efl_ui_focus_manager_register(sub, child3, root, NULL);
   //now force submanager to flush things
   efl_event_callback_call(manager, EFL_UI_FOCUS_MANAGER_EVENT_PRE_FLUSH, NULL);
   ck_assert_ptr_eq(unregistered, NULL);
   fail_if(!_set_equal(registered, set1));

   efl_del(sub);
   efl_del(manager);
   efl_del(root);
   efl_del(child1);
   efl_del(child2);
   efl_del(child3);
   elm_shutdown();
}
END_TEST

START_TEST(correct_unregister)
{
   Eina_List *set = NULL;
   Efl_Ui_Focus_Object *root;
   Efl_Ui_Focus_Manager *manager, *sub;
   elm_init(0, NULL);

   _setup(&manager, &sub, &root);

   TEST_OBJ_NEW(child1, 0, 0, 10, 10);
   TEST_OBJ_NEW(child2, 10, 0, 10, 10);
   TEST_OBJ_NEW(child3, 0, 10, 10, 10);

   set = eina_list_append(set, child3);

   //test register stuff
   efl_ui_focus_manager_register(sub, child1, root, NULL);
   efl_ui_focus_manager_register(sub, child2, root, NULL);
   efl_ui_focus_manager_register(sub, child3, root, NULL);
   efl_event_callback_call(manager, EFL_UI_FOCUS_MANAGER_EVENT_PRE_FLUSH, NULL);
   eina_list_free(unregistered);
   unregistered = NULL;
   eina_list_free(registered);
   registered = NULL;

   //test unregister stuff
   efl_ui_focus_manager_unregister(sub, child3);
   efl_event_callback_call(manager, EFL_UI_FOCUS_MANAGER_EVENT_PRE_FLUSH, NULL);
   ck_assert_ptr_eq(registered, NULL);
   fail_if(!_set_equal(unregistered, set));
   eina_list_free(unregistered);
   unregistered = NULL;

   efl_del(sub);
   efl_del(manager);
   efl_del(root);
   efl_del(child1);
   efl_del(child2);
   efl_del(child3);
   elm_shutdown();
}
END_TEST

START_TEST(correct_un_register)
{
   Eina_List *set_add = NULL, *set_del = NULL;
   Efl_Ui_Focus_Object *root;
   Efl_Ui_Focus_Manager *manager, *sub;
   elm_init(0, NULL);

   _setup(&manager, &sub, &root);

   TEST_OBJ_NEW(child1, 0, 0, 10, 10);
   TEST_OBJ_NEW(child2, 10, 0, 10, 10);
   TEST_OBJ_NEW(child3, 0, 10, 10, 10);

   set_add = eina_list_append(set_add, child2);
   set_del = eina_list_append(set_del, child3);
   //test register stuff
   efl_ui_focus_manager_register(sub, child1, root, NULL);
   efl_ui_focus_manager_register(sub, child3, root, NULL);
   efl_event_callback_call(manager, EFL_UI_FOCUS_MANAGER_EVENT_PRE_FLUSH, NULL);
   eina_list_free(unregistered);
   unregistered = NULL;
   eina_list_free(registered);
   registered = NULL;

   //test unregister stuff
   efl_ui_focus_manager_unregister(sub, child3);
   efl_ui_focus_manager_register(sub, child2, root, NULL);
   efl_event_callback_call(manager, EFL_UI_FOCUS_MANAGER_EVENT_PRE_FLUSH, NULL);
   fail_if(!_set_equal(registered, set_add));
   fail_if(!_set_equal(unregistered, set_del));

   efl_del(sub);
   efl_del(manager);
   efl_del(root);
   efl_del(child1);
   efl_del(child2);
   efl_del(child3);
   elm_shutdown();
}
END_TEST

void elm_test_focus_sub(TCase *tc)
{
   tcase_add_test(tc, correct_register);
   tcase_add_test(tc, correct_unregister);
   tcase_add_test(tc, correct_un_register);
}