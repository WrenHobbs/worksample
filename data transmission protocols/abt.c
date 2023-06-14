#include "../include/simulator.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* ******************************************************************
 ALTERNATING BIT AND GO-BACK-N NETWORK EMULATOR: VERSION 1.1  J.F.Kurose

   This code should be used for PA2, unidirectional data transfer 
   protocols (from A to B). Network properties:
   - one way network delay averages five time units (longer if there
     are other messages in the channel for GBN), but can be larger
   - packets can be corrupted (either the header or the data portion)
     or lost, according to user-defined probabilities
   - packets will be delivered in the order in which they were sent
     (although some can be lost).
**********************************************************************/

struct queue_elem {
    struct pkt *pkt;
    struct queue_elem *next;
};

struct pktqueue {
    struct queue_elem *front;
    struct queue_elem *back;
};

struct pktqueue *buffer;
struct pkt *a_currentpkt;
int a_sendnum;
int b_pktnum;

// Add packet to queue
void queue(struct pktqueue *queue, struct pkt* packet) {
    struct queue_elem *new_elem = (struct queue_elem *) malloc(sizeof(struct queue_elem));
    new_elem->next = NULL;
    new_elem->pkt = packet;
    if (queue->back == NULL) {
        if (queue->front == NULL) {
            queue->front = new_elem;
            return;
        }
        queue->front->next = new_elem;
    }
    else queue->back->next = new_elem;
    queue->back = new_elem;
}

// Get packet from front of queue
struct pkt *dequeue(struct pktqueue *queue) {
    if (queue->front == NULL) return NULL;
    struct queue_elem *output_elem = queue->front;
    queue->front = queue->front->next;
    if (queue->front == queue->back) queue->back = NULL;
    struct pkt *output = output_elem->pkt;
    free(output_elem);
    return output;
}

/********* STUDENTS WRITE THE NEXT SIX ROUTINES *********/

/* called from layer 5, passed the data to be sent to other side */
void A_output(message)
  struct msg message;
{
  // Create packet
  struct pkt *newpkt = (struct pkt *) malloc(sizeof(struct pkt));
  newpkt->acknum = 0;
  newpkt->seqnum = a_sendnum;
  newpkt->checksum = a_sendnum;
  for (int i = 0;i<20;i++) {
    newpkt->checksum += message.data[i];
    newpkt->payload[i] = message.data[i];
  }
  // If packet not ready to be sent, queue it:
  if (a_currentpkt != NULL) queue(buffer, newpkt);
  // Otherwise, send it off:
  else {
    a_currentpkt = newpkt;
    tolayer3(0, *newpkt);
    starttimer(0, 20);
  }
}

/* called from layer 3, when a packet arrives for layer 4 */
void A_input(packet)
  struct pkt packet;
{
  // Validate checksum
  int check = packet.acknum;
  for (int i=0;i<20;i++) check += packet.payload[i];
  // If checksum is valid:
  if (check == packet.checksum) {
    // if ack applies to current sent packet:
    if (packet.acknum == a_sendnum) {
      stoptimer(0);
      a_sendnum++;
      struct pkt *newpkt = dequeue(buffer);
      free(a_currentpkt);
      a_currentpkt = newpkt;
      if (newpkt != NULL) {
        tolayer3(0, *newpkt);
        starttimer(0, 20);
      }
    }
  }
}

/* called when A's timer goes off */
void A_timerinterrupt()
{
  tolayer3(0, *a_currentpkt);
  starttimer(0, 20);
}  

/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
void A_init()
{ 
  buffer = (struct pktqueue *) malloc(sizeof(struct pktqueue));
  buffer->back = NULL;
  buffer->front = NULL;
  a_currentpkt = NULL;
  a_sendnum = 0;
}

/* Note that with simplex transfer from a-to-B, there is no B_output() */

/* called from layer 3, when a packet arrives for layer 4 at B*/
void B_input(packet)
  struct pkt packet;
{
  // Validate checksum
  int check = packet.seqnum + packet.acknum;
  for (int i=0;i<20;i++) check += packet.payload[i];
  // If checksum is valid:
  if (check == packet.checksum) {
    if (packet.seqnum <= b_pktnum) {
      //create ack message
      struct pkt *ack = (struct pkt *) malloc(sizeof(struct pkt));
      ack->acknum = packet.seqnum;
      ack->seqnum = 0;
      ack->checksum = packet.seqnum;
      for (int i=0;i<20;i++) {
        ack->checksum += packet.payload[i];
        ack->payload[i] = packet.payload[i];
      }
      // send ack message back
      tolayer3(1, *ack);
      free(ack);
      // If next data, deliver to upper layer
      if (packet.seqnum == b_pktnum) {
        tolayer5(1, packet.payload);
        b_pktnum++;
      }
    }
  }
}

/* the following routine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
void B_init()
{
  b_pktnum = 0;
}
