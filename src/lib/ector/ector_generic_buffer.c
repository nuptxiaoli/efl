#ifdef HAVE_CONFIG_H
# include "config.h"
#else
# define EFL_BETA_API_SUPPORT
#endif

#include <Eo.h>
#include "ector_private.h"
#include "ector_generic_buffer.eo.h"

#define MY_CLASS ECTOR_GENERIC_BUFFER_CLASS

EOLIAN static Efl_Gfx_Colorspace
_ector_generic_buffer_cspace_get(Eo *obj EINA_UNUSED, Ector_Generic_Buffer_Data *pd)
{
   return pd->cspace;
}

EOLIAN static void
_ector_generic_buffer_border_get(Eo *obj EINA_UNUSED, Ector_Generic_Buffer_Data *pd EINA_UNUSED, int *l, int *r, int *t, int *b)
{
   if (l) *l = pd->l;
   if (r) *r = pd->r;
   if (t) *t = pd->t;
   if (b) *b = pd->b;
}

EOLIAN static void
_ector_generic_buffer_size_get(Eo *obj EINA_UNUSED, Ector_Generic_Buffer_Data *pd, int *w, int *h)
{
   if (w) *w = pd->w;
   if (h) *h = pd->h;
}

EOLIAN static Ector_Buffer_Flag
_ector_generic_buffer_flags_get(Eo *obj EINA_UNUSED, Ector_Generic_Buffer_Data *pd EINA_UNUSED)
{
   return ECTOR_BUFFER_FLAG_NONE;
}

EOLIAN static Eo_Base *
_ector_generic_buffer_eo_base_constructor(Eo *obj, Ector_Generic_Buffer_Data *pd)
{
   eo_do_super(obj, MY_CLASS, obj = eo_constructor());
   pd->eo = obj;

   return obj;
}

#include "ector_generic_buffer.eo.c"
