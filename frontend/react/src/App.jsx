import React, { useState } from 'react'
import StreamViewer from './components/StreamViewer'
import Controls from './components/Controls'

export default function App() {
  const [streamSrc, setStreamSrc] = useState('/device_stream?_t=0')

  function useUrl(url) {
    if (!url) return;
    setStreamSrc(`/device_stream?url=${encodeURIComponent(url)}&_t=${Date.now()}`)
  }
  function useIp(ip) {
    if (!ip) return;
    setStreamSrc(`/device_stream?target_ip=${encodeURIComponent(ip)}&_t=${Date.now()}`)
  }
  function useDefault() {
    setStreamSrc(`/device_stream?_t=${Date.now()}`)
  }

  return (
    <div className="app">
      <header>
        <h1>NutriCycle — ESP32 Live Feed</h1>
      </header>
      <main>
        <StreamViewer streamSrc={streamSrc} />
        <Controls onUseUrl={useUrl} onUseIp={useIp} onUseDefault={useDefault} />
      </main>
    </div>
  )
}
