#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <netinet/tcp.h>
#include <limits.h>

// Global variable that gives interrupts and error handles a soft way
// to inform the running loop that something has happened and the program
// should try to stop
volatile bool keeprunning = true;

// Signalpipes is a way for interrupt handlers to react signals
volatile int signalpipe_fd[2];

// Handler sends the signal number to pipe for processing in order with
// other events and signals in the main loop.
// This creates a pseudo Go like experience where we wait for things
// on a multiselect loop.
static void signal_to_pipe_handler(int sig)
{
	if (write( signalpipe_fd[1], &sig, sizeof(int) ) != sizeof(int)) {
		perror("write");
		keeprunning = false;
	}
}

// Set up ctrl-c and timer signaling.
// The function returns 0 on success, and nonzero on failure.
int setup_timer_signaling()
{
	// Set the signal from ctrl-c keypress to go to the generic signal handler.
	// Discussion:
	// We use signal() here to be maximally compatible (It is part of standard C).
	// We should use sigaction() for more fine grained control of the signaling
	// and to be safe from badly unspecified functionality of signal() on embedded
	// systems that might have strange implementations of the C-standard.
	// Posix helps here, but only part of the way.
	if (signal( SIGINT, signal_to_pipe_handler ) == SIG_ERR) {
		perror("signal");
		return EXIT_FAILURE;
	}

	// Set the signal from interval timer to go to the generic signal handler.
	if (signal( SIGALRM, signal_to_pipe_handler ) == SIG_ERR) {
		perror("signal");
		return EXIT_FAILURE;
	}

	// Set up the global pipe that signal handler will use to communicate
   // with the main loop.
	// Discussion:
	// We should use signalfd() here, but depending on the libc and linux kernel
	// in use signalfd() might not work with poll() in the main loop. So the more
	// conservative choice is to use pipe().
	// We could save lines by using pipe2(), but it is a gnu extension and as such
	// not part of standard C and as such will have to wait.
	if (pipe( (int *)signalpipe_fd ) != 0 ) {
		perror("pipe");
		return EXIT_FAILURE;
	}

	// Set read part of signaling pipe to be nonblocking.
	// Blocking on either reading or writing to pipes would indicate we have fallen
	// badly behind on signal processing or something similarly extreme would have
	// happened. It is better to let following layers of communication to pick
	// the error situation and react to it than to hang on pipe operation.
	if (fcntl( signalpipe_fd[0], F_SETFL, O_NONBLOCK ) != 0) {
		perror("fcntl");
		return EXIT_FAILURE;
	}

	// Set write part of signaling pipe to be nonblocking.
	if (fcntl( signalpipe_fd[1], F_SETFL, O_NONBLOCK ) != 0) {
		perror("fcntl");
		return EXIT_FAILURE;
	}

	// Set up an interval timer that sends a signal (SIGALRM) repeatedly every 100 ms.
	// Discussion:
	// setitimer is a very problematic source of signals. A process can have only three
	// of them, and other parts of program might need timers, leading to challenging
	// timer resource decisions.
	// timer_create()/timer_settime() is much better at interval signals, but it needs
	// the rt library at link time and depending on implementation it might create an
	// additional thread to handle timing, which might be a problem or not depending on
	// the thread/process/clone budget of the embedded system.
	struct itimerval tv = { .it_interval.tv_usec = 20000, .it_value.tv_usec = 20000 }; // C99 guarantees zeroed fields better than memset
	if (setitimer( ITIMER_REAL, &tv, NULL ) != 0) {
		perror("setitimer");
		return EXIT_FAILURE;
	}

	return 0;
}

// A helper function to connect to a tcp port
int connect_to_tcp_port( const char *addr, int port )
{
	// Define a structure to describe the tcp connection
	struct sockaddr_in client;
	memset( &client, 0, sizeof(client) );
	client.sin_family = AF_INET;
	client.sin_port = htons(port);

	// Convert IPv4 address from text to binary
	// Discussion:
	// In space no one can hear you DNS request. Using a numeric ip address
	if (inet_pton( AF_INET, addr, &client.sin_addr ) != 1) {
		fprintf(stderr, "inet_pton: %s is not a valid network address\n", addr);
		return -1;
	}

	// Create a socket we later connect to the server address
	int sockfd;
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		return -1;
	}

	// Set the connect() call for socket to try only a few times to connect to server.
	// This is to speed up error handling if the server can not be found.
	// Discussion:
	// This is borderline linux specific attribute. It is not mandatory for the functioning
	// of the client, but it makes testing easier. If the embedded environment does not
	// support TCP_SYNCNT these lines can be commented out without other negative effects
	// than that connect() will timeout very slowly. Usually in about 20 seconds.
	int synRetries = 1; // Send at maximum 2 SYN packets. Timeout of connect() is roughly 3 seconds
	setsockopt( sockfd, IPPROTO_TCP, TCP_SYNCNT, &synRetries, sizeof(synRetries) );

	// Connect to the server over tcp
	if (connect( sockfd, (const struct sockaddr *)(&client), sizeof(client) ) < 0) {
		perror("connect");
		return -1;
	}

	// Set socket to be nonblocking
	// It is easier to handle unexpected error situations on the main loop dynamically
	// than using timeouts on blocking calls. So setting socket to nonblocking
	if (fcntl( sockfd, F_SETFL, O_NONBLOCK) != 0 ) {
		perror("fcntl");
		return -1;
	}

	return sockfd;
}

// Helper function for reading from socket
int read_from_socket( int fd, char *buf, int bufsize )
{
	int got = read( fd, buf, bufsize-1);
	if (got<1) {
		perror("read_from_socket");
		return -1;
	}
	if (buf[got-1] != '\n') {
		fprintf( stderr, "Broken data packet %s\n", buf );
		return -1;
	}

	// Zero out the newline
	buf[got-1] = 0;

	return got-1;
}

// A helper function to connect to a udp port
int connect_to_udp_port( const char *addr, int port )
{
	// Define a structure to describe the tcp connection
	struct sockaddr_in client;
	memset( &client, 0, sizeof(client) );
	client.sin_family = AF_INET;
	client.sin_port = htons(port);

	// Convert IPv4 address from text to binary
	// Discussion:
	// In space no one can hear you DNS request. Using a numeric ip address
	if (inet_pton( AF_INET, addr, &client.sin_addr ) != 1) {
		fprintf(stderr, "inet_pton: %s is not a valid network address\n", addr);
		return -1;
	}

	// Create a socket we later use to send udp packets
	int sockfd;
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket");
		return -1;
	}

	// Connect to the server over udp
	if (connect( sockfd, (const struct sockaddr *)(&client), sizeof(client) ) < 0) {
		perror("connect");
		return -1;
	}

	return sockfd;
}

int main( int argc, char **argv )
{
	// Set up an interval timer
	if (setup_timer_signaling() != 0) {
		perror("setup_timer_signaling");
		exit(EXIT_FAILURE);
	}

	// Use a default addr string, or if something is defined on command line use it.
	// Mainly intended for debug and testing.
	const char *addr_string = "127.0.0.1";
	if ( argc>1 ) {
		addr_string = argv[1];
	}

	// Connect to command socket
	int fd_4000 = connect_to_udp_port(addr_string, 4000);
	
	// Connect to data source sockets
	int fd_4001 = connect_to_tcp_port(addr_string, 4001);
	if (fd_4001 < 0) exit(EXIT_FAILURE);
	int fd_4002 = connect_to_tcp_port(addr_string, 4002);
	if (fd_4002 < 0) exit(EXIT_FAILURE);
	int fd_4003 = connect_to_tcp_port(addr_string, 4003);
	if (fd_4003 < 0) exit(EXIT_FAILURE);

	// Define a list of file descriptors we will wait for on poll()
	struct pollfd wait_for_these[] = { { signalpipe_fd[0], POLLIN },
	                                   { fd_4001, POLLIN },
	                                   { fd_4002, POLLIN },
	                                   { fd_4003, POLLIN },
	                                 };
	int wait_array_len = sizeof(wait_for_these) / sizeof(wait_for_these[0]);

	
	// Define a list of string buffers where we hold json string representation of the data.
	// Discussion:
	// This makes it easier to use printf() field width formatting to achieve the pseudo columnar look
	// that was present in the example
	char channel_str[3][20] = { {"\"--\","}, {"\"--\","}, {"\"--\""} };

	// This is an efficient way to keep track wether we have seen data for a while
	int lines_without_data = 0;

	// Frequency of output channel 3
	int chan3_frq = 500; // Initial default value the server starts with

	// Mainloop. Here we handle data and signals
	while( keeprunning ) {
		// Wait for file descriptors of sockets or signals to become active
		int n_events = poll( wait_for_these, wait_array_len, 10000 );

		// Bypass an interrupt from the timer
		if ( n_events < 0 && errno == EINTR ) continue;
		
		// If there are no events, go back to poll
		if(n_events<=0) continue;

		// Look through file descriptors that have something to read
		for(int i=0;i<wait_array_len;i++) {

			// We are only interested in POLLIN events, anything else we bypass
			if (wait_for_these[i].revents != POLLIN) continue;

			if (wait_for_these[i].fd == signalpipe_fd[0]) {
				int sig = SIGINT; // If anything goes wrong, treat it as SIGINT

				if (read( signalpipe_fd[0], &sig, sizeof(int) ) < (int)sizeof(int) ) {
					perror("read");
					exit(EXIT_FAILURE); // Failed to read from a system pipe. Something is very wrong. Exit
				}

				// The signal was from the interval timer. Process it
				if (sig == SIGALRM) {
					// Get the current wall clock time.
					// Discussion:
					// Check here that timespec.tv_sec is 64 bits. This depends on the kernel
					// version and cpu width. Older installations might have a 32 bit .tv_sec
					// which will overrun 2038.
					struct timespec tp;
					clock_gettime( CLOCK_REALTIME, &tp );

					// Create a ms based timestamp integer
					long long int ms_time = (tp.tv_sec*1000)+(tp.tv_nsec/1000000);

					// Print out latest values. Json is not a columnar format, but it also not
					// bothered by keeping the fields on their own columns to improve readability
					// as demonstrated in the requirement example
					printf("{\"timestamp\": %ld, \"out1\": %-7s \"out2\": %-7s \"out3\": %s}\n",
						ms_time, channel_str[0], channel_str[1], channel_str[2] );

					// We increment the variable lines without data here. Any data received later on will zero it
					lines_without_data += 1;

					// If we have not seen any data from any sources for 10 lines,
					// we conclude the server has died and it's best to exit.
					// The value in error message is off by one or two depending on situation
					// but is only intended to give a rough idea about the length of data loss.
					// Discussion:
					// We have two options here. Either to try to reconnect multiple times
					// or stop the program and let it be restarted.
					// We simply do not have enough information at the moment to know what is wrong.
					// The server might have died and/or restarted, the network might have died,
					// we might be bugged or a third program might have gone crazy and is eating
					// all resources. We must assume there is a Computer Operating Properly (COP) process or
					// or unix init/inittab or even systemd in the background trying to control
					// the situation and will restart us when it things everything is back in order again.
					if (lines_without_data > 50) {
						fprintf( stderr, "No data from server for a long time. Number of empty lines: %d\n", lines_without_data );
						keeprunning = false;
					}

					// Reset channel strings
					strcpy(channel_str[0], "\"--\",");
					strcpy(channel_str[1], "\"--\",");
					strcpy(channel_str[2], "\"--\"");
				}
				// The signal was ctrl-c. Stop the loop in a controlled fashion.
				else if (sig == SIGINT) {
					keeprunning = false;
				} else {
					fprintf( stderr, "Unknown signal %d\n", sig );
					keeprunning = false;
				}
				
				continue;
			}

			// Check and read from the sockets if they have data available
			char buf[20];
			if (wait_for_these[i].fd == fd_4001 ) {
				if (read_from_socket( fd_4001, buf, sizeof(buf) ) <2 ) {
					perror("reading 4001");
					exit(EXIT_FAILURE);
				}
				sprintf( channel_str[0], "\"%s\",", buf);
				lines_without_data = 0;
			}
			if (wait_for_these[i].fd == fd_4002 ) {
				if (read_from_socket( fd_4002, buf, sizeof(buf) ) <2 ) {
					perror("reading 4002");
					exit(EXIT_FAILURE);
				}
				sprintf( channel_str[1], "\"%s\",", buf);
				lines_without_data = 0;
			}
			if (wait_for_these[i].fd == fd_4003 ) {
				if (read_from_socket( fd_4003, buf, sizeof(buf) ) <2 ) {
					perror("reading 4003");
					exit(EXIT_FAILURE);
				}
				sprintf( channel_str[2], "\"%s\"", buf);
				lines_without_data = 0;

				// Parse channel 3 and react to it
				char *end;
				float chan3_val = strtof(buf, &end);
				// if end does not point to a zero char, the float was broken. Bypass further handling
				if (*end != '\0') {
					continue;
				}

				printf("%d %f\n", chan3_frq, chan3_val);
				if (chan3_val >= 3.0 && chan3_frq != 1000) {
					// Set output 1 to 1 Hz @ 8000 amp
					short int message_hz[4] =  { htons(2), htons(1), htons(255), htons(1000) };
					short int message_amp[4] = { htons(2), htons(1), htons(170), htons(8000) };
					sendto(fd_4000, message_hz,  sizeof(message_hz),  0, NULL, 0);
					sendto(fd_4000, message_amp, sizeof(message_amp), 0, NULL, 0);
					chan3_frq = 1000;
				}
				if (chan3_val < 3.0 && chan3_frq != 2000) {
					// Set output 1 to 2 Hz @ 4000 amp
					short int message_hz[4] =  { htons(2), htons(1), htons(255), htons(2000) };
					short int message_amp[4] = { htons(2), htons(1), htons(170), htons(4000) };
					sendto(fd_4000, message_hz,  sizeof(message_hz),  0, NULL, 0);
					sendto(fd_4000, message_amp, sizeof(message_amp), 0, NULL, 0);
					chan3_frq = 2000;
				}
			}
		}
	}

	// Unnecessary, but at least there's symmetry
	// Close the pipe
	close( signalpipe_fd[0] );
	close( signalpipe_fd[1] );
	// Close the sockets
	close( fd_4000 );
	close( fd_4001 );
	close( fd_4002 );
	close( fd_4003 );
}

