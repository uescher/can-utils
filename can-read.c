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


#include <signal.h>
#include <linux/socket.h>
#include <time.h>

#include <sys/timerfd.h>
#include <stdint.h>



void print_frame(struct can_frame pframe)
{
  int i;
//  printf("%08X", pframe.can_id);
//  printf("%02X", pframe.can_dlc);

  for (i = 0; i < pframe.can_dlc; i++) {
//    printf("%02X", pframe.data[7-i]);
  }
//  printf("\n");

  for (i = 0; i < 16; i++)
  {
    printf("%02X ", pframe.data[i-8]);
  }
  for (i = 4; i < 8; i++)
  {
//    printf("%02X ", pframe.data[i-8]);
  }
  printf("\n");

  fflush(stdout);


}


uint8_t fifo_snd_count = 0;
uint8_t fifo_snd_count_sent = 0;


#define FIFO_SND_BUFFER_SIZE 5

#define FIFO_SND_COUNT_MAX  12
#define FIFO_SND_COUNT_WARN  8
#define FIFO_SND_SENT_MAX    3

#define ACK_TIMEOUT         30 // 300ms

//10ms ist ein Tick

uint8_t fifo_snd_ack   = 0;
uint8_t fifo_snd_write = 0;
uint8_t fifo_snd_read  = 0;
struct can_frame fifo_snd[FIFO_SND_BUFFER_SIZE];

uint8_t can_rx_ack;
uint8_t can_counter_tx = 1;
uint8_t can_counter_rx = 1;


void calc_fifo_data(void) {
	if (fifo_snd_ack <= fifo_snd_read)
		fifo_snd_count_sent = fifo_snd_read - fifo_snd_ack;
	else
		fifo_snd_count_sent = fifo_snd_read - fifo_snd_ack + FIFO_SND_BUFFER_SIZE;

	if (fifo_snd_read <= fifo_snd_write)
		fifo_snd_count = fifo_snd_write - fifo_snd_read;
	else
		fifo_snd_count = fifo_snd_write - fifo_snd_read + FIFO_SND_BUFFER_SIZE;

	return;
}


void print_fifo_data(void) {
	calc_fifo_data();
	printf("print fifo: ack: %d[%d] read: %d write: %d ", fifo_snd_ack, *((uint8_t*)&fifo_snd[fifo_snd_ack]+6) ,fifo_snd_read, fifo_snd_write);
	printf("wait_ack: %d ",fifo_snd_count_sent);
	printf("wait_snd: %d\n",fifo_snd_count);

	return;
}



uint8_t fifo_snd_send(void) {

	uint8_t tmp = fifo_snd_read;

	uint8_t* can_frame_pointer = (uint8_t*) (&fifo_snd[tmp]);
	can_frame_pointer[5] = 1;
	can_frame_pointer[6] = can_counter_tx++;

	fifo_snd_read++;
	if (fifo_snd_read >= FIFO_SND_BUFFER_SIZE) 
		fifo_snd_read = 0; 

	print_fifo_data();

	return tmp;

}

uint8_t check_delay_ack(void) {

	if (fifo_snd_count_sent && *((uint8_t*)&fifo_snd[fifo_snd_ack]+7)==0) {
		*((uint8_t*)&fifo_snd[fifo_snd_ack]+7) = ACK_TIMEOUT;
		return 1;
	} else
		return 0;
}

void fifo_snd_frame(uint8_t tmp_id, int s, struct sockaddr_in remote) {
	struct can_frame frame_tmp;
	memcpy(&frame_tmp,&fifo_snd[tmp_id], sizeof(struct can_frame));

	uint8_t* can_frame_pointer = (uint8_t*) (&frame_tmp);
	can_frame_pointer[7] = can_counter_rx;

//		nbytes = send(udp_s, &frame, sizeof(struct can_frame),0);


	static uint8_t create_error = 0;
	create_error++;

//	if (create_error == 5)
//		printf("Packet wird nicht gesendet!(Fehlersimulation)\n");
//	else
		sendto(s,             // Socket to send result
               &frame_tmp,         // The datagram result to snd
               sizeof(struct can_frame), // The datagram lngth
               0,             // Flags: no options
               (struct sockaddr *)&remote,// addr
               sizeof(remote));     // Client address length


	printf("can>udp: ");
	print_frame(frame_tmp);

	*((uint8_t*)&fifo_snd[tmp_id]+7) = ACK_TIMEOUT;


}


void SignalHandler(void)
{
	int i;
	for (i = 0; i < FIFO_SND_BUFFER_SIZE; i++) {
		if (*((uint8_t*)&fifo_snd[i]+7))
			*((uint8_t*)&fifo_snd[i]+7) = *((uint8_t*)&fifo_snd[i]+7) - 1;
	}
}


uint8_t fifo_snd_next_ack(void) {
	uint8_t* can_frame_pointer = (uint8_t*) (&fifo_snd[fifo_snd_ack]);
	return can_frame_pointer[6];
}

uint8_t fifo_snd_check_ack(uint8_t ack) {

//	if(ack == 0)
//		ack +=FIFO_SND_BUFFER_SIZE;
	ack--;

	uint8_t tmp_goer = fifo_snd_ack;
	uint8_t tmp_goer_next = tmp_goer;	

	while( tmp_goer_next != fifo_snd_read  ) {
	
		tmp_goer = tmp_goer_next;

		if (  *((uint8_t*)&fifo_snd[tmp_goer]+6) == ack  ) {
			break;
		}

		tmp_goer_next++;
		if (tmp_goer_next >= FIFO_SND_BUFFER_SIZE) 
			tmp_goer_next = 0; 
	}

	uint8_t* can_frame_pointer = (uint8_t*) (&fifo_snd[tmp_goer]);
	if (can_frame_pointer[6] != ack) {
		printf("ACK ERROR\n");	
	} else {
		printf("ack wurde verarbeitet\n");
		fifo_snd_ack = tmp_goer + 1;
		if (fifo_snd_ack >= FIFO_SND_BUFFER_SIZE) 
			fifo_snd_ack = 0; 
	}

	print_fifo_data();

	return can_frame_pointer[6];
}


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
//    remote.sin_addr.s_addr = inet_addr("192.168.1.13");
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
	if ( bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0 ) {
	 return 1;
        }

	uint8_t error_msg[16] = {0x01,0,0,0xa0,0,0,0,0,0,0,0,0,0,0,0,0};

int8_t state = -1;


    int timerfd = timerfd_create(CLOCK_REALTIME,0);
    struct itimerspec timspec;
    bzero(&timspec, sizeof(timspec));

    timspec.it_interval.tv_sec = 0;
    timspec.it_interval.tv_nsec = 10000000;
    timspec.it_value.tv_sec = 1;
    timspec.it_value.tv_nsec = 0;


    int res = timerfd_settime(timerfd, 0, &timspec, 0);
    if(res < 0){
       perror("timerfd_settime:");
    }


while(1)
{


	struct timespec tv;
	tv.tv_sec = 0;  
	tv.tv_nsec = 10000000; //alle 10ms  
           

	FD_ZERO (&socket_set );
	FD_SET ( udp_s , &socket_set );
	if (state == 3 && (fifo_snd_count + fifo_snd_count_sent) < FIFO_SND_BUFFER_SIZE )
		FD_SET ( s     , &socket_set );
	FD_SET ( 0     , &socket_set ); //0 ist stdin

	int fd_max = 0;
	if (udp_s > fd_max) fd_max = udp_s;
	if (s > fd_max) fd_max = s;
	
	FD_SET ( timerfd , &socket_set ); //0 ist stdin
	if (timerfd > fd_max) fd_max = timerfd;

	nbytes = pselect(fd_max + 1, &socket_set, NULL, NULL, &tv, NULL);

	if (state==-1)
	{
	  printf("Reset\n");
	  uint8_t msg[16]={0,0,0,0xA0,0,0x10,0,0,0,0,0,0,0,0,0,0};
          sendto(udp_s, msg, 16,0,(struct sockaddr *)&remote,sizeof(remote));
          printf("fcnt>udp:");
          print_frame( *((struct can_frame*) msg));
          state = 0;
	}
	else if (state==0)
	{
	  printf("State0\n");
	  uint8_t msg[16]={0,0,0,0xA0,0,4,0,0,0,0,0,0,0,0,0,0};
          sendto(udp_s, msg, 16,0,(struct sockaddr *)&remote,sizeof(remote));
          printf("fcnt>udp:");
          print_frame( *((struct can_frame*) msg));
          state = 1;
	}
	else if (state==2)
	{
	  printf("State2\n");
	  uint8_t msg[16]={0,0,0,0xA0,0,2,0,1,0,0,0,0,0,0,0,0};
          sendto(udp_s, msg, 16,0,(struct sockaddr *)&remote,sizeof(remote));
          printf("fcnt>udp:");
          print_frame( *((struct can_frame*) msg));

          state = 3;
	}
	
	if (FD_ISSET(timerfd,&socket_set))
	{
		uint64_t expirations = 0;
		nbytes = read(timerfd, &expirations, sizeof(expirations));
		SignalHandler();
	}

	if (FD_ISSET(0,&socket_set))
	{
	  uint8_t msg[16]={0,0,0,0xA0,0,0x10,0,0,0,0,0,0,0,0,0,0};
          sendto(udp_s, msg, 16,0,(struct sockaddr *)&remote,sizeof(remote));
           printf("beendet\n");
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

		
		uint8_t* can_frame_pointer = (uint8_t*) (&frame);
		if (state == 1 && can_frame_pointer[5] == 6)
		{
                  can_counter_rx = can_frame_pointer[6]+1;
                  printf("State1\n");
                  state = 2;
		}

		if ( can_frame_pointer[5] & 2 && state > 2 )
			print_fifo_data();
		if ( can_frame_pointer[5] & 2 && state > 2 && fifo_snd_count_sent )
		{
			can_rx_ack = can_frame_pointer[7];
			fifo_snd_check_ack(can_rx_ack);			
			//printf("can_rx_ack ist nun %d\n",can_rx_ack);
		}

		if ( can_frame_pointer[5] & 1 )
		{
                  //printf("Erwartet: %02X  Empfangen: %02X\n",can_counter_rx, can_frame_pointer[6]);
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

		printf("udp>can: ");
		print_frame(frame);

		if (frame.can_dlc > 8)
			exit(0);
	}




	if (FD_ISSET(s,&socket_set))
	{
//		printf("Daten auf vcan verfügbar\n");
		printf("can>udp: ");

		memset(&frame,0,sizeof(struct can_frame));
		nbytes = read(s, &frame, sizeof(struct can_frame));

		if (nbytes < 0) {
			perror("can raw socket read");
			return 1;
		}

		calc_fifo_data();

		if ( (fifo_snd_count + fifo_snd_count_sent) < FIFO_SND_BUFFER_SIZE ) {

			memcpy(&fifo_snd[fifo_snd_write], &frame, sizeof(struct can_frame));
			fifo_snd_write++;

			calc_fifo_data();

			if (fifo_snd_write >= FIFO_SND_BUFFER_SIZE)
				fifo_snd_write = 0;
		                              
		
			if (fifo_snd_count >= FIFO_SND_COUNT_WARN)
				printf("\nwarn: send fifo nearly full\n\n");

			printf("fifo-write\n");
		} else {
			printf("\nwarn: send fifo is full\n");
		}

		if (fifo_snd_count && fifo_snd_count_sent >= FIFO_SND_SENT_MAX)
			print_fifo_data();

	}



//	if (fifo_snd_count && can_counter_tx == can_rx_ack)
	if (fifo_snd_count && fifo_snd_count_sent < FIFO_SND_SENT_MAX)
	{
		printf("fifo-read\n");

		uint8_t tmp_id;
		tmp_id = fifo_snd_send();

		fifo_snd_frame(tmp_id, udp_s, remote);

		fifo_snd_next_ack();

	}

	if (check_delay_ack())
		fifo_snd_frame(fifo_snd_ack, udp_s, remote);




}

	close(s);
	return 0;
}
