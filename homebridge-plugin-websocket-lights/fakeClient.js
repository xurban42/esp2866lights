const WebSocket = require('ws');
 
const ws = new WebSocket('ws://lights.local:81');
 
ws.on('open', function open() {
  ws.send('(update)');
});
 
ws.on('message', function incoming(data) {
  console.log('Recieved:', JSON.parse(data));
});