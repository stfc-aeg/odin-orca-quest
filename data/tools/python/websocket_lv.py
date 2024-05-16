import asyncio
import websockets
import json
import cv2
import numpy as np
import time

async def receive_images():
    async with websockets.connect('ws://localhost:9002') as websocket:
        i = 1
        while True:
            time.sleep(1)
            try:
                if i <= 10:
                    # Send a JSON request for a single image
                    request = {
                        'width': 4096,
                        'height': 2304,
                        'color_effect': 'plasma'
                    }
                elif i > 10 and i <= 20:
                    # Send a JSON request for a single image
                    request = {
                        'width': 2048,
                        'height': 1152,
                        'color_effect': 'plasma'
                    }
                elif i > 20 and i <= 30:
                    # Send a JSON request for a single image
                   request = {
                        'width': 1024,
                        'height': 576,
                        'color_effect': 'plasma'
                    }
                elif i > 30 and i <= 40:
                    # Send a JSON request for a single image
                    request = {
                        'width': 512,
                        'height': 288,
                        'color_effect': 'plasma'
                    }
                else:
                    # Send a JSON request for a single image
                    request = {
                        'width': 640,
                        'height': 480,
                        'color_effect': 'plasma'
                    }

                await websocket.send(json.dumps(request))
                print(f"{i} Sent JSON request: {request}")

                # Receive the image data
                data = await websocket.recv()
                print("Received image data.")

                nparr = np.frombuffer(data, np.uint8)
                image = cv2.imdecode(nparr, cv2.IMREAD_ANYCOLOR)

                # Save the received image
                cv2.imwrite(f'test_image_{i}.jpg', image)

                # Display the image
                cv2.imshow('Received Image', image)
                print("Displaying received image.")

                if cv2.waitKey(1) & 0xFF == ord('q'):
                    break

                i += 1

            except websockets.exceptions.ConnectionClosed:
                print("WebSocket connection closed by the server.")
                break
            except Exception as e:
                print(f"An error occurred: {e}")
                break

    print("WebSocket connection closed.")

asyncio.get_event_loop().run_until_complete(receive_images())