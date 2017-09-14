# nRF52 24-bit I2S Microphone Audio Recording Utility

Modern MEMS microphones are tiny, extremely sensitive and capable of recording audio with impressive clarity. Probably the most popular
embodiment of the MEMS microphone uses the I2S data data socket at 24-bit data word size. However, the term "24-bit" is misleading. The
name would suggest that the stereo "Word Select" (WS) frame would include 48 serial clock (SCK) pulses. Almost without exception modern I2S MEMS
microphones do output 24 data bits per stereo channel but require 32 SCK pulses per stereo channel to function properly. Consequently
the WS frame MUST be 64 SCK pulses wide for proper operation...

Unfortunately the architecture of the nRF52823 I2S module does not properly support 64 SCK pulses per WS frame while operating in 24-bit I2S
"Master" mode. The WS frame width is ALWAYS forced to 48 SCK pulses. This limitation is built into the chip at the mask level and cannot be 
fixed. However, the nRF52823 I2S module will properly decode 24-bit audio data from a 64 SCK pulse wide WS frame when operating in the I2S 
"Slave" mode. The I2S master and slave modes are actually a bit misleading; master mode only means that the nRF52823 I2S module is supplying
the SCK and WS clock pulses while slave mode only means that some other device is supplying the two clock signals. The origin of the clock
signals actually has no effect on which I2S device is sending data and which devices are receiving data...

The application in this repository takes advantage of the fact that neither the nRF52 I2S module nor the the I2S microphone care where the
SCK and WS clock pulses come from. The application uses two of the nRF52's PWM output channels to generate the WS and SCK pulse trains.
The frequency of the the WS PWM is 1/64 of the SCK PWM output. The I2S process is instantiated in slave mode with the I2S master clock
deactivated. The SCK PWM output pin is connected to the I2S SCk pin and similarly the WS PWM output is connected to the I2S WS/LRCK pin.
The I2S microphone is connected as usual to the SCK, WS/LRCK, VDD, GND and SDIN pins of the nRF52823.

The files in this repository work properly with the Nordic nRF5_SDK_13.0.0_04a0bfd SDK and the Nordic pca10040 DK board. There was one
necessary modification to the "app_pwm" library in the SDK. A new function: “app_pwm_ticks_init(…)” was added to allow setting the PWM 
carrier period in terms of 16MHz ticks instead of milliseconds. This essential to achieve the higher carrier frequencies necessary for the
I2S clock pulses. The main "i2s_Slave_External_PWM_CK_Mono" folder is intended to be copied into the
“…\nRF5_SDK_13.0.0_04a0bfd\examples\peripheral” folder of the SDK. The executable firmware is then built like any other peripheral example
in the SDK. The recommended gcc compiler and gnu "make" were used to build the firmware.

Testing and development were done using the Invensense ICS43432 I2S microphone breakout board from Pesky Products
(www.tindie.com/products/onehorse/ics43432-i2s-digital-microphone) and the Nordic pca10040 DK board. The application outputs the mono audio
byte stream via UART serial at 921600baud using CTS/RTS hardware flow control. Capture of the audio byte stream was done using an FTDI 
USB/Serial adapter capable of 921600baud and CTS/RTS flow control in combination with the "Realterm" serial terminal application.
(https://learn.sparkfun.com/tutorials/terminal-basics/real-term-windows). The "Audacity" audio application can then be used to render the captured byte stream to sound. (www.audacityteam.org).

## Step-by-Step Setup

### Software

1. Install the Nordic nRF5_SDK_13.0.0_04a0bfd SDK
2. If using the gcc compiler and gnu "make" method, install these utilities on an accessible path
3. Download/clone the repository
4. Copy the "i2s_Slave_External_PWM_CK_Mono" folder and subfolders into the “…\nRF5_SDK_13.0.0_04a0bfd\examples\peripheral” folder
5. Copy "app_pwm.c" and "app_pwm.h" to the “…\nRF5_SDK_13.0.0_04a0bfd\components\libraries\pwm” folder
6. Build the firmware and upload to the Nordic pca10040 board

### Hardware

1. Connect the SCK PWM output pin (PO.13) to the I2S SCK pin (PO.31)
2. Connect the WS/LRCK PWM output pin(PO.17) to the I2S WS/LRCK pin (PO.30)
3. Connect the SCK pin of the ICS43432 microphone to the I2S SCK pin (PO.31)
4. Connect the WS pin of the ICS43432 microphone to the I2S WS/LRCK pin (PO.30)
5. Connect the SD pin of the ICS43432 microphone to the I2S SDIN pin (PO.26)
6. Connect the L/R pin of the ICS43432 microphone to GND
7. Connect the 3V3 pin of the ICS43432 microphone to VDD
8. Connect the GND pin of the ICS43432 microphone to GND
9. Connect the Tx pin of the USB/serial adapter to the Rx pin (PO.8)
10. Connect the Rx pin of the USB/serial adapter to the Tx pin (PO.6)
11. Connect the CTS pin of the USB/serial adapter to the RTS pin (PO.5)
12. Connect the RTS pin of the USB/serial adapter to the CTS pin (PO.7)

### Data Capture

1. Set up the Realterm port to the FTDI comm port; 921600baud, 8-N-1 data/parity, Hardware Flow Control = CTS/RTS
2. Set up the Realterm display to "Hex(space)"
3. Set up the Realterm capture to a "capture.txt" file in a convenient location, no timestamp, "Direct Capture" selected
4. Open the serial port and verify hexadecimal data is passing in the terminal window
5. Go to the "Capture" tab and start collection with the "Start: Overwrite" button
6. End collection by pressing the "Stop Capture" button

### Rendering Sound from Audacity

1. In Audacity, navigate to the File->Import->Raw Data... option. Select the "capture.txt" file generated by Realterm
2. In the "Import Raw Data" pop-up dialog box select:
   * Encoding: Signed 32-bit PCM
   * Byte order: Big-Endian
   * Channels: 1 Channel (Mono)
   * Start Offset: 0 bytes
   * Amount to Import: 100 %
   * Sample Rate: 10000 Hz
3. Click on the "Import" button. If the audio stream looks strange, try repeating step "2" with start offsets of 1, 2 or 3 bytes
4. Navigate to the Effect->Normalize option and select "OK" in the "Normalize" pop-up dialog box
5. Press the "Play" button and listen to what you recorded! A sample ".wav" file recorded with the nRF2 is included the repository for comparison
