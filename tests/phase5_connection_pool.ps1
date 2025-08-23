# Phase 5: Connection Pooling Tests
# Tests connection reuse and pooling efficiency

param(
    [string]$ProxyPort = "8080",
    [int]$PoolTestConnections = 25,
    [switch]$Verbose
)

Write-Host "🔌 PHASE 5: Connection Pooling Tests" -ForegroundColor Blue
Write-Host "====================================" -ForegroundColor Blue

$testDir = Split-Path $MyInvocation.MyCommand.Path
$rootDir = Split-Path $testDir
$resultsDir = Join-Path $testDir "test_results"
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$logFile = Join-Path $resultsDir "phase5_connection_pooling_$timestamp.log"

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

function Test-ConnectionReuse {
    Write-Log "Testing connection reuse efficiency"
    
    # Test with single server to maximize reuse potential
    $testServer = "httpbin.org"
    $testPaths = @("/json", "/html", "/xml", "/status/200", "/user-agent")
    
    $reuseStats = @{
        TotalRequests = 0
        RequestTimes = @()
        FirstConnectionTimes = @()
        PooledConnectionTimes = @()
    }
    
    # First request to establish connection
    Write-Log "Phase 1: Establishing initial connections"
    foreach ($path in $testPaths) {
        $url = "http://$testServer$path"
        $startTime = Get-Date
        
        try {
            $response = curl -x "localhost:$ProxyPort" -m 15 -s -w "Time: %{time_total}s" $url 2>&1
            if ($LASTEXITCODE -eq 0) {
                $requestTime = (Get-Date) - $startTime
                $reuseStats.FirstConnectionTimes += $requestTime.TotalMilliseconds
                $reuseStats.TotalRequests++
                Write-Log "  First connection to $testServer$path`: $($requestTime.TotalMilliseconds)ms"
            }
        } catch {
            Write-Log "  Failed initial connection to $url" "WARN"
        }
        Start-Sleep -Milliseconds 200  # Small delay to ensure connection pooling
    }
    
    # Rapid requests to same server (should reuse connections)
    Write-Log "Phase 2: Testing connection reuse (rapid requests)"
    for ($round = 1; $round -le 3; $round++) {
        Write-Log "  Round $round - Testing pooled connections"
        foreach ($path in $testPaths) {
            $url = "http://$testServer$path"
            $startTime = Get-Date
            
            try {
                $response = curl -x "localhost:$ProxyPort" -m 10 -s $url 2>&1
                if ($LASTEXITCODE -eq 0) {
                    $requestTime = (Get-Date) - $startTime
                    $reuseStats.PooledConnectionTimes += $requestTime.TotalMilliseconds
                    $reuseStats.TotalRequests++
                    if ($Verbose) {
                        Write-Log "    Pooled connection to $testServer$path`: $($requestTime.TotalMilliseconds)ms"
                    }
                }
            } catch {
                Write-Log "    Failed pooled connection to $url" "WARN"
            }
            Start-Sleep -Milliseconds 50  # Minimal delay for rapid reuse
        }
    }
    
    # Calculate connection reuse efficiency
    $avgFirstConnection = if ($reuseStats.FirstConnectionTimes.Count -gt 0) {
        ($reuseStats.FirstConnectionTimes | Measure-Object -Average).Average
    } else { 0 }
    
    $avgPooledConnection = if ($reuseStats.PooledConnectionTimes.Count -gt 0) {
        ($reuseStats.PooledConnectionTimes | Measure-Object -Average).Average
    } else { 0 }
    
    $connectionSpeedup = if ($avgPooledConnection -gt 0) {
        [math]::Round($avgFirstConnection / $avgPooledConnection, 2)
    } else { 0 }
    
    Write-Log "Connection Reuse Analysis:"
    Write-Log "  Total Requests: $($reuseStats.TotalRequests)"
    Write-Log "  Avg First Connection Time: $([math]::Round($avgFirstConnection, 2))ms"
    Write-Log "  Avg Pooled Connection Time: $([math]::Round($avgPooledConnection, 2))ms"
    Write-Log "  Connection Reuse Speedup: ${connectionSpeedup}x faster"
    
    return @{
        TotalRequests = $reuseStats.TotalRequests
        AvgFirstConnectionTime = $avgFirstConnection
        AvgPooledConnectionTime = $avgPooledConnection
        ConnectionSpeedup = $connectionSpeedup
        EfficientReuse = ($connectionSpeedup -ge 1.5)  # Expect at least 1.5x improvement
    }
}

function Test-PoolConcurrency {
    Write-Log "Testing connection pool under concurrent load"
    
    # Test concurrent requests to same server
    $testServer = "httpbin.org"
    $concurrentWorkers = 8
    $requestsPerWorker = 5
    
    Write-Log "Starting $concurrentWorkers concurrent workers, $requestsPerWorker requests each to $testServer"
    
    $concurrentJobs = @()
    for ($i = 1; $i -le $concurrentWorkers; $i++) {
        $job = Start-Job -ScriptBlock {
            param($ProxyPort, $TestServer, $RequestCount, $WorkerId)
            
            $workerResults = @{
                WorkerId = $WorkerId
                RequestTimes = @()
                SuccessCount = 0
                ConnectionErrors = 0
            }
            
            for ($j = 1; $j -le $RequestCount; $j++) {
                $url = "http://$TestServer/json?worker=$WorkerId&request=$j"
                $startTime = Get-Date
                
                try {
                    $response = curl -x "localhost:$ProxyPort" -m 15 -s $url 2>&1
                    if ($LASTEXITCODE -eq 0) {
                        $requestTime = (Get-Date) - $startTime
                        $workerResults.RequestTimes += $requestTime.TotalMilliseconds
                        $workerResults.SuccessCount++
                    } else {
                        $workerResults.ConnectionErrors++
                    }
                } catch {
                    $workerResults.ConnectionErrors++
                }
                Start-Sleep -Milliseconds 100
            }
            
            return $workerResults
        } -ArgumentList $ProxyPort, $testServer, $requestsPerWorker, $i
        
        $concurrentJobs += $job
    }
    
    # Wait for completion
    $results = $concurrentJobs | Wait-Job | Receive-Job
    $concurrentJobs | Remove-Job
    
    # Analyze concurrent pool performance
    $allRequestTimes = $results | ForEach-Object { $_.RequestTimes }
    $totalSuccessful = ($results | Measure-Object -Property SuccessCount -Sum).Sum
    $totalErrors = ($results | Measure-Object -Property ConnectionErrors -Sum).Sum
    
    $avgConcurrentTime = if ($allRequestTimes.Count -gt 0) {
        ($allRequestTimes | Measure-Object -Average).Average
    } else { 0 }
    
    $maxTime = if ($allRequestTimes.Count -gt 0) {
        ($allRequestTimes | Measure-Object -Maximum).Maximum
    } else { 0 }
    
    $minTime = if ($allRequestTimes.Count -gt 0) {
        ($allRequestTimes | Measure-Object -Minimum).Minimum
    } else { 0 }
    
    $successRate = if (($totalSuccessful + $totalErrors) -gt 0) {
        [math]::Round(($totalSuccessful / ($totalSuccessful + $totalErrors)) * 100, 1)
    } else { 0 }
    
    Write-Log "Concurrent Connection Pool Performance:"
    Write-Log "  Total Successful Requests: $totalSuccessful"
    Write-Log "  Total Connection Errors: $totalErrors"
    Write-Log "  Success Rate: $successRate%"
    Write-Log "  Average Response Time: $([math]::Round($avgConcurrentTime, 2))ms"
    Write-Log "  Min/Max Response Time: $([math]::Round($minTime, 2))ms / $([math]::Round($maxTime, 2))ms"
    
    $goodConcurrentPool = ($successRate -ge 90) -and ($avgConcurrentTime -lt 1000) -and ($totalErrors -le 2)
    
    return @{
        TotalSuccessfulRequests = $totalSuccessful
        TotalConnectionErrors = $totalErrors
        SuccessRate = $successRate
        AvgConcurrentResponseTime = $avgConcurrentTime
        MaxResponseTime = $maxTime
        MinResponseTime = $minTime
        GoodConcurrentPool = $goodConcurrentPool
    }
}

function Test-PoolScalability {
    Write-Log "Testing connection pool scalability with multiple servers"
    
    # Test with multiple different servers to verify pool management
    $testServers = @(
        "httpbin.org",
        "jsonplaceholder.typicode.com",
        "api.github.com"
    )
    
    $scalabilityStats = @{
        ServersUsed = $testServers.Count
        RequestsPerServer = 0
        TotalRequests = 0
        ResponseTimes = @()
        ServerResponseTimes = @{}
    }
    
    # Initialize server response tracking
    foreach ($server in $testServers) {
        $scalabilityStats.ServerResponseTimes[$server] = @()
    }
    
    Write-Log "Testing pool scalability across $($testServers.Count) different servers"
    
    # Test multiple requests per server
    $requestsPerServer = 4
    for ($round = 1; $round -le 2; $round++) {
        Write-Log "  Round $round - Testing cross-server connection pooling"
        
        foreach ($server in $testServers) {
            for ($req = 1; $req -le $requestsPerServer; $req++) {
                $url = switch ($server) {
                    "httpbin.org" { "http://$server/json" }
                    "jsonplaceholder.typicode.com" { "http://$server/posts/1" }
                    "api.github.com" { "http://$server/users/octocat" }
                }
                
                $startTime = Get-Date
                try {
                    $response = curl -x "localhost:$ProxyPort" -m 15 -s -H "User-Agent: PoolTest" $url 2>&1
                    if ($LASTEXITCODE -eq 0) {
                        $requestTime = (Get-Date) - $startTime
                        $scalabilityStats.ResponseTimes += $requestTime.TotalMilliseconds
                        $scalabilityStats.ServerResponseTimes[$server] += $requestTime.TotalMilliseconds
                        $scalabilityStats.TotalRequests++
                        
                        if ($Verbose) {
                            Write-Log "    $server request $req`: $($requestTime.TotalMilliseconds)ms"
                        }
                    }
                } catch {
                    Write-Log "    Failed request to $server" "WARN"
                }
                Start-Sleep -Milliseconds 150
            }
        }
    }
    
    $scalabilityStats.RequestsPerServer = $requestsPerServer * 2  # 2 rounds
    
    # Analyze cross-server performance
    $avgResponseTime = if ($scalabilityStats.ResponseTimes.Count -gt 0) {
        ($scalabilityStats.ResponseTimes | Measure-Object -Average).Average
    } else { 0 }
    
    Write-Log "Cross-Server Pool Scalability:"
    Write-Log "  Servers tested: $($scalabilityStats.ServersUsed)"
    Write-Log "  Requests per server: $($scalabilityStats.RequestsPerServer)"
    Write-Log "  Total successful requests: $($scalabilityStats.TotalRequests)"
    Write-Log "  Average response time: $([math]::Round($avgResponseTime, 2))ms"
    
    # Check individual server performance
    foreach ($server in $testServers) {
        $serverTimes = $scalabilityStats.ServerResponseTimes[$server]
        if ($serverTimes.Count -gt 0) {
            $avgServerTime = ($serverTimes | Measure-Object -Average).Average
            Write-Log "    $server`: $([math]::Round($avgServerTime, 2))ms avg"
        }
    }
    
    $goodScalability = ($scalabilityStats.TotalRequests -ge ($testServers.Count * $requestsPerServer * 2 * 0.8)) -and ($avgResponseTime -lt 2000)
    
    return @{
        ServersUsed = $scalabilityStats.ServersUsed
        RequestsPerServer = $scalabilityStats.RequestsPerServer
        TotalRequests = $scalabilityStats.TotalRequests
        AvgResponseTime = $avgResponseTime
        GoodScalability = $goodScalability
    }
}

function Test-PoolTimeout {
    Write-Log "Testing connection pool timeout and cleanup"
    
    # Make requests then wait to test timeout behavior
    $testUrl = "http://httpbin.org/delay/1"  # 1 second delay
    $timeoutStats = @{
        InitialRequests = 0
        AfterTimeoutRequests = 0
        InitialTime = 0
        AfterTimeoutTime = 0
    }
    
    Write-Log "Phase 1: Creating pooled connections"
    for ($i = 1; $i -le 3; $i++) {
        $startTime = Get-Date
        try {
            $response = curl -x "localhost:$ProxyPort" -m 20 -s $testUrl 2>&1
            if ($LASTEXITCODE -eq 0) {
                $requestTime = (Get-Date) - $startTime
                $timeoutStats.InitialTime += $requestTime.TotalMilliseconds
                $timeoutStats.InitialRequests++
                Write-Log "  Initial pooled connection $i`: $($requestTime.TotalMilliseconds)ms"
            }
        } catch {
            Write-Log "  Failed initial connection $i" "WARN"
        }
        Start-Sleep -Milliseconds 300
    }
    
    # Wait longer than keep-alive timeout (30 seconds in our config)
    Write-Log "Phase 2: Waiting for connection timeout (35 seconds)..."
    Start-Sleep -Seconds 35
    
    Write-Log "Phase 3: Testing connections after timeout"
    for ($i = 1; $i -le 3; $i++) {
        $startTime = Get-Date
        try {
            $response = curl -x "localhost:$ProxyPort" -m 20 -s $testUrl 2>&1
            if ($LASTEXITCODE -eq 0) {
                $requestTime = (Get-Date) - $startTime
                $timeoutStats.AfterTimeoutTime += $requestTime.TotalMilliseconds
                $timeoutStats.AfterTimeoutRequests++
                Write-Log "  Post-timeout connection $i`: $($requestTime.TotalMilliseconds)ms"
            }
        } catch {
            Write-Log "  Failed post-timeout connection $i" "WARN"
        }
        Start-Sleep -Milliseconds 300
    }
    
    # Analyze timeout behavior
    $avgInitialTime = if ($timeoutStats.InitialRequests -gt 0) {
        $timeoutStats.InitialTime / $timeoutStats.InitialRequests
    } else { 0 }
    
    $avgAfterTimeoutTime = if ($timeoutStats.AfterTimeoutRequests -gt 0) {
        $timeoutStats.AfterTimeoutTime / $timeoutStats.AfterTimeoutRequests
    } else { 0 }
    
    Write-Log "Connection Pool Timeout Analysis:"
    Write-Log "  Initial requests completed: $($timeoutStats.InitialRequests)"
    Write-Log "  Post-timeout requests completed: $($timeoutStats.AfterTimeoutRequests)"
    Write-Log "  Avg initial response time: $([math]::Round($avgInitialTime, 2))ms"
    Write-Log "  Avg post-timeout response time: $([math]::Round($avgAfterTimeoutTime, 2))ms"
    
    # Post-timeout requests should be slightly slower (new connections) but still work
    $timeoutWorking = ($timeoutStats.AfterTimeoutRequests -ge 2) -and ($avgAfterTimeoutTime -gt 0)
    
    return @{
        InitialRequests = $timeoutStats.InitialRequests
        AfterTimeoutRequests = $timeoutStats.AfterTimeoutRequests
        AvgInitialTime = $avgInitialTime
        AvgAfterTimeoutTime = $avgAfterTimeoutTime
        TimeoutWorking = $timeoutWorking
    }
}

# Main execution
Write-Log "Phase 5 Connection Pooling Test Suite Starting"
Write-Log "Proxy Port: $ProxyPort"
Write-Log "Pool Test Connections: $PoolTestConnections"
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
Write-Log "Starting connection reuse tests..."
$reuseResults = Test-ConnectionReuse

Write-Log "Starting pool concurrency tests..."
$concurrencyResults = Test-PoolConcurrency

Write-Log "Starting pool scalability tests..."
$scalabilityResults = Test-PoolScalability

Write-Log "Starting pool timeout tests..."
$timeoutResults = Test-PoolTimeout

# Summary
Write-Log "================================================"
Write-Log "PHASE 5 CONNECTION POOLING TEST RESULTS"
Write-Log "================================================"

$tests = @{
    "Connection Reuse Efficiency" = $reuseResults.EfficientReuse
    "Concurrent Pool Performance" = $concurrencyResults.GoodConcurrentPool
    "Cross-Server Scalability" = $scalabilityResults.GoodScalability
    "Pool Timeout Management" = $timeoutResults.TimeoutWorking
    "Connection Speedup >= 1.5x" = ($reuseResults.ConnectionSpeedup -ge 1.5)
}

$passCount = 0
foreach ($test in $tests.Keys) {
    $status = if ($tests[$test]) { "✅ PASS" } else { "❌ FAIL" }
    Write-Log "$test`: $status"
    if ($tests[$test]) { $passCount++ }
}

$passRate = [math]::Round(($passCount / $tests.Count) * 100, 1)

Write-Log "Key Metrics:"
Write-Log "  Connection Reuse Speedup: $($reuseResults.ConnectionSpeedup)x"
Write-Log "  Concurrent Success Rate: $($concurrencyResults.SuccessRate)%"
Write-Log "  Cross-Server Requests: $($scalabilityResults.TotalRequests)"
Write-Log "  Pool Timeout Working: $(if ($timeoutResults.TimeoutWorking) { 'Yes' } else { 'No' })"
Write-Log "  Avg First Connection: $([math]::Round($reuseResults.AvgFirstConnectionTime, 2))ms"
Write-Log "  Avg Pooled Connection: $([math]::Round($reuseResults.AvgPooledConnectionTime, 2))ms"

Write-Log "Overall Pass Rate: $passCount/$($tests.Count) ($passRate%)"

if ($passRate -ge 75) {
    Write-Log "🎉 PHASE 5 CONNECTION POOLING: PASSED" "SUCCESS"
    if ($reuseResults.ConnectionSpeedup -ge 3.0) {
        Write-Log "🚀 Excellent connection pooling performance!" "SUCCESS"
    }
} else {
    Write-Log "⚠️  PHASE 5 CONNECTION POOLING: NEEDS ATTENTION" "WARN"
}

Write-Log "Test results saved to: $logFile"
Write-Host "`n📊 Check $logFile for detailed results" -ForegroundColor Cyan
