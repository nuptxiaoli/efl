#define EFL_NET_SOCKET_FD_PROTECTED 1
#define EFL_LOOP_FD_PROTECTED 1
#define EFL_IO_READER_FD_PROTECTED 1
#define EFL_IO_WRITER_FD_PROTECTED 1
#define EFL_IO_CLOSER_FD_PROTECTED 1
#define EFL_IO_READER_PROTECTED 1
#define EFL_IO_WRITER_PROTECTED 1
#define EFL_IO_CLOSER_PROTECTED 1
#define EFL_NET_SOCKET_PROTECTED 1

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "Ecore.h"
#include "Ecore_Con.h"
#include "ecore_con_private.h"

#include <fcntl.h>
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#ifdef HAVE_EVIL
# include <Evil.h>
#endif

#define MY_CLASS EFL_NET_SOCKET_FD_CLASS

typedef struct _Efl_Net_Socket_Fd_Data
{
   Eina_Stringshare *address_local;
   Eina_Stringshare *address_remote;
   int family;
   Eina_Bool close_on_exec;
} Efl_Net_Socket_Fd_Data;

static void
_efl_net_socket_fd_event_read(void *data EINA_UNUSED, const Eo_Event *event)
{
   efl_io_reader_can_read_set(event->object, EINA_TRUE);
   efl_io_reader_eos_set(event->object, EINA_FALSE);
}

static void
_efl_net_socket_fd_event_write(void *data EINA_UNUSED, const Eo_Event *event)
{
   efl_io_writer_can_write_set(event->object, EINA_TRUE);
}

static void
_efl_net_socket_fd_event_error(void *data EINA_UNUSED, const Eo_Event *event)
{
   efl_io_writer_can_write_set(event->object, EINA_FALSE);
   efl_io_reader_can_read_set(event->object, EINA_FALSE);
   efl_io_reader_eos_set(event->object, EINA_TRUE);
}

EOLIAN static Efl_Object *
_efl_net_socket_fd_efl_object_finalize(Eo *o, Efl_Net_Socket_Fd_Data *pd EINA_UNUSED)
{
   o = efl_finalize(efl_super(o, MY_CLASS));
   if (!o) return NULL;

   // TODO: only register "read" if "can_read" is being monitored?
   // TODO: only register "write" if "can_write" is being monitored?
   efl_event_callback_add(o, EFL_LOOP_FD_EVENT_WRITE, _efl_net_socket_fd_event_write, NULL);
   efl_event_callback_add(o, EFL_LOOP_FD_EVENT_READ, _efl_net_socket_fd_event_read, NULL);
   efl_event_callback_add(o, EFL_LOOP_FD_EVENT_ERROR, _efl_net_socket_fd_event_error, NULL);
   return o;
}

EOLIAN static Efl_Object *
_efl_net_socket_fd_efl_object_constructor(Eo *o, Efl_Net_Socket_Fd_Data *pd EINA_UNUSED)
{
   pd->family = AF_UNSPEC;
   return efl_constructor(efl_super(o, MY_CLASS));
}

EOLIAN static void
_efl_net_socket_fd_efl_object_destructor(Eo *o, Efl_Net_Socket_Fd_Data *pd)
{
   efl_destructor(efl_super(o, MY_CLASS));

   eina_stringshare_replace(&pd->address_local, NULL);
   eina_stringshare_replace(&pd->address_remote, NULL);
}

static void
_efl_net_socket_fd_set(Eo *o, Efl_Net_Socket_Fd_Data *pd, int fd)
{
   efl_io_reader_fd_reader_fd_set(o, fd);
   efl_io_writer_fd_writer_fd_set(o, fd);
   efl_io_closer_fd_closer_fd_set(o, fd);

   /* apply postponed values */
   efl_net_socket_fd_close_on_exec_set(o, pd->close_on_exec);
   if (pd->family == AF_UNSPEC)
     {
        ERR("efl_loop_fd_set() must be called after efl_net_server_fd_family_set()");
        return;
     }
}

static void
_efl_net_socket_fd_unset(Eo *o)
{
   efl_io_reader_fd_reader_fd_set(o, -1);
   efl_io_writer_fd_writer_fd_set(o, -1);
   efl_io_closer_fd_closer_fd_set(o, -1);

   efl_net_socket_address_local_set(o, NULL);
   efl_net_socket_address_remote_set(o, NULL);
}

EOLIAN static void
_efl_net_socket_fd_efl_loop_fd_fd_set(Eo *o, Efl_Net_Socket_Fd_Data *pd, int fd)
{
   if ((pd->family == AF_UNSPEC) && (fd >= 0))
     {
        struct sockaddr_storage addr;
        socklen_t addrlen = sizeof(addr);
        if (getsockname(fd, (struct sockaddr *)&addr, &addrlen) < 0)
          ERR("getsockname(%d): %s", fd, strerror(errno));
        else
          efl_net_socket_fd_family_set(o, addr.ss_family);
     }

   efl_loop_fd_set(efl_super(o, MY_CLASS), fd);

   if (fd >= 0) _efl_net_socket_fd_set(o, pd, fd);
   else _efl_net_socket_fd_unset(o);
}

EOLIAN static Eina_Error
_efl_net_socket_fd_efl_io_closer_close(Eo *o, Efl_Net_Socket_Fd_Data *pd EINA_UNUSED)
{
   Eina_Error ret;

   efl_io_writer_can_write_set(o, EINA_FALSE);
   efl_io_reader_can_read_set(o, EINA_FALSE);
   efl_io_reader_eos_set(o, EINA_TRUE);

   /* skip _efl_net_socket_fd_efl_loop_fd_fd_set() since we want to
    * retain efl_io_closer_fd_closer_fd_get() so close(super()) works
    * and we emit the events with proper addresses.
    */
   efl_loop_fd_set(efl_super(o, MY_CLASS), -1);

   ret = efl_io_closer_close(efl_super(o, MY_CLASS));

   /* do the cleanup our _efl_net_socket_fd_efl_loop_fd_fd_set() would do */
   _efl_net_socket_fd_unset(o);

   return ret;
}

EOLIAN static Eina_Error
_efl_net_socket_fd_efl_io_reader_read(Eo *o, Efl_Net_Socket_Fd_Data *pd EINA_UNUSED, Eina_Rw_Slice *rw_slice)
{
   Eina_Error ret;

   ret = efl_io_reader_read(efl_super(o, MY_CLASS), rw_slice);
   if (rw_slice && rw_slice->len > 0)
     efl_io_reader_can_read_set(o, EINA_FALSE); /* wait Efl.Loop.Fd "read" */

   return ret;
}

EOLIAN static Eina_Error
_efl_net_socket_fd_efl_io_writer_write(Eo *o, Efl_Net_Socket_Fd_Data *pd EINA_UNUSED, Eina_Slice *ro_slice, Eina_Slice *remaining)
{
   Eina_Error ret;

   ret = efl_io_writer_write(efl_super(o, MY_CLASS), ro_slice, remaining);
   if (ro_slice && ro_slice->len > 0)
     efl_io_writer_can_write_set(o, EINA_FALSE); /* wait Efl.Loop.Fd "write" */

   return ret;
}

EOLIAN static void
_efl_net_socket_fd_efl_net_socket_address_local_set(Eo *o EINA_UNUSED, Efl_Net_Socket_Fd_Data *pd, const char *address)
{
   eina_stringshare_replace(&pd->address_local, address);
}

EOLIAN static const char *
_efl_net_socket_fd_efl_net_socket_address_local_get(Eo *o EINA_UNUSED, Efl_Net_Socket_Fd_Data *pd)
{
   return pd->address_local;
}

EOLIAN static void
_efl_net_socket_fd_efl_net_socket_address_remote_set(Eo *o EINA_UNUSED, Efl_Net_Socket_Fd_Data *pd, const char *address)
{
   eina_stringshare_replace(&pd->address_remote, address);
}

EOLIAN static const char *
_efl_net_socket_fd_efl_net_socket_address_remote_get(Eo *o EINA_UNUSED, Efl_Net_Socket_Fd_Data *pd)
{
   return pd->address_remote;
}

EOLIAN static Eina_Bool
_efl_net_socket_fd_close_on_exec_set(Eo *o, Efl_Net_Socket_Fd_Data *pd, Eina_Bool close_on_exec)
{
   int flags, fd;

   pd->close_on_exec = close_on_exec;

   fd = efl_loop_fd_get(o);
   if (fd < 0) return EINA_TRUE; /* postpone until fd_set() */

   flags = fcntl(fd, F_GETFD);
   if (flags < 0)
     {
        ERR("fcntl(%d, F_GETFD): %s", fd, strerror(errno));
        return EINA_FALSE;
     }
   if (close_on_exec)
     flags |= FD_CLOEXEC;
   else
     flags &= (~FD_CLOEXEC);
   if (fcntl(fd, F_SETFD, flags) < 0)
     {
        ERR("fcntl(%d, F_SETFD, %#x): %s", fd, flags, strerror(errno));
        return EINA_FALSE;
     }

   return EINA_TRUE;
}

EOLIAN static Eina_Bool
_efl_net_socket_fd_close_on_exec_get(Eo *o, Efl_Net_Socket_Fd_Data *pd)
{
   int flags, fd;

   fd = efl_loop_fd_get(o);
   if (fd < 0) return pd->close_on_exec;

   /* if there is a fd, always query it directly as it may be modified
    * elsewhere by nasty users.
    */
   flags = fcntl(fd, F_GETFD);
   if (flags < 0)
     {
        ERR("fcntl(%d, F_GETFD): %s", fd, strerror(errno));
        return EINA_FALSE;
     }

   pd->close_on_exec = !!(flags & FD_CLOEXEC); /* sync */
   return pd->close_on_exec;
}

EOLIAN static void
_efl_net_socket_fd_family_set(Eo *o EINA_UNUSED, Efl_Net_Socket_Fd_Data *pd, int family)
{
   pd->family = family;
}

EOLIAN static int
_efl_net_socket_fd_family_get(Eo *o EINA_UNUSED, Efl_Net_Socket_Fd_Data *pd)
{
   return pd->family;
}

#include "efl_net_socket_fd.eo.c"
