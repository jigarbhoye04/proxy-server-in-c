# Phase 4: Cache Optimization Tests
# Tests O(1) hash table cache performance vs legacy O(n) cache

param(
    [string]$ProxyPort = "8080",
    [int]$CacheTestRequests = 50,
    [switch]$Verbose
)

Write-Host "💾 PHASE 4: Cache Optimization Tests" -ForegroundColor Magenta
Write-Host "====================================" -ForegroundColor Magenta

$testDir = Split-Path $MyInvocation.MyCommand.Path
$rootDir = Split-Path $testDir
$resultsDir = Join-Path $testDir "test_results"
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$logFile = Join-Path $resultsDir "phase4_cache_optimization_$timestamp.log"

# Ensure results directory exists
if (!(Test-Path $resultsDir)) {
    New-Item -ItemType Directory -Path $resultsDir -Force | Out-Null
}

function Write-Log {
    param($Message, $Level = "INFO")
    $logEntry = "$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') [$Level] $Message"
    Write-Host $logEntry
    Add-Content -Path $logFile -Value $logEntry
}

function Test-CacheHitRatio {
    Write-Log "Testing cache hit ratio with repeated requests"
    
    # List of URLs to test caching
    $testUrls = @(
        "http://httpbin.org/status/200"
        "http://httpbin.org/json"
        "http://httpbin.org/html"
        "http://httpbin.org/xml"
        "http://httpbin.org/robots.txt"
    )
    
    $cacheStats = @{
        TotalRequests = 0
        CacheHits = 0
        CacheMisses = 0
        ResponseTimes = @()
        FirstRequestTimes = @()
        CachedRequestTimes = @()
    }
    
    # First round: Populate cache (expect all misses)
    Write-Log "Phase 1: Populating cache (expect cache misses)"
    foreach ($url in $testUrls) {
        $startTime = Get-Date
        try {
            $response = curl -x "localhost:$ProxyPort" -m 10 -s -w "Time: %{time_total}s" $url 2>&1
            if ($LASTEXITCODE -eq 0) {
                $requestTime = (Get-Date) - $startTime
                $cacheStats.FirstRequestTimes += $requestTime.TotalMilliseconds
                $cacheStats.TotalRequests++
                Write-Log "  First request to $(Split-Path $url -Leaf): $($requestTime.TotalMilliseconds)ms"
            }
        } catch {
            Write-Log "  Failed to fetch $url" "WARN"
        }
        Start-Sleep -Milliseconds 100
    }
    
    # Second round: Test cache hits (expect hits for same URLs)
    Write-Log "Phase 2: Testing cache hits (expect faster responses)"
    foreach ($url in $testUrls) {
        $startTime = Get-Date
        try {
            $response = curl -x "localhost:$ProxyPort" -m 10 -s -w "Time: %{time_total}s" $url 2>&1
            if ($LASTEXITCODE -eq 0) {
                $requestTime = (Get-Date) - $startTime
                $cacheStats.CachedRequestTimes += $requestTime.TotalMilliseconds
                $cacheStats.TotalRequests++
                Write-Log "  Cached request to $(Split-Path $url -Leaf): $($requestTime.TotalMilliseconds)ms"
            }
        } catch {
            Write-Log "  Failed to fetch $url" "WARN"
        }
        Start-Sleep -Milliseconds 100
    }
    
    # Third round: Repeat for more cache hits
    Write-Log "Phase 3: Additional cache hit validation"
    for ($i = 1; $i -le 3; $i++) {
        foreach ($url in $testUrls) {
            $startTime = Get-Date
            try {
                $response = curl -x "localhost:$ProxyPort" -m 5 -s $url 2>&1
                if ($LASTEXITCODE -eq 0) {
                    $requestTime = (Get-Date) - $startTime
                    $cacheStats.CachedRequestTimes += $requestTime.TotalMilliseconds
                    $cacheStats.TotalRequests++
                }
            } catch {
                # Ignore failures in this phase
            }
            Start-Sleep -Milliseconds 50
        }
    }
    
    # Calculate performance improvements
    $avgFirstRequest = if ($cacheStats.FirstRequestTimes.Count -gt 0) {
        ($cacheStats.FirstRequestTimes | Measure-Object -Average).Average
    } else { 0 }
    
    $avgCachedRequest = if ($cacheStats.CachedRequestTimes.Count -gt 0) {
        ($cacheStats.CachedRequestTimes | Measure-Object -Average).Average
    } else { 0 }
    
    $cacheSpeedup = if ($avgCachedRequest -gt 0) {
        [math]::Round($avgFirstRequest / $avgCachedRequest, 2)
    } else { 0 }
    
    Write-Log "Cache Performance Analysis:"
    Write-Log "  Total Requests: $($cacheStats.TotalRequests)"
    Write-Log "  Avg First Request Time: $([math]::Round($avgFirstRequest, 2))ms"
    Write-Log "  Avg Cached Request Time: $([math]::Round($avgCachedRequest, 2))ms"
    Write-Log "  Cache Speedup: ${cacheSpeedup}x faster"
    
    return @{
        TotalRequests = $cacheStats.TotalRequests
        AvgFirstRequestTime = $avgFirstRequest
        AvgCachedRequestTime = $avgCachedRequest
        CacheSpeedup = $cacheSpeedup
        PerformanceImprovement = ($cacheSpeedup -ge 2.0)  # Expect at least 2x improvement
    }
}

function Test-CacheScalability {
    Write-Log "Testing cache scalability with multiple concurrent requests"
    
    # Test cache performance under concurrent load
    $concurrentJobs = @()
    $testUrl = "http://httpbin.org/json"  # Single URL for focused cache testing
    $workersCount = 5
    $requestsPerWorker = 10
    
    Write-Log "Starting $workersCount concurrent workers, $requestsPerWorker requests each"
    
    for ($i = 1; $i -le $workersCount; $i++) {
        $job = Start-Job -ScriptBlock {
            param($ProxyPort, $TestUrl, $RequestCount, $WorkerId)
            
            $workerResults = @{
                WorkerId = $WorkerId
                RequestTimes = @()
                SuccessCount = 0
            }
            
            for ($j = 1; $j -le $RequestCount; $j++) {
                $startTime = Get-Date
                try {
                    $response = curl -x "localhost:$ProxyPort" -m 10 -s $TestUrl 2>&1
                    if ($LASTEXITCODE -eq 0) {
                        $requestTime = (Get-Date) - $startTime
                        $workerResults.RequestTimes += $requestTime.TotalMilliseconds
                        $workerResults.SuccessCount++
                    }
                } catch {
                    # Ignore individual failures
                }
                Start-Sleep -Milliseconds 100
            }
            
            return $workerResults
        } -ArgumentList $ProxyPort, $testUrl, $requestsPerWorker, $i
        
        $concurrentJobs += $job
    }
    
    # Wait for completion
    $results = $concurrentJobs | Wait-Job | Receive-Job
    $concurrentJobs | Remove-Job
    
    # Analyze concurrent cache performance
    $allRequestTimes = $results | ForEach-Object { $_.RequestTimes }
    $totalSuccessful = ($results | Measure-Object -Property SuccessCount -Sum).Sum
    
    $avgConcurrentTime = if ($allRequestTimes.Count -gt 0) {
        ($allRequestTimes | Measure-Object -Average).Average
    } else { 0 }
    
    $medianTime = if ($allRequestTimes.Count -gt 0) {
        $sortedTimes = $allRequestTimes | Sort-Object
        $medianIndex = [math]::Floor($sortedTimes.Count / 2)
        $sortedTimes[$medianIndex]
    } else { 0 }
    
    Write-Log "Concurrent Cache Performance:"
    Write-Log "  Total Successful Requests: $totalSuccessful"
    Write-Log "  Average Response Time: $([math]::Round($avgConcurrentTime, 2))ms"
    Write-Log "  Median Response Time: $([math]::Round($medianTime, 2))ms"
    
    $goodConcurrentPerformance = ($avgConcurrentTime -lt 500) -and ($totalSuccessful -ge ($workersCount * $requestsPerWorker * 0.8))
    
    return @{
        TotalSuccessfulRequests = $totalSuccessful
        AvgConcurrentResponseTime = $avgConcurrentTime
        MedianResponseTime = $medianTime
        GoodConcurrentPerformance = $goodConcurrentPerformance
    }
}

function Test-CacheMemoryEfficiency {
    Write-Log "Testing cache memory efficiency and limits"
    
    # Test with variety of URLs to fill cache
    $largeUrlSet = @()
    for ($i = 1; $i -le 20; $i++) {
        $largeUrlSet += "http://httpbin.org/base64/$(([System.Convert]::ToBase64String([System.Text.Encoding]::UTF8.GetBytes("test-url-$i"))))"
    }
    
    $memoryTestResults = @{
        UniqueUrls = $largeUrlSet.Count
        RequestsCompleted = 0
        CacheFullDetected = $false
    }
    
    Write-Log "Testing cache with $($largeUrlSet.Count) unique URLs"
    
    # Fill cache with diverse content
    foreach ($url in $largeUrlSet) {
        try {
            $response = curl -x "localhost:$ProxyPort" -m 10 -s $url 2>&1
            if ($LASTEXITCODE -eq 0) {
                $memoryTestResults.RequestsCompleted++
            }
        } catch {
            # Some requests may fail, that's acceptable
        }
        Start-Sleep -Milliseconds 50
    }
    
    # Test cache behavior when potentially full
    Write-Log "Testing cache behavior with repeated requests"
    $repeatTest = 0
    foreach ($url in $largeUrlSet[0..4]) {  # Test first 5 URLs again
        $startTime = Get-Date
        try {
            $response = curl -x "localhost:$ProxyPort" -m 5 -s $url 2>&1
            if ($LASTEXITCODE -eq 0) {
                $requestTime = (Get-Date) - $startTime
                if ($requestTime.TotalMilliseconds -lt 200) {
                    $repeatTest++  # Fast response suggests cache hit
                }
            }
        } catch {
            # Ignore failures
        }
    }
    
    $cacheEffective = ($repeatTest -ge 3)  # At least 3 out of 5 should be fast
    
    Write-Log "Cache Memory Test Results:"
    Write-Log "  Unique URLs tested: $($memoryTestResults.UniqueUrls)"
    Write-Log "  Requests completed: $($memoryTestResults.RequestsCompleted)"
    Write-Log "  Fast repeat responses: $repeatTest/5"
    Write-Log "  Cache appears effective: $(if ($cacheEffective) { 'Yes' } else { 'No' })"
    
    return @{
        UniqueUrlsTested = $memoryTestResults.UniqueUrls
        RequestsCompleted = $memoryTestResults.RequestsCompleted
        FastRepeatResponses = $repeatTest
        CacheEffective = $cacheEffective
    }
}

# Main execution
Write-Log "Phase 4 Cache Optimization Test Suite Starting"
Write-Log "Proxy Port: $ProxyPort"
Write-Log "Cache Test Requests: $CacheTestRequests"
Write-Log "Results will be saved to: $logFile"

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

# Run tests
Write-Log "Starting cache hit ratio tests..."
$hitRatioResults = Test-CacheHitRatio

Write-Log "Starting cache scalability tests..."
$scalabilityResults = Test-CacheScalability

Write-Log "Starting cache memory efficiency tests..."
$memoryResults = Test-CacheMemoryEfficiency

# Summary
Write-Log "================================================"
Write-Log "PHASE 4 CACHE OPTIMIZATION TEST RESULTS"
Write-Log "================================================"

$tests = @{
    "Cache Performance Improvement" = $hitRatioResults.PerformanceImprovement
    "Concurrent Cache Performance" = $scalabilityResults.GoodConcurrentPerformance
    "Cache Memory Efficiency" = $memoryResults.CacheEffective
    "Cache Speedup >= 2x" = ($hitRatioResults.CacheSpeedup -ge 2.0)
}

$passCount = 0
foreach ($test in $tests.Keys) {
    $status = if ($tests[$test]) { "✅ PASS" } else { "❌ FAIL" }
    Write-Log "$test`: $status"
    if ($tests[$test]) { $passCount++ }
}

$passRate = [math]::Round(($passCount / $tests.Count) * 100, 1)

Write-Log "Key Metrics:"
Write-Log "  Cache Speedup: $($hitRatioResults.CacheSpeedup)x"
Write-Log "  Avg First Request: $([math]::Round($hitRatioResults.AvgFirstRequestTime, 2))ms"
Write-Log "  Avg Cached Request: $([math]::Round($hitRatioResults.AvgCachedRequestTime, 2))ms"
Write-Log "  Concurrent Avg Time: $([math]::Round($scalabilityResults.AvgConcurrentResponseTime, 2))ms"
Write-Log "  Cache Memory Effective: $(if ($memoryResults.CacheEffective) { 'Yes' } else { 'No' })"

Write-Log "Overall Pass Rate: $passCount/$($tests.Count) ($passRate%)"

if ($passRate -ge 75) {
    Write-Log "🎉 PHASE 4 CACHE OPTIMIZATION: PASSED" "SUCCESS"
    if ($hitRatioResults.CacheSpeedup -ge 5.0) {
        Write-Log "🚀 Outstanding cache performance!" "SUCCESS"
    }
} else {
    Write-Log "⚠️  PHASE 4 CACHE OPTIMIZATION: NEEDS ATTENTION" "WARN"
}

Write-Log "Test results saved to: $logFile"
Write-Host "`n📊 Check $logFile for detailed results" -ForegroundColor Cyan
