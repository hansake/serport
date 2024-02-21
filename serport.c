/* serport - a very simple serial device port to STDIO connector for ttyd
 *
 * serport.c
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
#include <getopt.h>
#include <libgen.h>
#include <error.h>
#include <errno.h>
#include <termios.h>
#include "serport.h"

/* Options */
char *program_name;
char *opt_serial_device;
unsigned int opt_baudrate = 9600;

/* Convert string to long
 */
long string_to_long(char *string)
    {
    long result;
    char *end_token;

    errno = 0;
    result = strtol(string, &end_token, 10);
    if ((errno != 0) || (*end_token != 0))
        {
        printf("Error: Invalid digit\n");
        exit(EXIT_FAILURE);
        }
    return (result);
    }

/* Show usage information
 */
void usage(int status)
    {
    if (status != EXIT_SUCCESS)
        fprintf(stderr, "Try `%s --help' for more information.\n", program_name);
   else
        {
        printf("Usage: %s [OPTIONS] [serial device]\n", program_name);
        printf("  Connect serial device to STDIO\n");
        printf("  -h, --help                show help\n");
        printf("  -b, --baudrate [BAUDRATE], default 9600\n");
        }
    exit(status);
    }

/* A very simple serial device port to STDIO connector for ttyd
 */
int main(int argc, char *argv[])
    {
    int status = 0;
    int optchr;

    int option_index = 0;
    static struct option long_options[] =
        {
            {"help",     no_argument,       NULL, 'h'},
            {"baudrate", required_argument, NULL, 'b'},
            {NULL, 0, NULL, 0}
        };


    program_name = basename(argv[0]);
    argv[0] = program_name;
    while ((optchr = getopt_long(argc, argv, "hb:", long_options, &option_index)) != -1)
        {
        switch (optchr)
            {
        case 'h':
            usage(EXIT_SUCCESS);
        case 'b':
            opt_baudrate = (unsigned int)string_to_long(optarg);
            break;
        default:
            usage(EXIT_FAILURE);
            }
        }

    if (optind >= argc)
        {
        fprintf(stderr, "Serial device missing\n");
        usage(EXIT_FAILURE);
        }

    if (optind < argc)
        opt_serial_device = argv[optind++];

    /* Configure input terminal */
    stdin_configure();

    /* Configure output terminal */
    stdout_configure();

    /* Configure serial device */
    serial_configure();

    /* Connect to serial device */
    if ((status = serial_connect()) == 0)
        {
        /* Start serial device to stdin thread */
        serial_input_thread_create();

        /* Start stdin to serial device loop */
        stdin_input_loop();
        }
    return (status);
    }

