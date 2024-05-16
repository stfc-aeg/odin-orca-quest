import cv2
import numpy as np
from tornado.escape import json_decode
import logging
from multiprocessing import Process, Queue, Pipe
import time
import tkinter as tk

from odin_data.control.ipc_channel import IpcChannel
from odin_data.control.ipc_message import IpcMessage, IpcMessageException

# Configuration Variables
DISPLAY_INTERVAL = 1  # Display update interval in seconds
LOG_LEVEL = 2  # Logging level (0 = none, 1 = debug, 2 = timings)

class LiveDataViewer:
    def __init__(self, endpoint, resize_x=2048, resize_y=1152, colour='plasma', config_pipe=None):
        self.endpoint = endpoint
        self.resize_x = resize_x
        self.resize_y = resize_y
        self.colour = colour
        self.config_pipe = config_pipe

    def update_config(self):
        while self.config_pipe.poll():
            config = self.config_pipe.recv()
            self.resize_x, self.resize_y = config.get('size', (self.resize_x, self.resize_y))
            self.colour = config.get('colour', self.colour)

    def read_data_from_socket(self, msg, image_queue):
        self.update_config()
        
        start_time = time.time() if LOG_LEVEL >= 2 else None

        header = json_decode(msg[0])
        if LOG_LEVEL >= 1:
            logging.debug("Received Data with Header: %s", header)

        dtype = 'float32' if header['dtype'] == "float" else header['dtype']
        data = np.frombuffer(msg[1], dtype=dtype)
        data = data.reshape((2304, 4096))  # Specific dimensions

        data = cv2.resize(data, (self.resize_x, self.resize_y))
        data = cv2.applyColorMap((data / 256).astype(np.uint8), self.get_colour_map())
        _, buffer = cv2.imencode('.jpg', data)
        buffer = np.array(buffer)

        while not image_queue.empty():
            image_queue.get()

        image_queue.put(buffer)

        if LOG_LEVEL >= 2:
            logging.info(f"Frame processed in {time.time() - start_time} seconds")

    def get_colour_map(self):
        return getattr(cv2, f'COLORMAP_{self.colour.upper()}', cv2.COLORMAP_PLASMA)

def create_gui(config_pipe):
    def update_settings():
        size = (int(width_var.get()), int(height_var.get()))
        colour = colour_var.get()
        config_pipe.send({'size': size, 'colour': colour})

    root = tk.Tk()
    root.title("Configuration Settings")

    tk.Label(root, text="Width:").pack()
    width_var = tk.Entry(root)
    width_var.pack()
    width_var.insert(0, "2048")

    tk.Label(root, text="Height:").pack()
    height_var = tk.Entry(root)
    height_var.pack()
    height_var.insert(0, "1152")

    tk.Label(root, text="Colour Map:").pack()
    colour_var = tk.Entry(root)
    colour_var.pack()
    colour_var.insert(0, "plasma")

    tk.Button(root, text="Update Settings", command=update_settings).pack()

    root.mainloop()

def capture_images(viewer, image_queue):
    channel = IpcChannel(IpcChannel.CHANNEL_TYPE_SUB, viewer.endpoint)
    channel.connect()
    channel.subscribe()

    while True:
        poll_success = channel.poll(1000)
        if poll_success:
            viewer.read_data_from_socket(channel.socket.recv_multipart(), image_queue)
        else:
            print("No reply received within timeout period")

def display_latest_image(image_queue):
    while True:
        if not image_queue.empty():
            frame = image_queue.get()
            img = cv2.imdecode(frame, cv2.IMREAD_COLOR)
            cv2.imshow('Live Image', img)
            if cv2.waitKey(1) & 0xFF == ord('q'):
                break
        time.sleep(DISPLAY_INTERVAL)

if __name__ == '__main__':
    logging.basicConfig(level=logging.DEBUG if LOG_LEVEL > 0 else logging.INFO)

    config_pipe_parent, config_pipe_child = Pipe()

    viewer = LiveDataViewer('tcp://192.168.0.31:5020', config_pipe=config_pipe_child)
    image_queue = Queue(maxsize=1)

    gui_process = Process(target=create_gui, args=(config_pipe_parent,))
    gui_process.start()

    capture_process = Process(target=capture_images, args=(viewer, image_queue))
    capture_process.start()


        # Display process
    try:
        display_latest_image(image_queue)
    finally:
        capture_process.terminate()
        capture_process.join()
        gui_process.terminate()
        gui_process.join()
        cv2.destroyAllWindows()