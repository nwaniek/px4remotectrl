/* Joystick interface for the PX4 drone */
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <fcntl.h>
#include <time.h>
#include <mhash.h>

#include <sys/time.h>
#include <time.h>
#include <arpa/inet.h>

#include <sys/ioctl.h>
#include <linux/joystick.h>

#include "js_packet.h"

#define SERVER_ADDR      "192.168.201.44"

#define SERVER_PORT     56000

#define BUFFER_LENGTH 2041

#define JOY_DEV "/dev/input/js2"

uint32_t crc32(unsigned char const *p, uint32_t len)
{
	int i;
	unsigned int crc = 0;
	while (len--) {
		crc ^= *p++;
		for (i = 0; i < 8; i++)
			crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
	}
	return crc;
}

uint64_t microsSinceEpoch()
{

    struct timeval tv;

    uint64_t micros = 0;

    gettimeofday(&tv, NULL);
    micros =  ((uint64_t)tv.tv_sec) * 1000000 + tv.tv_usec;

    return micros;
}

int main(int argc, char* argv[])
{
    int32_t joy_fd;
    int32_t axis[6] = {0};
    int32_t x, i;

    struct js_event js;

    int32_t server_descriptor;                                          //socket descriptor
    struct sockaddr_in server_addr;                  //stucture for server address, port, etc.
    int32_t temp;
    js_packet_t js_packet;

    uint64_t last_time_stamp = 0;

    //open joystick device
    if( ( joy_fd = open( JOY_DEV , O_RDONLY)) == -1 )
    {
        printf( "Couldn't open joystick %s\n", JOY_DEV);
        return -1;
    }


    printf("Joystick detected\n");

    //use non-blocking mode for reads
    fcntl( joy_fd, F_SETFL, O_NONBLOCK );


    js_packet.joy_id = JOY_ID;
    //initialize PX4 Joystick Client

    // get a socket descriptor
    if((server_descriptor = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    { 
        printf("PX4 Joystick Client - socket() error\n");
        exit(-1);
    }
    else
    {
        printf("PX4 Joystick Client - socket() is OK!\n");
    }
    fflush(stdout);

    memset(&server_addr, 0x00, sizeof(struct sockaddr_in));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_ADDR);

    // connect to tcp server
    temp = connect(server_descriptor,(struct sockaddr *) &server_addr,sizeof(server_addr));
    if(temp < 0)
    {
        printf("PX4 Joystick Client - connect() error\n");
        close(server_descriptor);
        exit(-1);
    }
    else
    {
        printf("PX4 Joystick Client connect ok\n");
    }

    while( 1 )
    {

        // read the joystick event
        read(joy_fd, &js, sizeof(struct js_event));

        // check the type of the event and store value
        switch (js.type & ~JS_EVENT_INIT)
        {
        case JS_EVENT_AXIS:
            js_packet.axis[ js.number ] = js.value;
            break;
        }
        // set yaw to 0 for the time being...
        js_packet.axis[0] = 0;

      usleep(1000);

	// send command to the drone
        if(microsSinceEpoch() - last_time_stamp > 40000) //CD: Changed this to 200000 instead of 40000
        {
            last_time_stamp = microsSinceEpoch();

            //write packet to TCP socket / write the sliders and buttons information 
            // Calculate CRC32 Checksum:
            
           js_packet.checksum = crc32((char *) &js_packet,sizeof(js_packet_t) - sizeof(uint32_t));

            temp = write(server_descriptor,&js_packet,sizeof(js_packet_t));
            if (temp < 0)
            {
                printf("PX4 Joystick Client - write() error\n");
                close(server_descriptor);
                exit(-1);
            }
        }
    }
    close( joy_fd );
    return 0;

}
