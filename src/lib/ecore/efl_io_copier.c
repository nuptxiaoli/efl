#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <Ecore.h>
#include "ecore_private.h"

#define MY_CLASS EFL_IO_COPIER_CLASS
#define DEF_READ_CHUNK_SIZE 4096

/* TODO: FIXME, add to eina_error.h */
#define eina_error_from_errno(x) x

typedef struct _Efl_Io_Copier_Data
{
   Efl_Io_Reader *source;
   Efl_Io_Writer *destination;
   Eina_Promise *job;
   Eina_Binbuf *buf;
   uint8_t *read_chunk; /* TODO: method to grow Eina_Binbuf so we can expand it and read directly to that */
   Eina_Slice line_delimiter;
   size_t buffer_limit;
   size_t read_chunk_size;
   struct {
      uint64_t read, written, total;
   } progress;
   Eina_Bool closed;
   Eina_Bool done;
} Efl_Io_Copier_Data;

static void _efl_io_copier_write(Eo *o, Efl_Io_Copier_Data *pd);
static void _efl_io_copier_read(Eo *o, Efl_Io_Copier_Data *pd);

#define _COPIER_DBG(o, pd) \
  do \
    { \
       if (eina_log_domain_level_check(_ecore_log_dom, EINA_LOG_LEVEL_DBG)) \
         { \
            DBG("copier={%p %s, refs=%d, closed=%d, done=%d, buf=%zd}", \
                o, \
                efl_class_name_get(efl_class_get(o)), \
                efl_ref_get(o), \
                efl_io_closer_closed_get(o), \
                pd->done, \
                pd->buf ? eina_binbuf_length_get(pd->buf): 0); \
            if (!pd->source) \
              DBG("source=NULL"); \
            else \
              DBG("source={%p %s, refs=%d, can_read=%d, eos=%d, closed=%d}", \
                  pd->source, \
                  efl_class_name_get(efl_class_get(pd->source)), \
                  efl_ref_get(pd->source), \
                  efl_io_reader_can_read_get(pd->source), \
                  efl_io_reader_eos_get(pd->source), \
                  efl_isa(pd->source, EFL_IO_CLOSER_MIXIN) ? \
                  efl_io_closer_closed_get(pd->source) : 0); \
            if (!pd->destination) \
              DBG("destination=NULL"); \
            else \
              DBG("destination={%p %s, refs=%d, can_write=%d, closed=%d}", \
                  pd->destination, \
                  efl_class_name_get(efl_class_get(pd->destination)), \
                  efl_ref_get(pd->destination), \
                  efl_io_writer_can_write_get(pd->destination), \
                  efl_isa(pd->destination, EFL_IO_CLOSER_MIXIN) ? \
                  efl_io_closer_closed_get(pd->destination) : 0); \
         } \
    } \
  while (0)

static void
_efl_io_copier_job(void *data, void *value EINA_UNUSED)
{
   Eo *o = data;
   Efl_Io_Copier_Data *pd = efl_data_scope_get(o, MY_CLASS);

   pd->job = NULL;

   _COPIER_DBG(o, pd);

   if (pd->source && efl_io_reader_can_read_get(pd->source))
     _efl_io_copier_read(o, pd);

   if (pd->destination && efl_io_writer_can_write_get(pd->destination))
     _efl_io_copier_write(o, pd);

   efl_event_callback_call(o, EFL_IO_COPIER_EVENT_PROGRESS, NULL);

   if (!pd->source || efl_io_reader_eos_get(pd->source))
     {
        if ((!pd->done) &&
            ((!pd->destination) || (eina_binbuf_length_get(pd->buf) == 0)))
          {
             pd->done = EINA_TRUE;
             efl_event_callback_call(o, EFL_IO_COPIER_EVENT_DONE, NULL);
          }
     }
}

static void
_efl_io_copier_job_schedule(Eo *o, Efl_Io_Copier_Data *pd)
{
   if (pd->job) return;

   pd->job = efl_loop_job(efl_loop_user_loop_get(o), o);
   eina_promise_then(pd->job, _efl_io_copier_job, NULL, o);
}

/* NOTE: the returned slice may be smaller than requested since the
 * internal binbuf may be modified from inside event calls.
 *
 * parameter slice_of_binbuf must have mem pointing to pd->binbuf
 */
static Eina_Slice
_efl_io_copier_dispatch_data_events(Eo *o, Efl_Io_Copier_Data *pd, Eina_Slice slice_of_binbuf)
{
   Eina_Slice tmp;
   size_t offset;

   tmp = eina_binbuf_slice_get(pd->buf);
   if ((slice_of_binbuf.bytes < tmp.bytes) ||
       (eina_slice_end_get(slice_of_binbuf) > eina_slice_end_get(tmp)))
     {
        CRI("slice_of_binbuf=" EINA_SLICE_FMT " must be inside binbuf=" EINA_SLICE_FMT,
            EINA_SLICE_PRINT(slice_of_binbuf), EINA_SLICE_PRINT(tmp));
        return (Eina_Slice){.mem = NULL, .len = 0};
     }

   offset = slice_of_binbuf.bytes - tmp.bytes;

   efl_event_callback_call(o, EFL_IO_COPIER_EVENT_DATA, &slice_of_binbuf);
   /* user may have modified pd->buf, like calling
    * efl_io_copier_buffer_limit_set()
    */
   tmp = eina_binbuf_slice_get(pd->buf);
   if (offset <= tmp.len)
     {
        tmp.len -= offset;
        tmp.bytes += offset;
     }
   if (tmp.len > slice_of_binbuf.len)
     tmp.len = slice_of_binbuf.len;
   slice_of_binbuf = tmp;

   if (pd->line_delimiter.len > 0)
     {
        efl_event_callback_call(o, EFL_IO_COPIER_EVENT_LINE, &slice_of_binbuf);
        /* user may have modified pd->buf, like calling
         * efl_io_copier_buffer_limit_set()
         */
        tmp = eina_binbuf_slice_get(pd->buf);
        if (offset <= tmp.len)
          {
             tmp.len -= offset;
             tmp.bytes += offset;
          }
        if (tmp.len > slice_of_binbuf.len)
          tmp.len = slice_of_binbuf.len;
        slice_of_binbuf = tmp;
     }

   return slice_of_binbuf;
}

static void
_efl_io_copier_read(Eo *o, Efl_Io_Copier_Data *pd)
{
   Eina_Rw_Slice rw_slice;
   Eina_Slice ro_slice;
   Eina_Error err;
   size_t used;

   EINA_SAFETY_ON_TRUE_RETURN(pd->closed);

   rw_slice.mem = pd->read_chunk;
   rw_slice.len = pd->read_chunk_size;

   used = eina_binbuf_length_get(pd->buf);
   if (pd->buffer_limit > 0)
     {
        if (pd->buffer_limit <= used)
          {
             // TODO: disconnect 'read' so stops calling?
             return;
          }
        else if (pd->buffer_limit > used)
          {
             size_t available = pd->buffer_limit - used;
             if (rw_slice.len > available)
               rw_slice.len = available;
          }
     }

   err = efl_io_reader_read(pd->source, &rw_slice);
   if (err)
     {
        efl_event_callback_call(o, EFL_IO_COPIER_EVENT_ERROR, &err);
        return;
     }

   ro_slice = eina_rw_slice_slice_get(rw_slice);
   if (!eina_binbuf_append_slice(pd->buf, ro_slice))
     {
        efl_event_callback_call(o, EFL_IO_COPIER_EVENT_ERROR, &EINA_ERROR_OUT_OF_MEMORY);
        return;
     }

   pd->progress.read += rw_slice.len;
   pd->done = EINA_FALSE;

   if (!pd->destination)
     {
        /* Note: if there is a destination, dispatch data and line
         * from write since it will remove from binbuf and make it
         * simple to not repeat data that was already sent.
         *
         * however, if there is no destination, then emit the event
         * here.
         *
         * Remember to get the actual binbuf memory, rw_slice/ro_slice
         * contains the pointer to pd->read_chunk and
         * _efl_io_copier_dispatch_data_events() needs a slice to
         * internal binbuf.
         */
        Eina_Slice binbuf_slice = eina_binbuf_slice_get(pd->buf);
        Eina_Slice ev_slice = {
          .mem = binbuf_slice.bytes + used,
          .len = binbuf_slice.len - used,
        };
        _efl_io_copier_dispatch_data_events(o, pd, ev_slice);
     }

   _efl_io_copier_job_schedule(o, pd);
}

static void
_efl_io_copier_write(Eo *o, Efl_Io_Copier_Data *pd)
{
   Eina_Slice ro_slice = eina_binbuf_slice_get(pd->buf);
   Eina_Error err;

   EINA_SAFETY_ON_TRUE_RETURN(pd->closed);

   if (ro_slice.len == 0)
     {
        // TODO: disconnect 'write' so stops calling?
        return;
     }

   if ((pd->line_delimiter.len > 0) &&
       (pd->source && !efl_io_reader_eos_get(pd->source)))
     {
        const uint8_t *p = eina_slice_find(ro_slice, pd->line_delimiter);
        if (p)
          ro_slice.len = p - ro_slice.bytes + pd->line_delimiter.len;
        else if ((pd->buffer_limit == 0) || (ro_slice.len < pd->buffer_limit))
          {
             // TODO: disconnect 'write' so stops calling?
             return;
          }
     }

   err = efl_io_writer_write(pd->destination, &ro_slice, NULL);
   if (err)
     {
        if (err != EAGAIN) // TODO: Eina_Error after mapping
          efl_event_callback_call(o, EFL_IO_COPIER_EVENT_ERROR, &err);
        return;
     }
   pd->progress.written += ro_slice.len;
   pd->done = EINA_FALSE;

   /* Note: dispatch data and line from write since it will remove
    * from binbuf and make it simple to not repeat data that was
    * already sent.
    */
   ro_slice = _efl_io_copier_dispatch_data_events(o, pd, ro_slice);

   if (!eina_binbuf_remove(pd->buf, 0, ro_slice.len))
     {
        efl_event_callback_call(o, EFL_IO_COPIER_EVENT_ERROR, &EINA_ERROR_OUT_OF_MEMORY);
        return;
     }

   _efl_io_copier_job_schedule(o, pd);
}

static void
_efl_io_copier_source_can_read_changed(void *data, const Eo_Event *event EINA_UNUSED)
{
   Eo *o = data;
   Efl_Io_Copier_Data *pd = efl_data_scope_get(o, MY_CLASS);
   if (pd->closed) return;

   _COPIER_DBG(o, pd);

   if (efl_io_reader_can_read_get(pd->source))
     _efl_io_copier_job_schedule(o, pd);
}

static void
_efl_io_copier_source_eos(void *data, const Eo_Event *event EINA_UNUSED)
{
   Eo *o = data;
   Efl_Io_Copier_Data *pd = efl_data_scope_get(o, MY_CLASS);
   if (pd->closed) return;

   _COPIER_DBG(o, pd);

   _efl_io_copier_job_schedule(o, pd);
}

static void
_efl_io_copier_source_size_apply(Eo *o, Efl_Io_Copier_Data *pd)
{
   if (pd->closed) return;
   pd->progress.total = efl_io_sizer_size_get(pd->source);

   _COPIER_DBG(o, pd);

   if (pd->destination && efl_isa(pd->destination, EFL_IO_SIZER_MIXIN))
     efl_io_sizer_resize(pd->destination, pd->progress.total);

   efl_event_callback_call(o, EFL_IO_COPIER_EVENT_PROGRESS, NULL);
}

static void
_efl_io_copier_source_resized(void *data, const Eo_Event *event EINA_UNUSED)
{
   Eo *o = data;
   Efl_Io_Copier_Data *pd = efl_data_scope_get(o, MY_CLASS);
   _efl_io_copier_source_size_apply(o, pd);
}

static void
_efl_io_copier_source_closed(void *data, const Eo_Event *event EINA_UNUSED)
{
   Eo *o = data;
   Efl_Io_Copier_Data *pd = efl_data_scope_get(o, MY_CLASS);
   if (pd->closed) return;

   _COPIER_DBG(o, pd);

   _efl_io_copier_job_schedule(o, pd);
}

EFL_CALLBACKS_ARRAY_DEFINE(source_cbs,
                          { EFL_IO_READER_EVENT_CAN_READ_CHANGED, _efl_io_copier_source_can_read_changed },
                          { EFL_IO_READER_EVENT_EOS, _efl_io_copier_source_eos });

EOLIAN static Efl_Io_Reader *
_efl_io_copier_source_get(Eo *o EINA_UNUSED, Efl_Io_Copier_Data *pd)
{
   return pd->source;
}

EOLIAN static void
_efl_io_copier_source_set(Eo *o, Efl_Io_Copier_Data *pd, Efl_Io_Reader *source)
{
   if (pd->source == source) return;

   if (pd->source)
     {
        if (efl_isa(pd->source, EFL_IO_SIZER_MIXIN))
          {
             efl_event_callback_del(pd->source, EFL_IO_SIZER_EVENT_CHANGED,
                                    _efl_io_copier_source_resized, o);
             pd->progress.total = 0;
          }
        if (efl_isa(pd->source, EFL_IO_CLOSER_MIXIN))
          {
             efl_event_callback_del(pd->source, EFL_IO_CLOSER_EVENT_CLOSED,
                                    _efl_io_copier_source_closed, o);
          }
        efl_event_callback_array_del(pd->source, source_cbs(), o);
        efl_unref(pd->source);
        pd->source = NULL;
     }

   if (source)
     {
        EINA_SAFETY_ON_TRUE_RETURN(pd->closed);
        pd->source = efl_ref(source);
        efl_event_callback_array_add(pd->source, source_cbs(), o);

        if (efl_isa(pd->source, EFL_IO_SIZER_MIXIN))
          {
             efl_event_callback_add(pd->source, EFL_IO_SIZER_EVENT_CHANGED,
                                    _efl_io_copier_source_resized, o);
             _efl_io_copier_source_size_apply(o, pd);
          }

        if (efl_isa(pd->source, EFL_IO_CLOSER_MIXIN))
          {
             efl_event_callback_add(pd->source, EFL_IO_CLOSER_EVENT_CLOSED,
                                     _efl_io_copier_source_closed, o);
          }
     }
}

static void
_efl_io_copier_destination_can_write_changed(void *data, const Eo_Event *event EINA_UNUSED)
{
   Eo *o = data;
   Efl_Io_Copier_Data *pd = efl_data_scope_get(o, MY_CLASS);
   if (pd->closed) return;

   _COPIER_DBG(o, pd);

   if (efl_io_writer_can_write_get(pd->destination))
     _efl_io_copier_job_schedule(o, pd);
}

static void
_efl_io_copier_destination_closed(void *data, const Eo_Event *event EINA_UNUSED)
{
   Eo *o = data;
   Efl_Io_Copier_Data *pd = efl_data_scope_get(o, MY_CLASS);
   if (pd->closed) return;

   _COPIER_DBG(o, pd);

   if (eina_binbuf_length_get(pd->buf) == 0)
     {
        if (!pd->done)
          {
             pd->done = EINA_TRUE;
             efl_event_callback_call(o, EFL_IO_COPIER_EVENT_DONE, NULL);
          }
     }
   else
     {
        Eina_Error err = eina_error_from_errno(EBADF);
        efl_event_callback_call(o, EFL_IO_COPIER_EVENT_ERROR, &err);
     }
}

EFL_CALLBACKS_ARRAY_DEFINE(destination_cbs,
                          { EFL_IO_WRITER_EVENT_CAN_WRITE_CHANGED, _efl_io_copier_destination_can_write_changed });

EOLIAN static Efl_Io_Writer *
_efl_io_copier_destination_get(Eo *o EINA_UNUSED, Efl_Io_Copier_Data *pd)
{
   return pd->destination;
}

EOLIAN static void
_efl_io_copier_destination_set(Eo *o, Efl_Io_Copier_Data *pd, Efl_Io_Writer *destination)
{
   if (pd->destination == destination) return;

   if (pd->destination)
     {
        efl_event_callback_array_del(pd->destination, destination_cbs(), o);
        if (efl_isa(pd->destination, EFL_IO_CLOSER_MIXIN))
          {
             efl_event_callback_del(pd->destination, EFL_IO_CLOSER_EVENT_CLOSED,
                                    _efl_io_copier_destination_closed, o);
          }
        efl_unref(pd->destination);
        pd->destination = NULL;
     }

   if (destination)
     {
        EINA_SAFETY_ON_TRUE_RETURN(pd->closed);
        pd->destination = efl_ref(destination);
        efl_event_callback_array_add(pd->destination, destination_cbs(), o);

        if (efl_isa(pd->destination, EFL_IO_CLOSER_MIXIN))
          {
             efl_event_callback_add(pd->destination, EFL_IO_CLOSER_EVENT_CLOSED,
                                     _efl_io_copier_destination_closed, o);
          }
        if (efl_isa(pd->destination, EFL_IO_SIZER_MIXIN) &&
            pd->source && efl_isa(pd->source, EFL_IO_SIZER_MIXIN))
          {
             efl_io_sizer_resize(pd->destination, pd->progress.total);
          }
     }
}

EOLIAN static void
_efl_io_copier_buffer_limit_set(Eo *o, Efl_Io_Copier_Data *pd, size_t size)
{
   size_t used;

   EINA_SAFETY_ON_TRUE_RETURN(pd->closed);

   if (pd->buffer_limit == size) return;
   pd->buffer_limit = size;
   if (size == 0) return;

   used = eina_binbuf_length_get(pd->buf);
   if (used > size) eina_binbuf_remove(pd->buf, size, used);
   if (pd->read_chunk_size > size) efl_io_copier_read_chunk_size_set(o, size);
}

EOLIAN static size_t
_efl_io_copier_buffer_limit_get(Eo *o EINA_UNUSED, Efl_Io_Copier_Data *pd)
{
   return pd->buffer_limit;
}

EOLIAN static void
_efl_io_copier_line_delimiter_set(Eo *o EINA_UNUSED, Efl_Io_Copier_Data *pd, const Eina_Slice *slice)
{
   EINA_SAFETY_ON_NULL_RETURN(slice);
   if (pd->line_delimiter.mem == slice->mem)
     {
        pd->line_delimiter.len = slice->len;
        return;
     }

   free((void *)pd->line_delimiter.mem);
   if (slice->len == 0)
     {
        pd->line_delimiter.mem = NULL;
        pd->line_delimiter.len = 0;
     }
   else
     {
        Eina_Rw_Slice rw_slice = eina_slice_dup(*slice);
        pd->line_delimiter = eina_rw_slice_slice_get(rw_slice);
     }
}

EOLIAN static const Eina_Slice *
_efl_io_copier_line_delimiter_get(Eo *o EINA_UNUSED, Efl_Io_Copier_Data *pd)
{
   return &pd->line_delimiter;
}


EOLIAN static void
_efl_io_copier_read_chunk_size_set(Eo *o EINA_UNUSED, Efl_Io_Copier_Data *pd, size_t size)
{
   void *tmp;

   EINA_SAFETY_ON_TRUE_RETURN(pd->closed);

   if (size == 0) size = DEF_READ_CHUNK_SIZE;
   if ((pd->read_chunk_size == size) && pd->read_chunk) return;

   tmp = realloc(pd->read_chunk, size);
   EINA_SAFETY_ON_NULL_RETURN(tmp);

   pd->read_chunk = tmp;
   pd->read_chunk_size = size;
}

EOLIAN static size_t
_efl_io_copier_read_chunk_size_get(Eo *o EINA_UNUSED, Efl_Io_Copier_Data *pd)
{
   return pd->read_chunk_size > 0 ? pd->read_chunk_size : DEF_READ_CHUNK_SIZE;
}

EOLIAN static Eina_Error
_efl_io_copier_efl_io_closer_close(Eo *o, Efl_Io_Copier_Data *pd)
{
   Eina_Error err = 0, r;

   EINA_SAFETY_ON_TRUE_RETURN_VAL(pd->closed, eina_error_from_errno(EINVAL));

   _COPIER_DBG(o, pd);

   if (pd->job)
     {
        eina_promise_cancel(pd->job);
        pd->job = NULL;
     }

   if (pd->source)
     {
        if (efl_isa(pd->source, EFL_IO_SIZER_MIXIN))
          {
             efl_event_callback_del(pd->source, EFL_IO_SIZER_EVENT_CHANGED,
                                    _efl_io_copier_source_resized, o);
             pd->progress.total = 0;
          }
        efl_event_callback_array_del(pd->source, source_cbs(), o);
        if (efl_isa(pd->source, EFL_IO_CLOSER_MIXIN) &&
            !efl_io_closer_closed_get(pd->source))
          {
             efl_event_callback_del(pd->source, EFL_IO_CLOSER_EVENT_CLOSED,
                                    _efl_io_copier_source_closed, o);
             err = efl_io_closer_close(pd->source);
          }
     }

   if (pd->destination)
     {
        efl_event_callback_array_del(pd->destination, destination_cbs(), o);
        if (efl_isa(pd->destination, EFL_IO_CLOSER_MIXIN) &&
            !efl_io_closer_closed_get(pd->destination))
          {
             efl_event_callback_del(pd->destination, EFL_IO_CLOSER_EVENT_CLOSED,
                                    _efl_io_copier_destination_closed, o);
             r = efl_io_closer_close(pd->destination);
             if (!err) err = r;
          }
     }

   pd->closed = EINA_TRUE;
   efl_event_callback_call(o, EFL_IO_CLOSER_EVENT_CLOSED, NULL);

   if (pd->buf)
     {
        eina_binbuf_free(pd->buf);
        pd->buf = NULL;
     }

   if (pd->read_chunk)
     {
        free(pd->read_chunk);
        pd->read_chunk = NULL;
        pd->read_chunk_size = 0;
     }

   return err;
}

EOLIAN static Eina_Bool
_efl_io_copier_efl_io_closer_closed_get(Eo *o EINA_UNUSED, Efl_Io_Copier_Data *pd)
{
   return pd->closed;
}

EOLIAN static void
_efl_io_copier_progress_get(Eo *o EINA_UNUSED, Efl_Io_Copier_Data *pd, uint64_t *read, uint64_t *written, uint64_t *total)
{
   if (read) *read = pd->progress.read;
   if (written) *written = pd->progress.written;
   if (total) *total = pd->progress.total;
}

EOLIAN static Eina_Binbuf *
_efl_io_copier_binbuf_steal(Eo *o EINA_UNUSED, Efl_Io_Copier_Data *pd)
{
   Eina_Binbuf *ret = pd->buf;
   pd->buf = eina_binbuf_new();
   return ret;
}

EOLIAN static Eo *
_efl_io_copier_efl_object_constructor(Eo *o, Efl_Io_Copier_Data *pd)
{
   pd->buf = eina_binbuf_new();

   EINA_SAFETY_ON_NULL_RETURN_VAL(pd->buf, NULL);

   return efl_constructor(efl_super(o, MY_CLASS));
}

EOLIAN static Eo *
_efl_io_copier_efl_object_finalize(Eo *o, Efl_Io_Copier_Data *pd)
{
   if (pd->read_chunk_size == 0)
     efl_io_copier_read_chunk_size_set(o, DEF_READ_CHUNK_SIZE);

   if (!efl_loop_user_loop_get(o))
     {
        ERR("Set a loop provider as parent of this copier!");
        return NULL;
     }

   if ((pd->source && efl_io_reader_can_read_get(pd->source)) ||
       (pd->destination && efl_io_writer_can_write_get(pd->destination)))
     _efl_io_copier_job_schedule(o, pd);

   _COPIER_DBG(o, pd);

   return efl_finalize(efl_super(o, MY_CLASS));
}

EOLIAN static void
_efl_io_copier_efl_object_destructor(Eo *o, Efl_Io_Copier_Data *pd)
{
   _COPIER_DBG(o, pd);

   efl_io_copier_source_set(o, NULL);
   efl_io_copier_destination_set(o, NULL);

   if (pd->job)
     {
        eina_promise_cancel(pd->job);
        pd->job = NULL;
     }

   efl_destructor(efl_super(o, MY_CLASS));

   if (pd->buf)
     {
        eina_binbuf_free(pd->buf);
        pd->buf = NULL;
     }

   if (pd->read_chunk)
     {
        free(pd->read_chunk);
        pd->read_chunk = NULL;
        pd->read_chunk_size = 0;
     }

   if (pd->line_delimiter.mem)
     {
        free((void *)pd->line_delimiter.mem);
        pd->line_delimiter.mem = NULL;
        pd->line_delimiter.len = 0;
     }
}

#include "efl_io_copier.eo.c"
