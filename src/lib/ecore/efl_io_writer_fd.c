#define EFL_IO_WRITER_PROTECTED 1
#define EFL_IO_WRITER_FD_PROTECTED 1

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <Ecore.h>
#include "ecore_private.h"

#define MY_CLASS EFL_IO_WRITER_FD_CLASS

/* TODO: FIXME, add to eina_error.h */
#define eina_error_from_errno(x) x

typedef struct _Efl_Io_Writer_Fd_Data
{
   int fd;
   Eina_Bool can_write;
} Efl_Io_Writer_Fd_Data;

EOLIAN static void
_efl_io_writer_fd_writer_fd_set(Eo *o EINA_UNUSED, Efl_Io_Writer_Fd_Data *pd, int fd)
{
   pd->fd = fd;
}

EOLIAN static int
_efl_io_writer_fd_writer_fd_get(Eo *o EINA_UNUSED, Efl_Io_Writer_Fd_Data *pd)
{
   return pd->fd;
}

EOLIAN static Eina_Error
_efl_io_writer_fd_efl_io_writer_write(Eo *o, Efl_Io_Writer_Fd_Data *pd EINA_UNUSED, Eina_Slice *ro_slice, Eina_Slice *remaining)
{
   int fd = efl_io_writer_fd_writer_fd_get(o);
   ssize_t r;

   if (fd < 0) goto error;;
   EINA_SAFETY_ON_NULL_GOTO(ro_slice, error);

   do
     {
        r = write(fd, ro_slice->mem, ro_slice->len);
        if (r < 0)
          {
             if (errno == EINTR) continue;

             if (remaining) *remaining = *ro_slice;
             ro_slice->len = 0;
             ro_slice->mem = NULL;
             if (errno == EAGAIN) efl_io_writer_can_write_set(o, EINA_FALSE);
             return eina_error_from_errno(errno);
          }
     }
   while (r < 0);

   if (remaining)
     {
        remaining->len = ro_slice->len - r;
        remaining->bytes = ro_slice->bytes + r;
     }
   ro_slice->len = r;
   if (r == 0) efl_io_writer_can_write_set(o, EINA_FALSE);
   return 0;

 error:
   if (remaining) *remaining = *ro_slice;
   ro_slice->len = 0;
   ro_slice->mem = NULL;
   return eina_error_from_errno(EINVAL);

}

EOLIAN static Eina_Bool
_efl_io_writer_fd_efl_io_writer_can_write_get(Eo *o EINA_UNUSED, Efl_Io_Writer_Fd_Data *pd)
{
   return pd->can_write;
}

EOLIAN static void
_efl_io_writer_fd_efl_io_writer_can_write_set(Eo *o, Efl_Io_Writer_Fd_Data *pd, Eina_Bool can_write)
{
   EINA_SAFETY_ON_TRUE_RETURN(efl_io_writer_fd_writer_fd_get(o) < 0);
   if (pd->can_write == can_write) return;
   pd->can_write = can_write;
   efl_event_callback_call(o, EFL_IO_WRITER_EVENT_CAN_WRITE_CHANGED, NULL);
}

#include "efl_io_writer_fd.eo.c"
