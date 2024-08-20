import sys
import zmq
import json
import numpy as np
import cv2

def main(socket_address):
    # Set up the ZeroMQ context and socket
    context = zmq.Context()
    socket = context.socket(zmq.SUB)

    # Connect to the LiveViewPlugin socket
    socket.connect(socket_address)

    # Subscribe to all messages
    socket.setsockopt_string(zmq.SUBSCRIBE, "")

    while True:
        # Receive the message header
        header = socket.recv_string()
        
        # Parse the JSON header
        header_data = json.loads(header)
        
        # Extract relevant information from the header
        frame_num = header_data["frame_num"]
        dtype = header_data["dtype"]
        shape = tuple(int(dim) for dim in header_data["shape"])
        
        # Receive the image data
        image_data = socket.recv()
        
        # Convert the received data to a NumPy array
        image = np.frombuffer(image_data, dtype=dtype).reshape(shape)
        
        # Display the image
        cv2.imshow(f"Live View - LiveX", image)
        
        # Wait for a key press (1 millisecond delay)
        key = cv2.waitKey(1)
        
        # Check if the 'q' key was pressed to quit the program
        if key == ord('q'):
            break

    # Clean up
    cv2.destroyAllWindows()
    socket.close()
    context.term()

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python live_view_client.py <socket_address>")
        sys.exit(1)

    socket_address = sys.argv[1]
    main(socket_address)
