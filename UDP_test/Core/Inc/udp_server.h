#ifndef UDP_SERVER_H
#define UDP_SERVER_H

/**
 * @file udp_server.h
 * @brief Simple UDP server for Nucleo F439ZI using LWIP
 *        - Receives UDP packets and prints via UART
 *        - Can send ACK to the last known client
 */

#include "lwip/udp.h"
#include "lwip/ip_addr.h"

/* Initialize the UDP server */
void udp_server_init(void);

/* Send ACK to the last known client */
void udp_send_ack(void);

/* Expose UDP PCB (optional, can be used for advanced operations) */
extern struct udp_pcb *udp_pcb;

#endif /* UDP_SERVER_H */
