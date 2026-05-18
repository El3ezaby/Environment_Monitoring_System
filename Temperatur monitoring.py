import socket
import tkinter as tk
from tkinter import messagebox

ESP_IP = "192.168.100.17"  # Default ESP32 IP
ESP_PORT = 5005  # Default ESP32 Port

def start_udp():
    global udp_socket
    try:
        udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        udp_socket.bind(("", int(5006)))  # Bind to UDP port
#         udp_socket.bind(("", int(port_field.get())))  # Bind to UDP port

        status_label.config(text="Connected", fg="green")
        root.after(100, receive_data)
    except Exception as e:
        messagebox.showerror("Error", f"Failed to start UDP: {e}")

def stop_udp():
    global udp_socket
    try:
        udp_socket.close()
        status_label.config(text="Disconnected", fg="red")
    except Exception as e:
        messagebox.showerror("Error", f"Failed to stop UDP: {e}")

def receive_data():
    try:
        udp_socket.settimeout(0.5)  # Avoid blocking
        data, _ = udp_socket.recvfrom(1024)
        parsed_data = data.decode().split(", ")
        for item in parsed_data:
            key, value = item.split(":")
            if key in data_fields:
                data_fields[key].delete(0, tk.END)
                data_fields[key].insert(0, value)
    except socket.timeout:
        pass
    except Exception as e:
        messagebox.showerror("Error", f"Error receiving data: {e}")
    root.after(100, receive_data)

def send_thresholds():
    temp = temp_field.get().strip()
    humidity = humidity_field.get().strip()
    ip = ip_field.get().strip()
    port = port_field.get().strip()

    if not ip or not port:
        messagebox.showerror("Error", "IP and Port cannot be empty!")
        return

    try:
        temp = min(100, max(0, float(temp)))
        humidity = min(100, max(0, float(humidity)))
        port = int(port)

        if 'udp_socket' not in globals() or udp_socket.fileno() == -1:
            messagebox.showerror("Error", "UDP connection is not active. Please connect first.")
            return

        message = f"{temp},{humidity}*"
        udp_socket.sendto(message.encode(), (ip, port))
        messagebox.showinfo("Success", "Thresholds sent successfully!")

    except ValueError:
        messagebox.showerror("Error", "Invalid input! Enter numeric values for temperature and humidity.")
    except Exception as e:
        messagebox.showerror("Error", f"Failed to send data: {e}")

root = tk.Tk()
root.title("ESP32 Sensor Monitor")
root.geometry("450x600")

# Title Label
title_label = tk.Label(root, text="ESP32 Monitoring System", font=("Arial", 14, "bold"), pady=10)
title_label.pack()

status_label = tk.Label(root, text="Disconnected", fg="red")
status_label.pack()

# IP and Port Fields
ip_port_frame = tk.Frame(root)
ip_port_frame.pack(pady=5)
tk.Label(ip_port_frame, text="ESP IP:").grid(row=0, column=0)
ip_field = tk.Entry(ip_port_frame, width=15)
ip_field.grid(row=0, column=1)
ip_field.insert(0, ESP_IP)

tk.Label(ip_port_frame, text="Port:").grid(row=0, column=2)
port_field = tk.Entry(ip_port_frame, width=6)
port_field.grid(row=0, column=3)
port_field.insert(0, str(ESP_PORT))

# Sensor Data Fields
data_fields = {}
labels = ["Temperature", "Humidity", "MQ2", "MQ3", "MQ4", "MQ5", "MQ6", "MQ7", "MQ8", "MQ9", "MQ135"]

sensor_frame = tk.Frame(root)
sensor_frame.pack(pady=5)

for i, label in enumerate(labels):
    row = i // 2
    col = (i % 2) * 2
    tk.Label(sensor_frame, text=f"{label}:").grid(row=row, column=col)
    entry = tk.Entry(sensor_frame, width=10)
    entry.grid(row=row, column=col+1)
    data_fields[label] = entry

# Threshold Fields
threshold_frame = tk.Frame(root)
threshold_frame.pack(pady=5)

tk.Label(threshold_frame, text="Temp Threshold:").grid(row=0, column=0)
temp_field = tk.Entry(threshold_frame, width=10)
temp_field.grid(row=0, column=1)

tk.Label(threshold_frame, text="Humidity Threshold:").grid(row=0, column=2)
humidity_field = tk.Entry(threshold_frame, width=10)
humidity_field.grid(row=0, column=3)

# Buttons Frame
button_frame = tk.Frame(root)
button_frame.pack(pady=10)

tk.Button(button_frame, text="Connect", command=start_udp, width=12).grid(row=0, column=0, padx=5)
tk.Button(button_frame, text="Disconnect", command=stop_udp, width=12).grid(row=0, column=1, padx=5)
tk.Button(button_frame, text="Send Thresholds", command=send_thresholds, width=12).grid(row=0, column=2, padx=5)

root.mainloop()