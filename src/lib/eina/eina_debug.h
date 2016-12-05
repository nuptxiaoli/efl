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

#ifndef EINA_DEBUG_H_
# define EINA_DEBUG_H_

# include "eina_config.h"
# include "eina_list.h"

/**
 * @page eina_debug_main Eina Debug
 *
 * @date 2015 (created)
 */

/**
 * @addtogroup Eina_Debug
 * @{
 */

#define SERVER_PATH ".edebug"
#define SERVER_NAME "efl_debug"
#define SERVER_MASTER_PORT 0
#define SERVER_SLAVE_PORT 1

enum
{
   EINA_DEBUG_OPCODE_INVALID = -1, /**< Invalid opcode value */
   EINA_DEBUG_OPCODE_REGISTER = 0, /**< Opcode used to register other opcodes */
   EINA_DEBUG_OPCODE_HELLO = 1 /**< Opcode used to send greetings to the daemon */
};

/**
 * @typedef Eina_Debug_Session
 *
 * A handle used to interact with the debug daemon.
 * It contains all the information related to this connection and needed
 * during send/receive/dispatch/...
 */
typedef struct _Eina_Debug_Session Eina_Debug_Session;

/**
 * @typedef Eina_Debug_Cb
 *
 * A callback invoked when a specific packet is received.
 *
 * @param session the session
 * @param cid the source id
 * @param buffer the packet payload data. It doesn't contain any transport information.
 * @param size the packet payload size
 */
typedef Eina_Bool (*Eina_Debug_Cb)(Eina_Debug_Session *session, int cid, void *buffer, int size);

/**
 * @typedef Eina_Debug_Opcode_Status_Cb
 *
 * When a connection or a disconnection is happening, this callback is invoked.
 * It is needed to inform the upper layer the opcodes are valid/invalid.
 *
 * @param status EINA_TRUE if opcodes have been received from the daemon, EINA_FALSE otherwise.
 */
typedef void (*Eina_Debug_Opcode_Status_Cb)(Eina_Bool status);

/**
 * @typedef Eina_Debug_Dispatch_Cb
 *
 * Dispatcher callback prototype used to override the default dispatcher of a
 * session.
 *
 * @param session the session
 * @param buffer the packet received
 *
 * The given packet is the entire data received, including the header.
 */
typedef Eina_Bool (*Eina_Debug_Dispatch_Cb)(Eina_Debug_Session *session, void *buffer);

/**
 * @typedef Eina_Debug_Timer_Cb
 *
 * A callback for a timer
 */
typedef Eina_Bool (*Eina_Debug_Timer_Cb)(void);

/**
 * @typedef Eina_Debug_Encode_Cb
 *
 * Callback prototype for packet encoding
 *
 * @param buffer the buffer to encode
 * @param size the size of the given buffer
 * @param ret_size the encoded buffer size
 * @return the encoded buffer
 */
typedef void *(*Eina_Debug_Encode_Cb)(const void *buffer, int size, int *ret_size);

/**
 * @typedef Eina_Debug_Decode_Cb
 *
 * Callback prototype for packet decoding
 *
 * @param buffer the buffer to decode
 * @param size the size of the given buffer
 * @param ret_size the decoded buffer size
 * @return the decoded buffer
 */
typedef void *(*Eina_Debug_Decode_Cb)(const void *buffer, int size, int *ret_size);

/**
 * @typedef Eina_Debug_Packet_Header
 *
 * Header of Eina Debug packet
 */
typedef struct
{
   int size; /**< Packet size after this element */
   /**<
    * During sending, it corresponds to the id of the destination. During reception, it is the id of the source
    * The daemon is in charge of swapping the id before forwarding the packet to the destination.
    */
   int cid;
   int opcode; /**< Opcode of the packet */
} Eina_Debug_Packet_Header;

/**
 * @typedef Eina_Debug_Opcode
 *
 * Structure to describe information for an opcode. It is used to register new opcodes
 */
typedef struct
{
   char *opcode_name; /**< Opcode string. On registration, the daemon uses it to calculate an opcode id */
   int *opcode_id; /**< A pointer to store the opcode id when received from the daemon */
   Eina_Debug_Cb cb; /**< Callback to call when a packet corresponding to the opcode is received */
} Eina_Debug_Opcode;

/**
 * @enum Eina_Debug_Basic_Codec
 *
 * Predefined codecs to encode / decode buffer
 */
typedef enum
{
   EINA_DEBUG_CODEC_SHELL
} Eina_Debug_Basic_Codec;

/**
 * @brief Disable debugging
 *
 * Useful for bridge to prevent a connection as slave and for the daemon.
 * Need to be invoked before eina_init.
 */
EAPI void eina_debug_disable(void);

/**
 * @brief Connect to the local daemon
 *
 * @param is_master true if the application is a debugger. EINA_FALSE otherwise.
 *
 * @return EINA_TRUE on success, EINA_FALSE otherwise.
 */
EAPI Eina_Debug_Session *eina_debug_local_connect(Eina_Bool is_master);

/**
 * @brief Connect to remote shell daemon
 *
 * This function executes the given command. The given script will be applied line by line until full consumption.
 * The last script command should be the execution of efl_debug_shell_bridge.
 *
 * @param cmd the command to execute
 * @param script a list of commands to apply after executing cmd
 *
 * @return EINA_TRUE on success, EINA_FALSE otherwise.
 */
EAPI Eina_Debug_Session *eina_debug_shell_remote_connect(const char *cmd, Eina_List *script);

/**
 * @brief Terminate the given session
 *
 * @param session the session to terminate
 *
 */
EAPI void eina_debug_session_terminate(Eina_Debug_Session *session);

/**
 * @brief Override the dispatcher of a specific session
 *
 * For example, it can be used to forward a packet to the main thread and use
 * the default dispatcher there.
 *
 * @param session the session
 * @disp_cb the new dispatcher for the given session
 */
EAPI void eina_debug_session_dispatch_override(Eina_Debug_Session *session, Eina_Debug_Dispatch_Cb disp_cb);

/**
 * @brief Add codec hooks on a specific session
 *
 * This function can be used to encode packets before sending and decode
 * packets during reception.
 * The encoding ratio is mostly used to determine header size on reception.
 *
 * @param session the session
 * @param enc_cb the encoding function
 * @param dec_cb the decoding function
 * @param encoding_ratio the encoding ratio
 */
EAPI void eina_debug_session_codec_hooks_add(Eina_Debug_Session *session,
      Eina_Debug_Encode_Cb enc_cb, Eina_Debug_Decode_Cb dec_cb, double encoding_ratio);

/**
 * @brief Add a predefined codec to a session
 *
 * @param session the session
 * @param codec the codec to use
 */
EAPI void eina_debug_session_basic_codec_add(Eina_Debug_Session *session, Eina_Debug_Basic_Codec codec);

/**
 * @brief Request insertion of a magic field on sending
 *
 * This feature is needed to ensure reliability on specific connections
 *
 * @param session the session
 */
EAPI void eina_debug_session_magic_set_on_send(Eina_Debug_Session *session);

/**
 * @brief Request to look for a magic field on reception
 *
 * This feature is needed to ensure reliability on specific connections
 *
 * @param session the session
 */
EAPI void eina_debug_session_magic_set_on_recv(Eina_Debug_Session *session);

/**
 * @brief Dispatch a given packet according to its header.
 *
 * This function checks the header contained into the packet and invokes
 * the correct callback according to the opcode.
 * This is the default dispatcher.
 *
 * @param session the session
 * @param buffer the packet
 *
 * @return EINA_TRUE on success, EINA_FALSE otherwise.
 */
EAPI Eina_Bool eina_debug_dispatch(Eina_Debug_Session *session, void *buffer);

/**
 * @brief Register opcodes to a session
 *
 * This function registers opcodes for the given session. If the session is not
 * connected, the request is not sent to the daemon. Otherwise, the request for
 * the opcodes ids is sent
 * On the reception from the daemon, status_cb function is invoked to inform
 * the requester that the opcodes can now be used.
 */
EAPI void eina_debug_opcodes_register(Eina_Debug_Session *session,
      const Eina_Debug_Opcode ops[], Eina_Debug_Opcode_Status_Cb status_cb);

/**
 * @brief Send a packet to the given destination
 *
 * dest is a client described by a session and a client id. It can be created
 * with eina_debug_client_new.
 *
 * @param session the session to use to send the packet
 * @param dest_id the destination id to send the packet to
 * @param op the opcode for this packet
 * @param data payload to send
 * @param size payload size
 *
 * @return the number of sent bytes
 */
EAPI int eina_debug_session_send(Eina_Debug_Session *session, int dest_id, int op, void *data, int size);

/**
 * @brief Add a timer
 *
 * Needed for polling debug
 *
 * @param timeout_ms timeout in ms
 * @param cb callback to call when the timeout is reached
 *
 * @return EINA_TRUE on success, EINA_FALSE otherwise
 */
EAPI Eina_Bool eina_debug_timer_add(unsigned int timeout_ms, Eina_Debug_Timer_Cb cb);

#endif
/**
 * @}
 */
