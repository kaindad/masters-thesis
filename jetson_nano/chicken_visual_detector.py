import cv2
import roboflow
from roboflow import Roboflow
import paho.mqtt.client as mqtt
import json
import time
from google.cloud import storage

# Roboflow setup
API_KEY = "YOUR_API_KEY"
PROJECT_ID = "YOUR_PROJECT_ID"
VERSION = "YOUR_VERSION"
rf = Roboflow(api_key=API_KEY, project_id=PROJECT_ID, version=VERSION)

# MQTT setup
broker_address = "YOUR_BROKER_ADDRESS"
client = mqtt.Client("P1")
client.connect(broker_address)

# Subscribe to activation channel


def on_message(client, userdata, message):
    global activate
    activate = True


client.on_message = on_message
client.subscribe("event/sneeze_sounds")
client.loop_start()

# Capture video from CSI camera
cap = cv2.VideoCapture("nvarguscamerasrc ! video/x-raw(memory:NVMM), width=(int)1280, height=(int)720, format=(string)NV12, framerate=(fraction)30/1 ! nvvidconv ! video/x-raw, format=(string)BGRx ! videoconvert ! video/x-raw, format=(string)BGR ! appsink", cv2.CAP_GSTREAMER)

# Define the codec and create VideoWriter object
fourcc = cv2.VideoWriter_fourcc(*'XVID')
out = cv2.VideoWriter('output.avi', fourcc, 20.0, (640, 480))

activate = False
start_time = None

while True:
    ret, frame = cap.read()
    if not ret:
        break

    if activate:
        if start_time is None:
            start_time = time.time()

        # Inference
        results = rf.infer(frame)

        # Prepare results in specified JSON format
        predictions = []
        for result in results:
            predictions.append({
                "class": result['class'],
                "confidence": result['confidence'],
                "bbox": {
                    "x": result['bbox'][0],
                    "y": result['bbox'][1],
                    "width": result['bbox'][2] - result['bbox'][0],
                    "height": result['bbox'][3] - result['bbox'][1]
                },
                "color": "#4892EA"
            })
        json_results = json.dumps({"predictions": predictions})

        # Send results to MQTT broker
        client.publish("event/sneeze_videos", json_results)

        print("Results sent to MQTT broker")

        # Write the frame
        out.write(frame)

        # Stop recording after 30 seconds
        if time.time() - start_time > 30:
            break

    cv2.imshow('Frame', frame)

    # Press 'q' to exit
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
out.release()
cv2.destroyAllWindows()
client.loop_stop()

# Send video to GCP cloud


def upload_blob(bucket_name, source_file_name, destination_blob_name):
    """Uploads a file to the bucket."""
    storage_client = storage.Client()
    bucket = storage_client.bucket(bucket_name)
    blob = bucket.blob(destination_blob_name)

    blob.upload_from_filename(source_file_name)

    print("File {} uploaded to {}.".format(
        source_file_name, destination_blob_name))


bucket_name = "YOUR_BUCKET_NAME"
source_file_name = "output.avi"
destination_blob_name = "output.avi"

upload_blob(bucket_name, source_file_name, destination_blob_name)
