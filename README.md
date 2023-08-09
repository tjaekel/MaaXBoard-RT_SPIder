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

## Testing SPI
connect J1 pin 19 and pin 21, use command "rawspi" in order to
send a sequence of bytes.

## Coming up next
* second SPI CS signal (a bus with two slaves connected)
* SYS_CFG: have a persistent storage of system parameters,
  (e.g. SPI modes)
* add Web Server:
  to access via Web Browser, from a Python script
* add USB Memory Stick:
  use as external script/file storage
* add TFTP:
  transfer files to/from USB Memory Stick
* add "Pico-C":
  my script engine to execute C-code scripts
* optimization:
  for performance, memory usage and footprint

