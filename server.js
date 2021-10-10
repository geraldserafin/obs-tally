const Net = require("net");
const port = 8081;
const server = new Net.Server();

const connectedDevices = new Set();
let connectedClient;

function broadcast(data) {
  for (let socket of connectedDevices) {
    socket.socket.write(data);
  }
}

function getActiveDevicesMACAdresses() {
  devices = "";
  if (connectedDevices.size > 0) {
    connectedDevices.forEach((socket) => {
      devices += socket.adress + ";";
    });
  }
  return devices;
}

function showActiveDevices() {
  console.log("Connected Devices:");
  connectedDevices.forEach((socket) => {
    console.log(` - ${socket.adress}`);
  });
}

function handleRequest(request, socket) {
  requestSplit = request.toString().split(":");

  const [op, adress, data] = requestSplit;

  switch (op) {
    case "X":
      socket.setTimeout(6000);
      socket.on("timeout", () => {
        console.log("DEVICE DISCONNECTED");

        connectedDevices.forEach((device) => {
          if (device.socket == socket) {
            connectedDevices.delete(device);
          }
        });
        socket.destroy();

        if (connectedClient) {
          connectedClient.write(getActiveDevicesMACAdresses());
        }
      });

      console.log("New device connected!");
      connectedDevices.add({ socket: socket, adress: adress });

      if (connectedClient) {
        connectedClient.write(getActiveDevicesMACAdresses());
      }

      showActiveDevices();
      break;

    case "OBS":
      connectedClient = socket;
      connectedClient.write(getActiveDevicesMACAdresses());
      console.log("client connected");

      socket.on("close", () => {
        socket.destroy();
        console.log("client disconnected");
      });
      break;

    case "PGM":
      broadcast(request);
      break;
  }
}

server.listen(port, () => {
  console.log(`Server listening for connection requests on port ${port}.`);
});

server.on("connection", (socket) => {
  socket.on("data", (request) => {
    handleRequest(request, socket);
  });

  socket.on("error", (err) => {
    console.log(err);
  });
});
