/*
 * FILE: rdt_receiver.cc
 * DESCRIPTION: Reliable data transfer receiver.
 * NOTE: This implementation aschecksumes there is no packet loss, corruption, or 
 *       reordering.  You will need to enhance it to deal with all these 
 *       situations.  In this implementation, the packet format is laid out as 
 *       the following:
 *       
 *       |<-  2 byte -->|<-  4 byte  ->|<-             the rest            ->|
 *       |   check sum  |     ack      |<-             none             ->|
 *
 *       The first byte of each packet indicates the size of the payload
 *       (excluding this single-byte header)
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <array>
#include "rdt_struct.h"
#include "rdt_receiver.h"
#include "rdt_protocol.h"

using std::array;
constexpr size_t windowSize = 10;
constexpr size_t headerSize = 7;
constexpr char payloadSize = RDT_PKTSIZE - headerSize;

static message* msg = new message;
static int msgOff = 0;
static int ackUp = 0;

static array<packet, windowSize> window;
static array<bool, windowSize> valid;

//fill packet
static void send(int ack) {
    packet pkt;
    memcpy(pkt.data + sizeof(short), &ack, sizeof(int));
    short checksum = CheckSum(&pkt);
    memcpy(pkt.data, &checksum, sizeof(short));
    Receiver_ToLowerLayer(&pkt);
}

/* receiver initialization, called once at the very beginning */
void Receiver_Init()
{
    fprintf(stdout, "At %.2fs: receiver initializing ...\n", GetSimulationTime());
}

/* receiver finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to use this opportunity to release some
   memory you allocated in Receiver_init(). */
void Receiver_Final()
{
    delete msg;
    fprintf(stdout, "At %.2fs: receiver finalizing ...\n", GetSimulationTime());
}

/* event handler, called when a packet is passed from the lower layer at the
   receiver */
void Receiver_FromLowerLayer(struct packet *pkt)
{
    //校验失败直接丢弃
    short checksum;
    memcpy(&checksum, pkt->data, sizeof(short));
    if(checksum != CheckSum(pkt)) return;


    int pktID = 0, payloadSize = 0;
    memcpy(&pktID, pkt->data + sizeof(short), sizeof(int));
    //如果收到了window内的pkt，则将其置位valid并存入window，并且返回目前的ack
    if(pktID > ackUp && pktID < ackUp + windowSize) {
        if(!valid[pktID % windowSize]) {
            memcpy(&(window[pktID % windowSize].data), pkt->data, RDT_PKTSIZE);
            valid[pktID % windowSize] = true;
        }
        send(ackUp - 1);
        return;
    }
    //如果是以前的pkt，返回目前的ack
    else if(pktID != ackUp) {
        send(ackUp - 1);
        return;
    }
    //否则说明是目前window的下界，更新window
    while(true){
        ackUp++;
        payloadSize = pkt->data[headerSize - 1];
        if(!msgOff){
            if(msg->size != 0) delete[] msg->data;
            memcpy(&msg->size, pkt->data + headerSize, sizeof(int));
            msg->data = new char[msg->size];
            memcpy(msg->data, pkt->data + headerSize + sizeof(int), payloadSize - sizeof(int));
            msgOff += payloadSize - sizeof(int);
        } else {
            memcpy(msg->data + msgOff, pkt->data + headerSize, payloadSize);
            msgOff += payloadSize;
        }

        if(msg->size == msgOff) {
            Receiver_ToUpperLayer(msg);
            msgOff = 0;
        }
        //从window中检查是否有包,有的话直接取出即可,并consume后置位invalid
        if(valid[ackUp % windowSize]){
            pkt = &window[ackUp % windowSize];
            memcpy(&pktID, pkt->data + sizeof(short), sizeof(int));
            valid[ackUp % windowSize] = false;
        }
        else break;
    };
    //更新window之后，上界作为新的ack
    send(pktID);
}