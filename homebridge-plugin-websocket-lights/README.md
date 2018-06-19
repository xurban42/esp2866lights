```{
    "bridge": {
        "name": "DemoESP2866WebsocketLights",
        "username": "AA:BB:CC:DD:EE:FF",
        "port": 51826,
        "pin": "123-45-678"
    },
  
    "description": "DEV NODEJS",
  
    "accessories": [
        {
            "name": "Kitchen Lights",
            "accessory": "WebSocketLights",
            "ip": "lights.local",
            "pin": "d2" #esp2866 PWD Pin
        },
        {
            "name": "Bedroom Lights",
            "accessory": "WebSocketLights",
            "ip": "lights.local",
            "pin": "d3" #esp2866 PWD Pin
        }
    ],
  
    "platforms": []
}```