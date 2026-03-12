__author__ = 'dgarrido'

# Ejemplo de dispositivo recibiendo desiredproperties de dispositivo gemelo
# y estableciendo reported properties
#
# al mismo tiempo que se mandan datos de telemetria

IOTHUB_DEVICE_CONNECTION_STRING='HostName=YOURHUB.azure-devices.net;DeviceId=YOURDEVICEID;SharedAccessKey=***'

import datetime
import random
import time
import json
from azure.iot.device import IoTHubDeviceClient, Message

period = 10
device_client = IoTHubDeviceClient.create_from_connection_string(IOTHUB_DEVICE_CONNECTION_STRING)

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

#"desired": {
#    "telemetryConfig": {
#        "sendFrequency": "5m"
#    },
#    ...
#},
# define behavior for receiving a twin patch
# Los parches se reciben siempre. Usar "success" en la propiedad reported no tiene ningún efecto
# Este metodo es invocado aunque no haya propiedades tipo desired
def twin_patch_handler(patch):
    print("the data in the desired properties patch was: {}".format(patch))
    config_setting = patch["telemetryConfig"]["sendFrequency"]
    print(config_setting)
    reported_properties = {"telemetryConfig": { "sendFrequency": "5m" ,
                            "status": "success"}}
    print("Setting reported property to {}".format(reported_properties["telemetryConfig"]))
    device_client.patch_twin_reported_properties(reported_properties)
        


def main():
    # The connection string for a device should never be stored in code. For the sake of simplicity we're using an environment variable here.
    # The client object is used to interact with your Azure IoT hub.
    #device_client = IoTHubDeviceClient.create_from_connection_string(IOTHUB_DEVICE_CONNECTION_STRING)

    # set the twin patch handler on the client
    device_client.on_twin_desired_properties_patch_received = twin_patch_handler

    # Connect the client.
    device_client.connect()

    # set the message handler on the client
    device_client.on_message_received = message_handler

    i=0
    while True:
        data=dict()
        data['temperature']=random.randint(25,30)
        data['humidity']=random.randint(50,100)
        data['pressure']=random.randint(900,1100)
        data['when']=datetime.datetime.now()
        json_data=json.dumps(data,default=str)

        message = Message(json_data)
        message.content_encoding = "utf-8"
        message.content_type = "application/json"
        print(str(data['temperature'])+' '+str(data['humidity'])+' '+str(data['pressure']))

        device_client.send_message(message)

        # send new reported properties
        #reported_properties = {"temperature": data['temperature'] }
        #print("Setting reported temperature to {}".format(reported_properties["temperature"]))
        #device_client.patch_twin_reported_properties(reported_properties)
        time.sleep(period)

if __name__ == '__main__':
    main()
