let io = null;

function init(server) {
  const { Server } = require('socket.io');
  io = new Server(server, { cors: { origin: '*' } });

  io.on('connection', (socket) => {
    console.log('Socket client connected:', socket.id);
    socket.on('disconnect', () => console.log('Socket disconnected:', socket.id));
  });

  return io;
}

function broadcast(event, data) {
  if (!io) return;
  io.emit(event, data);
}

module.exports = { init, broadcast };
