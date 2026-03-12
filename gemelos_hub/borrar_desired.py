__author__ = 'dgarrido'

from azure.iot.hub import IoTHubRegistryManager

HUB_CONNECTION_STRING='HostName=YOURHUB.azure-devices.net;SharedAccessKeyName=iothubowner;SharedAccessKey=***'
DEVICE_ID='YOURDEVICEID'

# Crea una instancia del administrador del registro IoT Hub
registry_manager = IoTHubRegistryManager(HUB_CONNECTION_STRING)

device_twin = registry_manager.get_twin(DEVICE_ID)

desired_properties = {
    "desired": {
        "telemetryConfig": None
    }
}

twin_patch = {
    "properties": desired_properties
}

registry_manager.update_twin(DEVICE_ID, twin_patch)
print(f"Desired property for device {device_twin} has been set.")
