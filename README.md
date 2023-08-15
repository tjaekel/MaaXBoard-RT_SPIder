# MaaXBoard-RT_SPIder
 MaaXBoard-RT SPIder framework

## MaaXBoard-RT SPIder
My project with USB VCP command shell, SPI transactions,
web server, Python host access, TFTP, USB memory stick ...

## What is working?
* USB VCP UART:
  connect with terminal, baudrate does not matter
* Debug UART:
  the Debug UART (LPUART1, via MCU-LINK) is used as well
  (few debug logs)
* Command Line Interpreter:
  enter commands via UART terminal, e.g.
  "help" (see all commands)
* "led" command:
  toggle the three LEDs on board on and off
* "rawspi" command:
  fire a SPI transaction with a byte sequence
* prepared: have SPI bus with two SPI_CSx signals:
  generate SPI_CSx in "SW mode" - works
* USB Memory Stick (with FAT32) and commands:
  "umdir", "umprint"
* Web Server:
  access via 100M PHY or via 1G PHY - both PHYs are working now
  use "ipaddr" to see which ETH PHY is connected, use name in Web Browser, e.g.:
  "maaxboard/" or "maaxboardg/" (100M vs. 1G, mDNS working)
  (WiFi is not enabled yet)

## Testing SPI
connect J1 pin 19 and pin 21, use command "rawspi" in order to
send a sequence of bytes.

## Coming up next
* second SPI CS signal (a bus with two slaves connected)
* SYS_CFG: have a persistent storage of system parameters,
  (e.g. SPI modes)
* Web Server:
  access from a Python script
* add TFTP:
  transfer files to/from USB Memory Stick
* add "Pico-C":
  my script engine to execute C-code scripts
* optimization:
  for performance, memory usage and footprint

## Remarks
The Debug UART (LPUART1, via MCU-LINK header), prints some logs,
esp. when a USB Memory Stick is plugged-in or removed, ETH cable removed or connected...
There is still a bit of code for the USB Memory Stick test, but it does
not format the USB device anymore, neither it creates a new file.
It should be safe to plug-in a FAT32 USB stick with existing files.

You can plug-in and remove without a command to release the USB Memory Stick
(watch the Debug UART).

## Issues
the USB-C VCP UART needs one key press to enable, to see something on this (main) UART

