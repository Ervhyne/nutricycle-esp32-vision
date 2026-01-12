let io = null;

function init(server) {
  const { Server } = require('socket.io');
  io = new Server(server, { cors: { origin: '*' } });

  io.on('connection', (socket) => {
    console.log('Socket client connected:', socket.id);
    socket.on('disconnect', () => console.log('Socket disconnected:', socket.id));

    // Allow clients to join rooms if needed in future
    socket.on('subscribe', (room) => {
      socket.join(room);
      console.log(`Socket ${socket.id} subscribed to ${room}`);
    });
    socket.on('unsubscribe', (room) => {
      socket.leave(room);
      console.log(`Socket ${socket.id} unsubscribed from ${room}`);
    });
  });

  return io;
}

function broadcast(event, data) {
  if (!io) return;
  io.emit(event, data);
}

function broadcastFrame(frameId, buf) {
  if (!io) return;
  // Emit binary frame along with metadata header
  io.emit('frame', { frameId: frameId }, buf);
}

function broadcastMeta(frameId, meta) {
  if (!io) return;
  io.emit('meta', Object.assign({ frameId: frameId }, meta || {}));
}

module.exports = { init, broadcast, broadcastFrame, broadcastMeta };
