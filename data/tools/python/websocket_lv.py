import asyncio
import websockets
import json
import cv2
import numpy as np
import time

async def receive_images():
    async with websockets.connect('ws://localhost:9002') as websocket:
        while True:
            time.sleep(1)
            try:
                # Send a JSON request for a single image
                request = {
                    'width': 4096,
                    'height': 2304,
                    'color_effect': 'grayscale'
                }
                await websocket.send(json.dumps(request))
                print(f"Sent JSON request: {request}")

                # Receive the image data
                data = await websocket.recv()
                print("Received image data.")

                for i in range(16):
                    print(f'pixel {i} value: {data[i]}')

                nparr = np.frombuffer(data, np.uint8)
                image = cv2.imdecode(nparr, cv2.IMREAD_ANYCOLOR)


                cv2.imwrite('test_image.jpg', image)

                # Display the image
                cv2.imshow('Received Image', image)
                print("Displaying received image.")

                if cv2.waitKey(1) & 0xFF == ord('q'):
                    break
            except websockets.exceptions.ConnectionClosed:
                print("WebSocket connection closed by the server.")
                break
            except Exception as e:
                print(f"An error occurred: {e}")
                break

    print("WebSocket connection closed.")

asyncio.get_event_loop().run_until_complete(receive_images())