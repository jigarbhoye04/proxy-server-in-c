# Performance Benchmark Suite
# Comprehensive testing of all 5 optimization phases

param(
    [string]$ProxyPort = "8080",
    [switch]$SkipIndividualTests,
    [switch]$QuickMode,
    [switch]$Verbose
)

Write-Host "🚀 PROXY SERVER PERFORMANCE BENCHMARK SUITE" -ForegroundColor Yellow
Write-Host "=============================================" -ForegroundColor Yellow

$testDir = Split-Path $MyInvocation.MyCommand.Path
$rootDir = Split-Path $testDir
$resultsDir = Join-Path $testDir "test_results"
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$benchmarkFile = Join-Path $resultsDir "performance_benchmark_$timestamp.log"

# Ensure results directory exists
if (!(Test-Path $resultsDir)) {
    New-Item -ItemType Directory -Path $resultsDir -Force | Out-Null
}

function Write-Log {
    param($Message, $Level = "INFO")
    $logEntry = "$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') [$Level] $Message"
    Write-Host $logEntry
    Add-Content -Path $benchmarkFile -Value $logEntry
}

function Test-BaselinePerformance {
    Write-Log "Testing baseline proxy performance"
    
    $baselineUrls = @(
        "http://httpbin.org/json",
        "http://httpbin.org/html",
        "http://httpbin.org/status/200",
        "http://jsonplaceholder.typicode.com/posts/1"
    )
    
    $baselineStats = @{
        TotalRequests = 0
        SuccessfulRequests = 0
        FailedRequests = 0
        ResponseTimes = @()
        TotalTime = 0
    }
    
    $overallStart = Get-Date
    
    foreach ($url in $baselineUrls) {
        $iterations = if ($QuickMode) { 2 } else { 3 }
        for ($i = 1; $i -le $iterations; $i++) {
            $startTime = Get-Date
            try {
                $response = curl -x "localhost:$ProxyPort" -m 15 -s $url 2>&1
                if ($LASTEXITCODE -eq 0) {
                    $requestTime = (Get-Date) - $startTime
                    $baselineStats.ResponseTimes += $requestTime.TotalMilliseconds
                    $baselineStats.SuccessfulRequests++
                } else {
                    $baselineStats.FailedRequests++
                }
            } catch {
                $baselineStats.FailedRequests++
            }
            $baselineStats.TotalRequests++
            Start-Sleep -Milliseconds 100
        }
    }
    
    $baselineStats.TotalTime = (Get-Date) - $overallStart
    
    $avgResponseTime = if ($baselineStats.ResponseTimes.Count -gt 0) {
        ($baselineStats.ResponseTimes | Measure-Object -Average).Average
    } else { 0 }
    
    $successRate = if ($baselineStats.TotalRequests -gt 0) {
        [math]::Round(($baselineStats.SuccessfulRequests / $baselineStats.TotalRequests) * 100, 1)
    } else { 0 }
    
    Write-Log "Baseline Performance Results:"
    Write-Log "  Total Requests: $($baselineStats.TotalRequests)"
    Write-Log "  Successful: $($baselineStats.SuccessfulRequests)"
    Write-Log "  Failed: $($baselineStats.FailedRequests)"
    Write-Log "  Success Rate: $successRate%"
    Write-Log "  Average Response Time: $([math]::Round($avgResponseTime, 2))ms"
    Write-Log "  Total Test Time: $([math]::Round($baselineStats.TotalTime.TotalSeconds, 2))s"
    
    return @{
        TotalRequests = $baselineStats.TotalRequests
        SuccessfulRequests = $baselineStats.SuccessfulRequests
        SuccessRate = $successRate
        AvgResponseTime = $avgResponseTime
        TotalTestTime = $baselineStats.TotalTime.TotalSeconds
    }
}

function Test-ConcurrentLoad {
    Write-Log "Testing concurrent load handling"
    
    $concurrentWorkers = if ($QuickMode) { 4 } else { 8 }
    $requestsPerWorker = if ($QuickMode) { 3 } else { 5 }
    
    Write-Log "Starting $concurrentWorkers concurrent workers, $requestsPerWorker requests each"
    
    $concurrentJobs = @()
    $testUrls = @(
        "http://httpbin.org/json",
        "http://httpbin.org/html",
        "http://jsonplaceholder.typicode.com/posts/1"
    )
    
    for ($i = 1; $i -le $concurrentWorkers; $i++) {
        $job = Start-Job -ScriptBlock {
            param($ProxyPort, $RequestCount, $WorkerId, $TestUrls)
            
            $workerResults = @{
                WorkerId = $WorkerId
                TotalRequests = 0
                SuccessfulRequests = 0
                ResponseTimes = @()
                Errors = 0
            }
            
            for ($j = 1; $j -le $RequestCount; $j++) {
                $url = $TestUrls[($j - 1) % $TestUrls.Count]
                $startTime = Get-Date
                
                try {
                    $response = curl -x "localhost:$ProxyPort" -m 15 -s $url 2>&1
                    if ($LASTEXITCODE -eq 0) {
                        $requestTime = (Get-Date) - $startTime
                        $workerResults.ResponseTimes += $requestTime.TotalMilliseconds
                        $workerResults.SuccessfulRequests++
                    } else {
                        $workerResults.Errors++
                    }
                } catch {
                    $workerResults.Errors++
                }
                $workerResults.TotalRequests++
                Start-Sleep -Milliseconds 50
            }
            
            return $workerResults
        } -ArgumentList $ProxyPort, $requestsPerWorker, $i, $testUrls
        
        $concurrentJobs += $job
    }
    
    # Wait for completion
    $results = $concurrentJobs | Wait-Job | Receive-Job
    $concurrentJobs | Remove-Job
    
    # Aggregate results
    $totalRequests = ($results | Measure-Object -Property TotalRequests -Sum).Sum
    $totalSuccessful = ($results | Measure-Object -Property SuccessfulRequests -Sum).Sum
    $totalErrors = ($results | Measure-Object -Property Errors -Sum).Sum
    $allResponseTimes = $results | ForEach-Object { $_.ResponseTimes }
    
    $concurrentSuccessRate = if ($totalRequests -gt 0) {
        [math]::Round(($totalSuccessful / $totalRequests) * 100, 1)
    } else { 0 }
    
    $avgConcurrentTime = if ($allResponseTimes.Count -gt 0) {
        ($allResponseTimes | Measure-Object -Average).Average
    } else { 0 }
    
    Write-Log "Concurrent Load Results:"
    Write-Log "  Workers: $concurrentWorkers"
    Write-Log "  Total Requests: $totalRequests"
    Write-Log "  Successful: $totalSuccessful"
    Write-Log "  Errors: $totalErrors"
    Write-Log "  Success Rate: $concurrentSuccessRate%"
    Write-Log "  Avg Response Time: $([math]::Round($avgConcurrentTime, 2))ms"
    
    return @{
        Workers = $concurrentWorkers
        TotalRequests = $totalRequests
        SuccessfulRequests = $totalSuccessful
        ConcurrentSuccessRate = $concurrentSuccessRate
        AvgConcurrentTime = $avgConcurrentTime
        TotalErrors = $totalErrors
    }
}

function Test-CacheEffectiveness {
    Write-Log "Testing cache effectiveness"
    
    $cacheTestUrl = "http://httpbin.org/json"
    $iterations = if ($QuickMode) { 5 } else { 10 }
    
    $cacheTimes = @{
        FirstRequests = @()
        CachedRequests = @()
    }
    
    # First request (cache miss)
    Write-Log "Making initial request (cache miss expected)"
    $startTime = Get-Date
    try {
        $response = curl -x "localhost:$ProxyPort" -m 15 -s $cacheTestUrl 2>&1
        if ($LASTEXITCODE -eq 0) {
            $firstRequestTime = (Get-Date) - $startTime
            $cacheTimes.FirstRequests += $firstRequestTime.TotalMilliseconds
            Write-Log "  First request: $($firstRequestTime.TotalMilliseconds)ms"
        }
    } catch {
        Write-Log "  First request failed" "WARN"
    }
    
    Start-Sleep -Milliseconds 200
    
    # Subsequent requests (cache hits expected)
    Write-Log "Making cached requests (cache hits expected)"
    for ($i = 1; $i -le $iterations; $i++) {
        $startTime = Get-Date
        try {
            $response = curl -x "localhost:$ProxyPort" -m 10 -s $cacheTestUrl 2>&1
            if ($LASTEXITCODE -eq 0) {
                $cachedRequestTime = (Get-Date) - $startTime
                $cacheTimes.CachedRequests += $cachedRequestTime.TotalMilliseconds
                if ($Verbose) {
                    Write-Log "  Cached request $i`: $($cachedRequestTime.TotalMilliseconds)ms"
                }
            }
        } catch {
            Write-Log "  Cached request $i failed" "WARN"
        }
        Start-Sleep -Milliseconds 100
    }
    
    $avgFirstRequest = if ($cacheTimes.FirstRequests.Count -gt 0) {
        ($cacheTimes.FirstRequests | Measure-Object -Average).Average
    } else { 0 }
    
    $avgCachedRequest = if ($cacheTimes.CachedRequests.Count -gt 0) {
        ($cacheTimes.CachedRequests | Measure-Object -Average).Average
    } else { 0 }
    
    $cacheSpeedup = if ($avgCachedRequest -gt 0) {
        [math]::Round($avgFirstRequest / $avgCachedRequest, 2)
    } else { 0 }
    
    Write-Log "Cache Effectiveness Results:"
    Write-Log "  Avg First Request: $([math]::Round($avgFirstRequest, 2))ms"
    Write-Log "  Avg Cached Request: $([math]::Round($avgCachedRequest, 2))ms"
    Write-Log "  Cache Speedup: ${cacheSpeedup}x"
    Write-Log "  Cached Requests Tested: $($cacheTimes.CachedRequests.Count)"
    
    return @{
        AvgFirstRequest = $avgFirstRequest
        AvgCachedRequest = $avgCachedRequest
        CacheSpeedup = $cacheSpeedup
        CachedRequestsCount = $cacheTimes.CachedRequests.Count
    }
}

function Test-ConnectionPooling {
    Write-Log "Testing connection pooling effectiveness"
    
    $poolTestServer = "httpbin.org"
    $poolPaths = @("/json", "/html", "/status/200")
    $rounds = if ($QuickMode) { 2 } else { 3 }
    
    $poolTimes = @{
        FirstConnections = @()
        PooledConnections = @()
    }
    
    # First round - establish connections
    Write-Log "Establishing initial connections"
    foreach ($path in $poolPaths) {
        $url = "http://$poolTestServer$path"
        $startTime = Get-Date
        try {
            $response = curl -x "localhost:$ProxyPort" -m 15 -s $url 2>&1
            if ($LASTEXITCODE -eq 0) {
                $connectionTime = (Get-Date) - $startTime
                $poolTimes.FirstConnections += $connectionTime.TotalMilliseconds
                Write-Log "  First connection to $path`: $($connectionTime.TotalMilliseconds)ms"
            }
        } catch {
            Write-Log "  Failed to connect to $path" "WARN"
        }
        Start-Sleep -Milliseconds 200
    }
    
    # Subsequent rounds - reuse connections
    Write-Log "Testing connection reuse ($rounds rounds)"
    for ($round = 1; $round -le $rounds; $round++) {
        foreach ($path in $poolPaths) {
            $url = "http://$poolTestServer$path"
            $startTime = Get-Date
            try {
                $response = curl -x "localhost:$ProxyPort" -m 10 -s $url 2>&1
                if ($LASTEXITCODE -eq 0) {
                    $pooledTime = (Get-Date) - $startTime
                    $poolTimes.PooledConnections += $pooledTime.TotalMilliseconds
                    if ($Verbose) {
                        Write-Log "    Pooled connection round $round to $path`: $($pooledTime.TotalMilliseconds)ms"
                    }
                }
            } catch {
                Write-Log "    Failed pooled connection to $path" "WARN"
            }
            Start-Sleep -Milliseconds 100
        }
    }
    
    $avgFirstConnection = if ($poolTimes.FirstConnections.Count -gt 0) {
        ($poolTimes.FirstConnections | Measure-Object -Average).Average
    } else { 0 }
    
    $avgPooledConnection = if ($poolTimes.PooledConnections.Count -gt 0) {
        ($poolTimes.PooledConnections | Measure-Object -Average).Average
    } else { 0 }
    
    $poolSpeedup = if ($avgPooledConnection -gt 0) {
        [math]::Round($avgFirstConnection / $avgPooledConnection, 2)
    } else { 0 }
    
    Write-Log "Connection Pooling Results:"
    Write-Log "  Avg First Connection: $([math]::Round($avgFirstConnection, 2))ms"
    Write-Log "  Avg Pooled Connection: $([math]::Round($avgPooledConnection, 2))ms"
    Write-Log "  Pool Speedup: ${poolSpeedup}x"
    Write-Log "  Pooled Connections Tested: $($poolTimes.PooledConnections.Count)"
    
    return @{
        AvgFirstConnection = $avgFirstConnection
        AvgPooledConnection = $avgPooledConnection
        PoolSpeedup = $poolSpeedup
        PooledConnectionsCount = $poolTimes.PooledConnections.Count
    }
}

function Run-IndividualPhaseTests {
    Write-Log "Running individual phase test scripts..."
    
    $phaseScripts = @(
        "phase1_memory_safety.ps1",
        "phase2_thread_pool.ps1",
        "phase4_cache.ps1",
        "phase5_connection_pool.ps1"
    )
    
    $phaseResults = @{}
    
    foreach ($script in $phaseScripts) {
        $scriptPath = Join-Path $testDir $script
        if (Test-Path $scriptPath) {
            Write-Log "Running $script..."
            try {
                $result = & PowerShell -File $scriptPath -ProxyPort $ProxyPort 2>&1
                $phaseResults[$script] = if ($LASTEXITCODE -eq 0) { "PASSED" } else { "FAILED" }
                Write-Log "  $script`: $($phaseResults[$script])"
            } catch {
                $phaseResults[$script] = "ERROR"
                Write-Log "  $script`: ERROR - $($_.Exception.Message)" "ERROR"
            }
        } else {
            Write-Log "  $script not found, skipping" "WARN"
            $phaseResults[$script] = "SKIPPED"
        }
    }
    
    return $phaseResults
}

# Main execution
Write-Log "Performance Benchmark Suite Starting"
Write-Log "Proxy Port: $ProxyPort"
Write-Log "Quick Mode: $(if ($QuickMode) { 'Enabled' } else { 'Disabled' })"
Write-Log "Individual Tests: $(if ($SkipIndividualTests) { 'Skipped' } else { 'Enabled' })"
Write-Log "Results will be saved to: $benchmarkFile"

# Check if proxy is running
try {
    $connection = Test-NetConnection -ComputerName "localhost" -Port $ProxyPort -WarningAction SilentlyContinue
    if (-not $connection.TcpTestSucceeded) {
        Write-Log "❌ Proxy server not running on port $ProxyPort" "ERROR"
        Write-Log "Please start the proxy server first: .\proxy_server $ProxyPort"
        exit 1
    }
    Write-Log "✅ Proxy server detected on port $ProxyPort"
} catch {
    Write-Log "❌ Cannot connect to proxy server" "ERROR"
    exit 1
}

$overallStart = Get-Date

# Run performance tests
Write-Log "Starting baseline performance test..."
$baselineResults = Test-BaselinePerformance

Write-Log "Starting concurrent load test..."
$concurrentResults = Test-ConcurrentLoad

Write-Log "Starting cache effectiveness test..."
$cacheResults = Test-CacheEffectiveness

Write-Log "Starting connection pooling test..."
$poolResults = Test-ConnectionPooling

# Run individual phase tests if not skipped
$phaseResults = @{}
if (-not $SkipIndividualTests) {
    $phaseResults = Run-IndividualPhaseTests
}

$overallTime = (Get-Date) - $overallStart

# Generate comprehensive report
Write-Log "================================================"
Write-Log "COMPREHENSIVE PERFORMANCE BENCHMARK RESULTS"
Write-Log "================================================"

Write-Log "📊 BASELINE PERFORMANCE"
Write-Log "  Success Rate: $($baselineResults.SuccessRate)%"
Write-Log "  Avg Response Time: $([math]::Round($baselineResults.AvgResponseTime, 2))ms"
Write-Log "  Total Test Time: $([math]::Round($baselineResults.TotalTestTime, 2))s"

Write-Log "🔄 CONCURRENT LOAD HANDLING"
Write-Log "  Workers: $($concurrentResults.Workers)"
Write-Log "  Success Rate: $($concurrentResults.ConcurrentSuccessRate)%"
Write-Log "  Avg Response Time: $([math]::Round($concurrentResults.AvgConcurrentTime, 2))ms"
Write-Log "  Total Errors: $($concurrentResults.TotalErrors)"

Write-Log "💾 CACHE EFFECTIVENESS"
Write-Log "  Cache Speedup: $($cacheResults.CacheSpeedup)x"
Write-Log "  First Request: $([math]::Round($cacheResults.AvgFirstRequest, 2))ms"
Write-Log "  Cached Request: $([math]::Round($cacheResults.AvgCachedRequest, 2))ms"

Write-Log "🔌 CONNECTION POOLING"
Write-Log "  Pool Speedup: $($poolResults.PoolSpeedup)x"
Write-Log "  First Connection: $([math]::Round($poolResults.AvgFirstConnection, 2))ms"
Write-Log "  Pooled Connection: $([math]::Round($poolResults.AvgPooledConnection, 2))ms"

if ($phaseResults.Count -gt 0) {
    Write-Log "🧪 INDIVIDUAL PHASE TESTS"
    foreach ($phase in $phaseResults.Keys) {
        $status = switch ($phaseResults[$phase]) {
            "PASSED" { "✅ PASSED" }
            "FAILED" { "❌ FAILED" }
            "ERROR" { "💥 ERROR" }
            "SKIPPED" { "⏭️ SKIPPED" }
        }
        Write-Log "  $phase`: $status"
    }
}

# Performance evaluation
$performanceScore = 0
$maxScore = 8

# Baseline (2 points)
if ($baselineResults.SuccessRate -ge 90) { $performanceScore += 1 }
if ($baselineResults.AvgResponseTime -lt 1000) { $performanceScore += 1 }

# Concurrent (2 points)
if ($concurrentResults.ConcurrentSuccessRate -ge 85) { $performanceScore += 1 }
if ($concurrentResults.TotalErrors -le 2) { $performanceScore += 1 }

# Cache (2 points)
if ($cacheResults.CacheSpeedup -ge 2.0) { $performanceScore += 1 }
if ($cacheResults.AvgCachedRequest -lt 500) { $performanceScore += 1 }

# Connection Pool (2 points)
if ($poolResults.PoolSpeedup -ge 1.5) { $performanceScore += 1 }
if ($poolResults.AvgPooledConnection -lt 800) { $performanceScore += 1 }

$performancePercentage = [math]::Round(($performanceScore / $maxScore) * 100, 1)

Write-Log "================================================"
Write-Log "🏆 OVERALL PERFORMANCE SCORE: $performanceScore/$maxScore ($performancePercentage%)"
Write-Log "⏱️  Total Benchmark Time: $([math]::Round($overallTime.TotalMinutes, 2)) minutes"

if ($performancePercentage -ge 87.5) {
    Write-Log "🚀 EXCELLENT PERFORMANCE - All optimizations working effectively!" "SUCCESS"
} elseif ($performancePercentage -ge 75) {
    Write-Log "✅ GOOD PERFORMANCE - Most optimizations working well" "SUCCESS"
} elseif ($performancePercentage -ge 62.5) {
    Write-Log "⚠️  FAIR PERFORMANCE - Some optimizations need attention" "WARN"
} else {
    Write-Log "❌ POOR PERFORMANCE - Significant optimization issues detected" "ERROR"
}

Write-Log "📊 Detailed results saved to: $benchmarkFile"
Write-Host "`n🎯 Performance benchmark complete!" -ForegroundColor Green
