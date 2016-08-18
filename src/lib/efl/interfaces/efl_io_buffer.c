#define EFL_IO_READER_PROTECTED 1
#define EFL_IO_WRITER_PROTECTED 1

#include "config.h"
#include "Efl.h"

#define MY_CLASS EFL_IO_BUFFER_CLASS

/* TODO: FIXME, add to eina_error.h */
#define eina_error_from_errno(x) x

typedef struct _Efl_Io_Buffer_Data
{
   uint8_t *bytes;
   size_t allocated;
   size_t used;
   size_t limit;
   size_t position_read;
   size_t position_write;
   Eina_Bool closed;
   Eina_Bool can_read;
   Eina_Bool can_write;
} Efl_Io_Buffer_Data;

static Eina_Bool
_efl_io_buffer_realloc(Eo *o, Efl_Io_Buffer_Data *pd, size_t size)
{
   void *tmp;
   size_t limit = efl_io_buffer_limit_get(o);

   if ((limit > 0) && (size > limit))
     size = limit;

   if (pd->allocated == size) return EINA_FALSE;

   if (efl_io_sizer_size_get(o) > size)
     {
        if (efl_io_buffer_position_read_get(o) > size)
          efl_io_buffer_position_read_set(o, size);
        if (efl_io_buffer_position_write_get(o) > size)
          efl_io_buffer_position_write_set(o, size);

        /* no efl_io_sizer_size_set() since it could recurse! */
        pd->used = size;
        efl_event_callback_call(o, EFL_IO_SIZER_EVENT_CHANGED, NULL);
     }

   if (size == 0)
     {
        free(pd->bytes);
        tmp = NULL;
     }
   else
     {
        tmp = realloc(pd->bytes, size);
        EINA_SAFETY_ON_NULL_RETURN_VAL(tmp, EINA_FALSE);
     }

   pd->bytes = tmp;
   pd->allocated = size;
   efl_event_callback_call(o, EFL_IO_BUFFER_EVENT_REALLOCATED, NULL);
   return EINA_TRUE;
}

static Eina_Bool
_efl_io_buffer_realloc_rounded(Eo *o, Efl_Io_Buffer_Data *pd, size_t size)
{
   if ((size > 0) && (size < 128))
     size = ((size / 32) + 1) * 32;
   else if (size < 1024)
     size = ((size / 128) + 1) * 128;
   else if (size < 8192)
     size = ((size / 1024) + 1) * 1024;
   else
     size = ((size / 4096) + 1) * 4096;

   return _efl_io_buffer_realloc(o, pd, size);
}

EOLIAN static void
_efl_io_buffer_preallocate(Eo *o, Efl_Io_Buffer_Data *pd, size_t size)
{
   EINA_SAFETY_ON_TRUE_RETURN(efl_io_closer_closed_get(o));
   if (pd->allocated < size)
     _efl_io_buffer_realloc_rounded(o, pd, size);
}

EOLIAN static void
_efl_io_buffer_limit_set(Eo *o, Efl_Io_Buffer_Data *pd, size_t limit)
{
   EINA_SAFETY_ON_TRUE_RETURN(efl_io_closer_closed_get(o));

   if (pd->limit == limit) return;
   pd->limit = limit;

   if (pd->allocated > limit)
     _efl_io_buffer_realloc(o, pd, limit);

   efl_io_reader_can_read_set(o, efl_io_buffer_position_read_get(o) < efl_io_sizer_size_get(o));
   efl_io_writer_can_write_set(o, (limit == 0) ||
                               (efl_io_buffer_position_write_get(o) < limit));
}

EOLIAN static size_t
_efl_io_buffer_limit_get(Eo *o EINA_UNUSED, Efl_Io_Buffer_Data *pd)
{
   return pd->limit;
}

EOLIAN static Eina_Bool
_efl_io_buffer_slice_get(Eo *o, Efl_Io_Buffer_Data *pd, Eina_Slice *slice)
{
   if (slice)
     {
        slice->mem = pd->bytes;
        slice->len = efl_io_sizer_size_get(o);
     }
   EINA_SAFETY_ON_TRUE_RETURN_VAL(efl_io_closer_closed_get(o), EINA_FALSE);
   return EINA_TRUE;
}

EOLIAN static Eina_Binbuf *
_efl_io_buffer_binbuf_steal(Eo *o, Efl_Io_Buffer_Data *pd)
{
   Eina_Binbuf *ret;
   EINA_SAFETY_ON_TRUE_RETURN_VAL(efl_io_closer_closed_get(o), NULL);

   ret = eina_binbuf_manage_new(pd->bytes, efl_io_sizer_size_get(o), EINA_FALSE);
   EINA_SAFETY_ON_NULL_RETURN_VAL(ret, NULL);

   pd->bytes = NULL;
   pd->allocated = 0;
   efl_event_callback_call(o, EFL_IO_BUFFER_EVENT_REALLOCATED, NULL);
   efl_io_sizer_resize(o, 0);

   return ret;
}

EOLIAN static Efl_Object *
_efl_io_buffer_efl_object_finalize(Eo *o, Efl_Io_Buffer_Data *pd EINA_UNUSED)
{
   size_t limit;

   o = efl_finalize(efl_super(o, MY_CLASS));
   if (!o) return NULL;

   efl_io_reader_can_read_set(o, efl_io_buffer_position_read_get(o) < efl_io_sizer_size_get(o));
   limit = efl_io_buffer_limit_get(o);
   efl_io_writer_can_write_set(o, (limit == 0) ||
                               (efl_io_buffer_position_write_get(o) < limit));

   return o;
}

EOLIAN static void
_efl_io_buffer_efl_object_destructor(Eo *o, Efl_Io_Buffer_Data *pd)
{
   if (!efl_io_closer_closed_get(o))
     efl_io_closer_close(o);

   efl_destructor(efl_super(o, MY_CLASS));

   if (pd->bytes)
     {
        free(pd->bytes);
        pd->bytes = NULL;
        pd->allocated = 0;
        pd->used = 0;
        pd->position_read = 0;
        pd->position_write = 0;
     }
}

EOLIAN static Eina_Error
_efl_io_buffer_efl_io_reader_read(Eo *o, Efl_Io_Buffer_Data *pd, Eina_Rw_Slice *rw_slice)
{
   Eina_Slice ro_slice;
   size_t used, read_pos, available;

   EINA_SAFETY_ON_TRUE_GOTO(efl_io_closer_closed_get(o), error);
   EINA_SAFETY_ON_NULL_GOTO(rw_slice, error);

   used = efl_io_sizer_size_get(o);
   read_pos = efl_io_buffer_position_read_get(o);
   available = used - read_pos;

   if (rw_slice->len > available)
     {
        rw_slice->len = available;
        if (rw_slice->len == 0)
          return eina_error_from_errno(EAGAIN);
     }

   ro_slice.len = rw_slice->len;
   ro_slice.mem = pd->bytes + read_pos;

   eina_rw_slice_copy(*rw_slice, ro_slice);
   efl_io_buffer_position_read_set(o, read_pos + ro_slice.len);

   return 0;

 error:
   rw_slice->len = 0;
   return eina_error_from_errno(EINVAL);
}

EOLIAN static Eina_Bool
_efl_io_buffer_efl_io_reader_can_read_get(Eo *o EINA_UNUSED, Efl_Io_Buffer_Data *pd)
{
   return pd->can_read;
}

EOLIAN static void
_efl_io_buffer_efl_io_reader_can_read_set(Eo *o, Efl_Io_Buffer_Data *pd, Eina_Bool can_read)
{
   EINA_SAFETY_ON_TRUE_RETURN(efl_io_closer_closed_get(o));
   if (pd->can_read == can_read) return;
   pd->can_read = can_read;
   efl_event_callback_call(o, EFL_IO_READER_EVENT_CAN_READ_CHANGED, NULL);
}

EOLIAN static Eina_Bool
_efl_io_buffer_efl_io_reader_eos_get(Eo *o, Efl_Io_Buffer_Data *pd EINA_UNUSED)
{
   return efl_io_closer_closed_get(o) ||
     efl_io_buffer_position_read_get(o) >= efl_io_sizer_size_get(o);
}

EOLIAN static void
_efl_io_buffer_efl_io_reader_eos_set(Eo *o, Efl_Io_Buffer_Data *pd EINA_UNUSED, Eina_Bool is_eos)
{
   EINA_SAFETY_ON_TRUE_RETURN(efl_io_closer_closed_get(o));
   if (is_eos)
     efl_event_callback_call(o, EFL_IO_READER_EVENT_EOS, NULL);
}

EOLIAN static Eina_Error
_efl_io_buffer_efl_io_writer_write(Eo *o, Efl_Io_Buffer_Data *pd, Eina_Slice *slice, Eina_Slice *remaining)
{
   size_t available, todo, write_pos, limit;
   int err = EINVAL;

   EINA_SAFETY_ON_TRUE_GOTO(efl_io_closer_closed_get(o), error);
   EINA_SAFETY_ON_NULL_GOTO(slice, error);

   write_pos = efl_io_buffer_position_write_get(o);
   available = pd->allocated - write_pos;
   limit = efl_io_buffer_limit_get(o);

   err = ENOSPC;
   if (available >= slice->len)
     todo = slice->len;
   else if ((limit > 0) && (pd->allocated == limit)) goto error;
   else
     {
        _efl_io_buffer_realloc_rounded(o, pd, write_pos + slice->len);
        if (pd->allocated >= write_pos + slice->len)
          todo = slice->len;
        else
          todo = pd->allocated - write_pos;

        if (todo == 0) goto error;
     }

   memcpy(pd->bytes + write_pos, slice->mem, todo);
   if (remaining)
     {
        remaining->len = slice->len - todo;
        if (remaining->len)
          remaining->mem = slice->bytes + todo;
        else
          remaining->mem = NULL;
     }
   slice->len = todo;

   if (pd->used < write_pos + todo)
     {
        pd->used = write_pos + todo;
        efl_event_callback_call(o, EFL_IO_SIZER_EVENT_CHANGED, NULL);
        efl_io_reader_can_read_set(o, pd->position_read < pd->used);
     }
   efl_io_buffer_position_write_set(o, write_pos + todo);

   return 0;

 error:
   if (remaining) *remaining = *slice;
   slice->len = 0;
   return eina_error_from_errno(err);
}

EOLIAN static Eina_Bool
_efl_io_buffer_efl_io_writer_can_write_get(Eo *o EINA_UNUSED, Efl_Io_Buffer_Data *pd)
{
   return pd->can_write;
}

EOLIAN static void
_efl_io_buffer_efl_io_writer_can_write_set(Eo *o, Efl_Io_Buffer_Data *pd, Eina_Bool can_write)
{
   EINA_SAFETY_ON_TRUE_RETURN(efl_io_closer_closed_get(o));
   if (pd->can_write == can_write) return;
   pd->can_write = can_write;
   efl_event_callback_call(o, EFL_IO_WRITER_EVENT_CAN_WRITE_CHANGED, NULL);
}

EOLIAN static Eina_Error
_efl_io_buffer_efl_io_closer_close(Eo *o, Efl_Io_Buffer_Data *pd)
{
   EINA_SAFETY_ON_TRUE_RETURN_VAL(efl_io_closer_closed_get(o), eina_error_from_errno(EINVAL));
   efl_io_sizer_resize(o, 0);
   pd->closed = EINA_TRUE;
   efl_event_callback_call(o, EFL_IO_CLOSER_EVENT_CLOSED, NULL);
   return 0;
}

EOLIAN static Eina_Bool
_efl_io_buffer_efl_io_closer_closed_get(Eo *o EINA_UNUSED, Efl_Io_Buffer_Data *pd)
{
   return pd->closed;
}

EOLIAN static Eina_Error
_efl_io_buffer_efl_io_sizer_resize(Eo *o, Efl_Io_Buffer_Data *pd, uint64_t size)
{
   Eina_Error ret = 0;
   Eina_Bool reallocated;
   size_t old_size, pos_read, pos_write;

   EINA_SAFETY_ON_TRUE_RETURN_VAL(efl_io_closer_closed_get(o), eina_error_from_errno(EINVAL));

   if (efl_io_sizer_size_get(o) == size) return 0;

   old_size = pd->used;
   pd->used = size;

   efl_event_freeze(o);
   reallocated = _efl_io_buffer_realloc_rounded(o, pd, size);
   efl_event_thaw(o);

   if (size > pd->allocated)
     {
        pd->used = size = pd->allocated;
        ret = eina_error_from_errno(ENOSPC);
     }

   if (old_size < size)
     memset(pd->bytes + old_size, 0, size - old_size);

   pos_read = efl_io_buffer_position_read_get(o);
   if (pos_read > size)
     efl_io_buffer_position_read_set(o, size);
   else
     efl_io_reader_can_read_set(o, pos_read < size);

   pos_write = efl_io_buffer_position_write_get(o);
   if (pos_write > size)
     efl_io_buffer_position_write_set(o, size);
   else
     {
        size_t limit = efl_io_buffer_limit_get(o);
        efl_io_writer_can_write_set(o, (limit == 0) || (pos_write < limit));
     }

   efl_event_callback_call(o, EFL_IO_SIZER_EVENT_CHANGED, NULL);
   if (reallocated)
     efl_event_callback_call(o, EFL_IO_BUFFER_EVENT_REALLOCATED, NULL);

   return ret;
}

EOLIAN static uint64_t
_efl_io_buffer_efl_io_sizer_size_get(Eo *o EINA_UNUSED, Efl_Io_Buffer_Data *pd)
{
   return pd->used;
}

EOLIAN static Eina_Error
_efl_io_buffer_efl_io_positioner_seek(Eo *o, Efl_Io_Buffer_Data *pd EINA_UNUSED, int64_t offset, Efl_Io_Positioner_Whence whence)
{
   size_t size;

   EINA_SAFETY_ON_TRUE_RETURN_VAL(efl_io_closer_closed_get(o), eina_error_from_errno(EINVAL));

   size = efl_io_sizer_size_get(o);

   if (whence == EFL_IO_POSITIONER_WHENCE_CURRENT)
     {
        whence = EFL_IO_POSITIONER_WHENCE_START;
        offset += efl_io_positioner_position_get(o);
     }
   else if (whence == EFL_IO_POSITIONER_WHENCE_END)
     {
        whence = EFL_IO_POSITIONER_WHENCE_START;
        offset += size;
     }

   EINA_SAFETY_ON_TRUE_RETURN_VAL(whence != EFL_IO_POSITIONER_WHENCE_START,
                                  eina_error_from_errno(EINVAL));

   EINA_SAFETY_ON_TRUE_RETURN_VAL(offset < 0,
                                  eina_error_from_errno(EINVAL));

   EINA_SAFETY_ON_TRUE_RETURN_VAL((size_t)offset > size,
                                  eina_error_from_errno(EINVAL));

   efl_io_buffer_position_read_set(o, offset);
   efl_io_buffer_position_write_set(o, offset);

   return 0;
}

EOLIAN static uint64_t
_efl_io_buffer_efl_io_positioner_position_get(Eo *o, Efl_Io_Buffer_Data *pd EINA_UNUSED)
{
   uint64_t r = efl_io_buffer_position_read_get(o);
   uint64_t w = efl_io_buffer_position_write_get(o);
   /* if using Efl.Io.Positioner.position, on set it will do both
    * read/write to the same offset, however on Efl.Io.Reader.read it
    * will only update position_read (and similarly for
    * Efl.Io.Writer), thus on the next position.get we want the
    * greatest position.
    *
    * This allows the buffer to be used solely as reader or writer
    * without the need to know it have two internal offsets.
    */
   if (r >= w)
     return r;
   return w;
}

EOLIAN static Eina_Bool
_efl_io_buffer_position_read_set(Eo *o, Efl_Io_Buffer_Data *pd, uint64_t position)
{
   size_t size;
   Eina_Bool changed;

   EINA_SAFETY_ON_TRUE_RETURN_VAL(efl_io_closer_closed_get(o), EINA_FALSE);

   size = efl_io_sizer_size_get(o);
   EINA_SAFETY_ON_TRUE_RETURN_VAL(position > size, EINA_FALSE);

   if (pd->position_read == position) return EINA_TRUE;

   changed = efl_io_positioner_position_get(o) != position;

   pd->position_read = position;
   efl_event_callback_call(o, EFL_IO_BUFFER_EVENT_POSITION_READ_CHANGED, NULL);
   if (changed)
     efl_event_callback_call(o, EFL_IO_POSITIONER_EVENT_CHANGED, NULL);

   efl_io_reader_can_read_set(o, position < size);
   efl_io_reader_eos_set(o, position >= size);
   return EINA_TRUE;
}

EOLIAN static uint64_t
_efl_io_buffer_position_read_get(Eo *o EINA_UNUSED, Efl_Io_Buffer_Data *pd)
{
   return pd->position_read;
}

EOLIAN static Eina_Bool
_efl_io_buffer_position_write_set(Eo *o, Efl_Io_Buffer_Data *pd, uint64_t position)
{
   size_t size;
   size_t limit;
   Eina_Bool changed;

   EINA_SAFETY_ON_TRUE_RETURN_VAL(efl_io_closer_closed_get(o), EINA_FALSE);

   size = efl_io_sizer_size_get(o);
   EINA_SAFETY_ON_TRUE_RETURN_VAL(position > size, EINA_FALSE);

   if (pd->position_write == position) return EINA_TRUE;

   changed = efl_io_positioner_position_get(o) != position;

   pd->position_write = position;
   efl_event_callback_call(o, EFL_IO_BUFFER_EVENT_POSITION_WRITE_CHANGED, NULL);
   if (changed)
     efl_event_callback_call(o, EFL_IO_POSITIONER_EVENT_CHANGED, NULL);

   limit = efl_io_buffer_limit_get(o);
   efl_io_writer_can_write_set(o, (limit == 0) || (position < limit));
   return EINA_TRUE;
}

EOLIAN static uint64_t
_efl_io_buffer_position_write_get(Eo *o EINA_UNUSED, Efl_Io_Buffer_Data *pd)
{
   return pd->position_write;
}

#include "interfaces/efl_io_buffer.eo.c"
