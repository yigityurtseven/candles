from flask import Flask, request, send_file
import serial
import time

app = Flask(__name__)

PORT = '/dev/cu.usbserial-210'  
# PORT = '/dev/cu.usbmodem2101'  
try:
    ser = serial.Serial(PORT, 9600, timeout=1)
    time.sleep(2)
except:
    print("Serial port bulunamadi, test modunda calisiyor.")
    ser = None

def send(cmd):
    print("Sending:", cmd)
    if ser:
        ser.write((cmd + "\n").encode())

@app.route('/')
def home():
    return send_file('index.html')

@app.route('/cmd')
def cmd():
    val = request.args.get('val')
    send(val)
    return "ok"

if __name__ == "__main__":
    app.run(host='0.0.0.0', port=3000)