# serport
Connects serial tty device to STDIO. To be used with ttyd.
Used to make a terminal server with a web interface using
RS232 connections to a number of old computers.

The main reason for making the serport program was to get
ZMODEM to work.

```
ttyd --writable -t enableZmodem=true -p 7680 -t fontSize=16 -w /home/hal serport -b 9600 /dev/ttyUSB0
```
Used with my cloned ttyd that is extended with a command
to get hex and ASCII dumps of data traffic for debugging.
[hansake/ttyd: Share your terminal over the web](https://github.com/hansake/ttyd)

```
ttyd --hexdump --writable -t enableZmodem=true -p 7680 -t fontSize=16 -w /home/hal serport -b 9600 /dev/ttyUSB0
```
When using this debug command I discovered that the ZMODEM protocol sent data byte by byte
which did not work very well with the zmodem.js implementation used in ttyd.
