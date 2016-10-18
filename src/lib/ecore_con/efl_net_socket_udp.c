#define EFL_NET_SOCKET_UDP_PROTECTED 1
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

#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_UDP_H
# include <netinet/udp.h>
#endif
#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif
#ifdef HAVE_EVIL
# include <Evil.h>
#endif

#define MY_CLASS EFL_NET_SOCKET_UDP_CLASS

typedef struct _Efl_Net_Socket_Udp_Data
{
   Eina_Bool cork;
} Efl_Net_Socket_Udp_Data;

EOLIAN static void
_efl_net_socket_udp_efl_loop_fd_fd_set(Eo *o, Efl_Net_Socket_Udp_Data *pd EINA_UNUSED, int fd)
{
   efl_loop_fd_set(efl_super(o, MY_CLASS), fd);

   if (fd >= 0)
     {
        struct sockaddr_storage addr;
        socklen_t addrlen;
        int family;

        /* apply postponed values */
        efl_net_socket_udp_cork_set(o, pd->cork);

        family = efl_net_socket_fd_family_get(o);
        if (family == AF_UNSPEC) return;

        addrlen = sizeof(addr);
        if (getsockname(fd, (struct sockaddr *)&addr, &addrlen) < 0)
          ERR("getsockname(%d): %s", fd, strerror(errno));
        else
          {
             char str[INET6_ADDRSTRLEN + sizeof("[]:65536")];
             if (efl_net_ip_port_fmt(str, sizeof(str), (struct sockaddr *)&addr))
               efl_net_socket_address_local_set(o, str);
          }

        addrlen = sizeof(addr);
        if (getpeername(fd, (struct sockaddr *)&addr, &addrlen) < 0)
          ERR("getpeername(%d): %s", fd, strerror(errno));
        else
          {
             char str[INET6_ADDRSTRLEN + sizeof("[]:65536")];
             if (efl_net_ip_port_fmt(str, sizeof(str), (struct sockaddr *)&addr))
               efl_net_socket_address_remote_set(o, str);
          }
     }
}

static inline int
_cork_option_get(void)
{
#if defined(HAVE_UDP_CORK)
   return UDP_CORK;
#else
   return -1;
#endif
}

EOLIAN static Eina_Bool
_efl_net_socket_udp_cork_set(Eo *o, Efl_Net_Socket_Udp_Data *pd, Eina_Bool cork)
{
   int value, fd, option;
   Eina_Bool old = pd->cork;

   option = _cork_option_get();
   if (EINA_UNLIKELY(option < 0))
     {
        if (cork)
          ERR("Could not find a UDP_CORK equivalent on your system");
        return EINA_FALSE;
     }

   pd->cork = cork;

   fd = efl_loop_fd_get(o);
   if (fd < 0) return EINA_TRUE; /* postpone until fd_set() */

   value = cork;
   if (setsockopt(fd, IPPROTO_UDP, option, &value, sizeof(value)) < 0)
     {
        ERR("setsockopt(%d, IPPROTO_UDP, 0x%x, %d): %s",
            fd, option, value, strerror(errno));
        pd->cork = old;
        return EINA_FALSE;
     }

   return EINA_TRUE;
}

EOLIAN static Eina_Bool
_efl_net_socket_udp_cork_get(Eo *o, Efl_Net_Socket_Udp_Data *pd)
{
   int value = 0, fd;
   socklen_t valuelen;
   int option;

   option = _cork_option_get();
   if (EINA_UNLIKELY(option < 0))
     {
        ERR("Could not find a UDP_CORK equivalent on your system");
        return EINA_FALSE;
     }

   fd = efl_loop_fd_get(o);
   if (fd < 0) return pd->cork;

   /* if there is a fd, always query it directly as it may be modified
    * elsewhere by nasty users.
    */
   valuelen = sizeof(value);
   if (getsockopt(fd, IPPROTO_UDP, option, &value, &valuelen) < 0)
     {
        ERR("getsockopt(%d, IPPROTO_UDP, 0x%x): %s",
            fd, option, strerror(errno));
        return EINA_FALSE;
     }

   pd->cork = !!value; /* sync */
   return pd->cork;
}

#include "efl_net_socket_udp.eo.c"