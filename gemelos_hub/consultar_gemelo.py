from azure.iot.hub import IoTHubRegistryManager

HUB_CONNECTION_STRING='HostName=YOURHUB.azure-devices.net;SharedAccessKeyName=iothubowner;SharedAccessKey=***'
DEVICE_ID='YOURDEVICEID'

# Crea una instancia del administrador del registro IoT Hub
registry_manager = IoTHubRegistryManager(HUB_CONNECTION_STRING)

# Consulta el gemelo del dispositivo
try:
    device_twin = registry_manager.get_twin(DEVICE_ID)
    print("Device Twin: {}".format(device_twin))
    print(device_twin.properties.desired["telemetryConfig"]["sendFrequency"])
except Exception as e:
    print("Error fetching device twin: {}".format(e))