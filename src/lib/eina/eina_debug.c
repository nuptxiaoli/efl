/* EINA - EFL data type library
 * Copyright (C) 2015 Carsten Haitzler
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library;
 * if not, see <http://www.gnu.org/licenses/>.
 */

# ifndef _GNU_SOURCE
#  define _GNU_SOURCE 1
# endif

#define DEBUGON 1

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>

#include "eina_debug.h"
#include "eina_types.h"
#include "eina_list.h"
#include "eina_mempool.h"
#include "eina_util.h"
#include "eina_evlog.h"
#include "eina_debug_private.h"

#ifdef __CYGWIN__
# define LIBEXT ".dll"
#else
# define LIBEXT ".so"
#endif

// yes - a global debug spinlock. i expect contention to be low for now, and
// when needed we can split this up into mroe locks to reduce contention when
// and if that day comes
Eina_Spinlock _eina_debug_lock;

// only init once
static Eina_Bool _inited = EINA_FALSE;
static char *_my_app_name = NULL;

extern Eina_Bool eina_module_init(void);
extern Eina_Bool eina_mempool_init(void);
extern Eina_Bool eina_list_init(void);

#define MAX_EVENTS   4

extern Eina_Spinlock      _eina_debug_thread_lock;

static Eina_Bool _debug_disabled = EINA_FALSE;

/* Local session */
/* __thread here to allow debuggers to be master and slave by using two different threads */
static __thread Eina_Debug_Session *_session = NULL;
static Eina_Debug_Session *_last_local_session = NULL;

/* Opcode used to load a module
 * needed by the daemon to notify loading success */
static int _module_init_opcode = EINA_DEBUG_OPCODE_INVALID;

// some state for debugging
static unsigned int _poll_time = 0;
static Eina_Debug_Timer_Cb _poll_timer_cb = NULL;

/* Magic number used in special cases for reliability */
#ifndef _WIN32
static char magic[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
#endif

typedef struct
{
   const Eina_Debug_Opcode *ops;
   Eina_Debug_Opcode_Status_Cb status_cb;
} _opcode_reply_info;

struct _Eina_Debug_Session
{
   Eina_Debug_Cb *cbs; /* Table of callbacks indexed by opcode id */
   Eina_List *opcode_reply_infos;
   Eina_List *script; /* Remaining list of script lines to apply */
   Eina_Debug_Dispatch_Cb dispatch_cb; /* Session dispatcher */
   Eina_Debug_Encode_Cb encode_cb; /* Packet encoder */
   Eina_Debug_Decode_Cb decode_cb; /* Packet decoder */
   double encoding_ratio; /* Encoding ratio */
   int cbs_length; /* cbs table size */
   int fd_in; /* File descriptor to read */
   int fd_out; /* File descriptor to write */
   int max_packet_size; /* Max packet size, useful for shell */
   short segment_sync; /* Data to send between packet segments or at the end */
   Eina_Bool magic_on_send : 1; /* Indicator to send magic on send */
   Eina_Bool magic_on_recv : 1; /* Indicator to expect magic on recv */
   Eina_Bool wait_for_input : 1; /* Indicator to wait for input before continuing sending */
};

static void _opcodes_register_all();
static void _thread_start(Eina_Debug_Session *session);

EAPI int
eina_debug_session_send(Eina_Debug_Session *session, int dest, int op, void *data, int size)
{
   int total_size = 0;
   unsigned char *buf = NULL, *data_buf = NULL;
   int nb = -1;

   if (!session) return -1;
   // send protocol packet. all protocol is an int for size of packet then
   // included in that size (so a minimum size of 4) is a 4 byte opcode
   // (all opcodes are 4 bytes as a string of 4 chars), then the real
   // message payload as a data blob after that
   total_size = size + sizeof(Eina_Debug_Packet_Header);
   buf = alloca(total_size);
   Eina_Debug_Packet_Header *hdr = (Eina_Debug_Packet_Header *)buf;
   hdr->size = total_size;
   hdr->opcode = op;
   hdr->cid = dest;
   if (size > 0) memcpy(buf + sizeof(Eina_Debug_Packet_Header), data, size);
   if (session->encode_cb)
     {
        int new_size = 0;
        void *new_buf = session->encode_cb(buf, total_size, &new_size);
        buf = new_buf;
        total_size = new_size;
     }
#ifndef _WIN32
   if (session->magic_on_send) write(session->fd_out, magic, 4);
   e_debug("socket: %d / opcode %X / packet size %ld / bytes to send: %d",
         session->fd_out, op, hdr->size + sizeof(int), total_size);
   data_buf = buf;
   while (total_size > 0)
     {
        nb = write(session->fd_out, data_buf,
              session->max_packet_size && total_size > session->max_packet_size ?
              session->max_packet_size : total_size);
        data_buf += nb;
        total_size -= nb;
        if (session->segment_sync)
           write(session->fd_out, &(session->segment_sync), sizeof(session->segment_sync));
     }
   if (session->encode_cb) free(buf);
#endif
   return size;
}

static void
_monitor_service_greet()
{
   // say hello to our debug daemon - tell them our PID and protocol
   // version we speak
   /* Version + Pid + App name */
   int size = 8 + (_my_app_name ? strlen(_my_app_name) : 0) + 1;
   unsigned char *buf = alloca(size);
   int version = 1; // version of protocol we speak
   int pid = getpid();
   memcpy(buf + 0, &version, 4);
   memcpy(buf + 4, &pid, 4);
   if (_my_app_name)
      memcpy(buf + 8, _my_app_name, strlen(_my_app_name) + 1);
   else
      buf[8] = '\0';
   eina_debug_session_send(_session, 0, EINA_DEBUG_OPCODE_HELLO, buf, size);
}

#ifndef _WIN32
/*
 * Used to consume a script line.
 * WAIT token is used to wait for input after a script line has been
 * applied.
 */
static void
_script_consume()
{
   const char *line = NULL;
   do {
        line = eina_list_data_get(_session->script);
        _session->script = eina_list_remove_list(_session->script, _session->script);
        if (line)
          {
             if (!strncmp(line, "WAIT", 4))
               {
                  e_debug("Wait for input");
                  _session->wait_for_input = EINA_TRUE;
                  return;
               }
             else if (!strncmp(line, "SLEEP_1", 7))
               {
                  e_debug("Sleep 1s");
                  sleep(1);
               }
             else
               {
                  e_debug("Apply script line: %s", line);
                  write(_session->fd_out, line, strlen(line));
                  write(_session->fd_out, "\n", 1);
               }
          }
   }
   while (line);
   /* When all the script has been applied, we can begin to send debug packets */
   _monitor_service_greet();
   _opcodes_register_all();
}

static int
_packet_receive(unsigned char **buffer)
{
   double ratio = 1.0;
   int size_sz = sizeof(int);
   unsigned char *recv_buf = NULL;
   int rret;
   int recv_size = 0;

   if (!_session) return -1;

   if (_session->wait_for_input)
     {
        /* Wait for input */
        char c;
        e_debug_begin("Characters received: ");
        while (read(_session->fd_in, &c, 1) == 1) e_debug_continue("%c", c);
        e_debug_end();
        _session->wait_for_input = EINA_FALSE;
        _script_consume();
        return 0;
     }

   if (_session->magic_on_recv)
     {
        /* All the bytes before the magic field have to be dropped. */
        char c;
        int ret, i = 0;
        do
          {
             ret = read(_session->fd_in, &c, 1);
             if (ret != 1) return 0;
             if (c == magic[i]) i++;
             else i = 0;
          }
        while (i < 4);
        e_debug("Magic found");
     }
   ratio = _session->decode_cb && _session->encoding_ratio ? _session->encoding_ratio : 1.0;
   size_sz *= ratio;
   recv_buf = malloc(size_sz);
   /*
    * Retrieve packet size
    * We need to take into account that the size could arrive in more than one chunk
    */
   do
     {
        while ((rret = read(_session->fd_in, recv_buf + recv_size, size_sz - recv_size)) == -1 &&
              errno == EAGAIN);
        if (rret <= 0) goto error;
        recv_size += rret;
     }
   while (recv_size != size_sz);

   e_debug("%d - %d", _session->fd_in, rret);
   if (rret == size_sz)
     {
        int size = 0;
        if (_session->decode_cb)
          {
             /* Decode the size if needed */
             void *size_buf = _session->decode_cb(recv_buf, size_sz, NULL);
             size = *(int *)size_buf;
             free(size_buf);
          }
        else size = *(int *)recv_buf;
        e_debug("%d/%d - %d", getpid(), _session->fd_in, size);
        // allocate a buffer for the next bytes to receive
        recv_buf = realloc(recv_buf, size * ratio);
        if (recv_buf)
          {
             int cur_packet_size = sizeof(int);
             unsigned char *packet_buf = malloc(size);
             memcpy(packet_buf, &size, sizeof(int));
             /* Receive all the remaining packet bytes */
             while (cur_packet_size < size)
               {
                  while ((rret = read(_session->fd_in, recv_buf, (size - cur_packet_size) * ratio)) == -1 &&
                        errno == EAGAIN);
                  if (rret <= 0)
                    {
                       free(recv_buf);
                       free(packet_buf);
                       return -1;
                    }
                  /* Decoding all the packets */
                  if (_session->decode_cb)
                    {
                       int decoded_size = 0;
                       char *decoded_buf = _session->decode_cb(recv_buf, rret, &decoded_size);
                       if (decoded_buf && decoded_size)
                         {
                            memcpy(packet_buf + cur_packet_size + sizeof(int), decoded_buf, decoded_size);
                            cur_packet_size += decoded_size;
                            free(decoded_buf);
                         }
                    }
                  else
                    {
                       memcpy(packet_buf + cur_packet_size, recv_buf, rret);
                       cur_packet_size += rret;
                    }
               }
             free(recv_buf);
             if (cur_packet_size != size)
               {
                  // we didn't get payload as expected - error on comms
                  e_debug("EINA DEBUG ERROR: "
                        "Invalid payload+header size: read %i expected %i", cur_packet_size, size);
                  free(packet_buf);
                  return -1;
               }
             else
                *buffer = packet_buf;
             /*
              * In case of an interactive shell, 0x0A is needed at the end of the packet to be sent
              * So we need to retrieve it too.
              * Magic number can't help here because we don't have it in both directions.
              */
             if (_session->segment_sync)
               {
                  char c[2];
                  while (read(_session->fd_in, c, 2) == -1 && errno == EAGAIN);
               }
             return cur_packet_size;
          }
        else
          {
             // we couldn't allocate memory for payloa buffer
             // internal memory limit error
             e_debug("EINA DEBUG ERROR: "
                   "Cannot allocate %u bytes for op", (unsigned int)(size * ratio));
             return -1;
          }
     }
error:
   if (rret == -1) perror("Read from socket");
   if (rret)
      e_debug("EINA DEBUG ERROR: "
            "Invalid size read %i != %i", rret, size_sz);

   if (recv_buf) free(recv_buf);
   return -1;
}
#endif

EAPI void
eina_debug_disable()
{
   _debug_disabled = EINA_TRUE;
}

EAPI void
eina_debug_session_terminate(Eina_Debug_Session *session)
{
   /* FIXME: Maybe just close fd here so the thread terminates its own session by itself */
   if (!session) return;

   _opcode_reply_info *info = NULL;

   EINA_LIST_FREE(session->opcode_reply_infos, info)
      free(info);

   free(session->cbs);
   free(session);
}

EAPI void
eina_debug_session_dispatch_override(Eina_Debug_Session *session, Eina_Debug_Dispatch_Cb disp_cb)
{
   if (!session) return;
   if (!disp_cb) disp_cb = eina_debug_dispatch;
   session->dispatch_cb = disp_cb;
}

typedef struct {
     Eina_Module *handle;
     int (*init)(void);
     int (*shutdown)(void);
} _module_info;

#define _LOAD_SYMBOL(cls_struct, pkg, sym) \
   do \
     { \
        char func_name[1024]; \
        snprintf(func_name, sizeof(func_name), "%s_debug_" #sym, pkg); \
        (cls_struct).sym = eina_module_symbol_get((cls_struct).handle, func_name); \
        if (!(cls_struct).sym) \
          { \
             e_debug("Failed loading symbol '%s' from the library.", func_name); \
             eina_module_free((cls_struct).handle); \
             (cls_struct).handle = NULL; \
             return EINA_FALSE; \
          } \
     } \
   while (0)

static Eina_Bool
_module_init_cb(Eina_Debug_Session *session, int cid, void *buffer, int size)
{
   _module_info minfo;
   char module_path[1024];
   const char *module_name = buffer;
   if (size <= 0) return EINA_FALSE;
   e_debug("Init module %s", module_name);
   snprintf(module_path, sizeof(module_path), PACKAGE_LIB_DIR "/lib%s_debug"LIBEXT, module_name);
   minfo.handle = eina_module_new(module_path);
   if (!minfo.handle || !eina_module_load(minfo.handle))
     {
        e_debug("Failed loading debug module %s.", module_name);
        if (minfo.handle) eina_module_free(minfo.handle);
        minfo.handle = NULL;
        return EINA_TRUE;
     }

   _LOAD_SYMBOL(minfo, module_name, init);
   _LOAD_SYMBOL(minfo, module_name, shutdown);

   minfo.init();

   eina_debug_session_send(session, cid, _module_init_opcode, buffer, size);
   return EINA_TRUE;
}

static Eina_Bool
_module_shutdown_cb(Eina_Debug_Session *session EINA_UNUSED, int cid EINA_UNUSED, void *buffer EINA_UNUSED, int size EINA_UNUSED)
{
   return EINA_TRUE;
}

static const Eina_Debug_Opcode _EINA_DEBUG_MONITOR_OPS[] = {
       {"module/init", &_module_init_opcode, &_module_init_cb},
       {"module/shutdown", NULL, &_module_shutdown_cb},
       {NULL, NULL, NULL}
};

static void
_static_opcode_register(Eina_Debug_Session *session,
      int op_id, Eina_Debug_Cb cb)
{
   if(session->cbs_length < op_id + 1)
     {
        int i = session->cbs_length;
        session->cbs_length = op_id + 16;
        session->cbs = realloc(session->cbs, session->cbs_length * sizeof(Eina_Debug_Cb));
        for(; i < session->cbs_length; i++)
           session->cbs[i] = NULL;
     }
   session->cbs[op_id] = cb;
}

/*
 * Response of the daemon containing the ids of the requested opcodes.
 * PTR64 + (opcode id)*
 */
static Eina_Bool
_callbacks_register_cb(Eina_Debug_Session *session, int src_id EINA_UNUSED, void *buffer, int size)
{
   _opcode_reply_info *info = NULL;
   int *os;
   unsigned int count, i;

   uint64_t info_64;
   memcpy(&info_64, buffer, sizeof(uint64_t));
   info = (_opcode_reply_info *)info_64;

   if (!info) return EINA_FALSE;

   os = (int *)((unsigned char *)buffer + sizeof(uint64_t));
   count = (size - sizeof(uint64_t)) / sizeof(int);

   for (i = 0; i < count; i++)
     {
        if (info->ops[i].opcode_id) *(info->ops[i].opcode_id) = os[i];
        _static_opcode_register(session, os[i], info->ops[i].cb);
        e_debug("Opcode %s -> %d", info->ops[i].opcode_name, os[i]);
     }
   if (info->status_cb) info->status_cb(EINA_TRUE);

   return EINA_TRUE;
}

static void
_opcodes_registration_send(Eina_Debug_Session *session,
      _opcode_reply_info *info)
{
   unsigned char *buf;

   int count = 0;
   int size = sizeof(uint64_t);

   while(info->ops[count].opcode_name)
     {
        size += strlen(info->ops[count].opcode_name) + 1;
        count++;
     }

   buf = alloca(size);

   uint64_t info_64 = (uint64_t)info;
   memcpy(buf, &info_64, sizeof(uint64_t));
   int size_curr = sizeof(uint64_t);

   count = 0;
   while(info->ops[count].opcode_name)
     {
        int len = strlen(info->ops[count].opcode_name) + 1;
        memcpy(buf + size_curr, info->ops[count].opcode_name, len);
        size_curr += len;
        count++;
     }

   eina_debug_session_send(session, 0,
         EINA_DEBUG_OPCODE_REGISTER,
         buf,
         size);
}

static void
_opcodes_register_all()
{
   Eina_List *l;
   _opcode_reply_info *info = NULL;

   info = calloc(1, sizeof(*info));
   info->ops = _EINA_DEBUG_MONITOR_OPS;
   _session->opcode_reply_infos = eina_list_append(
         _session->opcode_reply_infos, info);

   _static_opcode_register(_session,
         EINA_DEBUG_OPCODE_REGISTER, _callbacks_register_cb);
   EINA_LIST_FOREACH(_session->opcode_reply_infos, l, info)
        _opcodes_registration_send(_session, info);;
}

static void
_opcodes_unregister_all(Eina_Debug_Session *session)
{
   Eina_List *l;
   _opcode_reply_info *info = NULL;

   if (!session) return;
   session->cbs_length = 0;
   free(session->cbs);
   session->cbs = NULL;

   EINA_LIST_FOREACH(session->opcode_reply_infos, l, info)
     {
        const Eina_Debug_Opcode *op = info->ops;
        while(!op->opcode_name)
          {
             if (op->opcode_id) *(op->opcode_id) = EINA_DEBUG_OPCODE_INVALID;
             op++;
          }
        if (info->status_cb) info->status_cb(EINA_FALSE);
     }
}

static const char *
_socket_home_get()
{
   // get possible debug daemon socket directory base
   const char *dir = getenv("XDG_RUNTIME_DIR");
   if (!dir) dir = eina_environment_home_get();
   if (!dir) dir = eina_environment_tmp_get();
   return dir;
}

#ifndef _WIN32
#define LENGTH_OF_SOCKADDR_UN(s) \
   (strlen((s)->sun_path) + (size_t)(((struct sockaddr_un *)NULL)->sun_path))
#endif

EAPI Eina_Debug_Session *
eina_debug_local_connect(Eina_Bool is_master)
{
#ifndef _WIN32
   char buf[4096];
   int fd, socket_unix_len, curstate = 0;
   struct sockaddr_un socket_unix;
#endif

   Eina_Debug_Session *session = calloc(1, sizeof(*session));
   session->dispatch_cb = eina_debug_dispatch;
   // try this socket file - it will likely be:
   //   ~/.ecore/efl_debug/0
   // or maybe
   //   /var/run/UID/.ecore/efl_debug/0
   // either way a 4k buffer should be ebough ( if it's not we're on an
   // insane system)
#ifndef _WIN32
   snprintf(buf, sizeof(buf), "%s/%s/%s/%i", _socket_home_get(), SERVER_PATH, SERVER_NAME,
         is_master ? SERVER_MASTER_PORT : SERVER_SLAVE_PORT);
   // create the socket
   fd = socket(AF_UNIX, SOCK_STREAM, 0);
   if (fd < 0) goto err;
   // set the socket to close when we exec things so they don't inherit it
   if (fcntl(fd, F_SETFD, FD_CLOEXEC) < 0) goto err;
   // set up some socket options on addr re-use
   if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&curstate,
                  sizeof(curstate)) < 0)
     goto err;
   // sa that it's a unix socket and where the path is
   socket_unix.sun_family = AF_UNIX;
   strncpy(socket_unix.sun_path, buf, sizeof(socket_unix.sun_path) - 1);
   socket_unix_len = LENGTH_OF_SOCKADDR_UN(&socket_unix);
   // actually connect to efl_debugd service
   if (connect(fd, (struct sockaddr *)&socket_unix, socket_unix_len) < 0)
      goto err;
   // we succeeded
   session->fd_out = session->fd_in = fd;
   // start the monitor thread
   _thread_start(session);
   _last_local_session = session;
   return session;
err:
   // some kind of connection failure here, so close a valid socket and
   // get out of here
   if (fd >= 0) close(fd);
   if (session) free(session);
#else
   (void) _session;
   (void) type;
#endif
   return NULL;
}

EAPI Eina_Debug_Session *
eina_debug_shell_remote_connect(const char *cmd, Eina_List *script)
{
#ifndef _WIN32
   int pipeToShell[2], pipeFromShell[2];
   int pid = -1;
   pipe(pipeToShell);
   pipe(pipeFromShell);

   pid = fork();
   if (pid == -1) return EINA_FALSE;
   if (!pid)
     {
        int i = 0;
        const char *args[16] = { 0 };
        char *tmp = strdup(cmd);
        /* Child */
        close(STDIN_FILENO);
        dup2(pipeToShell[0], STDIN_FILENO);
        close(STDOUT_FILENO);
        dup2(pipeFromShell[1], STDOUT_FILENO);
        args[i++] = tmp;
        do
          {
             tmp = strchr(tmp, ' ');
             if (tmp)
               {
                  *tmp = '\0';
                  args[i++] = ++tmp;
               }
          }
        while (tmp);
        args[i++] = 0;
        execv(args[0], (char **)args);
        perror("execv");
        _exit(-1);
     }
   else
     {
        Eina_Debug_Session *session = calloc(1, sizeof(*session));
        /* Parent */
        session->dispatch_cb = eina_debug_dispatch;
        session->fd_in = pipeFromShell[0];
        eina_debug_session_magic_set_on_recv(session);
        if (fcntl(session->fd_in, F_SETFL, O_NONBLOCK) == -1) perror(0);
        session->fd_out = pipeToShell[1];
        session->script = script;
        _script_consume(session);
        // start the monitor thread
        _thread_start(session);
        return session;
     }
#else
   (void) cmd;
   (void) script;
   return NULL;
#endif
}

EAPI Eina_Bool
eina_debug_timer_add(unsigned int timeout_ms, Eina_Debug_Timer_Cb cb)
{
   _poll_time = timeout_ms;
   _poll_timer_cb = cb;
   return EINA_TRUE;
}

// this is a DEDICATED debug thread to monitor the application so it works
// even if the mainloop is blocked or the app otherwise deadlocked in some
// way. this is an alternative to using external debuggers so we can get
// users or developers to get useful information about an app at all times
static void *
_monitor(void *_data)
{
#ifndef _WIN32
   int ret;
   struct epoll_event event;
   struct epoll_event events[MAX_EVENTS];
   int epfd = epoll_create(MAX_EVENTS);

   _session = _data;
   event.data.fd = _session->fd_in;
   event.events = EPOLLIN;
   ret = epoll_ctl(epfd, EPOLL_CTL_ADD, _session->fd_in, &event);
   if (ret) perror("epoll_ctl/add");

   _monitor_service_greet();
   _opcodes_register_all();

   // set a name for this thread for system debugging
#ifdef EINA_HAVE_PTHREAD_SETNAME
# ifndef __linux__
   pthread_set_name_np
# else
   pthread_setname_np
# endif
     (pthread_self(), "Edbg-mon");
#endif

   // sit forever processing commands or timeouts in the debug monitor
   // thread - this is separate to the rest of the app so it shouldn't
   // impact the application specifically
   for (;_session;)
     {
        // if we are in a polling mode then set up a timeout and wait for it
        int timeout = _poll_time ? (int)_poll_time : -1; //in milliseconds

        ret = epoll_wait(epfd, events, MAX_EVENTS, timeout);

        // if the fd for debug daemon says it's alive, process it
        if (ret)
          {
             int i;
             //check which fd are set/ready for read
             for (i = 0; i < ret; i++)
               {
                  if (events[i].events & EPOLLHUP)
                    {
                       free(_session);
                       _session = NULL;
                    }
                  else if (events[i].events & EPOLLIN)
                    {
                       int size;
                       unsigned char *buffer;

                       size = _packet_receive(&buffer);
                       // if not negative - we have a real message
                       if (size > 0)
                         {
                            if(!_session->dispatch_cb(_session, buffer))
                              {
                                 // something we don't understand
                                 e_debug("EINA DEBUG ERROR: Unknown command");
                              }
                         }
                       else if (size == 0)
                         {
                            // May be due to a response from a script line
                         }
                       else
                         {
                            // major failure on debug daemon control fd - get out of here.
                            //   else goto fail;
                            close(_session->fd_in);
                            //TODO if its not main _session we will tell the main_loop
                            //that it disconneted
                         }
                    }
               }
          }
        else
          {
             if (_poll_time && _poll_timer_cb)
               {
                  if (!_poll_timer_cb()) _poll_time = 0;
               }
          }
     }
#endif
   return NULL;
}

// start up the debug monitor if we haven't already
static void
_thread_start(Eina_Debug_Session *session)
{
   pthread_t monitor_thread;
   int err;
   sigset_t oldset, newset;

   sigemptyset(&newset);
   sigaddset(&newset, SIGPIPE);
   sigaddset(&newset, SIGALRM);
   sigaddset(&newset, SIGCHLD);
   sigaddset(&newset, SIGUSR1);
   sigaddset(&newset, SIGUSR2);
   sigaddset(&newset, SIGHUP);
   sigaddset(&newset, SIGQUIT);
   sigaddset(&newset, SIGINT);
   sigaddset(&newset, SIGTERM);
#ifdef SIGPWR
   sigaddset(&newset, SIGPWR);
#endif
   sigprocmask(SIG_BLOCK, &newset, &oldset);

   err = pthread_create(&monitor_thread, NULL, _monitor, session);
   sigprocmask(SIG_SETMASK, &oldset, NULL);
   if (err != 0)
     {
        e_debug("EINA DEBUG ERROR: Can't create monitor debug thread!");
        abort();
     }
}

/*
 * Sends to daemon:
 * - Pointer to ops: returned in the response to determine which opcodes have been added
 * - List of opcode names seperated by \0
 */
EAPI void
eina_debug_opcodes_register(Eina_Debug_Session *session, const Eina_Debug_Opcode ops[],
      Eina_Debug_Opcode_Status_Cb status_cb)
{
   if (!session) session = _last_local_session;
   if (!session) return;

   _opcode_reply_info *info = malloc(sizeof(*info));
   info->ops = ops;
   info->status_cb = status_cb;

   session->opcode_reply_infos = eina_list_append(
         session->opcode_reply_infos, info);

   //send only if _session's fd connected, if not -  it will be sent when connected
   if(session && session->fd_in && !session->script)
      _opcodes_registration_send(session, info);
}

Eina_Bool
eina_debug_dispatch(Eina_Debug_Session *session, void *buffer)
{
   Eina_Debug_Packet_Header *hdr =  buffer;
   int opcode = hdr->opcode;
   Eina_Debug_Cb cb = NULL;

   if(opcode < session->cbs_length) cb = session->cbs[opcode];

   if (cb)
     {
        cb(session, hdr->cid,
              (unsigned char *)buffer + sizeof(*hdr),
              hdr->size - sizeof(*hdr));
        free(buffer);
        return EINA_TRUE;
     }
   else e_debug("Invalid opcode %d", opcode);
   free(buffer);
   return EINA_FALSE;
}

/*
 * Encoder for shell sessions
 * Each byte is encoded in two bytes.
 * 0x0A is appended at the end of the new buffer, as it is needed by shells
 */
static void *
_shell_encode_cb(const void *data, int src_size, int *dest_size)
{
   const char *src = data;
   int new_size = src_size * 2;
   char *dest = malloc(new_size);
   int i;
   for (i = 0; i < src_size; i++)
     {
        dest[(i << 1) + 0] = ((src[i] & 0xF0) >> 4) + 0x40;
        dest[(i << 1) + 1] = ((src[i] & 0x0F) >> 0) + 0x40;
     }
   if (dest_size) *dest_size = new_size;
   return dest;
}

/*
 * Decoder for shell sessions
 * Each two bytes are merged into one byte.
 * The appended 0x0A appended during encoding cannot be handled
 * in this function, as it is not a part of the packet.
 */
static void *
_shell_decode_cb(const void *data, int src_size, int *dest_size)
{
   const char *src = data;
   int i = 0, j;
   char *dest = malloc(src_size / 2);
   if (!dest) goto error;
   for (i = 0, j = 0; j < src_size; j++)
     {
        if ((src[j] & 0xF0) == 0x40 && (src[j + 1] & 0xF0) == 0x40)
          {
             dest[i++] = ((src[j] - 0x40) << 4) | ((src[j + 1] - 0x40));
             j++;
          }
     }
   goto end;
error:
   free(dest);
   dest = NULL;
end:
   if (dest_size) *dest_size = i;
   return dest;
}

EAPI void
eina_debug_session_codec_hooks_add(Eina_Debug_Session *session,
      Eina_Debug_Encode_Cb enc_cb, Eina_Debug_Decode_Cb dec_cb, double encoding_ratio)
{
   if (!_session) return;
   session->encode_cb = enc_cb;
   session->decode_cb = dec_cb;
   session->encoding_ratio = encoding_ratio;
}

EAPI void
eina_debug_session_basic_codec_add(Eina_Debug_Session *session, Eina_Debug_Basic_Codec codec)
{
   switch(codec)
     {
      case EINA_DEBUG_CODEC_SHELL:
         eina_debug_session_codec_hooks_add(session, _shell_encode_cb, _shell_decode_cb, 2.0);
         session->max_packet_size = 4000;
         session->segment_sync = 0x0A0A;
         break;
      default:
         e_debug("EINA DEBUG ERROR: Bad basic encoding");
     }
}

EAPI void
eina_debug_session_magic_set_on_send(Eina_Debug_Session *session)
{
   if (!session) return;
   session->magic_on_send = EINA_TRUE;
}

EAPI void
eina_debug_session_magic_set_on_recv(Eina_Debug_Session *session)
{
   if (!session) return;
   session->magic_on_recv = EINA_TRUE;
}

#ifdef __linux__
   extern char *__progname;
#endif

Eina_Bool
eina_debug_init(void)
{
   pthread_t self;

   // if already inbitted simply release our lock that we may have locked on
   // shutdown if we are re-initted again in the same process
   if (_inited)
     {
        eina_spinlock_release(&_eina_debug_thread_lock);
        return EINA_TRUE;
     }
   // mark as initted
   _inited = EINA_TRUE;
   eina_module_init();
   eina_mempool_init();
   eina_list_init();
#ifdef __linux__
   _my_app_name = __progname;
#endif
   // For Windows support GetModuleFileName can be used
   // set up thread things
   eina_spinlock_new(&_eina_debug_lock);
   eina_spinlock_new(&_eina_debug_thread_lock);
   self = pthread_self();
   _eina_debug_thread_mainloop_set(&self);
   _eina_debug_thread_add(&self);
#if defined(HAVE_GETUID) && defined(HAVE_GETEUID)
   // if we are setuid - don't debug!
   if (getuid() != geteuid()) return EINA_TRUE;
#endif
   // if someone uses the EFL_NODEBUG env var - do not do debugging. handy
   // for when this debug code is buggy itself
   if (getenv("EFL_NODEBUG")) return EINA_TRUE;
   if (!_debug_disabled)
     {
        eina_debug_local_connect(EINA_FALSE);
     }
   _eina_debug_cpu_init();
   _eina_debug_bt_init();
   return EINA_TRUE;
}

Eina_Bool
eina_debug_shutdown(void)
{
   _eina_debug_bt_shutdown();
   _eina_debug_cpu_shutdown();
   eina_spinlock_take(&_eina_debug_thread_lock);
   // yes - we never free on shutdown - this is because the monitor thread
   // never exits. this is not a leak - we intend to never free up any
   // resources here because they are allocated once only ever.
   return EINA_TRUE;
}
