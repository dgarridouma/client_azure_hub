__author__ = 'dgarrido'

IOTHUB_DEVICE_CONNECTION_STRING='HostName=YOURHUB.azure-devices.net;DeviceId=YOURDEVICEID;SharedAccessKey=***'

from azure.iot.device import IoTHubDeviceClient

device_client = IoTHubDeviceClient.create_from_connection_string(IOTHUB_DEVICE_CONNECTION_STRING)

reported_properties = {"telemetryConfig": None}
device_client.patch_twin_reported_properties(reported_properties)

