/**
 * \addtogroup uip
 * @{
 */

/**
 * \file
 * Configuration options for uIP.
 * \author Adam Dunkels <adam@dunkels.com>
 *
 * This file is used for tweaking various configuration options for
 * uIP. You should make a copy of this file into one of your project's
 * directories instead of editing this example "uipopt.h" file that
 * comes with the uIP distribution.
 */

/*
 * Copyright (c) 2001-2003, Adam Dunkels.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This file is part of the uIP TCP/IP stack.
 *
 * $Id: uipopt.h,v 1.14 2010/12/24 00:39:04 dak664 Exp $
 *
 */

#ifndef __UIPOPT_H__
#define __UIPOPT_H__

#include <stdint.h>

#include "net/net.h"

typedef uint32_t uip_stats_t;

#if CONFIG_UIP_MAX_TCP_MSS
#undef CONFIG_UIP_TCP_MSS
#define CONFIG_UIP_TCP_MSS \
	(CONFIG_UIP_BUFSIZE - CONFIG_UIP_LLH_LEN - UIP_TCPIP_HLEN)
#endif

#if CONFIG_UIP_DEFAULT_RECEIVE_WINDOW
#undef CONFIG_UIP_RECEIVE_WINDOW
#define CONFIG_UIP_RECEIVE_WINDOW (CONFIG_UIP_TCP_MSS)
#endif

#if CONFIG_UIP_DEFAULT_BUFSIZE
#undef CONFIG_UIP_BUFSIZE
#define CONFIG_UIP_BUFSIZE (CONFIG_UIP_LINK_MTU + CONFIG_UIP_LLH_LEN)
#endif

/**
 * Print out a uIP log message.
 *
 * This function must be implemented by the module that uses uIP, and
 * is called by uIP whenever a log message is generated.
 */
void uip_log(char *msg);

#define UIP_LITTLE_ENDIAN  3412
#define UIP_BIG_ENDIAN     1234

#if defined CONFIG_BIG_ENDIAN
#define UIP_BYTE_ORDER     (UIP_BIG_ENDIAN)
#else
#define UIP_BYTE_ORDER     (UIP_LITTLE_ENDIAN)
#endif

/** @} */
/*------------------------------------------------------------------------------*/

/**
 * \defgroup uipoptapp Application specific configurations
 * @{
 *
 * An uIP application is implemented using a single application
 * function that is called by uIP whenever a TCP/IP event occurs. The
 * name of this function must be registered with uIP at compile time
 * using the UIP_APPCALL definition.
 *
 * uIP applications can store the application state within the
 * uip_conn structure by specifying the type of the application
 * structure by typedef:ing the type uip_tcp_appstate_t and uip_udp_appstate_t.
 *
 * The file containing the definitions must be included in the
 * uipopt.h file.
 *
 * The following example illustrates how this can look.
 \code

 void httpd_appcall(void);
 #define UIP_APPCALL     httpd_appcall

 struct httpd_state {
 uint8_t state;
 uint16_t count;
 char *dataptr;
 char *script;
 };
 typedef struct httpd_state uip_tcp_appstate_t
 \endcode
*/

/**
 * \var #define UIP_APPCALL
 *
 * The name of the application function that uIP should call in
 * response to TCP/IP events.
 *
 */

#define UIP_APPCALL net_call_callback
#define UIP_UDP_APPCALL net_call_callback

/**
 * \var typedef uip_tcp_appstate_t
 *
 * The type of the application state that is to be stored in the
 * uip_conn structure. This usually is typedef:ed to a struct holding
 * application state information.
 */

typedef struct uip_tcp_appstate_t
{
} uip_tcp_appstate_t;

/**
 * \var typedef uip_udp_appstate_t
 *
 * The type of the application state that is to be stored in the
 * uip_conn structure. This usually is typedef:ed to a struct holding
 * application state information.
 */
/** @} */

typedef struct uip_udp_appstate_t
{
} uip_udp_appstate_t;

#endif /* __UIPOPT_H__ */
/** @} */
/** @} */
