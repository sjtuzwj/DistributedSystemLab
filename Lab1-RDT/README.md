# Lab1 GBN RDT
517021910799 朱文杰 zwjaaa@sjtu.edu.cn
## Design
____
### Protocol Part
I use checksum algorithm from internet, which regard data as stream of 16bit data, add them and reverse.
```C++
for(int i = 2; i < RDT_PKTSIZE; i += 2) 
    sum += *(short *)(&(pkt->data[i]));
while(sum >> 16) 
    sum = (sum >> 16) + (sum & 0xffff);
return ~sum;
```
____
### Receiver Part
#### Packet
| checksum | last ack packet ID | 
#### Message
Because the whole message is in the payload and the size is in the first 4 bytes, the first packet of the message will hold the size, which helps me to determine when to stop receive a message.

#### Window
I use a window buffer to hold packets whose ID 10 higher than lower bound,
``` C++
//如果收到了window内的pkt，则将其置位valid并存入window，并返回目前的ack
    if(pktID > ackUp && pktID < ackUp + windowSize) {
        if(!valid[pktID % windowSize]) {
            memcpy(&(window[pktID % windowSize].data), pkt->data, RDT_PKTSIZE);
            valid[pktID % windowSize] = true;
        }
        send(ackUp - 1);
        return;
    }
``` 
so that once the lower bound is updated, it can rapidly get the packet in the window and update the boundary quickly, instead of simply discarding them. 
```C++
        //从window中检查是否有包,有的话直接取出即可,并consume后置位invalid
        if(valid[ackUp % windowSize]){
            pkt = &window[ackUp % windowSize];
            memcpy(&pktID, pkt->data + sizeof(short), sizeof(int));
            valid[ackUp % windowSize] = false;
        }
```
____
### Sender Part
#### Packet
|  checkchecksum   |  packetID |  messageID | payload size |  payload    |
#### Message
Once the sender get message from upper layer, it will hold the message in the buffer, and if window is empty by observing the timer, it will try to fill the window and send.  
The ackUp variable will hold the upper bound of packet received, and those packet in the window (ackUp, ackUp + 10) will be sent repeadly until the ackUp is updated. If get ACK before timeout, the timer will be reset immediately and only the packets after the ACK will be sent, otherwise all packets in the window will be sent.
#### Window
An optimization is that, although the sender sends all packets in the window repeadly, the window in the receiver will hold them，the cost of GBN is majarly from the sender but not receiver。
____
## Result
```shell
At 0.00s: sender initializing ...
At 0.00s: receiver initializing ...
At 1000.44s: sender finalizing ...
At 1000.44s: receiver finalizing ...

## Simulation completed at time 1000.44s with
	989267 characters sent
	989267 characters delivered
	31940 packets passed between the sender and the receiver
## Congratulations! This session is error-free, loss-free, and in order.
```
