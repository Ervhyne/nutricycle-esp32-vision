import React, { useEffect, useRef, useState } from 'react'

export default function StreamViewer({ streamSrc }) {
  const imgRef = useRef(null)
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

  // Optional: show an error image if the stream fails to load
  function onImgError() {
    setMsg('Stream error or not reachable. Try probing or check gateway logs.')
  }

  return (
    <div className="viewer">
      <div style={{flex:1}}>
        <div style={{display:'flex', justifyContent:'center', alignItems:'center'}}>
          <img ref={imgRef} alt="Live feed" style={{borderRadius:12, maxWidth:'100%', background:'#000', width:480, height:360}} src={streamSrc} onError={onImgError} />
        </div>
        <div style={{textAlign:'center', marginTop:8}}>
          <small>{msg}</small>
        </div>
      </div>
    </div>
  )
}
