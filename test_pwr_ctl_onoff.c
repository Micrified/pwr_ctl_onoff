#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <inttypes.h>
#include <string.h>
#include <sys/ioctl.h>
#include <signal.h>

// Buffer size required to receive a PID
#define RX_BUF_SIZE	sizeof(pid_t)

// Path of the character device to open
#define DEVICE		"/dev/pwr_ctl_onoff-0"

// Commands for IOCTL of the module
#define CMD_PID_SET	0
#define CMD_SIG_TST	1

/* Signal handler */
void on_signal(int sig)
{
	fprintf(stdout, "\tOk (SIGNAL = %d)\n", sig);
}

/* Read: PID set in the module */
int device_read (int dev_fd)
{
	uint8_t rx_buf[RX_BUF_SIZE];
	off_t offset = 0;
	pid_t pid;

	fprintf(stdout, "Test: read pid\n");

	// Get PID bytes
	while (read(dev_fd, rx_buf + offset, 1) == 1 && offset < RX_BUF_SIZE) {
		offset++;
	}

	// Verify buffer filled
	if (offset != RX_BUF_SIZE) {
		fprintf(stderr, "Error when receiving PID (fd=%d)\n", dev_fd);
		return -1;
	}

	pid = ((pid_t)rx_buf[0] << 0) |
              ((pid_t)rx_buf[1] << 8) |
              ((pid_t)rx_buf[2] << 16) |
              ((pid_t)rx_buf[3] << 24);

	fprintf(stdout, "\tOK (PID = %d)\n", pid);
	return 0;
}

/* Set: PID in the module */
int device_set_pid (int dev_fd, int pid)
{
	fprintf(stdout, "Test: set pid (PID = %d)\n", getpid());
	if (ioctl(dev_fd, CMD_PID_SET, pid) != 0) {
		fprintf(stderr, "Error when setting pid (fd=%d): %s\n", dev_fd, strerror(errno));
		return -1;
	}
	fprintf(stdout, "\tOk\n");
	return 0;
}

/* Test: signal */
int device_test_sig (int dev_fd)
{
	fprintf(stdout, "Test: signal\n");

	// Install handler
	if (signal(SIGUSR1, on_signal) == SIG_ERR) {
		fprintf(stderr, "Error when installing signal handler: %s\n",
			strerror(errno));
		return -1;
	}

	// Request test
        if (ioctl(dev_fd, CMD_SIG_TST, 0) != 0) {
                fprintf(stderr, "Error when requesting signal test (fd=%d): %s\n",
                        dev_fd, strerror(errno));
                return -1;
        }

	// Sleep (such that signal may be received)
	sleep(1);
	return 0;
}


int main (void)
{
	int dev_fd = -1;
	int retval = EXIT_SUCCESS;

	// Open device file descriptor
	if ((dev_fd = open(DEVICE, O_RDONLY)) == -1)  {
		fprintf(stderr, "Error when opening: " DEVICE ": %s\n", strerror(errno));
		retval = EXIT_FAILURE;
		goto end;
	}

	// Set PID
	if (device_set_pid(dev_fd, getpid()) != 0) {
		retval = EXIT_FAILURE;
		goto end;
	}

	// Get PID
	if (device_read(dev_fd) != 0) {
		retval = EXIT_FAILURE;
		goto end;
	}

	// Test signal
	if (device_test_sig(dev_fd) != 0) {
		retval = EXIT_FAILURE;
		goto end;
	}
end:
	close(dev_fd);

	return retval;
}
