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
  web page to enter commands is up and running
  access MCU via Network from host Python script works (find a demo Python script in folder "Python")
* TFTP:
  transfer files to/from USB memory stick via TFTP
* ITM_Print:
  output debug messages to SWO Viewer in IDE
* SPI with two GPIO SPI CS signals prepared (working),
  just extend the SPI API calls (using a number for which CS to generate)

## Testing SPI
connect J1 pin 19 and pin 21, use command "rawspi" in order to
send a sequence of bytes.

## Coming up next
* SYS_CFG: have a persistent storage of system parameters,
  (e.g. SPI modes)
* add "Pico-C":
  my script engine to execute C-code scripts
* optimization:
  for performance, memory usage and footprint
* WIFI is prepared but not yet working (files in project, but compile errors)

## Remarks
The Debug UART (LPUART1, via MCU-LINK header), prints some logs,
esp. when a USB Memory Stick is plugged-in or removed, ETH cable removed or connected...
There is still a bit of code for the USB Memory Stick test, but it does
not format the USB device anymore, neither it creates a new file.
It should be safe to plug-in a FAT32 USB stick with existing files.

You can plug-in and remove without a command to release the USB Memory Stick
(watch the Debug UART).

When you use files, e.g. via "sdprint" or via TFTP - you have to specify the file name as:
"1:/filename" (with the drive letter 1:/ in front)

For ITM_Print (SWO Viewer) use: 960000000 [Hz] as Core Speed (or "Detect") and
132000000 [Hz] for the SWO Trace Speed.
Connect and start the SWO viewer: the command "test" will use it.

## Issues
the USB-C VCP UART needs one key press to enable, to see something on this (main) UART

