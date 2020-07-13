/*
 * FILE: rdt_sender.cc
 * DESCRIPTION: Reliable data transfer sender.
 * NOTE: This implementation aschecksumes there is no packet loss, corruption, or 
 *       reordering.  You will need to enhance it to deal with all these 
 *       situations.  In this implementation, the packet format is laid out as 
 *       the following:
 *       
 *   |<- 2 byte -> |<- 4 byte  ->|<-  1 byte  ->|<-  1 byte  ->|<-  The rest   ->|
 *   |  checkchecksum   |  packetID |  messageID | payload size |  payload    |
 *
 *   |<- 4 byte  ->|<-  The rest   ->|
 *  | msg size     |    msg data    | 
 *       The first byte of each packet indicates the size of the payload
 *       (excluding this single-byte header)
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <array>

#include "rdt_struct.h"
#include "rdt_sender.h"
#include "rdt_protocol.h"

using std::array;
//Design Argument
constexpr size_t bufferSize = 10000;
constexpr size_t windowSize = 10;
constexpr double timeout = 0.3;
constexpr size_t headerSize = 7;
constexpr char payloadSize = RDT_PKTSIZE - headerSize;

static array<message,bufferSize> buffer;
static int msgID = 0;
static int msgNum = 0;
//use index module to implement loop 
static int msgIndex = 0;
static int msgOff = 0;

static array<packet,windowSize> window;
//pkt generate totally
static int pktID = 0;
//pkt need to send in the future
static int pktNum = 0;
//pkt need to send now
static int pktSendID = 0;
//pkt that has been ACKed
static int ackUp = 0;

static void send() {
    packet pkt;
    while(pktSendID < pktID) {
        memcpy(&pkt, &(window[pktSendID % windowSize]), sizeof(packet));
        Sender_ToLowerLayer(&pkt);
        pktSendID++;
    }
}
//fill window and send packet
//将buffer中的msg装入window
static void fillWindow() {
    message msg = buffer[msgIndex % bufferSize];
    packet pkt;
    char curPayloadSize = 0;
    while (pktNum < windowSize && msgIndex < msgID) {
        if(msg.size > msgOff + payloadSize)
            curPayloadSize = payloadSize;
        else if (msg.size > msgOff) 
            curPayloadSize = msg.size - msgOff;
        else return;
        //fill packet
        memcpy(pkt.data + sizeof(short), &pktID, sizeof(int));
        memcpy(pkt.data + sizeof(short) + sizeof(int), &curPayloadSize, sizeof(char));
        memcpy(pkt.data + headerSize, msg.data + msgOff, curPayloadSize);
        short checksum = CheckSum(&pkt);
        memcpy(pkt.data, &checksum, sizeof(short));
        //fill window with packet
        memcpy(&(window[pktID % windowSize]), &pkt, sizeof(packet));
        if (msg.size > msgOff + payloadSize)
            msgOff += payloadSize;
        else if (msg.size > msgOff) {
            //fetch next msg
            msgNum--;
            msgIndex++;
            if(msgIndex < msgID){
                msg = buffer[msgIndex % bufferSize];
            }
            msgOff = 0;
        }
        pktID++;
        pktNum++;
    }
    send();
}


/* sender initialization, called once at the very beginning */
void Sender_Init()
{
    fprintf(stdout, "At %.2fs: sender initializing ...\n", GetSimulationTime());
}

/* sender finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to take this opportunity to release some
   memory you allocated in Sender_init(). */
void Sender_Final()
{
    for(int i = 0; i < bufferSize; i++) {
        if(buffer[i].size) delete[] buffer[i].data;
    }
    fprintf(stdout, "At %.2fs: sender finalizing ...\n", GetSimulationTime());
}

/* event handler, called when a message is passed from the upper layer at the
   sender */
//将msg装入buffer
void Sender_FromUpperLayer(struct message *msg)
{
    //cleanup buffer
    if(buffer[msgID % bufferSize].size) delete[] buffer[msgID % bufferSize].data;

    //fill size and data
    buffer[msgID % bufferSize].size = msg->size + sizeof(int);
    buffer[msgID % bufferSize].data = new char[buffer[msgID % bufferSize].size];
    memcpy(buffer[msgID % bufferSize].data, &msg->size, sizeof(int));
    memcpy(buffer[msgID % bufferSize].data + sizeof(int), msg->data, msg->size);
    msgID++;
    msgNum++;

    //no timer, the window is empty
    if(Sender_isTimerSet()) return;
    Sender_StartTimer(timeout);
    fillWindow();
}

/* event handler, called when a packet is passed from the lower layer at the
   sender */
//更新ACK
void Sender_FromLowerLayer(struct packet *pkt)
{
    short checksum;
    memcpy(&checksum, pkt->data, sizeof(short));
    if(checksum != CheckSum(pkt)) return;

    int ack;
    memcpy(&ack, pkt->data + sizeof(short), sizeof(int));

    if(ackUp <= ack && ack < pktID){
        Sender_StartTimer(timeout);
        //update pkt num
        pktNum -= (ack - ackUp + 1);
        //update up bound
        ackUp = ack + 1;
        fillWindow();
    }
    //no packet now
    if(ack == pktID - 1) Sender_StopTimer();
}

/* event handler, called when the timer expires */
void Sender_Timeout()
{
    Sender_StartTimer(timeout);
    pktSendID = ackUp;
    fillWindow();
}