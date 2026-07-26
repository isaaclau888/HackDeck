# HackDeck
<img width="1920" height="1080" alt="HackDeck_2026-Jul-26_08-19-04PM-000_CustomizedView14507058763_png" src="https://github.com/user-attachments/assets/432468ca-2f31-4b9b-ae95-9bafb7a1ab88" />

HackDeck is a pocket-sized ESP32 touchscreen tool for testing hardware and checking pinouts.
Firstly, it shows live stats like displays your team’s GitHub updates, Slack messages, and project statuses.
Secondly, it serves as a quick hardware pinout reference and component tester for your workbench.
And last but not least, it lets you touch control custom web/GML tools or games you write for it.

<img width="1632" height="1300" alt="unfold-hackdeck" src="https://github.com/user-attachments/assets/a3372eab-2753-4fcd-98e6-716c5fb800b7" />

## How to build it?
Please print all the 3D parts. <p>
<img width="518" height="487" alt="螢幕截圖 2026-07-27 上午5 39 52" src="https://github.com/user-attachments/assets/433bc401-ab2b-4894-a5fd-8d10c0218d8c" />

# How to assemble all the parts?
First, place the screen body down. <p>
<img width="508" height="427" alt="螢幕截圖 2026-07-27 上午5 42 49" src="https://github.com/user-attachments/assets/bdcf44f2-88d0-42db-99fc-5e4f1efd9427" />

Secondly, follow the wiring diagram and wire the parts. <p>
<img width="5000" height="3622" alt="IMG_2321" src="https://github.com/user-attachments/assets/0ef5f370-1b5d-4ee4-aaa0-5afdd943a126" />

1. IP5306 Power Module
 * BAT+: Connect to the Red wire from your LiPo Battery. 
 * BAT-: Connect to the Black wire from your LiPo Battery. 
 * OUT+: Connect to Pin 2 (Middle Pin) of the SS12D07 Slide Switch. (Use 28 AWG wire)
 * OUT-: Connect to GND on the INA219 module. (Use 28 AWG wire)
2. SS12D07 Slide Switch
 * Pin 2 (Middle Pin): Connect to OUT+ on the IP5306 module. (Use 28 AWG wire)
 * Pin 1 or 3 (Side Pin): Connect to VIN+ on the INA219 module. (Use 28 AWG wire)
3. INA219 Sensor Module
 * VIN+: Connect to the Side Pin of the SS12D07 Slide Switch. (Use 28 AWG wire)
 * VIN-: Connect to Port P1, Pin 1 (5V) on the ESP32 board. (Use PicoBlade cable)
 * GND: Connect to OUT- on the IP5306 and to Port P1, Pin 4 (GND) on the ESP32 board. (Use PicoBlade cable)
 * VCC: Connect to Port P4, Pin 2 (3.3V) on the ESP32 board. (Use PicoBlade cable)
 * SDA: Connect to Port P3, Pin 3 (IO19) on the ESP32 board. (Use PicoBlade cable)
 * SCL: Connect to Port P3, Pin 4 (IO20) on the ESP32 board. (Use PicoBlade cable)
4. ESP32-8048S043 Display Board
 * Port P1 (Bottom Centre, next to USB-C):
   * Pin 1 (5V): Wire coming from VIN- on the INA219 module.
   * Pin 4 (GND): Wire coming from GND on the INA219 module.
 * Port P4 (Bottom Left Corner):
   * Pin 2 (3.3V): Wire going to VCC on the INA219 module.
 * Port P3 (Middle Left Edge):
   * Pin 3 (IO19): Wire going to SDA on the INA219 module.
   * Pin 4 (IO20): Wire going to SCL on the INA219 module.
     
Secondly, follow the wiring diagram and wire the parts. <p>
