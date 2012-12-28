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


while(1)
{
	int i = 0;


	FD_ZERO (&socket_set );
	FD_SET ( udp_s , &socket_set );
	FD_SET ( s     , &socket_set );

	int fd_max = 0;
	if (udp_s > fd_max) fd_max = udp_s;
	if (s > fd_max) fd_max = s;

//	printf("Jetzt wird sleep(5) aufgerufen\n");
//	sleep(5);
//	printf("Jetzt wird select() aufgerufen\n");
	nbytes = select(fd_max + 1, &socket_set, NULL, NULL, NULL);
//	printf("Es wurde select() aufgerufen. Rückgabewert war %i \n", nbytes);

	if (FD_ISSET(udp_s,&socket_set))
	{
//		printf("Daten auf udp verfügbar\n");
		printf("udp: ");
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


		uint8_t static can_counter_rx = 0;
		uint8_t* can_frame_pointer = (uint8_t*) (&frame);
		if (can_frame_pointer[5] == 1)
		{
                  //printf("Erwartet: %02X  Empfangen: %02X",can_counter_rx, can_frame_pointer[6]);
                  if (can_counter_rx != can_frame_pointer[6])
                  {
                    printf("\nERROR\n\nERROR\n\n");
                    send(s, error_msg, sizeof(struct can_frame),0);
                  }
                  can_counter_rx = can_frame_pointer[6]+1;
		}
		uint8_t counter = can_frame_pointer[6];
		nbytes = send(s, &frame, sizeof(struct can_frame),0);

		printf("%08X", frame.can_id);
		printf("%02X", frame.can_dlc);

		for (i = 0; i < frame.can_dlc; i++) {
		    printf("%02X", frame.data[7-i]);
		}
		printf("\n");

		for (i = 0; i < 16; i++)
		{
			printf("%02X ", frame.data[i-8]);
		}
		printf("\n");

		fflush(stdout);

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

		uint8_t* can_frame_pointer = (uint8_t*) (&frame);
		static uint8_t can_counter_tx = 0;
		can_frame_pointer[5] = 1;
		can_frame_pointer[6] = can_counter_tx++;
//		nbytes = send(udp_s, &frame, sizeof(struct can_frame),0);

		nbytes = sendto(udp_s,             // Socket to send result
                   &frame,         // The datagram result to snd
                   sizeof(struct can_frame), // The datagram lngth
                   0,             // Flags: no options
                   (struct sockaddr *)&remote,// addr
                   sizeof(remote));     // Client address length



		printf("%08X", frame.can_id);
		printf("%02X", frame.can_dlc);

		for (i = 0; i < frame.can_dlc; i++)
		{
			printf("%02X", frame.data[i]);
		}

		printf("\n");

		for (i = 0; i < 16; i++)
		{
			printf("%02X ", frame.data[i-8]);
		}
		printf("\n");


		fflush(stdout);

	}
}

	close(s);
	return 0;
}
