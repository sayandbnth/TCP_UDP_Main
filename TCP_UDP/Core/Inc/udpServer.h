/**
 * @file    udp_server.h
 * @brief   UDP server interface using LwIP on NUCLEO-F439ZI
 *
 * Provides APIs to:
 *  - Initialize the UDP server
 *  - Send an application-level ACK to the last known client
 *  - Query whether a client has sent data
 */

#ifndef INC_UDPSERVER_H_
#define INC_UDPSERVER_H_

#include <stdbool.h>

/* ========================= Public API ========================= */

/**
 * @brief Initialize the UDP server.
 *
 * Creates a UDP PCB, binds it to the configured port,
 * and registers the receive callback.
 */
void udpServerInit(void);

/**
 * @brief Send a fixed ACK packet to the last known UDP client.
 *
 * Does nothing if no client has sent data yet.
 */
void udpSendAck(void);

/**
 * @brief Check whether a UDP client has sent at least one packet.
 *
 * @retval true  Client has sent data
 * @retval false No client activity detected
 */
bool udpServerClientConnected(void);

#endif /* INC_UDP_SERVER_H_ */
