# NutriCycle Detection System - Windows Startup Script
# Double-click this file to start the system

Write-Host "=" -NoNewline
Write-Host ("=" * 60)
Write-Host "NutriCycle Detection System - Starting..."
Write-Host ("=" * 60)

# Check if Python is installed
try {
    $pythonVersion = python --version 2>&1
    Write-Host "✅ Python found: $pythonVersion"
} catch {
    Write-Host "❌ Python not found. Please install Python 3.8+ first."
    Read-Host "Press Enter to exit"
    exit
}

# Navigate to backend directory
$backendPath = Join-Path $PSScriptRoot "backend"

if (-not (Test-Path $backendPath)) {
    Write-Host "❌ Backend directory not found"
    Read-Host "Press Enter to exit"
    exit
}

Set-Location $backendPath

# Check if dependencies are installed
Write-Host "`nChecking dependencies..."
$pipList = pip list 2>&1 | Out-String
if ($pipList -notmatch "flask" -or $pipList -notmatch "ultralytics") {
    Write-Host "⚠️  Dependencies not installed. Installing now..."
    pip install -r requirements.txt
}

# Start Flask server
Write-Host "`n" -NoNewline
Write-Host ("=" * 60)
Write-Host "🚀 Starting Flask Server..."
Write-Host ("=" * 60)
Write-Host "`nServer will start on http://localhost:5000"
Write-Host "Open frontend/index.html in your browser to view the dashboard"
Write-Host "`nPress Ctrl+C to stop the server`n"

# Run the Flask app
python app.py

# Cleanup on exit
Write-Host "`n" -NoNewline
Write-Host ("=" * 60)
Write-Host "Server stopped"
Write-Host ("=" * 60)

Read-Host "Press Enter to exit"
