#include "udp_server.h"
#include "lwip/udp.h"
#include "lwip/pbuf.h"
#include "lwip/ip_addr.h"
#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdio.h>

/* ------------------------- Configuration ------------------------- */
#define UDP_SERVER_PORT    5005       // UDP server listening port
#define UDP_SERVER_BUFFER  20         // Buffer size for incoming UDP data

extern UART_HandleTypeDef huart3;    // UART handle defined in main.c

/* ------------------------- UDP Server State ---------------------- */
struct udp_pcb *udp_pcb = NULL;      // UDP Protocol Control Block
static ip_addr_t last_client_addr;   // Store IP of last client for ACK
static u16_t last_client_port = 0;   // Store port of last client for ACK
static uint8_t client_known = 0;     // Flag: true if at least one client has sent data

/* ------------------------- UART Print Function ------------------ */
/* Blocking UART print. Transmits a null-terminated string over USART3. */
static void uart_print(const char *msg)
{
    if (msg)
        HAL_UART_Transmit(&huart3, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
}

/* ------------------------- UDP Receive Callback ----------------- */
/* This function is called by LWIP when a UDP packet arrives. */
static void udp_receive_callback(void *arg,
                                 struct udp_pcb *upcb,
                                 struct pbuf *p,
                                 const ip_addr_t *addr,
                                 u16_t port)
{
    if (!p || p->tot_len == 0) return; // No data received

    /* ---------------- Copy UDP Payload ---------------- */
    char buf[UDP_SERVER_BUFFER] = {0};
    u16_t len = (p->tot_len < (UDP_SERVER_BUFFER - 1)) ? p->tot_len : (UDP_SERVER_BUFFER - 1);
    pbuf_copy_partial(p, buf, len, 0);  // Copy data from pbuf
    buf[len] = '\0';                     // Ensure null-termination for safe printing

    /* ---------------- Print UDP Packet via UART ---------------- */
    char uart_buf[128];
    snprintf(uart_buf, sizeof(uart_buf), "UDP %s:%d -> %s\r\n",
             ipaddr_ntoa(addr), port, buf); // Format: IP:port -> message
    uart_print(uart_buf);

    /* ---------------- Store Last Client Info ---------------- */
    ip_addr_copy(last_client_addr, *addr);
    last_client_port = port;
    client_known = 1; // Mark that we now know a client

    /* ---------------- Optional Echo ---------------- */
    // If you want to echo the message back to sender, uncomment:
    // udp_sendto(upcb, p, addr, port);

    /* Free the pbuf after processing */
    pbuf_free(p);
}

/* ------------------------- Initialize UDP Server ------------------ */
/* Creates a UDP PCB, binds to any IP and the configured port, and
   sets the receive callback. */
void udp_server_init(void)
{
    udp_pcb = udp_new();                  // Allocate new UDP PCB
    if (!udp_pcb) {
        return;                           // Allocation failed
    }

    if (udp_bind(udp_pcb, IP_ADDR_ANY, UDP_SERVER_PORT) != ERR_OK) {
        udp_remove(udp_pcb);             // Cleanup if bind failed
        udp_pcb = NULL;
        return;
    }

    udp_recv(udp_pcb, udp_receive_callback, NULL); // Set receive callback
}

/* ------------------------- Send ACK to Last Client --------------- */
/* Sends a fixed raw byte ACK to the last known client, if any. */
void udp_send_ack(void)
{
    if (!client_known || !udp_pcb) return; // No client to send to

    uint8_t ack_data[] = {0xFF, 0x88, 0x00, 0xA1, 0x02}; // ACK payload
    const uint16_t ack_len = sizeof(ack_data);

    /* Allocate a pbuf for transport layer */
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, ack_len, PBUF_RAM);
    if (!p) return; // Allocation failed

    /* Copy the raw ACK bytes into pbuf */
    pbuf_take(p, ack_data, ack_len);

    /* Send the pbuf to last known client */
    udp_sendto(udp_pcb, p, &last_client_addr, last_client_port);

    /* Free the pbuf after sending */
    pbuf_free(p);
}
