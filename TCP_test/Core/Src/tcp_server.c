/* --------------------------------------------------------------------------
 * File: tcp_server.c
 * Description:
 *   Simple TCP server using LwIP on STM32 (Nucleo F439ZI)
 *   - Listens on a TCP port
 *   - Accepts a single client
 *   - Receives data and prints it via UART
 *   - Can send a fixed ACK packet to the connected client
 * -------------------------------------------------------------------------- */

#include "tcp_server.h"
#include "lwip/tcp.h"
#include "lwip/pbuf.h"
#include "stm32f4xx_hal.h"
#include "lwip/ip_addr.h"
#include <string.h>
#include <stdio.h>

/* ------------------------- Configuration ------------------------- */
#define TCP_SERVER_PORT    5000    // TCP server listening port
#define TCP_SERVER_BUFFER  20      // Buffer size for received TCP data

extern UART_HandleTypeDef huart3;  // UART handle defined in main.c

/* ------------------------- TCP Control Blocks -------------------- */
/* Listening PCB (server socket) */
static struct tcp_pcb *tcp_listen_pcb = NULL;

/* Active client PCB (single client supported) */
static struct tcp_pcb *tcp_client_pcb = NULL;

/* ------------------------- UART Helper --------------------------- */
/* Blocking UART transmit function for debug/log output */
static void uart_print(const char *msg)
{
    if (msg)
        HAL_UART_Transmit(&huart3, (uint8_t *)msg, strlen(msg), 10);
}

/* --------------------------------------------------------------------------
 * TCP Receive Callback
 * Called by LwIP when data is received from the connected TCP client
 * -------------------------------------------------------------------------- */
static err_t tcp_receive_callback(void *arg,
                                  struct tcp_pcb *tpcb,
                                  struct pbuf *p,
                                  err_t err)
{
    LWIP_UNUSED_ARG(arg);
    LWIP_UNUSED_ARG(err);

    /* If p == NULL, the client has closed the connection */
    if (!p)
    {
        if (tcp_client_pcb == tpcb)
            tcp_client_pcb = NULL;

        tcp_close(tpcb);
        return ERR_OK;
    }

    /* Inform LwIP that the received data has been processed */
    tcp_recved(tpcb, p->tot_len);

    /* ---------------- Copy received TCP payload ---------------- */
    char buf[TCP_SERVER_BUFFER] = {0};
    u16_t len = (p->tot_len < (TCP_SERVER_BUFFER - 1)) ?
                 p->tot_len : (TCP_SERVER_BUFFER - 1);

    pbuf_copy_partial(p, buf, len, 0);
    buf[len] = '\0';  // Ensure null-termination for safe printing

    /* ---------------- Optional Echo ---------------- */
    /* Uncomment to echo received data back to the client */
    // tcp_write(tpcb, buf, len, TCP_WRITE_FLAG_COPY);
    // tcp_output(tpcb);

    /* ---------------- Print received data via UART ---------------- */
    char uart_buf[64];
    snprintf(uart_buf, sizeof(uart_buf),
             "TCP %s:%d -> %s\r\n",
             ipaddr_ntoa(&tpcb->remote_ip),
             tpcb->remote_port,
             buf);

    uart_print(uart_buf);

    /* Free the received pbuf */
    pbuf_free(p);

    return ERR_OK;
}

/* --------------------------------------------------------------------------
 * TCP Error Callback
 * Called by LwIP when a fatal TCP error occurs
 * -------------------------------------------------------------------------- */
static void tcp_error_callback(void *arg, err_t err)
{
    LWIP_UNUSED_ARG(arg);
    LWIP_UNUSED_ARG(err);

    /* Clear client PCB on error or disconnect */
    tcp_client_pcb = NULL;
}

/* --------------------------------------------------------------------------
 * TCP Accept Callback
 * Called when a new client connects to the TCP server
 * -------------------------------------------------------------------------- */
static err_t tcp_accept_callback(void *arg,
                                 struct tcp_pcb *newpcb,
                                 err_t err)
{
    LWIP_UNUSED_ARG(arg);
    LWIP_UNUSED_ARG(err);

    /* Store connected client PCB */
    tcp_client_pcb = newpcb;

    /* Set TCP priority */
    tcp_setprio(newpcb, TCP_PRIO_NORMAL);

    /* Register receive and error callbacks */
    tcp_recv(newpcb, tcp_receive_callback);
    tcp_err(newpcb, tcp_error_callback);

    uart_print("TCP client connected\r\n");

    return ERR_OK;
}

/* --------------------------------------------------------------------------
 * Initialize TCP Server
 * Creates a listening socket and registers accept callback
 * -------------------------------------------------------------------------- */
void tcp_server_init(void)
{
    /* Create a new TCP PCB */
    tcp_listen_pcb = tcp_new();
    if (!tcp_listen_pcb) return;

    /* Bind TCP PCB to port */
    if (tcp_bind(tcp_listen_pcb, IP_ADDR_ANY, TCP_SERVER_PORT) != ERR_OK)
    {
        tcp_abort(tcp_listen_pcb);
        tcp_listen_pcb = NULL;
        return;
    }

    /* Put PCB into listening state with backlog = 1 */
    tcp_listen_pcb = tcp_listen_with_backlog(tcp_listen_pcb, 1);
    if (!tcp_listen_pcb) return;

    /* Register accept callback */
    tcp_accept(tcp_listen_pcb, tcp_accept_callback);

    uart_print("TCP server initialized\r\n");
}

/* --------------------------------------------------------------------------
 * Send ACK to Connected Client
 * Sends a fixed raw ACK packet if a client is connected
 * -------------------------------------------------------------------------- */
void tcp_send_ack(void)
{
    if (!tcp_client_pcb) return;

    uint8_t ack_data[] = {0xFF, 0x88, 0x00, 0xA1, 0x02};

    tcp_write(tcp_client_pcb,
              ack_data,
              sizeof(ack_data),
              TCP_WRITE_FLAG_COPY);

    tcp_output(tcp_client_pcb);
}
