/*
 * -----------------------------------------------------------------------------
 * File: tcp_server.h
 * Created on: 29-Jan-2026
 * Author: Robotics 01
 *
 * Description:
 *   TCP server interface for STM32 using LwIP.
 *   - Initializes a TCP server listening on a configured port
 *   - Supports a single client connection
 *   - Provides a function to send a fixed ACK packet to the client
 * -----------------------------------------------------------------------------
 */

#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include "lwip/tcp.h"

/* --------------------------------------------------------------------------
 * Function Prototypes
 * -------------------------------------------------------------------------- */

/**
 * @brief  Initialize the TCP server.
 *         - Creates a TCP listening PCB
 *         - Binds it to the configured port
 *         - Registers accept and receive callbacks
 */
void tcp_server_init(void);

/**
 * @brief  Send a fixed ACK packet to the currently connected TCP client.
 *         Does nothing if no client is connected.
 */
void tcp_send_ack(void);

#endif /* TCP_SERVER_H */
