config:

```{
    "bridge": {
        "name": "DemoMinimalisticHttpBlinds",
        "username": "AA:BB:CC:DD:EE:FF",
        "port": 51826,
        "pin": "123-45-678"
    },
  
    "description": "DEV NODEJS MACBOOK",
  
    "accessories": [
        {
            "name": "Kitchen Lights",
            "accessory": "MinimalisticWebSocketLights",
            "ip": "lights.local",
            "pin": "d2"
        },
        {
            "name": "Bedroom Lights",
            "accessory": "MinimalisticWebSocketLights",
            "ip": "lights.local",
            "pin": "d3"
        }
    ],
  
    "platforms": []
}```