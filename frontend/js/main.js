// NutriCycle Frontend - Main JavaScript
// Handles video streaming and statistics updates

const API_BASE_URL = 'http://localhost:3000';
let isDetectionActive = false;
let statsUpdateInterval = null;
let streamStatusInterval = null;
let batchHistoryInterval = null;
let isBatchActive = false;

let overlayCanvas = null;
let overlayCtx = null;
let socket = null; // Socket.IO client instance (realtime detections)
// Theme management
const THEME_STORAGE_KEY = 'nutricycle-theme';

function applyTheme(theme) {
    const root = document.documentElement;
    root.setAttribute('data-theme', theme);
    try {
        localStorage.setItem(THEME_STORAGE_KEY, theme);
    } catch (e) {
        // Ignore storage errors (e.g. private mode)
    }

    const toggle = document.getElementById('themeToggle');
    if (toggle) {
        const label = toggle.querySelector('.theme-toggle-label');
        const icon = toggle.querySelector('.theme-toggle-icon');
        if (label) {
            label.textContent = theme === 'dark' ? 'Light mode' : 'Dark mode';
        }
        if (icon) {
            icon.textContent = theme === 'dark' ? '☀️' : '🌙';
        }
    }
}

function initTheme() {
    let theme = 'light';
    try {
        const stored = localStorage.getItem(THEME_STORAGE_KEY);
        if (stored === 'light' || stored === 'dark') {
            theme = stored;
        } else if (window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches) {
            theme = 'dark';
        }
    } catch (e) {
        // Fallback to default theme
    }

    applyTheme(theme);

    const toggle = document.getElementById('themeToggle');
    if (toggle) {
        toggle.addEventListener('click', () => {
            const current = document.documentElement.getAttribute('data-theme') || 'light';
            const next = current === 'light' ? 'dark' : 'light';
            applyTheme(next);
        });
    }
}

// Initialize on page load
document.addEventListener('DOMContentLoaded', function() {
    initTheme();
    console.log('NutriCycle Contamination Monitoring Dashboard loaded');
    checkServerStatus();
    startBatchHistoryUpdate();
});

// Check if backend server is running
async function checkServerStatus() {
    try {
        const response = await fetch(`${API_BASE_URL}/`);
        const data = await response.json();
        console.log('Server status:', data);
        
        if (data.detection_active) {
            updateUIState(true);
            startVideoStream();
            startStatsUpdate();
        }
    } catch (error) {
        console.error('Cannot connect to backend server:', error);
        alert('Backend server is not running. Please start the Flask server first.');
    }
}

// Start detection
async function startDetection() {
    try {
        const response = await fetch(`${API_BASE_URL}/start_detection`, {
            method: 'POST'
        });
        const data = await response.json();
        
        if (data.status === 'started' || data.status === 'already_running') {
            updateUIState(true);
            startVideoStream();
            startStatsUpdate();
            console.log('Detection started');
        }
    } catch (error) {
        console.error('Error starting detection:', error);
        alert('Failed to start detection. Check console for details.');
    }
}

// Stop detection
async function stopDetection() {
    try {
        const response = await fetch(`${API_BASE_URL}/stop_detection`, {
            method: 'POST'
        });
        const data = await response.json();
        
        if (data.status === 'stopped') {
            updateUIState(false);
            stopVideoStream();
            stopStatsUpdate();
            console.log('Detection stopped');
        }
    } catch (error) {
        console.error('Error stopping detection:', error);
    }
}

// Reset statistics
async function resetStats() {
    try {
        const response = await fetch(`${API_BASE_URL}/reset_statistics`, {
            method: 'POST'
        });
        const data = await response.json();
        
        if (data.status === 'reset') {
            updateStatistics();
            console.log('Statistics reset');
        }
    } catch (error) {
        console.error('Error resetting statistics:', error);
    }
}

// Start video stream
// Uses Node gateway `/video_feed` proxy to fetch MJPEG stream from the private Python server
function startVideoStream() {
    const videoFeed = document.getElementById('videoFeed');
    videoFeed.src = `${API_BASE_URL}/video_feed?t=${Date.now()}`;
    videoFeed.onerror = function() {
        console.error('Error loading video stream');
    };

    // Initialize overlay canvas for bounding boxes
    initOverlay();
    connectSocket();
}

// Stop video stream
function stopVideoStream() {
    const videoFeed = document.getElementById('videoFeed');
    videoFeed.src = 'about:blank';

    disconnectSocket();
    if (overlayCtx && overlayCanvas) overlayCtx.clearRect(0, 0, overlayCanvas.width, overlayCanvas.height);
}

// Initialize overlay canvas and event handlers
function initOverlay() {
    overlayCanvas = document.getElementById('overlayCanvas');
    overlayCtx = overlayCanvas ? overlayCanvas.getContext('2d') : null;
    const videoFeed = document.getElementById('videoFeed');

    function updateOverlaySize() {
        if (!overlayCanvas || !videoFeed) return;
        // Use client size for drawing (matches displayed element)
        overlayCanvas.width = videoFeed.clientWidth;
        overlayCanvas.height = videoFeed.clientHeight;
    }

    // Update size on load and resize
    window.addEventListener('resize', updateOverlaySize);
    videoFeed.addEventListener('load', updateOverlaySize);
    updateOverlaySize();
}

// (Removed) Detection polling via repeated /detect uploads. The frontend now relies on realtime `detections` events broadcast by the Node gateway via Socket.IO.

// Draw detections onto the overlay canvas (scales from source image to displayed canvas)
function drawDetections(detections) {
    if (!overlayCanvas || !overlayCtx) return;

    // Clear overlay
    overlayCtx.clearRect(0, 0, overlayCanvas.width, overlayCanvas.height);

    // Determine source image size from video element
    const videoFeed = document.getElementById('videoFeed');
    const srcWidth = videoFeed && (videoFeed.naturalWidth || videoFeed.videoWidth) ? (videoFeed.naturalWidth || videoFeed.videoWidth) : overlayCanvas.width;
    const srcHeight = videoFeed && (videoFeed.naturalHeight || videoFeed.videoHeight) ? (videoFeed.naturalHeight || videoFeed.videoHeight) : overlayCanvas.height;

    // Avoid division by zero
    const scaleX = srcWidth ? (overlayCanvas.width / srcWidth) : 1;
    const scaleY = srcHeight ? (overlayCanvas.height / srcHeight) : 1;

    // Color map for waste types
    const colorMap = {
        'leafy_vegetables': '#16a34a',
        'plastic': '#dc2626',
        'metal': '#ea580c',
        'paper': '#eab308',
        'unknown': '#6b7280'
    };

    detections.forEach(d => {
        const bbox = d.bbox || [0,0,0,0];
        const x1 = Math.round(bbox[0] * scaleX);
        const y1 = Math.round(bbox[1] * scaleY);
        const x2 = Math.round(bbox[2] * scaleX);
        const y2 = Math.round(bbox[3] * scaleY);
        const w = x2 - x1;
        const h = y2 - y1;
        const color = colorMap[d.type] || '#ffffff';

        // Draw box
        overlayCtx.strokeStyle = color;
        overlayCtx.lineWidth = Math.max(2, Math.round(overlayCanvas.width / 200));
        overlayCtx.strokeRect(x1, y1, w, h);

        // Draw label background
        const label = `${d.original_class} ${(d.confidence * 100).toFixed(0)}%`;
        overlayCtx.font = `${Math.max(12, Math.round(overlayCanvas.width / 60))}px Arial`;
        const textWidth = overlayCtx.measureText(label).width + 8;
        const textHeight = Math.max(14, Math.round(overlayCanvas.width / 80));
        overlayCtx.fillStyle = color;
        overlayCtx.fillRect(x1, y1 - textHeight - 6, textWidth, textHeight + 4);

        // Text
        overlayCtx.fillStyle = '#ffffff';
        overlayCtx.fillText(label, x1 + 4, y1 - 6);
    });
}

// Socket.IO helpers
function connectSocket() {
    if (socket && socket.connected) return;
    try {
        socket = io(API_BASE_URL);
        socket.on('connect', () => console.log('Socket connected:', socket.id));
        socket.on('detections', (data) => {
            const detections = data && data.detections ? data.detections : (Array.isArray(data) ? data : []);
            drawDetections(detections);
        });
        socket.on('disconnect', () => console.log('Socket disconnected'));
    } catch (err) {
        console.error('Socket init failed', err);
    }
}

function disconnectSocket() {
    if (!socket) return;
    try {
        socket.disconnect();
    } catch (err) {
        console.warn('Socket disconnect error', err);
    }
    socket = null;
}

// Stop video stream
function stopVideoStream() {
    const videoFeed = document.getElementById('videoFeed');
    videoFeed.src = 'about:blank';
}

// Update statistics from backend
async function updateStatistics() {
    try {
        const response = await fetch(`${API_BASE_URL}/statistics`);
        const data = await response.json();
        
        // Update stat counters
        document.getElementById('stat-leafy').textContent = data.leafy_vegetables || 0;
        document.getElementById('stat-plastic').textContent = data.plastic || 0;
        document.getElementById('stat-metal').textContent = data.metal || 0;
        document.getElementById('stat-paper').textContent = data.paper || 0;
        document.getElementById('stat-unknown').textContent = data.unknown || 0;
        
    } catch (error) {
        console.error('Error updating statistics:', error);
    }
}

// Start periodic statistics update
function startStatsUpdate() {
    if (statsUpdateInterval) {
        clearInterval(statsUpdateInterval);
    }
    
    // Update stats every 1 second for real-time monitoring
    statsUpdateInterval = setInterval(updateStatistics, 1000);
    updateStatistics(); // Update immediately
    
    // Also start stream status updates
    startStreamStatusUpdate();
}

// Stop periodic statistics update
function stopStatsUpdate() {
    if (statsUpdateInterval) {
        clearInterval(statsUpdateInterval);
        statsUpdateInterval = null;
    }
    
    stopStreamStatusUpdate();
}

// Update stream status (connection, FPS, latency)
async function updateStreamStatus() {
    try {
        const response = await fetch(`${API_BASE_URL}/stream_status`);
        const data = await response.json();
        
        // Update network status display
        const networkStatus = document.getElementById('networkStatus');
        document.getElementById('statusConnection').textContent = data.connected ? 'Connected' : 'Disconnected';
        document.getElementById('statusFps').textContent = data.fps || 0;
        document.getElementById('statusLatency').textContent = data.latency || 0;
        document.getElementById('statusQuality').textContent = data.quality || 'N/A';
        
        // Change color based on connection status
        if (!data.connected) {
            networkStatus.classList.add('warning');
        } else {
            networkStatus.classList.remove('warning');
        }
        
        // Show/hide contamination alert
        const alertBanner = document.getElementById('alertBanner');
        if (data.contamination_detected) {
            alertBanner.classList.add('visible');
        } else {
            alertBanner.classList.remove('visible');
        }
        
        // Show quality degradation warning if applicable
        if (data.quality && data.quality !== 'UXGA' && data.quality !== 'N/A') {
            console.warn(`Stream quality reduced to ${data.quality} due to network latency`);
        }
        
    } catch (error) {
        console.error('Error updating stream status:', error);
    }
}

// Start stream status update interval
function startStreamStatusUpdate() {
    if (streamStatusInterval) {
        clearInterval(streamStatusInterval);
    }
    
    streamStatusInterval = setInterval(updateStreamStatus, 1000);
    updateStreamStatus(); // Update immediately
}

// Stop stream status update interval
function stopStreamStatusUpdate() {
    if (streamStatusInterval) {
        clearInterval(streamStatusInterval);
        streamStatusInterval = null;
    }
}

// Update UI state
function updateUIState(active) {
    isDetectionActive = active;
    
    const statusBadge = document.getElementById('statusBadge');
    const systemStatus = document.getElementById('systemStatus');
    
    if (active) {
        statusBadge.textContent = 'Active';
        statusBadge.className = 'status-badge active';
        systemStatus.textContent = 'Running';
    } else {
        statusBadge.textContent = 'Inactive';
        statusBadge.className = 'status-badge inactive';
        systemStatus.textContent = 'Stopped';
    }
}

// Batch control functions
async function startBatch() {
    try {
        const response = await fetch(`${API_BASE_URL}/start_batch`, {
            method: 'POST'
        });
        const data = await response.json();
        
        if (data.status === 'started' || data.status === 'already_active') {
            isBatchActive = true;
            document.getElementById('btnStartBatch').disabled = true;
            document.getElementById('btnEndBatch').disabled = false;
            console.log('Batch started:', data.start_time);
        }
    } catch (error) {
        console.error('Error starting batch:', error);
        alert('Failed to start batch. Check console for details.');
    }
}

async function endBatch() {
    try {
        const response = await fetch(`${API_BASE_URL}/end_batch`, {
            method: 'POST'
        });
        const data = await response.json();
        
        if (data.status === 'ended') {
            isBatchActive = false;
            document.getElementById('btnStartBatch').disabled = false;
            document.getElementById('btnEndBatch').disabled = true;
            console.log('Batch ended:', data.summary);
            
            // Refresh batch history
            updateBatchHistory();
        }
    } catch (error) {
        console.error('Error ending batch:', error);
    }
}

// Update batch history display
async function updateBatchHistory() {
    try {
        const response = await fetch(`${API_BASE_URL}/batch_history`);
        const data = await response.json();
        
        const contentDiv = document.getElementById('batchHistoryContent');
        
        if (data.batches && data.batches.length > 0) {
            // Create table
            let tableHTML = `
                <table class="batch-table">
                    <thead>
                        <tr>
                            <th>Batch ID</th>
                            <th>Start Time</th>
                            <th>Duration (s)</th>
                            <th>Items Detected</th>
                            <th>Types</th>
                        </tr>
                    </thead>
                    <tbody>
            `;
            
            // Add rows (reverse to show newest first)
            data.batches.slice().reverse().forEach(batch => {
                const startTime = new Date(batch.start_time).toLocaleTimeString();
                const types = Object.keys(batch.contamination_types).join(', ') || 'None';
                
                tableHTML += `
                    <tr>
                        <td>#${batch.batch_id}</td>
                        <td>${startTime}</td>
                        <td>${batch.duration}</td>
                        <td>${batch.total_items}</td>
                        <td>${types}</td>
                    </tr>
                `;
            });
            
            tableHTML += '</tbody></table>';
            
            // Show active batch info if applicable
            if (data.active_batch) {
                tableHTML = `<div style="background: #fff3cd; padding: 10px; border-radius: 5px; margin-bottom: 10px;">
                    <strong>📦 Active Batch:</strong> ${data.current_batch_items} items detected so far...
                </div>` + tableHTML;
            }
            
            contentDiv.innerHTML = tableHTML;
        } else {
            contentDiv.innerHTML = '<div class="no-batches">No batch records yet. Start a batch to begin tracking.</div>';
        }
    } catch (error) {
        console.error('Error updating batch history:', error);
    }
}

// Start batch history periodic update
function startBatchHistoryUpdate() {
    if (batchHistoryInterval) {
        clearInterval(batchHistoryInterval);
    }
    
    batchHistoryInterval = setInterval(updateBatchHistory, 5000); // Update every 5 seconds
    updateBatchHistory(); // Update immediately
}

// Make functions globally available
window.startDetection = startDetection;
window.stopDetection = stopDetection;
window.resetStats = resetStats;
window.startBatch = startBatch;
window.endBatch = endBatch;
