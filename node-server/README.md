NutriCycle Node Gateway

This Node.js gateway acts as the public entry point for ESP32 devices and the frontend. It forwards image payloads to the local Python AI `/detect` endpoint and broadcasts detection JSON to connected frontend clients via Socket.IO.

Quick start:

1. Copy `.env.example` to `.env` and adjust values
2. npm install
3. npm run dev (or npm start)

Endpoints:
- POST /upload -> Accepts an image (multipart/form-data `image`, raw image bytes, or JSON with base64 `image`), forwards to Python, returns detection JSON. Use `X-API-KEY` header if `API_KEY` set.
- POST /detect -> Proxy to Python `/detect` and optionally broadcast detections to clients.
- GET  /video_feed -> Proxy to Python `/video_feed` (MJPEG stream)
- GET  /health -> Basic health check

Example cURL (raw bytes):

curl -X POST http://localhost:3000/upload --data-binary @frame.jpg -H "Content-Type: application/octet-stream" -H "X-API-KEY: changeme"

Example cURL (multipart):

curl -X POST http://localhost:3000/upload -F "image=@frame.jpg" -H "X-API-KEY: changeme"

Socket.IO: clients can connect to the gateway socket and listen for `detections` events:

// Browser sample (frontend)
const socket = io('http://localhost:3000');
socket.on('detections', data => console.log('Detections', data));

Video (MJPEG): open `http://localhost:3000/video_feed` in your browser to view the proxied MJPEG stream from the private Python server.

Security notes:
- Set a strong `API_KEY` before exposing the gateway.
- Keep `PYTHON_DETECT_URL` pointing at `localhost` so Python is not exposed publicly.
- Use `MAX_BODY_SIZE` and rate limiting to protect from abuse.