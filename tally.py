from os import name
import tkinter as tk
from tkinter import messagebox

from ctypes import Array
from tkinter.constants import ANCHOR, S
from obswebsocket import obsws, requests, events

import socket
import threading as th

import serial

SERVER, SERVER_PORT = "81.95.192.186", 8081
OBS_WS, OBS_WS_PORT, OBS_PASSWORD = "localhost", 4444, "secret"

# GET CONNECTED DEVICES FROM THE SERVER
def listenForDevicesFromServer():
    while True:
        received = sock.recv(1024).decode().strip(";").split(";")

        if received: 
            global devices

            for device in received:
                devices["WIFI:" + device] = ""

            toRemove = []

            for device in devices:
                communication = device.split(":")[0]
                mac = device.split(":")[1]
                
                if mac not in received and communication == "WIFI":
                    toRemove.append(device)
            
            for device in toRemove:
                del devices[device]

            devicesListWidget.delete(0, "end")

            for device in devices:
                devicesListWidget.insert(tk.END, device)


def CommunicateCOM():
    serialDevice.write(b"LIST:WLECOME:x\n")
    while True:
        message = serialDevice.readline().decode().strip(";\r\n")

        if message:
            if "DEVICE_LIST:" in message:
                received = message.split(":")[1].split(";")

                global devices

                for device in received:
                    devices["MESH:" + device] = ""

                toRemove = []

                for device in devices:
                    communication = device.split(":")[0]
                    mac = device.split(":")[1]
                    
                    if mac not in received and communication == "MESH":
                        toRemove.append(device)
                
                for device in toRemove:
                    del devices[device]

                devicesListWidget.delete(0, "end")

                for device in devices:
                    devicesListWidget.insert(tk.END, device)
            

# OBS EVENTS HANDLERS
def on_sceneSwitch(message):
    currentScene = message.getSceneName()
    deviceMac    = str()

    for device, scene in devices.items():
        if scene == currentScene:
            deviceMac += device.split(":")[1]

    if deviceMac:
        sock.sendall(bytes("PGM:" + deviceMac + ":X\r", "ascii"))
        serialDevice.write(str("PGM:" + deviceMac + ":X\n").encode())
    else:
        sock.sendall(bytes("PGM:X:X\r", "ascii"))
        serialDevice.write(str("PGM:X:X\n").encode())
        print("Scene have no device asigned")


# ASIGN SCENE TO DEVICE
def asignScene(scene):
    device = devicesListWidget.get(ANCHOR)
    if device != "":
        scene  = defaultScene.get()
        devices[device] = scene
    else:
        print("No device selected")

    print(devices)


# DISCONNECT WHEN CLOSING THE APP
def on_closing():
    if messagebox.askokcancel("Quit", "Do you want to quit?"):
        sock.shutdown(socket.SHUT_RDWR)
        obs.disconnect()    
        root.destroy()


# GUI SETUP
root = tk.Tk()

devicesListWidget = tk.Listbox(root) 
devicesListWidget.grid(row=1, column=1)


# SOCKETS CONNECTION
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect((SERVER, SERVER_PORT));

obs = obsws(OBS_WS, OBS_WS_PORT, OBS_PASSWORD)

# REGISTER OBS EVENTS
obs.register(on_sceneSwitch, events.SwitchScenes)
obs.connect()

# WELCOME MESSAGE TO SERVER
sock.sendall(bytes("OBS:" + "WELCOME" + ":X\r", "ascii"))


scenesObj  = obs.call(requests.GetSceneList()).getScenes()
scenes = []

for scene in scenesObj:
    scenes.append(scene["name"])

defaultScene = tk.StringVar()
defaultScene.set("")

scenesListWidget = tk.OptionMenu(root, defaultScene, *scenes, command=asignScene)
scenesListWidget.grid(row=2, column=1)

devices = {}

# LISTEN FOR DEVICES
th.Thread(target=listenForDevicesFromServer, args=(), daemon=True).start()


# SERIAL
serialDevice = serial.Serial("COM4", 115200)
serialDevice.timeout = 1

th.Thread(target=CommunicateCOM, args=(), daemon=True).start()

# MAIN LOOP
root.protocol("WM_DELETE_WINDOW", on_closing)
root.mainloop()
