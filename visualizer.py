import socket
import json
import matplotlib.pyplot as plt
from datetime import datetime

# Escuchar en el puerto 23
server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server.bind(('0.0.0.0', 23001))
server.listen(1)

data = {'humedad1': [], 'humedad2': [], 'nivel': [], 'time': []}

while True:
    conn, addr = server.accept()
    received = conn.recv(1024).decode()
    
    # Si es JSON
    if received.startswith('{'):
        d = json.loads(received.strip())
        data['humedad1'].append(d['humedad1'])
        data['humedad2'].append(d['humedad2'])
        data['nivel'].append(d['nivel'])
        data['time'].append(datetime.now())
        
        print(f"Recibido: {d}")
        
        plt.clf()
        plt.plot(data['time'], data['humedad1'], label='Humedad 1')
        plt.plot(data['time'], data['humedad2'], label='Humedad 2')
        plt.plot(data['time'], data['nivel'], label='nivel')
        plt.legend()
        plt.pause(0.1)
    
    conn.close()
