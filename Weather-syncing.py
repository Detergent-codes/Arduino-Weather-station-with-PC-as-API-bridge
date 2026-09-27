import serial
import serial.tools.list_ports
import time
import sys
import requests
import math
from datetime import datetime

# ==========================================
#          USER CONFIGURATION
# ==========================================
CITY = 'Your city' # Paste your city name
API_KEY = 'Your OpenWeatherMap API key' # Paste your API key
UPDATE_INTERVAL_MINUTES = 1  # Fetch internet weather every 1 minute
SERIAL_PORT = 'COM5' #COM port of the arduino 
BAUD_RATE = 9600
# ==========================================

WEATHER_URL = f"http://api.openweathermap.org/data/2.5/weather?q={CITY}&appid={API_KEY}&units=metric"
FORECAST_URL = f"http://api.openweathermap.org/data/2.5/forecast?q={CITY}&appid={API_KEY}&units=metric"

def get_weather_data():
    try:
        w_res = requests.get(WEATHER_URL, timeout=5).json()
        temp = f"{w_res['main']['temp']:.1f} C"
        cond = w_res['weather'][0]['description'].title()
        hi = f"{w_res['main']['temp_max']:.1f} C"
        lo = f"{w_res['main']['temp_min']:.1f} C"
        wspeed = f"{w_res['wind']['speed']} m/s"
        wdeg = str(w_res['wind'].get('deg', 0))
        hum = f"{w_res['main']['humidity']} %"
        press = f"{w_res['main']['pressure']} hPa"
        
        T = w_res['main']['temp']
        RH = w_res['main']['humidity']
        alpha = ((17.27 * T) / (237.7 + T)) + math.log(RH / 100.0)
        dew_calc = (237.7 * alpha) / (17.27 - alpha)
        dew = f"{dew_calc:.1f} C"
        
        f_res = requests.get(FORECAST_URL, timeout=5).json()
        tz_offset = f_res['city']['timezone']
        
        def format_f_time(dt):
            local_dt = datetime.utcfromtimestamp(dt + tz_offset)
            return local_dt.strftime("%I %p").lstrip('0')
            
        f1_time = format_f_time(f_res['list'][1]['dt'])
        f1_temp = f"{f_res['list'][1]['main']['temp']:.0f}C"
        f2_time = format_f_time(f_res['list'][2]['dt'])
        f2_temp = f"{f_res['list'][2]['main']['temp']:.0f}C"
        
        now = datetime.now()
        fake_local_epoch = int((now - datetime(1970, 1, 1)).total_seconds())
        
        return f"<{fake_local_epoch}|{temp}|{cond}|{hi}|{lo}|{wspeed}|{wdeg}|{dew}|{hum}|{press}|{f1_time}|{f1_temp}|{f2_time}|{f2_temp}>\n"
    except Exception as e:
        print(f"API Error: {e}")
        return None

def connect_to_arduino():
    print(f"\nScanning for Arduino on {SERIAL_PORT}...")
    while True:
        active_ports = [p.device for p in serial.tools.list_ports.comports()]
        if SERIAL_PORT in active_ports:
            try:
                arduino = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
                arduino.setDTR(False)
                time.sleep(1)
                arduino.flushInput()
                arduino.setDTR(True)
                time.sleep(1)
                
                timeout_start = time.time()
                while time.time() - timeout_start < 5:
                    if arduino.in_waiting > 0:
                        if "ARDUINO_READY" in arduino.readline().decode('utf-8', errors='ignore'):
                            print("SUCCESS: Connected to Arduino!\n")
                            return arduino
                arduino.close()
            except serial.SerialException:
                pass 
        time.sleep(2)

try:
    last_weather_fetch = 0
    while True:
        arduino = connect_to_arduino()
        try:
            while True:
                # Send heavy data once per interval
                if time.time() - last_weather_fetch > (UPDATE_INTERVAL_MINUTES * 60):
                    print("Fetching fresh OpenWeatherMap data...")
                    last_weather_fetch = time.time() # Update timer to prevent IP bans if API fails
                    new_data = get_weather_data()
                    
                    if new_data:
                        arduino.write(new_data.encode('utf-8'))
                        arduino.flush()
                else:
                    # Send tiny invisible heartbeat every 1 second
                    arduino.write(b'<H>')
                    arduino.flush()
                
                if arduino.in_waiting > 0:
                    arduino.read(arduino.in_waiting)
                    
                time.sleep(1)
                
        except serial.SerialException:
            print("\n[!] CONNECTION LOST.")
            arduino.close()
            
except KeyboardInterrupt:
    sys.exit()