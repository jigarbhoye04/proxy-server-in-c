# Run All Tests Script
# Executes all phase tests and performance benchmark

param(
    [string]$ProxyPort = "8080",
    [switch]$QuickMode,
    [switch]$Verbose
)

Write-Host "🧪 RUNNING ALL PROXY SERVER TESTS" -ForegroundColor Cyan
Write-Host "=================================" -ForegroundColor Cyan

$testDir = Split-Path $MyInvocation.MyCommand.Path
$allTestsLog = Join-Path $testDir "test_results" "all_tests_$(Get-Date -Format 'yyyyMMdd_HHmmss').log"

function Write-Log {
    param($Message)
    $logEntry = "$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') $Message"
    Write-Host $logEntry
    Add-Content -Path $allTestsLog -Value $logEntry
}

# Check if proxy is running
Write-Log "Checking proxy server on port $ProxyPort..."
try {
    $connection = Test-NetConnection -ComputerName "localhost" -Port $ProxyPort -WarningAction SilentlyContinue
    if (-not $connection.TcpTestSucceeded) {
        Write-Log "❌ Proxy server not running on port $ProxyPort"
        Write-Log "Please start the proxy server first:"
        Write-Log "  gcc -o proxy_server src/proxy_server_with_cache.c -lpthread -lws2_32"
        Write-Log "  ./proxy_server $ProxyPort"
        exit 1
    }
    Write-Log "✅ Proxy server detected on port $ProxyPort"
} catch {
    Write-Log "❌ Cannot connect to proxy server"
    exit 1
}

$testScripts = @(
    "phase1_memory_safety.ps1",
    "phase2_thread_pool.ps1", 
    "phase4_cache.ps1",
    "phase5_connection_pool.ps1"
)

$testResults = @{}
$overallStart = Get-Date

# Run individual phase tests
Write-Log "Running individual phase tests..."
foreach ($script in $testScripts) {
    $scriptPath = Join-Path $testDir $script
    if (Test-Path $scriptPath) {
        Write-Log "🔄 Running $script..."
        try {
            $scriptArgs = @("-ProxyPort", $ProxyPort)
            if ($QuickMode) { $scriptArgs += "-QuickMode" }
            if ($Verbose) { $scriptArgs += "-Verbose" }
            
            & PowerShell -File $scriptPath @scriptArgs | Out-Null
            $testResults[$script] = if ($LASTEXITCODE -eq 0) { "✅ PASSED" } else { "❌ FAILED" }
        } catch {
            $testResults[$script] = "💥 ERROR"
            Write-Log "  Error running $script`: $($_.Exception.Message)"
        }
        Write-Log "  $script`: $($testResults[$script])"
    } else {
        Write-Log "  ⚠️ $script not found"
        $testResults[$script] = "⏭️ SKIPPED"
    }
}

# Run comprehensive performance benchmark
Write-Log "🚀 Running comprehensive performance benchmark..."
$benchmarkPath = Join-Path $testDir "performance_benchmark.ps1"
if (Test-Path $benchmarkPath) {
    try {
        $benchmarkArgs = @("-ProxyPort", $ProxyPort, "-SkipIndividualTests")
        if ($QuickMode) { $benchmarkArgs += "-QuickMode" }
        if ($Verbose) { $benchmarkArgs += "-Verbose" }
        
        & PowerShell -File $benchmarkPath @benchmarkArgs | Out-Null
        $benchmarkResult = if ($LASTEXITCODE -eq 0) { "✅ PASSED" } else { "❌ FAILED" }
    } catch {
        $benchmarkResult = "💥 ERROR"
        Write-Log "  Error running performance benchmark: $($_.Exception.Message)"
    }
    Write-Log "  Performance Benchmark: $benchmarkResult"
} else {
    Write-Log "  ⚠️ Performance benchmark script not found"
    $benchmarkResult = "⏭️ SKIPPED"
}

$totalTime = (Get-Date) - $overallStart

# Generate summary
Write-Log "================================================"
Write-Log "🏁 ALL TESTS COMPLETE"
Write-Log "================================================"

$passedTests = ($testResults.Values | Where-Object { $_ -eq "✅ PASSED" }).Count
$totalTests = $testResults.Count + 1  # +1 for benchmark

Write-Log "📊 INDIVIDUAL PHASE TEST RESULTS:"
foreach ($test in $testResults.Keys) {
    Write-Log "  $test`: $($testResults[$test])"
}

Write-Log "🚀 PERFORMANCE BENCHMARK: $benchmarkResult"

$overallScore = if ($benchmarkResult -eq "✅ PASSED") { $passedTests + 1 } else { $passedTests }
$successRate = [math]::Round(($overallScore / $totalTests) * 100, 1)

Write-Log "🎯 OVERALL RESULTS:"
Write-Log "  Tests Passed: $overallScore/$totalTests ($successRate%)"
Write-Log "  Total Test Time: $([math]::Round($totalTime.TotalMinutes, 2)) minutes"

if ($successRate -ge 90) {
    Write-Log "🎉 EXCELLENT - All proxy optimizations working perfectly!"
} elseif ($successRate -ge 75) {
    Write-Log "✅ GOOD - Proxy performing well with minor issues"
} elseif ($successRate -ge 50) {
    Write-Log "⚠️ FAIR - Some optimizations need attention"
} else {
    Write-Log "❌ POOR - Significant issues detected, review implementation"
}

Write-Log "📝 Test logs saved to: $allTestsLog"
Write-Host "`n🏆 Test suite complete! Check test_results/ for detailed logs." -ForegroundColor Green
