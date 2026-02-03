/**
 * @file    tcp_server.h
 * @brief   TCP server interface using LwIP on STM32F4
 *
 * Provides APIs to:
 *  - Initialize the TCP server
 *  - Send application-level ACKs to the connected client
 *  - Query client connection status
 */

#ifndef INC_TCPSERVER_H_
#define INC_TCPSERVER_H_

#include <stdbool.h>

/* ========================= Public API ========================= */

/**
 * @brief Initialize the TCP server.
 *
 * Creates a listening TCP PCB, binds it to the configured port,
 * and registers accept, receive, and error callbacks.
 */
void tcpServerInit(void);

/**
 * @brief Send a fixed ACK packet to the currently connected TCP client.
 *
 * Does nothing if no client is connected or the connection
 * is not fully established.
 */
void tcpSendAck(void);

/**
 * @brief Check whether a TCP client is currently connected.
 *
 * @retval true  Client is connected
 * @retval false No active client connection
 */
bool tcpServerClientConnected(void);

#endif /* INC_TCPSERVER_H_ */
