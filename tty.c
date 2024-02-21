/* serport - a very simple serial device port to STDIO connector for ttyd
 *
 * tty.c
 *
 * Copyright (c) 2024  Hans-Åke Lund
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/file.h>
#include <termios.h>
#include <stdbool.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <poll.h>
#include "serport.h"

static int serialfd;
static bool connected = false;
static struct termios serialio, serialio_old, stdout_new, stdout_old, stdin_new, stdin_old;
static pthread_mutex_t mutex_serial_input_ready = PTHREAD_MUTEX_INITIALIZER;
static pthread_t serial_thread;

void serial_configure(void)
    {
    int status;
    speed_t baudrate;

    /* Check and translate valid baudrates */
    switch (opt_baudrate)
        {
    case 2400:
        baudrate = B2400;
        break;
    case 4800:
        baudrate = B4800;
        break;
    case 9600:
        baudrate = B9600;
        break;
    case 19200:
        baudrate = B19200;
        break;
    /* Add more baudrates as needed and possible */
    default:
        fprintf(stderr, "Invalid baudrate %d\n", opt_baudrate);
        exit(EXIT_FAILURE);
        }

    memset(&serialio, 0, sizeof(serialio));

    /* Set input speed */
    status = cfsetispeed(&serialio, baudrate);
    if (status == -1)
        {
        fprintf(stderr, "Could not configure input speed (%s)\n", strerror(errno));
        exit(EXIT_FAILURE);
        }

    /* Set output speed */
    status = cfsetospeed(&serialio, baudrate);
    if (status == -1)
        {
        fprintf(stderr, "Could not configure output speed (%s)\n", strerror(errno));
        exit(EXIT_FAILURE);
        }

    /* Control, input, output, local modes for tty device */
    serialio.c_cflag |= CLOCAL | CREAD | CS8;
    serialio.c_oflag = 0;
    serialio.c_lflag = 0;

    /* Control characters */
    serialio.c_cc[VTIME] = 0; /* Inter-character timer unused */
    serialio.c_cc[VMIN]  = 1; /* Blocking read until 1 character received */

    }

void serial_disconnect(void)
    {
    tcsetattr(serialfd, TCSANOW, &serialio_old);
    if (connected)
        {
        fprintf(stderr, "Disconnected");
        flock(serialfd, LOCK_UN);
        close(serialfd);
        connected = false;
        }
    }

int serial_connect(void)
    {
    int status;

    /* Open tty device */
    serialfd = open(opt_serial_device, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (serialfd < 0)
        {
        fprintf(stderr, "Could not open serial device (%s)\n\r", strerror(errno));
        exit(EXIT_FAILURE);
        }

    /* Make sure device is of tty type */
    if (!isatty(serialfd))
        {
        fprintf(stderr, "Not a tty device\n\r");
        exit(EXIT_FAILURE);
        }

    /* Save current serial device settings */
    if (tcgetattr(serialfd, &serialio_old) < 0)
        {
        fprintf(stderr, "Saving current serial device settings failed\n\r");
        exit(EXIT_FAILURE);
        }

    /* Lock device file */
    status = flock(serialfd, LOCK_EX | LOCK_NB);
    if ((status == -1) && (errno == EWOULDBLOCK))
        {
        fprintf(stderr, "Device file is locked by another process\n\r");
        exit(EXIT_FAILURE);
        }

    /* Flush stale I/O data (if any) */
    tcflush(serialfd, TCIOFLUSH);

    /* Activate new stdin settings */
    status = tcsetattr(serialfd, TCSANOW, &serialio);
    if (status == -1)
        {
        fprintf(stderr, "Could not apply new serial device settings (%s)\n\r", strerror(errno));
        exit(EXIT_FAILURE);
        }

    connected = true;

    /* Make sure we disconnect and restore old serial device settings on exit */
    atexit(&serial_disconnect);

    return (status);
    }

void *serial_input_thread(void *arg)
    {
    char input_buffer[BUFSIZ];
    ssize_t byte_count;
    ssize_t read_byte_count;
    ssize_t bytes_written;
    int timeout;
    struct pollfd fds[1];
    int nfds = 1;
    int rc;

    /* Signal that serial input is ready */
    pthread_mutex_unlock(&mutex_serial_input_ready);

    /* Initialize the pollfd structure */
    memset(fds, 0, sizeof(fds));
    fds[0].fd = serialfd;
    fds[0].events = POLLIN;

    /* Set the timeout to 2 ms */
    /* may be dependent on baudrate */
    timeout = 2;

    /* Input loop for serial device */
    while (1)
        {
        byte_count = 0;
        while (1) /* try to read until timeout or buffer full */
            {
            /* If buffer full, write to STDOUT */
            if (byte_count >= BUFSIZ)
                break;
            rc = poll(fds, nfds, timeout);
            /* Check to see if the poll call failed */
            if (rc < 0)
                {
                fprintf(stderr, "serial poll() failed\n");
                pthread_exit(0);
                }
            /* If timeout, write to STDOUT */
            if (rc == 0)
                break;
            /* Input from serial device ready */
            read_byte_count = read(serialfd, &input_buffer[byte_count], 1);
            if (read_byte_count < 0)
                {
                /* No error actually occurred */
                if (errno == EINTR)
                    continue;
                fprintf(stderr, "Could not read from serial device (%s)", strerror(errno));
                pthread_exit(0);
                }
            else if (read_byte_count == 0)
                {
                fprintf(stderr, "Could not read from serial device (%s)", strerror(errno));
                pthread_exit(0);
                }
            byte_count += read_byte_count;
            }
        /* Write all bytes read to STDOUT */
        while (byte_count > 0)
            {
            bytes_written = write(STDOUT_FILENO, input_buffer, byte_count);
            if (bytes_written < 0)
                {
                fprintf(stderr, "Could not write to stdout (%s)", strerror(errno));
                break;
                }
            byte_count -= bytes_written;
            }
        }
    pthread_exit(0);
    }

void serial_input_thread_create(void)
    {
    pthread_mutex_lock(&mutex_serial_input_ready);

    if (pthread_create(&serial_thread, NULL, serial_input_thread, NULL) != 0)
        {
        fprintf(stderr, "pthread_create() error");
        exit(1);
        }
    }

void stdin_restore(void)
    {
    tcsetattr(STDIN_FILENO, TCSANOW, &stdin_old);
    }

void stdin_configure(void)
    {
    int status;

    /* Save current stdin settings */
    if (tcgetattr(STDIN_FILENO, &stdin_old) < 0)
        {
        fprintf(stderr, "Saving current stdin settings failed");
        exit(EXIT_FAILURE);
        }

    /* Prepare new stdin settings */
    memcpy(&stdin_new, &stdin_old, sizeof(stdin_old));

    /* Reconfigure stdin (RAW configuration) */
    cfmakeraw(&stdin_new);

    /* Control characters */
    stdin_new.c_cc[VTIME] = 0; /* Inter-character timer unused */
    stdin_new.c_cc[VMIN]  = 1; /* Blocking read until 1 character received */

    /* Activate new stdin settings */
    status = tcsetattr(STDIN_FILENO, TCSANOW, &stdin_new);
    if (status == -1)
        {
        fprintf(stderr, "Could not apply new stdin settings (%s)", strerror(errno));
        exit(EXIT_FAILURE);
        }

    /* Make sure we restore old stdin settings on exit */
    atexit(&stdin_restore);
    }

void stdout_restore(void)
    {
    tcsetattr(STDOUT_FILENO, TCSANOW, &stdout_old);
    }

void stdout_configure(void)
    {
    int status;

    /* Save current stdout settings */
    if (tcgetattr(STDOUT_FILENO, &stdout_old) < 0)
        {
        fprintf(stderr, "Saving current stdio settings failed");
        exit(EXIT_FAILURE);
        }

    /* Prepare new stdout settings */
    memcpy(&stdout_new, &stdout_old, sizeof(stdout_old));

    /* Reconfigure stdout (RAW configuration) */
    cfmakeraw(&stdout_new);

    /* Control characters */
    stdout_new.c_cc[VTIME] = 0; /* Inter-character timer unused */
    stdout_new.c_cc[VMIN]  = 1; /* Blocking read until 1 character received */

    /* Activate new stdout settings */
    status = tcsetattr(STDOUT_FILENO, TCSANOW, &stdout_new);
    if (status == -1)
        {
        fprintf(stderr, "Could not apply new stdout settings (%s)", strerror(errno));
        exit(EXIT_FAILURE);
        }

    /* Make sure we restore old stdout settings on exit */
    atexit(&stdout_restore);
    }

void stdin_input_loop(void)
    {
    char input_buffer[BUFSIZ];
    ssize_t byte_count;
    ssize_t read_byte_count;
    ssize_t bytes_written;
    int timeout;
    struct pollfd fds[1];
    int nfds = 1;
    int rc;

    /* Initialize the pollfd structure */
    memset(fds, 0, sizeof(fds));
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;

    /* Set the timeout to 2 ms */
    /* may be dependent on baudrate */
    timeout = 2;

    /* Input loop for STDIN */
    while (1)
        {
        byte_count = 0;
        while (1) /* try to read until timeout or buffer full */
            {
            /* If buffer full, write to serial device */
            if (byte_count >= BUFSIZ)
                break;
            rc = poll(fds, nfds, timeout);
            /* Check to see if the poll call failed */
            if (rc < 0)
                {
                fprintf(stderr, "STDIN poll() failed\n");
                exit(EXIT_FAILURE);
                }
            /* If timeout, write to serial device */
            if (rc == 0)
                break;
            /* Input from STDIN ready */
            read_byte_count = read(STDIN_FILENO, &input_buffer[byte_count], 1);
            if (read_byte_count < 0)
                {
                /* No error actually occurred */
                if (errno == EINTR)
                    continue;
                fprintf(stderr, "Could not read from STDIN (%s)", strerror(errno));
                exit(EXIT_FAILURE);
                }
            else if (read_byte_count == 0)
                {
                fprintf(stderr, "Could not read from STDIN (%s)", strerror(errno));
                exit(EXIT_FAILURE);
                }
            byte_count += read_byte_count;
            }
        /* Write all bytes read to serial device */
        while (byte_count > 0)
            {
            bytes_written = write(serialfd, input_buffer, byte_count);
            if (bytes_written < 0)
                {
                fprintf(stderr, "Could not write to serial device (%s)", strerror(errno));
                break;
                }
            byte_count -= bytes_written;
            }
        }
    }


