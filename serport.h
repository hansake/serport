/* serport - a very simple serial device port to STDIO connector for ttyd
 *
 * serport.h
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

/* serport.c */
extern char *opt_serial_device;
extern unsigned int opt_baudrate;

/* tty.c */
void serial_configure(void);
int serial_connect(void);
void serial_input_thread_create(void);
void stdin_configure(void);
void stdout_configure(void);
void stdin_input_loop(void);

