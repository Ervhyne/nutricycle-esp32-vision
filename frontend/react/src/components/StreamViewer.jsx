import React, { useEffect, useRef, useState } from 'react'
import { io } from 'socket.io-client'

export default function StreamViewer({ streamSrc }) {
  const imgRef = useRef(null)
  const canvasRef = useRef(null)
  const socketRef = useRef(null)
  const clearTimerRef = useRef(null)
  const [msg, setMsg] = useState('')

  // Update image src when streamSrc prop changes
  useEffect(() => {
    if (!imgRef.current) return
    // Ensure we add a timestamp to bust caches
    const src = streamSrc.includes('&_t=') || streamSrc.includes('?_t=') ? streamSrc : streamSrc + `&_t=${Date.now()}`
    imgRef.current.src = src
    setMsg(`Streaming: ${streamSrc}`)
  }, [streamSrc])

  // Heartbeat refresh to keep MJPEG alive in some browsers
  useEffect(() => {
    const id = setInterval(() => {
      if (!imgRef.current) return
      const base = imgRef.current.src.split('&_t=')[0]
      imgRef.current.src = base + `&_t=${Date.now()}`
    }, 20000)
    return () => clearInterval(id)
  }, [])

  // Setup Socket.IO connection for detection events
  useEffect(() => {
    // Use Vite env var or fallback to localhost:3000
    const serverUrl = import.meta.env.VITE_API_URL || 'http://localhost:3000'
    socketRef.current = io(serverUrl)

    socketRef.current.on('connect', () => console.log('[StreamViewer] socket connected', socketRef.current.id))

    socketRef.current.on('detections', (data) => {
      // data: { detections: [...], image_width, image_height }
      drawDetections(data)
    })

    // Show live frames via socket if available (binary frames with metadata)
    socketRef.current.on('frame', (meta, buf) => {
      try {
        // Determine device filter from streamSrc query (if provided)
        const q = streamSrc && streamSrc.includes('?') ? streamSrc.slice(streamSrc.indexOf('?') + 1) : '';
        const params = new URLSearchParams(q);
        const targetDevice = params.get('device_id') || params.get('device') || null;
        if (meta && meta.deviceId && targetDevice && meta.deviceId !== targetDevice) return;

        // Buffer may arrive as ArrayBuffer or Buffer-like; convert to Uint8Array
        const bytes = new Uint8Array(buf instanceof ArrayBuffer ? buf : buf.buffer ? buf.buffer : buf);
        // Convert to base64 in chunks to avoid stack limits
        let binary = '';
        const chunk = 0x8000;
        for (let i=0; i<bytes.length; i+=chunk) {
          binary += String.fromCharCode.apply(null, Array.from(bytes.subarray(i, i+chunk)));
        }
        const b64 = btoa(binary);
        if (imgRef.current) {
          imgRef.current.src = 'data:image/jpeg;base64,' + b64;
          setMsg(meta && meta.deviceId ? `Live: ${meta.deviceId}` : 'Live');
        }
      } catch (e) {
        console.warn('[StreamViewer] frame handling failed', e);
      }
    })

    return () => {
      try { socketRef.current.disconnect() } catch (e) {}
    }
  }, [])

  // Draw detections to canvas overlay
  function drawDetections(data) {
    if (!canvasRef.current || !imgRef.current) return
    const canvas = canvasRef.current
    const ctx = canvas.getContext('2d')
    const rect = imgRef.current.getBoundingClientRect()

    // Use dimensions from the detection payload when available, otherwise fallback to image natural size
    const imgW = data && data.image_width ? data.image_width : (imgRef.current.naturalWidth || rect.width)
    const imgH = data && data.image_height ? data.image_height : (imgRef.current.naturalHeight || rect.height)

    // Set canvas size to rendered image size
    canvas.width = Math.floor(rect.width)
    canvas.height = Math.floor(rect.height)
    canvas.style.width = `${rect.width}px`
    canvas.style.height = `${rect.height}px`

    // Clear and draw transparent background
    ctx.clearRect(0, 0, canvas.width, canvas.height)

    if (!data || !data.detections || data.detections.length === 0) return

    const scaleX = canvas.width / imgW
    const scaleY = canvas.height / imgH

    ctx.lineWidth = 3
    ctx.font = '14px sans-serif'
    ctx.textBaseline = 'top'

    data.detections.forEach((d) => {
      const [x1, y1, x2, y2] = d.bbox
      const w = (x2 - x1) * scaleX
      const h = (y2 - y1) * scaleY
      const x = x1 * scaleX
      const y = y1 * scaleY

      // Color by type
      const color = getColorForType(d.type)
      ctx.strokeStyle = color
      ctx.fillStyle = color

      // Draw rect
      ctx.strokeRect(x, y, w, h)

      // Draw label background
      const label = `${d.original_class} ${Math.round(d.confidence * 100)}%`
      const textWidth = ctx.measureText(label).width
      const pad = 6
      ctx.fillRect(x, y - 20, textWidth + pad, 20)

      // Draw label text
      ctx.fillStyle = '#fff'
      ctx.fillText(label, x + 4, y - 18)

      // Reset fillStyle
      ctx.fillStyle = color
    })

    // Clear previous timer and set a new one to remove boxes after 1 second
    if (clearTimerRef.current) clearTimeout(clearTimerRef.current)
    clearTimerRef.current = setTimeout(() => {
      ctx.clearRect(0, 0, canvas.width, canvas.height)
    }, 1000)
  }

  function getColorForType(type) {
    switch (type) {
      case 'plastic': return 'rgba(255,0,0,0.9)'
      case 'metal': return 'rgba(255,165,0,0.9)'
      case 'paper': return 'rgba(255,255,0,0.9)'
      case 'leafy_vegetables': return 'rgba(0,255,0,0.9)'
      default: return 'rgba(128,128,128,0.9)'
    }
  }

  // Optional: show an error image if the stream fails to load
  function onImgError() {
    setMsg('Stream error or not reachable. Try probing or check gateway logs.')
  }

  return (
    <div className="viewer" style={{position:'relative', display:'inline-block'}}>
      <div style={{flex:1}}>
        <div style={{display:'flex', justifyContent:'center', alignItems:'center'}}>
          <img ref={imgRef} alt="Live feed" style={{borderRadius:12, maxWidth:'100%', background:'#000', width:480, height:360, display:'block'}} src={streamSrc} onError={onImgError} />
          {/* Canvas overlay positioned over the image */}
          <canvas ref={canvasRef} style={{position:'absolute', left:0, top:0, pointerEvents:'none', transform:'translate(0,0)'}} />
        </div>
        <div style={{textAlign:'center', marginTop:8}}>
          <small>{msg}</small>
        </div>
      </div>
    </div>
  )
}
