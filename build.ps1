# HTTP Proxy Server - Build Script
# Compiles the proxy server with all components

Write-Host "================================================================" -ForegroundColor Green
Write-Host "HTTP PROXY SERVER - BUILD SCRIPT" -ForegroundColor Green  
Write-Host "================================================================" -ForegroundColor Green
Write-Host "Building proxy server..." -ForegroundColor White
Write-Host ""

# Build command
$buildCmd = "gcc -o proxy_server.exe src/proxy_server.c src/components/cache.c src/components/connection_pool.c src/components/http_parser.c src/components/platform.c src/components/proxy_server.c src/components/thread_pool.c -I include -lws2_32 -lpthread"

Write-Host "[BUILD] Compiling proxy server..." -ForegroundColor Cyan
Write-Host "Command: $buildCmd" -ForegroundColor Gray
Write-Host ""

try {
    Invoke-Expression $buildCmd
    
    if (Test-Path "proxy_server.exe") {
        Write-Host "================================================================" -ForegroundColor Green
        Write-Host "BUILD SUCCESSFUL!" -ForegroundColor Green
        Write-Host "================================================================" -ForegroundColor Green
        Write-Host "Executable: proxy_server.exe" -ForegroundColor White
        Write-Host ""
        Write-Host "Usage:" -ForegroundColor Yellow
        Write-Host "  .\proxy_server.exe 8080" -ForegroundColor White
        Write-Host ""
        Write-Host "Run tests:" -ForegroundColor Yellow  
        Write-Host "  .\tests\test_proxy.ps1" -ForegroundColor White
        Write-Host ""
    } else {
        Write-Host "BUILD FAILED: Executable not created" -ForegroundColor Red
        exit 1
    }
} catch {
    Write-Host "BUILD FAILED: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
