# ***Arduino weather station with TFT display***



**A sleek, touch-enabled smart desk clock and weather dashboard built for the Arduino UNO R4 Minima. Because the R4 Minima lacks onboard Wi-Fi, this project utilizes a Python script running on a connected PC to fetch OpenWeatherMap API data and pipe it over a USB connection. Offline fallback using the R4's internal hardware RTC, and a multi-page touch interface.**





### ***Core features***



**Three-Page Touch UI:** Navigate through a **multi-screen interface** using capacitive/resistive touch edge-tapping. The interface includes a main dashboard displaying time and basic weather, a geometric wind compass, and an environment/forecast tab.



**Offline Fallback:** If the PC connection is lost, it **falls back to the UNO R4's internal hardware Real-Time Clock (RTC)** to maintain **accurate timekeeping** without missing a second.



**Python USB Bridge:** The **PC acts as the heavy lifter**. It handles API rate limiting, calculates complex environmental metrics (like Dew Point via the Magnus-Tetens formula), **and sends lightweight, parsed data packets to the microcontroller.**



**Auto-Reconnecting Serial:** The **Python script actively scans COM ports** and **automatically re-establishes the connection** if the Arduino is temporarily unplugged.





### ***Hardware Setup***



**Required Components:** An Arduino UNO R4 Minima and a standard 2.5" 8-bit parallel TFT LCD Shield.



**Assembly:** Align the pins on the back of the TFT shield with the female headers on the Arduino R4 Minima and press them firmly together.



**Connection:** Connect the assembled device to your PC using a standard USB-C cable.







### ***Software \& Code setup***



1. ###### **Arduino code (C++)**



* Open the Arduino IDE and install the following libraries via the Library Manager:
1. **GFX Library for Arduino** (for display rendering)
2. **TouchScreen** (for resistive touch mapping. Sometimes pre-installed in Arduino IDE)



* Open the **Arduino\_TFT-weather-station.ino** sketch.



* Review the USER CUSTOMIZATION SETTINGS block at the top of the file to configure your preferred colors, text sizes, and feature toggles (e.g., showing/hiding seconds).



* Upload the sketch to your UNO R4 Minima.





###### ***2. PC Bridge (Python)***



* For this, use of VScode is recommended



* Ensure Python 3 is installed on your machine.



* Open a terminal and install the required packages:
1. ***pip install pyserial requests***



* Open the **Weather-syncing.py** file in a text editor.



* Update the **USER CONFIGURATION** block with your specific details:
1. **API\_KEY:** Your free OpenWeatherMap API key.
2. **CITY:** Your target city
3. **SERIAL\_PORT:** The COM port your Arduino is connected to (e.g., "COM5").





### ***How to run the program***



1. ###### **Starting the System**



To initiate the data sync, simply run the Python script. For seamless daily use, create a Windows Batch file (.bat) on your desktop containing the following execution command:



***@echo off***

***python "C:\\path\\to\\your\\Weather-syncing.py"***

***pause***



Double-clicking this shortcut will open a background terminal, scan for the Arduino, perform a two-way handshake, and immediately push the first batch of weather data.





2. ###### **Navigation \& Interaction**



**Page Turning:** Tap the far-left or far-right edges of the TFT screen to navigate back and forth between the three data pages.



**Status Monitoring:** The main dashboard features a dynamic status badge in the top right corner. It will display a green SYNCED tag when receiving active pings from the PC, and will switch to a red OFFLINE tag if the USB data stream is interrupted.

