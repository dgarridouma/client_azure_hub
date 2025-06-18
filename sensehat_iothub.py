__author__ = 'dgarrido'

IOTHUB_DEVICE_CONNECTION_STRING='[PUT YOUR CREDENTIALS HERE]'

import datetime
import random
import time
import json
from azure.iot.device import IoTHubDeviceClient, Message

from sense_emu import SenseHat
from threading import Lock

sense = SenseHat() 
period = 10
blue = (0, 0, 255)
yellow = (255, 255, 0)
lock = Lock()


# define behavior for receiving a message
#{
#  "period": 1,
#  "message": "period changed"
#}
def message_handler(message):
    global period
    dict_command=json.loads(message.data)
    period = int(dict_command['period'])
    print(dict_command['message'])
    lock.acquire()
    sense.show_message(dict_command['message'], text_colour=yellow, back_colour=blue, scroll_speed=0.05)
    lock.release()



def main():
    # The connection string for a device should never be stored in code. For the sake of simplicity we're using an environment variable here.
    # The client object is used to interact with your Azure IoT hub.
    device_client = IoTHubDeviceClient.create_from_connection_string(IOTHUB_DEVICE_CONNECTION_STRING)

    # Connect the client.
    device_client.connect()

    # set the message handler on the client
    device_client.on_message_received = message_handler

    i=0
    while True:
        data=dict()
        data['temperature']=sense.temperature
        data['humidity']=sense.humidity
        data['pressure']=sense.pressure
        data['when']=datetime.datetime.now()
        json_data=json.dumps(data,default=str)

        message = Message(json_data)
        message.content_encoding = "utf-8"
        message.content_type = "application/json"

        lock.acquire()
        sense.show_message("{:.2f}".format(data['temperature'])+' '+"{:.2f}".format(data['humidity'])+' '+"{:.2f}".format(data['pressure']), text_colour=yellow, back_colour=blue, scroll_speed=0.05)
        lock.release()

        print(str(data['temperature'])+' '+str(data['humidity'])+' '+str(data['pressure']))

        device_client.send_message(message)
        time.sleep(period)

if __name__ == '__main__':
    main()
