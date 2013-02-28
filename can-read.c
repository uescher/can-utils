#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>


void print_frame(struct can_frame pframe)
{
  int i;
  printf("%08X", pframe.can_id);
  printf("%02X", pframe.can_dlc);

  for (i = 0; i < pframe.can_dlc; i++) {
    printf("%02X", pframe.data[7-i]);
  }
  printf("\n");

  for (i = 0; i < 16; i++)
  {
    printf("%02X ", pframe.data[i-8]);
  }
  printf("\n");

  fflush(stdout);


}


uint8_t fifo_snd_count = 0;
uint8_t fifo_snd_write = 0;
uint8_t fifo_snd_read = 0;
struct can_frame fifo_snd[16];

uint8_t can_rx_ack;
uint8_t can_counter_tx = 1;



int main(int argc, char **argv)
{
	printf("Program gestartet\n");
	struct can_frame frame;
	int nbytes;

	fd_set socket_set;


	int udp_s = 0;
//	socklen_t remote_len;
//	char message[255];
	struct sockaddr_in local, remote ;

	udp_s = socket(PF_INET,SOCK_DGRAM,0);
	local.sin_family = AF_INET;
	local.sin_port = htons( 4223 );
//	local.sin_addr.s_addr = inet_addr("192.168.2.100"); //htonl( INADDR_ANY );
	local.sin_addr.s_addr = htonl( INADDR_ANY );
	bind(udp_s, ( struct sockaddr *) &local, sizeof(local));



	memset(&remote,0,sizeof remote);
	remote.sin_family = AF_INET;
    remote.sin_port = htons(4223);
//    remote.sin_addr.s_addr = inet_addr("192.168.2.101");
    remote.sin_addr.s_addr = inet_addr("10.42.23.99");
//    remote.sin_addr.s_addr = inet_addr("192.168.1.4");
//    remote.sin_addr.s_addr = inet_addr("0.0.0.0");




	int s; /* can raw socket */
	struct sockaddr_can addr;
	struct ifreq ifr;
	frame.can_id = 0x654321;
	frame.can_id |= CAN_EFF_FLAG;
	frame.can_dlc = 5;
	if ((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) { return 1; }
	addr.can_family = AF_CAN;
	strcpy(ifr.ifr_name, "vcan0");
	if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) { return 1; }
	addr.can_ifindex = ifr.ifr_ifindex;

	struct can_filter rfilter[1];

	rfilter[0].can_id   = 0;
	rfilter[0].can_mask = 0;

	setsockopt(s, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, 1);
	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) { return 1; }
	//if ((nbytes = write(s, &frame, sizeof(frame))) != sizeof(frame)) { return 1; }


	printf("jetzt wird gelesen\n");

	uint8_t error_msg[16];
	error_msg[0] = 0x01;
	error_msg[1] = 0x00;
	error_msg[2] = 0x00;
	error_msg[3] = 0xA0;
	error_msg[4] = 0x00;
	error_msg[5] = 0x00;
	error_msg[6] = 0x00;
	error_msg[7] = 0x00;
	error_msg[8] = 0x00;
	error_msg[9] = 0x00;
	error_msg[10] = 0x00;
	error_msg[11] = 0x00;
	error_msg[12] = 0x00;
	error_msg[13] = 0x00;
	error_msg[14] = 0x00;
	error_msg[15] = 0x00;


int8_t state = -1;

while(1)
{

	struct timeval tv;
	tv.tv_sec = 0;  
	tv.tv_usec = 100000; //alle 100ms  
           

	FD_ZERO (&socket_set );
	FD_SET ( udp_s , &socket_set );
	FD_SET ( s     , &socket_set );
	FD_SET ( 0     , &socket_set ); //0 ist stdin

	int fd_max = 0;
	if (udp_s > fd_max) fd_max = udp_s;
	if (s > fd_max) fd_max = s;

//	printf("Jetzt wird sleep(5) aufgerufen\n");
//	sleep(5);
//	printf("Jetzt wird select() aufgerufen\n");
	nbytes = select(fd_max + 1, &socket_set, NULL, NULL, &tv);
//	printf("Es wurde select() aufgerufen. Rückgabewert war %i \n", nbytes);

	
	if (state==-1)
	{
	  printf("Reset\n");
	  uint8_t msg[16]={0,0,0,0xA0,0,0x10,0,0,0,0,0,0,0,0,0,0};
          sendto(udp_s, msg, 16,0,(struct sockaddr *)&remote,sizeof(remote));
          printf("fcnt: ");
          print_frame( *((struct can_frame*) msg));
          state = 0;
	}
	else if (state==0)
	{
	  printf("State0\n");
	  uint8_t msg[16]={0,0,0,0xA0,0,4,0,0,0,0,0,0,0,0,0,0};
          sendto(udp_s, msg, 16,0,(struct sockaddr *)&remote,sizeof(remote));
          printf("fcnt: ");
          print_frame( *((struct can_frame*) msg));
          state = 1;
	}
	else if (state==2)
	{
	  printf("State2\n");
	  uint8_t msg[16]={0,0,0,0xA0,0,2,0,1,0,0,0,0,0,0,0,0};
          sendto(udp_s, msg, 16,0,(struct sockaddr *)&remote,sizeof(remote));
          printf("fcnt: ");
          print_frame( *((struct can_frame*) msg));
          state = 3;
	}
	
	if (FD_ISSET(0,&socket_set))
	{
	  uint8_t msg[16]={0,0,0,0xA0,0,0x10,0,0,0,0,0,0,0,0,0,0};
          sendto(udp_s, msg, 16,0,(struct sockaddr *)&remote,sizeof(remote));
           printf("beendet");
           exit(0);
	}
	
	//Daten von UDP => CAN
	if (FD_ISSET(udp_s,&socket_set))
	{
//		printf("Daten auf udp verfügbar\n");
		memset(&frame,0,sizeof(struct can_frame));
		nbytes = read(udp_s, &frame, sizeof(struct can_frame));

		if (nbytes < sizeof(struct can_frame)) {
			printf("%i < %i\n", nbytes, sizeof(struct can_frame) );
			//return 1;
		}


		if (nbytes < 0) {
			perror("can raw socket read");
			return 1;
		}

		
		uint8_t static can_counter_rx = 1;
		uint8_t* can_frame_pointer = (uint8_t*) (&frame);
		if (state == 1 && can_frame_pointer[5] == 6)
		{
                  can_counter_rx = can_frame_pointer[6]+1;
                  printf("State1\n");
                  state = 2;
		}

		if (can_frame_pointer[5] & 2 == 2 )
		{
                  can_rx_ack = can_frame_pointer[7];
		}

		if (can_frame_pointer[5] & 1 == 1)
		{
                  //printf("Erwartet: %02X  Empfangen: %02X",can_counter_rx, can_frame_pointer[6]);
                  if (can_counter_rx != can_frame_pointer[6])
                  {
                    printf("\nERROR\n");
                    printf("Erwartet: %02X  Empfangen: %02X",can_counter_rx, can_frame_pointer[6]);
                    printf("\nERROR\n");
                    send(s, error_msg, sizeof(struct can_frame),0);
                  }
                  can_counter_rx = can_frame_pointer[6]+1;

		  nbytes = send(s, &frame, sizeof(struct can_frame),0);
		}
		uint8_t counter = can_frame_pointer[6];

                printf("udp: ");
                print_frame(frame);

                if (frame.can_dlc > 8)
		  exit(0);
	}




	if (FD_ISSET(s,&socket_set))
	{
//		printf("Daten auf vcan verfügbar\n");
                printf("can: ");

		memset(&frame,0,sizeof(struct can_frame));
		nbytes = read(s, &frame, sizeof(struct can_frame));

		if (nbytes < 0) {
			perror("can raw socket read");
			return 1;
		}

		memcpy(&fifo_snd[fifo_snd_write], &frame, sizeof(struct can_frame));
		fifo_snd_write++;
		fifo_snd_count++;

		if (fifo_snd_write > 16)
		  fifo_snd_write = 0;
                                  
		
		if (fifo_snd_count > 10)
                  printf("\nsend fifo full\n\n");

                printf("fifo-write\n");
        }

        if (fifo_snd_count && can_counter_tx == can_rx_ack)
        {
                printf("fifo-read");
		fifo_snd_count--;
		memcpy(&frame,&fifo_snd[fifo_snd_read], sizeof(struct can_frame));
		fifo_snd_read++;
                if (fifo_snd_read > 16) 
                  fifo_snd_read = 0; 

		uint8_t* can_frame_pointer = (uint8_t*) (&frame);
		can_frame_pointer[5] = 1;
		can_frame_pointer[6] = can_counter_tx++;
//		nbytes = send(udp_s, &frame, sizeof(struct can_frame),0);

		nbytes = sendto(udp_s,             // Socket to send result
                   &frame,         // The datagram result to snd
                   sizeof(struct can_frame), // The datagram lngth
                   0,             // Flags: no options
                   (struct sockaddr *)&remote,// addr
                   sizeof(remote));     // Client address length


                   printf("can: ");
                   print_frame(frame);

	}
}

	close(s);
	return 0;
}
