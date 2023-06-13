/*
  ***************************************************************************************************************
  ***************************************************************************************************************
  ***************************************************************************************************************

  File:		  udpServerRAW.c
  Author:     ControllersTech.com
  Updated:    Jul 23, 2021

  ***************************************************************************************************************
  Copyright (C) 2017 ControllersTech.com

  This is a free software under the GNU license, you can redistribute it and/or modify it under the terms
  of the GNU General Public License version 3 as published by the Free Software Foundation.
  This software library is shared with public for educational purposes, without WARRANTY and Author is not liable for any damages caused directly
  or indirectly by this software, read more about this on the GNU General Public License.

  ***************************************************************************************************************
*/


#include "lwip/pbuf.h"
#include "lwip/udp.h"
#include "lwip/tcp.h"

#include "stdio.h"
#include "udpServerRAW.h"
#include "Artnet.h"
#include "string.h"

uint8_t node_ip_address[4] = {192,168,0,111};

void udp_receive_callback(void *arg, struct udp_pcb *upcb, struct pbuf *p, const ip_addr_t *addr, u16_t port);


/* IMPLEMENTATION FOR UDP Server :   source:https://www.geeksforgeeks.org/udp-server-client-implementation-c/

1. Create UDP socket.
2. Bind the socket to server address.
3. Wait until datagram packet arrives from client.
4. Process the datagram packet and send a reply to client.
5. Go back to Step 3.
*/

void udpServer_init(void)
{
	// UDP Control Block structure
   struct udp_pcb *upcb;
   err_t err;

   /* 1. Create a new UDP control block  */
   upcb = udp_new();

   /* 2. Bind the upcb to the local port */
   ip_addr_t myIPADDR;
   IP_ADDR4(&myIPADDR, node_ip_address[0], node_ip_address[1], node_ip_address[2], node_ip_address[3]);

   err = udp_bind(upcb, &myIPADDR, 6454);  // 6454 is the default Art-Net port


   /* 3. Set a receive callback for the upcb */
   if(err == ERR_OK)
   {
	   udp_recv(upcb, udp_receive_callback, NULL);
   }
   else
   {
	   udp_remove(upcb);
   }
}

// udp_receive_callback will be called, when the client sends some data to the server
/* 4. Process the datagram packet and send a reply to client. */

void udp_receive_callback(void *arg, struct udp_pcb *upcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{


	/* Get the IP of the Client */
	char *remoteIP = ipaddr_ntoa(addr);

	uint8_t left_opcode = ((uint8_t*)p->payload)[8];
	uint8_t right_opcode = ((uint8_t*)p->payload)[9];
	uint16_t cur_opcode = (right_opcode << 8) | left_opcode;

	if( cur_opcode == 0x2000 )
	{
		struct pbuf *txBuf;
		// 0x2000 are ArtPoll packages we must reply within 3 seconds with an ArtPoll Reply
		// or else the node will be considered disconnected

		struct artnet_reply_s art_poll_reply;
		uint8_t id[8];
		sprintf((char *) id, "Art-Net\0");
		memcpy( art_poll_reply.id, id, sizeof(art_poll_reply.id) );
		memcpy( art_poll_reply.ip, node_ip_address, sizeof(art_poll_reply.ip) );
		art_poll_reply.opCode = ART_POLL_REPLY;
		art_poll_reply.port = ART_NET_PORT;

		memset(art_poll_reply.goodinput, 0x08, 4);
		memset(art_poll_reply.goodoutput, 0x80, 4);
		memset(art_poll_reply.porttypes, 0xc0, 4);

		uint8_t shortname[18];
		uint8_t longname[64];

		sprintf((char *) shortname, "Artnet Polaris 2");
		sprintf((char *) longname, "Ethernet -> WS2811 Bridge. PoE+ Enabled. Polaris 2");
		memcpy( art_poll_reply.shortname, shortname, sizeof(shortname) );
		memcpy( art_poll_reply.longname, longname, sizeof(longname) );

		art_poll_reply.etsaman[0] = 0;
		art_poll_reply.etsaman[1] = 0;
		art_poll_reply.verH       = 1;
		art_poll_reply.ver        = 0;
		art_poll_reply.subH       = 0;
		art_poll_reply.sub        = 0;
		art_poll_reply.oemH       = 0;
		art_poll_reply.oem        = 0x2828;
		art_poll_reply.ubea       = 0;
		art_poll_reply.status     = 0;
		art_poll_reply.swvideo    = 0;
		art_poll_reply.swmacro    = 0;
		art_poll_reply.swremote   = 0;
		art_poll_reply.style      = 0;

		art_poll_reply.numbportsH = 0;
		art_poll_reply.numbports  = 1;
		art_poll_reply.status2    = 0x08;

		art_poll_reply.bindip[0] = node_ip_address[0];
		art_poll_reply.bindip[1] = node_ip_address[1];
		art_poll_reply.bindip[2] = node_ip_address[2];
		art_poll_reply.bindip[3] = node_ip_address[3];

        uint8_t swin[4]  = {0x01,0x02,0x03,0x04};
        uint8_t swout[4] = {0x01,0x02,0x03,0x04};
        for(uint8_t i = 0; i < 4; i++)
        {
        	art_poll_reply.swout[i] = swout[i];
        	art_poll_reply.swin[i] = swin[i];
        }
        sprintf((char *)art_poll_reply.nodereport, "%i DMX output universes active.", art_poll_reply.numbports);

    	/* allocate pbuf from RAM*/
    	txBuf = pbuf_alloc(PBUF_TRANSPORT, sizeof(art_poll_reply), PBUF_RAM); //239 is the length of a

    	/* copy the data into the buffer  */
    	pbuf_take(txBuf, &art_poll_reply, sizeof(art_poll_reply));

    	/* Connect to the remote client */
    	ip_addr_t broadcast_addr = IPADDR4_INIT_BYTES(192,168,0,255);
    	udp_connect(upcb, &broadcast_addr, port);

    	/* Send a Reply to the Client */
    	udp_send(upcb, txBuf);

		pbuf_free(txBuf);
		new_data = 0;
	}

	if( cur_opcode == 0x5000 ) // These are DMX data packets
	{
		uint8_t left_size = ((uint8_t*)p->payload)[16];
		uint8_t right_size = ((uint8_t*)p->payload)[17];
		uint16_t cur_data_size = (left_size << 8) | right_size;

		if( cur_data_size != LED_data_size)
		{
			if( LED_data )
			{
				free(LED_data);
			}
			LED_data = malloc( cur_data_size * sizeof(uint8_t) );
			LED_data_size = cur_data_size;
		}

		for( unsigned int i = 0; i < cur_data_size; i++ )
		{
			LED_data[i] = ((uint8_t*)p->payload)[18+i];
		}
		new_data = 1;
	}

	/* allocate pbuf from RAM*/
	//txBuf = pbuf_alloc(PBUF_TRANSPORT,len, PBUF_RAM);

	/* copy the data into the buffer  */
	//pbuf_take(txBuf, buf, len);

	/* Connect to the remote client */
	//udp_connect(upcb, addr, port);

	/* Send a Reply to the Client */
	//udp_send(upcb, txBuf);

	/* free the UDP connection, so we can accept new clients */
	udp_disconnect(upcb);

	/* Free the p_tx buffer */
	//pbuf_free(txBuf);

	/* Free the p buffer */
	pbuf_free(p);

}

