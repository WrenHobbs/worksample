#include "../include/simulator.h"
#include <stdio.h>
#include <stdlib.h>

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
    int status;
    float time_sent;
    struct queue_elem *next;
};

struct pktqueue {
    struct queue_elem *front;
    struct queue_elem *back;
    int size;
};

void queue(struct pktqueue *queue, struct pkt* packet) {
  struct queue_elem *new_elem = (struct queue_elem *) malloc(sizeof(struct queue_elem));
  new_elem->next = NULL;
  new_elem->pkt = packet;
  new_elem->status = 0;
  new_elem->time_sent = get_sim_time();
  queue->size++;
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
    queue->size--;
    return output;
}

// Given a current timer element, get the next element that needs timing
struct queue_elem *next_timer(struct pktqueue *queue, struct queue_elem *curr) {
  if (queue->size == 0) return NULL;
  struct queue_elem *iter = curr->next;
  if (iter == NULL) return curr;
  while (iter != curr) {
    if (iter->status == 0) return iter;
    if (iter->next == NULL) iter = queue->front;
    else iter = iter->next;
  }
  return curr;
}

struct pktqueue *a_queue;
struct pktqueue *window;
struct queue_elem *current_timer;
int next_seqnum;
struct pkt **b_buffer;
int b_acks;
int b_nextstore;
int winsize;

#define BUFFERSIZE 1000
#define RTT 15



/********* STUDENTS WRITE THE NEXT SEVEN ROUTINES *********/

/* called from layer 5, passed the data to be sent to other side */
void A_output(message)
  struct msg message;
{
  printf("A got message: %.20s\n", message.data);
  fflush(NULL);
  // Create packet for message
  struct pkt *new_pkt = (struct pkt *) malloc(sizeof(struct pkt));
  new_pkt->seqnum = next_seqnum;
  next_seqnum++;
  new_pkt->acknum = 0;
  new_pkt->checksum = new_pkt->seqnum;
  for (int i=0;i<20;i++) {
    new_pkt->checksum += message.data[i];
    new_pkt->payload[i] = message.data[i];
  }
  // If window isn't full, add this packet and send
  if (window->size < winsize) {
    tolayer3(0, *new_pkt);
    queue(window, new_pkt);
    if (current_timer == NULL) {
      starttimer(0, RTT + (5*winsize));
      current_timer = window->front;
      }
  }
  // Else, add to queue
  else queue(a_queue, new_pkt);
}

/* called from layer 3, when a packet arrives for layer 4 */
void A_input(packet)
  struct pkt packet;
{
  printf("A got ack back\n");
  fflush(NULL);
  //Validate checksum
  int check = packet.acknum + packet.seqnum;
  for (int i=0;i<20;i++) check += packet.payload[i];
  if (check == packet.checksum) {
    printf("\tAck number %d, message: %.20s\n", packet.acknum, packet.payload);
    fflush(NULL);
    // Find window element associated with this ack
    struct queue_elem *window_elem = window->front;
    while (window_elem != NULL && window_elem->pkt->seqnum != packet.seqnum) {
      window_elem = window_elem->next;
    }
    if (window_elem == NULL) return;
    printf("\tWindow element: %.20s\n", window_elem->pkt->payload);
    fflush(NULL);
    // Update window element status
    window_elem->status = 1;

    // While window front is a received packet:
    while (window->size != 0 && window->front->status == 1) {
      printf("\tWindow front:\n\t\tMessage: %.20s\n\t\tStatus: %d\n", window->front->pkt->payload, window->front->status);
      fflush(NULL);
      // remove front from window
      struct pkt *del = dequeue(window);
      free(del);
      // send next packet and add to window
      printf("\ta_queue size: %d, window size: %d\n", a_queue->size, window->size);
      fflush(NULL);
      if (a_queue->size != 0) {
        struct pkt *new_pkt = dequeue(a_queue);
        tolayer3(0, *new_pkt);
        queue(window, new_pkt);
        if (current_timer == NULL) {
          starttimer(0, RTT + (5*winsize));
          current_timer = window->front;
        }
      }
    }

    // If this window element is the current timed packet:
    if (current_timer == NULL) current_timer = window->front;
    if (current_timer == window_elem) {
      printf("\tResetting current timer\n");
      fflush(NULL);
      // Stop timer
      stoptimer(0);
      // Get next packet to time
      current_timer = next_timer(window, current_timer);
      // Start timer for this packet
      if (current_timer != NULL && current_timer->status == 0) starttimer(0, (RTT+(5*winsize))-(get_sim_time()-current_timer->time_sent));
    }
  }
}

/* called when A's timer goes off */
void A_timerinterrupt()
{
  printf("Timer interrupt for packet %d\n", current_timer->pkt->seqnum);
  fflush(NULL);
  // Resend this packet
  tolayer3(0, *(current_timer->pkt));
  // Reset time sent for packet
  current_timer->time_sent = get_sim_time();
  // Get next packet to time
  current_timer = next_timer(window, current_timer);
  // Start timer for this packet
  if (current_timer != NULL) {
    starttimer(0, (RTT  +(5*winsize))-(get_sim_time()-current_timer->time_sent));
    printf("\tNew timer for packet %d\n", current_timer->pkt->seqnum);
    fflush(NULL);
    }
}  

/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
void A_init()
{
  a_queue = (struct pktqueue *) malloc(sizeof(struct pktqueue));
  window = (struct pktqueue *) malloc(sizeof(struct pktqueue));
  winsize = getwinsize();
  next_seqnum = 0;
  current_timer = NULL;
}

/* Note that with simplex transfer from a-to-B, there is no B_output() */

/* called from layer 3, when a packet arrives for layer 4 at B*/
void B_input(packet)
  struct pkt packet;
{
  //Validate checksum
  int check = packet.acknum + packet.seqnum;
  for (int i=0;i<20;i++) check += packet.payload[i];
  if (check == packet.checksum) {
    printf("B received packet:\n\tSeqnum: %d\n\tPayload: %.20s\n", packet.seqnum, packet.payload);
    fflush(NULL);
    // Create ack message
    struct pkt *new_ack = (struct pkt *) malloc(sizeof(struct pkt));
    new_ack->acknum = b_acks;
    new_ack->seqnum = packet.seqnum;
    new_ack->checksum = b_acks + packet.seqnum;
    b_acks++;
    for (int i=0;i<20;i++) {
      new_ack->checksum += packet.payload[i];
      new_ack->payload[i] = packet.payload[i];
    }
    // Set new ack as package in b buffer
    if (b_buffer[packet.seqnum] != NULL) free(b_buffer[packet.seqnum]);
    b_buffer[packet.seqnum] = new_ack;
    // Send ack back to A
    tolayer3(1, *new_ack);
    // Store all possible messages
    while (b_buffer[b_nextstore] != NULL) {
      printf("\tSending up: %.20s\n", b_buffer[b_nextstore]->payload);
      tolayer5(1, b_buffer[b_nextstore]->payload);
      // free(b_buffer[b_nextstore]);
      b_nextstore++;
    }
  }
}

/* the following rouytine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
void B_init()
{
  b_buffer = calloc(BUFFERSIZE, sizeof(struct pkt *));
  b_acks = 0;
  b_nextstore = 0;
}
