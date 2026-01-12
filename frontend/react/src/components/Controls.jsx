import React, { useState, useEffect } from 'react'

function looksLikeIp(s) {
  if (!s) return false
  return /^\d{1,3}(?:\.\d{1,3}){3}$/.test(s.trim())
}

function ensureUrl(s) {
  if (!s) return s
  if (/^https?:\/\//.test(s)) return s
  return 'http://' + s
}

export default function Controls({ onUseUrl, onUseIp, onUseDefault }) {
  const [url, setUrl] = useState('')
  const [ip, setIp] = useState('')
  const [probe, setProbe] = useState(null)
  const [msg, setMsg] = useState('')
  const [devices, setDevices] = useState([])
  const [selectedDevice, setSelectedDevice] = useState('')
  const [normalizedTarget, setNormalizedTarget] = useState('')

  useEffect(() => {
    // Try to fetch registered devices from the gateway and populate dropdown
    let mounted = true
    fetch('/devices').then(r => r.json()).then(j => {
      if (!mounted) return
      if (j && typeof j === 'object') {
        // Expecting an object of id -> url OR id -> {url, lastSeen}
        const arr = Object.keys(j).map(id => {
          const v = j[id]
          const url = (typeof v === 'string') ? v : (v && typeof v === 'object' ? v.url : '')
          const lastSeen = (v && typeof v === 'object' ? v.lastSeen : undefined)
          return { id, url, lastSeen }
        })
        setDevices(arr)
        if (arr.length) setSelectedDevice(arr[0].id)
      }
    }).catch(e => {
      // ignore - endpoint may require auth or be empty
    })
    return () => { mounted = false }
  }, [])

  async function probeTarget(val, isUrl=false) {
    if (!val) return setMsg('Enter IP or URL first')
    setMsg('Probing...')
    try {
      let q
      if (isUrl) {
        const full = ensureUrl(val)
        q = `?url=${encodeURIComponent(full)}`
      } else {
        // if input looks like IP but contains port or path, allow as URL too
        if (looksLikeIp(val)) q = `?target_ip=${encodeURIComponent(val)}`
        else q = `?url=${encodeURIComponent(ensureUrl(val))}`
      }
      const r = await fetch(`/probe_stream${q}`)
      const j = await r.json()
      setProbe(j)
      setMsg(j.ok ? `Probe OK: ${j.contentType || j.status}` : `Probe failed: ${j.error || 'unknown'}`)
    } catch (e) {
      setMsg('Probe error: ' + e.toString())
    }
  }

  function handleUseUrl() {
    if (!url) return
    if (looksLikeIp(url)) {
      onUseIp(url)
      setMsg('Using device IP (from URL field)')
    } else {
      onUseUrl(ensureUrl(url))
      setMsg('Using URL')
    }
  }

  function handleUseDeviceSelection() {
    if (!selectedDevice) return
    const dev = devices.find(d => d.id === selectedDevice)
    if (dev && dev.url) {
      onUseUrl(ensureUrl(dev.url))
      setMsg(`Using registered device url for ${selectedDevice}`)
    } else {
      setMsg('Selected device has no registered URL')
    }
  }

  // Fetch normalized target (URL or target_ip) and show in UI so user can copy/open it
  async function fetchNormalized() {
    try {
      let q = null
      if (url) {
        q = `url=${encodeURIComponent(ensureUrl(url))}`
      } else if (selectedDevice) {
        const dev = devices.find(d => d.id === selectedDevice)
        if (dev && dev.url) q = `url=${encodeURIComponent(ensureUrl(dev.url))}`
        else if (dev && !dev.url) { setNormalizedTarget(''); setMsg('Selected device has no registered URL'); return }
      } else if (ip) {
        q = `target_ip=${encodeURIComponent(ip)}`
      } else {
        setNormalizedTarget('')
        return
      }
      const r = await fetch(`/normalize_stream?${q}`)
      if (!r.ok) {
        setNormalizedTarget('')
        return
      }
      const j = await r.json()
      setNormalizedTarget(j.target || '')
    } catch (e) {
      setNormalizedTarget('')
    }
  }

  // keep normalized target in sync when relevant inputs change (debounced)
  useEffect(() => {
    const t = setTimeout(() => { fetchNormalized() }, 200)
    return () => clearTimeout(t)
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [url, ip, selectedDevice, devices])

  // Open HLS stream for current selection (url, ip, or selected device)
  async function handleOpenHls() {
    try {
      setMsg('Checking HLS availability...')
      const a = await fetch('/hls/available')
      const aj = await a.json()
      if (!a.ok || !aj.available) { setMsg('HLS/ffmpeg not available on server'); return }

      setMsg('Starting HLS...')
      let q = null
      if (selectedDevice) q = `device_id=${encodeURIComponent(selectedDevice)}`
      else if (url) q = `url=${encodeURIComponent(ensureUrl(url))}`
      else if (ip) q = `target_ip=${encodeURIComponent(ip)}`
      else { setMsg('Select a device or enter URL/IP first'); return }

      const r = await fetch(`/hls?${q}`)
      const j = await r.json()
      if (!r.ok || !j.ok) { setMsg('Failed to start HLS'); return }
      const p = j.playlist
      if (p) {
        window.open(p, '_blank')
        setMsg('HLS started')
      } else {
        setMsg('HLS not ready')
      }
    } catch (e) {
      setMsg('HLS error: ' + e.toString())
    }
  }

  return (
    <aside className="controls">
      <label>ESP32 stream URL (optional)</label>
      <input value={url} onChange={e => setUrl(e.target.value)} placeholder="http://192.168.4.1:81/stream" />
      <button onClick={() => { handleUseUrl() }}>Use URL</button>
      <button onClick={() => probeTarget(url, true)} style={{marginLeft:8}}>Probe URL</button>

      <label style={{marginTop: 12}}>Device IP (optional)</label>
      <input value={ip} onChange={e => setIp(e.target.value)} placeholder="192.168.1.15" />
      <button onClick={() => { onUseIp(ip); setMsg('Using Device IP') }}>Use Device IP (proxy)</button>
      <button onClick={() => probeTarget(ip)} style={{marginLeft:8}}>Probe IP</button>

      {devices && devices.length > 0 && (
        <div style={{marginTop:12}}>
          <label>Registered devices</label>
          <select value={selectedDevice} onChange={e => setSelectedDevice(e.target.value)} style={{width:'100%', padding:8, marginTop:6, background:'#0b1220', color:'#e6edf3'}}>
            {devices.map(d => {
              const rssiText = (d.rssi || d.rssi === 0) ? ` (RSSI ${d.rssi} dBm)` : ''
              return <option key={d.id} value={d.id}>{d.id} — {d.url || '(no url)'}{rssiText}</option>
            })}
          </select>
          <div style={{marginTop:6}}>
            <button onClick={handleUseDeviceSelection}>Use registered device stream URL</button>
            <button style={{marginLeft:8}} onClick={() => handleOpenHls()}>Open HLS</button>
            {selectedDevice && (
              <div style={{marginTop:6}}>
                <small style={{color:'#94a3b8'}}>Selected RSSI: {(() => {
                  const d = devices.find(x => x.id === selectedDevice); return d && (d.rssi || d.rssi === 0) ? `${d.rssi} dBm` : 'unknown'
                })()}</small>
              </div>
            )}
          </div>
        </div>
      )}

      <button style={{background:'#10b981', color:'#fff', marginTop: 12}} onClick={() => { onUseDefault(); setMsg('Using default /device_stream') }}>Use default /device_stream</button>

      <div style={{marginTop:12}}>
        <small style={{color:'#94a3b8'}}>Tip: you can set the env var <code>ESP32_STREAM_URL</code> on the gateway (or paste a URL above).</small>
      </div>

      {/* Normalized target display */}
      {normalizedTarget && (
        <div style={{marginTop:12}}>
          <label>Corrected target</label>
          <div style={{display:'flex', gap:8, marginTop:6}}>
            <input readOnly value={normalizedTarget} style={{flex:1, padding:8, background:'#0b1220', color:'#e6edf3', borderRadius:6}} />
            <button onClick={() => { navigator.clipboard && navigator.clipboard.writeText(normalizedTarget); setMsg('Copied normalized URL') }}>Copy</button>
            <a href={normalizedTarget} target="_blank" rel="noopener noreferrer"><button>Open</button></a>
          </div>
        </div>
      )}

      <div style={{marginTop:12}}>
        <small style={{color:'#94a3b8'}}>Status: {msg}</small>
        <pre style={{fontSize:12, maxWidth:320, whiteSpace:'pre-wrap'}}>{probe ? JSON.stringify(probe, null, 2) : ''}</pre>
      </div>
    </aside>
  )
}