/**
 * @file    udp_server.c
 * @brief   UDP server using LwIP on NUCLEO-F439ZI
 *
 * This module implements a UDP server that:
 *  - Listens on a fixed UDP port
 *  - Receives UDP datagrams from any client
 *  - Logs received packets over UART
 *  - Stores the last sender's IP address and port
 *  - Sends a fixed application-level ACK to the first client
 *
 * Notes:
 *  - UDP is connectionless; no session or state tracking is performed
 *    beyond remembering the last sender.
 *  - ACKs are always sent to the last known client address/port.
 */

#include "lwip/udp.h"
#include "lwip/pbuf.h"
#include "lwip/ip_addr.h"
#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdio.h>
#include <udpServer.h>

/* ========================= Configuration ========================= */

/** UDP listening port */
#define UDP_SERVER_PORT    5005

/** Maximum size of received UDP payload */
#define UDP_SERVER_BUFFER  20

/** UART handle defined in main application */
extern UART_HandleTypeDef huart3;

/* ========================= UDP Server State ========================= */

/**
 * @brief UDP protocol control block
 *
 * Represents the UDP endpoint bound to UDP_SERVER_PORT.
 */
static struct udp_pcb *udpPcb = NULL;

/**
 * @brief Last known client address information
 *
 * Used for sending ACK packets back to the most recent sender.
 */
static ip_addr_t lastClientAddr;   /**< Last client IP address */
static u16_t     lastClientPort;   /**< Last client UDP port */

/**
 * @brief Indicates whether at least one UDP packet has been received
 */
static bool clientKnown = false;

/* ========================= UART Helper ========================= */

/**
 * @brief Transmit a null-terminated string over UART
 *
 * @param msg Pointer to string to transmit
 *
 * Notes:
 * - Uses blocking HAL_UART_Transmit.
 * - Intended for debug/logging purposes.
 */
static void uartPrint(const char *msg)
{
    if (msg)
        HAL_UART_Transmit(&huart3,
                          (uint8_t *)msg,
                          strlen(msg),
                          HAL_MAX_DELAY);
}

/* ========================= UDP Receive Callback ========================= */

/**
 * @brief Callback invoked by LwIP when a UDP packet is received
 *
 * @param arg   User-defined argument (unused)
 * @param upcb  UDP PCB associated with this packet (unused)
 * @param p     pbuf chain containing received UDP payload
 * @param addr  IP address of the sender
 * @param port  UDP source port of the sender
 *
 * Behavior:
 *  - Copies payload into a local buffer (truncated if necessary)
 *  - Logs sender address, port, and payload over UART
 *  - Stores sender address/port for future ACK transmissions
 *  - Sends ACK immediately upon receiving the first packet
 *  - Frees the received pbuf after processing
 */
static void udpReceiveCallback(void *arg,
                                 struct udp_pcb *upcb,
                                 struct pbuf *p,
                                 const ip_addr_t *addr,
                                 u16_t port)
{
    LWIP_UNUSED_ARG(arg);
    LWIP_UNUSED_ARG(upcb);

    /* Ignore empty or invalid packets */
    if (!p || p->tot_len == 0)
        return;

    /* Copy UDP payload into a local buffer */
    char buf[UDP_SERVER_BUFFER] = {0};
    u16_t len = (p->tot_len < (UDP_SERVER_BUFFER - 1))
                ? p->tot_len
                : (UDP_SERVER_BUFFER - 1);

    pbuf_copy_partial(p, buf, len, 0);
    buf[len] = '\0';   /* Ensure null termination for safe printing */

    /* Log received packet via UART */
    char uartBuf[128];
    snprintf(uartBuf, sizeof(uartBuf),
             "UDP %s:%d -> %s\r\n",
             ipaddr_ntoa(addr),
             port,
             buf);
    uartPrint(uartBuf);

    /* Store sender information for future ACK transmissions */
    ip_addr_copy(lastClientAddr, *addr);
    lastClientPort = port;

    /*
     * Send ACK only once, on the first received packet.
     * Subsequent packets will update client info but
     * will not trigger additional ACKs.
     */
    if (!clientKnown) {
        clientKnown = true;
        udpSendAck();
    }

    /* Free received pbuf to avoid memory leaks */
    pbuf_free(p);
}

/* ========================= UDP Server Initialization ========================= */

/**
 * @brief Initialize the UDP server
 *
 * Creates a UDP PCB, binds it to the configured port,
 * and registers the receive callback.
 */
void udpServerInit(void)
{
    udpPcb = udp_new();
    if (!udpPcb)
        return;   /* PCB allocation failed */

    if (udp_bind(udpPcb, IP_ADDR_ANY, UDP_SERVER_PORT) != ERR_OK) {
        /* Binding failed: clean up PCB */
        udp_remove(udpPcb);
        udpPcb = NULL;
        return;
    }

    /* Register receive callback */
    udp_recv(udpPcb, udpReceiveCallback, NULL);
}

/* ========================= Send ACK to Client ========================= */

/**
 * @brief Send a fixed application-level ACK to the last known UDP client
 *
 * Notes:
 * - ACK is sent only if a client has been previously detected.
 * - Allocates a temporary pbuf for the ACK payload.
 * - Caller is responsible for ensuring udp_pcb is valid.
 */
void udpSendAck(void)
{
    if (!clientKnown || !udpPcb)
        return;

    const uint8_t ackData[] = {0xFF, 0x88, 0x00, 0xA1, 0x02};

    /* Allocate transport-layer pbuf for ACK */
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT,
                                sizeof(ackData),
                                PBUF_RAM);
    if (!p)
        return;   /* Allocation failed */

    /* Copy ACK payload into pbuf */
    pbuf_take(p, ackData, sizeof(ackData));

    /* Send ACK to last known client */
    udp_sendto(udpPcb, p,
               &lastClientAddr,
               lastClientPort);

    /* Free pbuf after transmission */
    pbuf_free(p);
}

/* ========================= Client Status ========================= */

/**
 * @brief Check whether a UDP client has sent at least one packet
 *
 * @retval true  At least one packet received
 * @retval false No packets received yet
 */
bool udpServerClientConnected(void)
{
    return clientKnown;
}
