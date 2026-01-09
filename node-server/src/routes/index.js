const express = require('express');
const router = express.Router();
const multer = require('multer');
const upload = multer({ storage: multer.memoryStorage() });

const { uploadLimiter } = require('../middleware/limiter');
const requireApiKey = require('../middleware/auth');
const detectController = require('../controllers/detectController');
const videoController = require('../controllers/videoController');

router.get('/health', (req, res) => res.json({ status: 'ok', gateway: true }));

// Video feed proxy (MJPEG) - Node will proxy `/video_feed` to the local Python server
router.get('/video_feed', videoController.proxyVideoFeed);

// ESP32 uploads (multipart or raw bytes)
router.post('/upload', requireApiKey, uploadLimiter, upload.single('image'), detectController.upload);

// Detect proxy
router.post('/detect', requireApiKey, upload.single('image'), detectController.detect);

module.exports = router;
