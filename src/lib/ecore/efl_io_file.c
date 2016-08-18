#define EFL_IO_READER_PROTECTED 1
#define EFL_IO_WRITER_PROTECTED 1
#define EFL_IO_READER_FD_PROTECTED 1
#define EFL_IO_WRITER_FD_PROTECTED 1
#define EFL_IO_CLOSER_FD_PROTECTED 1
#define EFL_IO_SIZER_FD_PROTECTED 1
#define EFL_IO_POSITIONER_FD_PROTECTED 1

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <Ecore.h>
#include "ecore_private.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MY_CLASS EFL_IO_FILE_CLASS

typedef struct _Efl_Io_File_Data
{
   const char *path;
   uint32_t flags;
   uint32_t mode;
   uint64_t last_position;
   // TODO: monitor reader.can_read,changed/writer.can_write,changed events in order to dynamically connect to Loop_Fd events.
} Efl_Io_File_Data;

/* TODO: FIXME, add to eina_error.h */
#define eina_error_from_errno(x) x

static void
_efl_io_file_state_update(Eo *o, Efl_Io_File_Data *pd)
{
   uint64_t pos = efl_io_positioner_position_get(o);
   uint64_t size = efl_io_sizer_size_get(o);
   uint32_t flags = pd->flags & O_ACCMODE;

   if ((flags == O_RDWR) || (flags == O_RDONLY))
     {
        efl_io_reader_can_read_set(o, pos < size);
        efl_io_reader_eos_set(o, pos >= size);
     }

   if ((flags == O_RDWR) || (flags == O_WRONLY))
     efl_io_writer_can_write_set(o, EINA_TRUE);

   if (pd->last_position != pos)
     {
        pd->last_position = pos;
        efl_event_callback_call(o, EFL_IO_POSITIONER_EVENT_CHANGED, NULL);
     }
}

EOLIAN static void
_efl_io_file_efl_loop_fd_fd_file_set(Eo *o, Efl_Io_File_Data *pd, int fd)
{
   efl_loop_fd_file_set(efl_super(o, MY_CLASS), fd);
   efl_io_positioner_fd_positioner_fd_set(o, fd);
   efl_io_sizer_fd_sizer_fd_set(o, fd);
   efl_io_reader_fd_reader_fd_set(o, fd);
   efl_io_writer_fd_writer_fd_set(o, fd);
   efl_io_closer_fd_closer_fd_set(o, fd);
   if (fd >= 0) _efl_io_file_state_update(o, pd);
}

EOLIAN static void
_efl_io_file_flags_set(Eo *o, Efl_Io_File_Data *pd, uint32_t flags)
{
   EINA_SAFETY_ON_TRUE_RETURN(efl_finalized_get(o));
   pd->flags = flags;
}

EOLIAN static uint32_t
_efl_io_file_flags_get(Eo *o EINA_UNUSED, Efl_Io_File_Data *pd)
{
   return pd->flags; // TODO: query from fd?
}

EOLIAN static void
_efl_io_file_mode_set(Eo *o, Efl_Io_File_Data *pd, uint32_t mode)
{
   EINA_SAFETY_ON_TRUE_RETURN(efl_finalized_get(o));
   pd->mode = mode;
}

EOLIAN static uint32_t
_efl_io_file_mode_get(Eo *o EINA_UNUSED, Efl_Io_File_Data *pd)
{
   return pd->mode;
}

EOLIAN static void
_efl_io_file_efl_object_destructor(Eo *o, Efl_Io_File_Data *pd)
{
   if (!efl_io_closer_closed_get(o))
     efl_io_closer_close(o);

   efl_destructor(efl_super(o, MY_CLASS));

   eina_stringshare_del(pd->path);
}

EOLIAN static Efl_Object *
_efl_io_file_efl_object_finalize(Eo *o, Efl_Io_File_Data *pd)
{
   int fd = efl_loop_fd_file_get(o);
   if (fd < 0)
     {
        EINA_SAFETY_ON_NULL_RETURN_VAL(pd->path, NULL);

        if (pd->mode)
          fd = open(pd->path, pd->flags, pd->mode);
        else
          fd = open(pd->path, pd->flags);

        if (fd < 0)
          {
             WRN("Could not open file '%s': %s", pd->path, strerror(errno));
             return NULL;
          }

        efl_loop_fd_file_set(o, fd);
     }

   return efl_finalize(efl_super(o, MY_CLASS));
}

EOLIAN static Eina_Bool
_efl_io_file_efl_file_file_set(Eo *o, Efl_Io_File_Data *pd, const char *file, const char *key EINA_UNUSED)
{
   EINA_SAFETY_ON_TRUE_RETURN_VAL(efl_finalized_get(o), EINA_FALSE);

   eina_stringshare_replace(&pd->path, file);
   return EINA_TRUE;
}

EOLIAN static void
_efl_io_file_efl_file_file_get(Eo *o EINA_UNUSED, Efl_Io_File_Data *pd, const char **file, const char **key)
{
   if (file) *file = pd->path;
   if (key) *key = NULL;
}

EOLIAN static Eina_Error
_efl_io_file_efl_io_reader_read(Eo *o, Efl_Io_File_Data *pd, Eina_Rw_Slice *rw_slice)
{
   Eina_Error err = efl_io_reader_read(efl_super(o, MY_CLASS), rw_slice);
   if (err) return err;
   _efl_io_file_state_update(o, pd);
   return 0;
}

EOLIAN static Eina_Error
_efl_io_file_efl_io_writer_write(Eo *o, Efl_Io_File_Data *pd, Eina_Slice *slice, Eina_Slice *remaining)
{
   Eina_Error err = efl_io_writer_write(efl_super(o, MY_CLASS), slice, remaining);
   if (err) return err;
   _efl_io_file_state_update(o, pd);
   return 0;
}

EOLIAN static Eina_Error
_efl_io_file_efl_io_closer_close(Eo *o, Efl_Io_File_Data *pd EINA_UNUSED)
{
   Eina_Error ret;
   efl_io_reader_can_read_set(o, EINA_FALSE);
   efl_io_reader_eos_set(o, EINA_TRUE);
   efl_io_writer_can_write_set(o, EINA_FALSE);

   ret = efl_io_closer_close(efl_super(o, MY_CLASS));

   efl_loop_fd_file_set(o, -1);

   return ret;
}

EOLIAN static Eina_Error
_efl_io_file_efl_io_sizer_resize(Eo *o, Efl_Io_File_Data *pd, uint64_t size)
{
   Eina_Error err = efl_io_sizer_resize(efl_super(o, MY_CLASS), size);
   if (err) return err;
   _efl_io_file_state_update(o, pd);
   return 0;
}

EOLIAN static Eina_Error
_efl_io_file_efl_io_positioner_seek(Eo *o, Efl_Io_File_Data *pd, int64_t offset, Efl_Io_Positioner_Whence whence)
{
   Eina_Error err = efl_io_positioner_seek(efl_super(o, MY_CLASS), offset, whence);
   if (err) return err;
   _efl_io_file_state_update(o, pd);
   return 0;
}

#include "efl_io_file.eo.c"
