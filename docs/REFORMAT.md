# 🔧 NutriCycle Project — Comprehensive Refactor & Correction Prompt

## 📌 CONTEXT (READ FIRST)

This project already EXISTS and is FUNCTIONAL but ARCHITECTURALLY WRONG and INEFFICIENT.

❗ This is NOT a greenfield build.
❗ DO NOT recreate from scratch.
❗ DO NOT change technologies unless explicitly instructed.
❗ The task is to **REFACTOR, RESTRUCTURE, and OPTIMIZE** the current implementation.

The project involves:
- ESP32 camera device
- Desktop-hosted AI (Python, YOLO/OpenCV)
- Node.js backend
- Frontend showing live detection with bounding boxes

The system currently works but suffers from:
- Severe performance issues
- Poor separation of concerns
- Incorrect networking assumptions
- Inefficient video & detection handling

Your goal is to FIX these problems while preserving the working parts.

---

## 🚨 CURRENT PROBLEMS (WHAT IS WRONG)

### 1️⃣ Architectural Violations
- AI server is doing **too many responsibilities**:
  - Image processing
  - Bounding box drawing
  - Frontend streaming
- No clear API gateway layer
- ESP32 communicates directly with AI logic
- Frontend depends directly on AI responses

❌ This tightly couples hardware, AI, and UI logic.

---

### 2️⃣ Performance Bottlenecks
- Server-side drawing of bounding boxes on every frame
- Re-encoding annotated images repeatedly
- Large image payloads sent continuously over HTTP/Ngrok
- ESP32 limited bandwidth + CPU being overused
- Ngrok relaying heavy binary image data

❌ Result:
- Slow feed
- Choppy tracking
- High latency
- Poor “CCTV-like” experience

---

### 3️⃣ Incorrect Networking Assumptions
- Assumption that ESP32 and server must be on the same network
- Overexposure of AI server to the internet
- Lack of proper gateway abstraction
- No clear boundary between public and private services

---

### 4️⃣ Poor Scalability & Maintainability
- Hard to extend to:
  - Multiple machines
  - Additional waste classes
  - Analytics & history
- Hard to secure
- Hard to debug
- Hard to optimize

---

## ✅ TARGET ARCHITECTURE (WHAT IS CORRECT)

You MUST refactor toward the following **correct architecture**:

ESP32
↓ HTTP (Ngrok)
Node.js API (Gateway)
↓ localhost HTTP
Python AI Server
↓ JSON metadata
Node.js API
↓ WebSocket / HTTP
Frontend (Canvas Overlay)

yaml
Copy code

### Key Principles
- ESP32 is a **dumb sender** (camera + sensors only)
- Node.js is the **single public entry point**
- Python AI is **private and local**
- Frontend draws bounding boxes **client-side**
- AI returns **metadata, not images**

---

## 🧠 CORRECT RESPONSIBILITIES (NON-NEGOTIABLE)

### ESP32
- Capture image frames
- Send frames via HTTP POST
- No AI logic
- No drawing
- No frontend responsibility

---

### Python AI Server
- Receive images from Node.js (not directly from ESP32)
- Run object detection (YOLO/OpenCV)
- Return:
  - Bounding box coordinates
  - Class labels
  - Confidence scores
- NO:
  - Drawing boxes
  - Serving frontend
  - Internet exposure

---

### Node.js Backend
- Public API exposed via Ngrok
- Authenticates ESP32 & frontend
- Acts as:
  - API gateway
  - Traffic controller
  - Data relay
- Forwards images to Python locally
- Sends detection metadata to frontend
- Stores logs / history (if present)

---

### Frontend
- Receives raw image stream
- Receives detection metadata (JSON)
- Draws bounding boxes using Canvas
- Responsible for visualization ONLY

---

## ⚡ PERFORMANCE CORRECTION REQUIREMENTS

You MUST implement:
- Client-side bounding box rendering
- JSON-based detection payloads
- Reduced image payload size
- Separation of inference and rendering

Expected improvements:
- Lower latency
- Smoother box movement
- CCTV-like tracking
- Reduced CPU usage on server
- Reduced bandwidth over Ngrok

---

## 📦 WHAT MUST BE DONE (TASK LIST)

### Refactor Tasks
- [ ] Decouple AI inference from image rendering
- [ ] Move bounding box drawing to frontend
- [ ] Introduce Node.js as the sole public gateway
- [ ] Restrict Python AI to localhost/private access
- [ ] Redesign request/response payloads to JSON metadata
- [ ] Preserve existing detection logic where possible
- [ ] Preserve existing ESP32 capture logic
- [ ] Refactor, not rewrite, existing modules

---

### Networking Tasks
- [ ] Ensure ESP32 does NOT require same LAN as server
- [ ] Use Ngrok ONLY for Node.js
- [ ] Ensure Python AI is never exposed publicly
- [ ] Use localhost communication between Node.js and Python

---

### Code Quality Expectations
- Clear module boundaries
- Minimal breaking changes
- Reusable services
- Explicit interfaces between components
- Clean separation of concerns

---

## ❌ WHAT YOU MUST NOT DO

- ❌ Do NOT rebuild the project from scratch
- ❌ Do NOT remove ESP32
- ❌ Do NOT move AI to ESP32
- ❌ Do NOT expose Python AI publicly
- ❌ Do NOT keep server-side box drawing
- ❌ Do NOT tightly couple frontend with AI logic

---

## 🧪 SUCCESS CRITERIA (HOW TO KNOW IT’S FIXED)

The refactor is successful if:

- Bounding boxes move smoothly like CCTV
- Feed latency is significantly reduced
- ESP32 works over any internet connection
- AI server runs privately on desktop
- Node.js cleanly orchestrates communication
- Frontend draws boxes using Canvas
- System is extensible and maintainable

---

## 🧠 FINAL DIRECTIVE

This refactor is about **CORRECTNESS, PERFORMANCE, and ARCHITECTURAL HYGIENE**.

Do not optimize prematurely.
Do not add unnecessary features.
Fix what is wrong.
Preserve what already works.
Follow real-world computer vision system design principles.

Proceed step by step.