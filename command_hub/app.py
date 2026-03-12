from flask import Flask
from azure.iot.hub import IoTHubRegistryManager

HUB_CONNECTION_STRING='HostName=YOURHUB.azure-devices.net;SharedAccessKeyName=iothubowner;SharedAccessKey=***'
DEVICE_ID='YOURDEVICE'

app = Flask(__name__)

@app.route('/command')
def send_command():
    """Send a command to a device."""
    # [START iot_send_command]
    print("Sending command to device")

    registry_manager = IoTHubRegistryManager(HUB_CONNECTION_STRING)

    command = '{ "period": 1, "message": "period changed" }'

    data = command.encode("utf-8")

    registry_manager.send_c2d_message(DEVICE_ID, data)

    return 'Command sent to device'
    # [END iot_send_command]

@app.route('/')
def hello():
    """Return a friendly HTTP greeting."""
    return 'Hello World!'

if __name__ == '__main__':
    app.run(host='127.0.0.1', port=8080, debug=True)
