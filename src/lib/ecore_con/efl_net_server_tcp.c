#define EFL_NET_SERVER_TCP_PROTECTED 1
#define EFL_NET_SERVER_FD_PROTECTED 1
#define EFL_NET_SERVER_PROTECTED 1
#define EFL_LOOP_FD_PROTECTED 1

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "Ecore.h"
#include "Ecore_Con.h"
#include "ecore_con_private.h"

#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_TCP_H
# include <netinet/tcp.h>
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

#define MY_CLASS EFL_NET_SERVER_TCP_CLASS

/* TODO: FIXME, add to eina_error.h */
#define eina_error_from_errno(x) x

EOLIAN static Eina_Error
_efl_net_server_tcp_efl_net_server_serve(Eo *o, void *pd EINA_UNUSED, const char *address)
{
   struct sockaddr_storage addr = {};
   char *str, *host, *port;
   int r, fd, extra_flags = 0;
   socklen_t addrlen;
   char buf[INET6_ADDRSTRLEN + sizeof("[]:65536")];
   Eina_Error err = 0;

   EINA_SAFETY_ON_NULL_RETURN_VAL(address, eina_error_from_errno(EINVAL));

   // TODO: change to getaddrinfo() and move to a thread...
   str = host = strdup(address);
   EINA_SAFETY_ON_NULL_RETURN_VAL(str, EINA_ERROR_OUT_OF_MEMORY);

   if (host[0] == '[')
     {
        struct sockaddr_in6 *a = (struct sockaddr_in6 *)&addr;
        /* IPv6 is: [IP]:port */
        host++;
        port = strchr(host, ']');
        if (!port)
          {
             ERR("missing ']' in IPv6 address: %s", address);
             err = eina_error_from_errno(EINVAL);
             goto invalid_address;
          }
        *port = '\0';
        port++;

        if (port[0] == ':')
          port++;
        else
          port = NULL;
        a->sin6_family = AF_INET6;
        a->sin6_port = htons(port ? atoi(port) : 0);
        r = inet_pton(AF_INET6, host, &(a->sin6_addr));
        addrlen = sizeof(*a);
     }
   else
     {
        struct sockaddr_in *a = (struct sockaddr_in *)&addr;
        port = strchr(host, ':');
        if (port)
          {
             *port = '\0';
             port++;
          }
        a->sin_family = AF_INET;
        a->sin_port = htons(port ? atoi(port) : 0);
        r = inet_pton(AF_INET, host, &(a->sin_addr));
        addrlen = sizeof(*a);
     }

   if (r != 1)
     {
        ERR("could not parse IP '%s' (%s)", host, address);
        err = eina_error_from_errno(EINVAL);
        goto invalid_address;
     }
   free(str);

   efl_net_server_fd_family_set(o, addr.ss_family);

   if (efl_net_ip_port_fmt(buf, sizeof(buf), (struct sockaddr *)&addr))
     efl_net_server_address_set(o, buf);

   if (efl_net_server_fd_close_on_exec_get(o))
     extra_flags |= SOCK_CLOEXEC;

   fd = socket(addr.ss_family, SOCK_STREAM | extra_flags, IPPROTO_TCP);
   if (fd < 0)
     {
        err = eina_error_from_errno(errno);
        ERR("socket(%d, SOCK_STREAM | %#x, IPPROTO_TCP): %s",
            addr.ss_family, extra_flags, strerror(errno));
        goto error_socket;
     }

   efl_loop_fd_set(o, fd);

   r = bind(fd, (struct sockaddr *)&addr, addrlen);
   if (r < 0)
     {
        err = eina_error_from_errno(errno);
        ERR("bind(%d, %s): %s", fd, address, strerror(errno));
        goto error_listen;
     }

   r = listen(fd, 0);
   if (r < 0)
     {
        err = eina_error_from_errno(errno);
        ERR("listen(%d): %s", fd, strerror(errno));
        goto error_listen;
     }

   efl_net_server_serving_set(o, EINA_TRUE);
   return 0;

 invalid_address:
   free(str);
   goto error_socket;

 error_listen:
   close(fd);
 error_socket:
   efl_event_callback_call(o, EFL_NET_SERVER_EVENT_ERROR, &err);
   return err;
}

static Efl_Callback_Array_Item *_efl_net_server_tcp_client_cbs(void);

static void
_efl_net_server_tcp_client_event_closed(void *data, const Eo_Event *event)
{
   Eo *server = data;
   Eo *client = event->object;

   efl_event_callback_array_del(client, _efl_net_server_tcp_client_cbs(), server);
   if (efl_parent_get(client) == server)
     efl_parent_set(client, NULL);

   efl_net_server_clients_count_set(server, efl_net_server_clients_count_get(server) - 1);
}

EFL_CALLBACKS_ARRAY_DEFINE(_efl_net_server_tcp_client_cbs,
                           { EFL_IO_CLOSER_EVENT_CLOSED, _efl_net_server_tcp_client_event_closed });

static void
_efl_net_server_tcp_efl_net_server_fd_client_add(Eo *o, void *pd EINA_UNUSED, int client_fd)
{
   Eo *client = efl_add(EFL_NET_SOCKET_TCP_CLASS, o,
                        efl_event_callback_array_add(efl_self, _efl_net_server_tcp_client_cbs(), o),
                        efl_loop_fd_set(efl_self, client_fd));
   if (!client)
     {
        ERR("could not create client object fd=%d", client_fd);
        close(client_fd);
        return;
     }

   efl_net_server_clients_count_set(o, efl_net_server_clients_count_get(o) + 1);
   efl_event_callback_call(o, EFL_NET_SERVER_EVENT_CLIENT_ADD, client);

   if (efl_ref_get(client) == 1) /* users must take a reference themselves */
     {
        DBG("client %s was not handled, closing it...",
            efl_net_socket_address_remote_get(client));
        efl_io_closer_close(client);
     }
}

static void
_efl_net_server_tcp_efl_net_server_fd_client_reject(Eo *o, void *pd EINA_UNUSED, int client_fd)
{
   struct sockaddr_storage addr;
   socklen_t addrlen;
   char str[INET6_ADDRSTRLEN + sizeof("[]:65536")] = "";

   addrlen = sizeof(addr);
   if (getpeername(client_fd, (struct sockaddr *)&addr, &addrlen) < 0)
     ERR("getpeername(%d): %s", client_fd, strerror(errno));
   else
     efl_net_ip_port_fmt(str, sizeof(str), (struct sockaddr *)&addr);

   close(client_fd);
   efl_event_callback_call(o, EFL_NET_SERVER_EVENT_CLIENT_REJECTED, str);
}

#include "efl_net_server_tcp.eo.c"
