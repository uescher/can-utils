#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <linux/can.h>
#include <linux/can/raw.h>

int main(int argc, char **argv)
{
	int s; /* can raw socket */
	int nbytes;
	struct sockaddr_can addr;
	struct can_frame frame;
	struct ifreq ifr;
	frame.can_id = 0x654321;
	frame.can_id |= CAN_EFF_FLAG;
	frame.can_dlc = 5;
	if ((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) { return 1; }
	addr.can_family = AF_CAN;
	strcpy(ifr.ifr_name, "vcan0");
	if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) { return 1; }
	addr.can_ifindex = ifr.ifr_ifindex;
	setsockopt(s, SOL_CAN_RAW, CAN_RAW_FILTER, NULL, 0);
	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) { return 1; }
	if ((nbytes = write(s, &frame, sizeof(frame))) != sizeof(frame)) { return 1; }
	close(s);
	return 0;
}