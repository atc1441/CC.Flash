Implement Texas flash programming protocol via Arduino board.
See
http://www.ti.com/tool/cc-debugger
and
http://akb77.com/g/rf/program-cc-debugger-cc2511-with-arduino/
for details

1. Upload CC_Flash.ino sketch to Arduino
2. Connect
    Target          Arduino or set it to the pins you need
        DC (P2.1) - pin 19
        DD (P2.2) - pin 23
        RST       - pin 33
        GND       - GND
3. Start program CC.Flash.exe (need .NET 2.0)
