/**
 * @file    tcp_server.c
 * @brief   Robust single-client TCP server using LwIP on STM32F439ZI
 *
 * This module implements a simple TCP server that:
 *  - Listens on a fixed TCP port
 *  - Accepts only one client at a time
 *  - Receives incoming data packets
 *  - Sends an application-level ACK back to the client
 *  - Logs received data over UART
 *
 * The implementation is designed for embedded systems using LwIP RAW API.
 */

#include "lwip/tcp.h"
#include "lwip/pbuf.h"
#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdio.h>
#include <tcpServer.h>

/* ========================= Configuration ========================= */

/** TCP listening port number */
#define TCP_SERVER_PORT    5000

/** Maximum size of received TCP payload */
#define TCP_SERVER_BUFFER  64

/** UART handle declared in main application */
extern UART_HandleTypeDef huart3;

/* ========================= TCP Server State ========================= */

/**
 * @brief Listening PCB for the TCP server
 *
 * This PCB remains in LISTEN state and accepts new incoming connections.
 */
static struct tcp_pcb *tcpListenPcb = NULL;

/**
 * @brief PCB of the currently connected TCP client
 *
 * Only one client connection is supported at a time.
 */
static struct tcp_pcb *tcpClientPcb = NULL;

/**
 * @brief Indicates whether a TCP client is currently connected
 */
static bool clientConnected = false;

/* ========================= UART Helper ========================= */

/**
 * @brief Transmit a null-terminated string over UART
 *
 * @param msg String to be transmitted
 */
static void uartPrint(const char *msg)
{
    if (msg)
        HAL_UART_Transmit(&huart3, (uint8_t *)msg,
                          strlen(msg), HAL_MAX_DELAY);
}

/* ========================= TCP Client Management ========================= */

/**
 * @brief Gracefully close the active TCP client connection
 *
 * Clears all callbacks, closes the PCB, and resets connection state.
 */
static void tcpCloseClient(void)
{
    if (!tcpClientPcb)
        return;

    tcp_arg(tcpClientPcb, NULL);
    tcp_recv(tcpClientPcb, NULL);
    tcp_err(tcpClientPcb, NULL);
    tcp_close(tcpClientPcb);

    tcpClientPcb = NULL;
    clientConnected = false;
}

/* ========================= TCP Receive Callback ========================= */

/**
 * @brief Callback invoked when TCP data is received
 *
 * @param arg  User argument (unused)
 * @param tpcb TCP control block for the connection
 * @param p    Received packet buffer (pbuf chain)
 * @param err  LwIP error code
 *
 * @return ERR_OK on success
 */
static err_t tcpReceiveCallback(void *arg,
                                  struct tcp_pcb *tpcb,
                                  struct pbuf *p,
                                  err_t err)
{
    LWIP_UNUSED_ARG(arg);
    LWIP_UNUSED_ARG(err);

    /* NULL pbuf indicates remote side closed the connection */
    if (!p) {
        tcpCloseClient();
        return ERR_OK;
    }

    /* Inform TCP stack that data has been received and processed */
    tcp_recved(tpcb, p->tot_len);

    /* Copy received payload into a local buffer */
    char buf[TCP_SERVER_BUFFER] = {0};
    u16_t len = (p->tot_len < TCP_SERVER_BUFFER)
                ? p->tot_len
                : TCP_SERVER_BUFFER - 1;
    pbuf_copy_partial(p, buf, len, 0);

    /* Log received data via UART */
    char uartBuf[128];
    snprintf(uartBuf, sizeof(uartBuf),
             "TCP %s:%d -> %s\r\n",
             ipaddr_ntoa(&tpcb->remote_ip),
             tpcb->remote_port,
             buf);
    uartPrint(uartBuf);

    /* Send application-level ACK after receiving data */
    tcpSendAck();

    /* Free received pbuf */
    pbuf_free(p);

    return ERR_OK;
}

/* ========================= TCP Error Callback ========================= */

/**
 * @brief Callback invoked when a TCP error occurs
 *
 * Typically triggered by connection reset or abort.
 *
 * @param arg User argument (unused)
 * @param err Error code
 */
static void tcpErrorCallback(void *arg, err_t err)
{
    LWIP_UNUSED_ARG(arg);
    LWIP_UNUSED_ARG(err);

    tcpClientPcb = NULL;
    clientConnected = false;
}

/* ========================= TCP Accept Callback ========================= */

/**
 * @brief Callback invoked when a new TCP client connects
 *
 * @param arg    User argument (unused)
 * @param newpcb PCB of the newly accepted connection
 * @param err    LwIP error code
 *
 * @return ERR_OK on success
 */
static err_t tcpAcceptCallback(void *arg,
                                 struct tcp_pcb *newPcb,
                                 err_t err)
{
    LWIP_UNUSED_ARG(arg);
    LWIP_UNUSED_ARG(err);

    /* Ensure only one client is connected at a time */
    tcpCloseClient();

    tcpClientPcb = newPcb;
    clientConnected = true;

    /* Configure TCP connection parameters */
    tcp_setprio(newPcb, TCP_PRIO_NORMAL);
    tcp_recv(newPcb, tcpReceiveCallback);
    tcp_err(newPcb, tcpErrorCallback);

    /* Disable Nagle algorithm for low-latency small packets */
    tcp_nagle_disable(newPcb);

    /*
     * Do not send data here:
     * TCP may not yet be fully in ESTABLISHED state.
     * ACK will be sent on first received packet.
     */

    return ERR_OK;
}

/* ========================= TCP Server Initialization ========================= */

/**
 * @brief Initialize and start the TCP server
 *
 * Creates a listening PCB, binds it to the configured port,
 * and registers the accept callback.
 */
void tcpServerInit(void)
{
    tcpCloseClient();

    if (tcpListenPcb) {
        tcp_close(tcpListenPcb);
        tcpListenPcb = NULL;
    }

    tcpListenPcb = tcp_new();
    if (!tcpListenPcb)
        return;

    if (tcp_bind(tcpListenPcb, IP_ADDR_ANY, TCP_SERVER_PORT) != ERR_OK) {
        tcp_abort(tcpListenPcb);
        tcpListenPcb = NULL;
        return;
    }

    struct tcp_pcb *pcb =
        tcp_listen_with_backlog(tcpListenPcb, 1);
    if (!pcb) {
        tcp_abort(tcpListenPcb);
        tcpListenPcb = NULL;
        return;
    }

    tcpListenPcb = pcb;
    tcp_accept(tcpListenPcb, tcpAcceptCallback);
}

/* ========================= TCP ACK Sender ========================= */

/**
 * @brief Send application-level ACK to connected TCP client
 *
 * ACK is sent only when the TCP connection is fully established.
 */
void tcpSendAck(void)
{
    if (!tcpClientPcb || !clientConnected)
        return;

    if (tcpClientPcb->state != ESTABLISHED)
        return;

    const uint8_t ackData[] = {0xFF, 0x88, 0x00, 0xA1, 0x02};

    if (tcp_write(tcpClientPcb, ackData,
                  sizeof(ackData),
                  TCP_WRITE_FLAG_COPY) != ERR_OK)
        return;

    tcp_output(tcpClientPcb);
}

/* ========================= Connection Status ========================= */

/**
 * @brief Check whether a TCP client is currently connected
 *
 * @return true if client is connected, false otherwise
 */
bool tcpServerClientConnected(void)
{
    return clientConnected;
}
