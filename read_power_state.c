#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <inttypes.h>
#include <string.h>
#include <sys/ioctl.h>
#include <signal.h>

#define RX_BUF_SIZE   sizeof(pid_t)
#define DEVICE        "/dev/pwr_ctl_onoff-0"

void on_signal(int sig)
{
	fprintf(stdout, "SIGNAL: %d\n", sig);
}


int main (void)
{
	int dev_fd = -1;
	ssize_t n_read = 0;
	uint8_t rx_buf[RX_BUF_SIZE];
	off_t rx_buf_offset = 0;
	pid_t rx_pid = 0;

	// Open device file descriptor
	if ((dev_fd = open(DEVICE, O_RDONLY)) == -1)  {
		fprintf(stderr, "Error when opening: " DEVICE ": %s\n", strerror(errno));
		return 0;
	}

	// Set PID using IOCTL
	if (ioctl(dev_fd, 0, getpid()) != 0) {
		fprintf(stderr, "Error when setting device parameters for : " DEVICE ": %s\n",
			strerror(errno));
		return 0;
	}

	// Read PID into buffer
	while (read(dev_fd, rx_buf + rx_buf_offset, 1) == 1 && rx_buf_offset < RX_BUF_SIZE) {
		rx_buf_offset++;
	}

	// Reconstruct PID
	rx_pid = ((pid_t)rx_buf[3] << 24) | 
	         ((pid_t)rx_buf[2] << 16) |
	         ((pid_t)rx_buf[1] << 8)  |
	         ((pid_t)rx_buf[0] << 0);
	fprintf(stdout, "PID (rx) = %d\n", rx_pid);


	// Now install a signal handler
	if (signal(SIGUSR1, on_signal) == SIG_ERR) {
		fprintf(stderr, "Error when installing signal handler: %s", strerror(errno));
		goto end;
	}

	// Ask device to test signal handler
	if (ioctl(dev_fd, 1, 0) != 0) {
		fprintf(stderr, "Error when asking for a signal test from: " DEVICE ": %s\n",
			strerror(errno));
		goto end;
	}

	// Sleep for a second
	sleep(1);

	// Close buffer
end:
	close(dev_fd);

	return EXIT_SUCCESS;
}