# Phase 2: Thread Pool Performance Tests
# Tests concurrent connection handling and thread pool efficiency

param(
    [string]$ProxyPort = "8080",
    [int]$ConcurrentConnections = 10,
    [int]$TotalRequests = 100,
    [switch]$Verbose
)

Write-Host "🧵 PHASE 2: Thread Pool Performance Tests" -ForegroundColor Blue
Write-Host "==========================================" -ForegroundColor Blue

$testDir = Split-Path $MyInvocation.MyCommand.Path
$rootDir = Split-Path $testDir
$resultsDir = Join-Path $testDir "test_results"
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$logFile = Join-Path $resultsDir "phase2_thread_pool_$timestamp.log"

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

function Measure-ConcurrentPerformance {
    param($ConnectionCount, $RequestsPerConnection)
    
    Write-Log "Testing $ConnectionCount concurrent connections, $RequestsPerConnection requests each"
    
    $jobs = @()
    $startTime = Get-Date
    
    # Create concurrent jobs
    for ($i = 1; $i -le $ConnectionCount; $i++) {
        $job = Start-Job -ScriptBlock {
            param($ProxyPort, $RequestCount, $WorkerId)
            
            $results = @{
                WorkerId = $WorkerId
                SuccessCount = 0
                FailureCount = 0
                TotalTime = 0
                ResponseTimes = @()
            }
            
            for ($j = 1; $j -le $RequestCount; $j++) {
                $requestStart = Get-Date
                try {
                    $response = curl -x "localhost:$ProxyPort" -m 10 -s "http://httpbin.org/status/200" 2>&1
                    if ($LASTEXITCODE -eq 0) {
                        $results.SuccessCount++
                    } else {
                        $results.FailureCount++
                    }
                } catch {
                    $results.FailureCount++
                }
                $requestTime = (Get-Date) - $requestStart
                $results.ResponseTimes += $requestTime.TotalMilliseconds
                
                # Small delay to prevent overwhelming
                Start-Sleep -Milliseconds 50
            }
            
            $results.TotalTime = (Get-Date) - $requestStart
            return $results
        } -ArgumentList $ProxyPort, $RequestsPerConnection, $i
        
        $jobs += $job
    }
    
    # Wait for all jobs to complete
    Write-Log "Waiting for $ConnectionCount concurrent workers to complete..."
    $results = $jobs | Wait-Job | Receive-Job
    $jobs | Remove-Job
    
    $endTime = Get-Date
    $totalDuration = $endTime - $startTime
    
    # Analyze results
    $totalSuccess = ($results | Measure-Object -Property SuccessCount -Sum).Sum
    $totalFailures = ($results | Measure-Object -Property FailureCount -Sum).Sum
    $allResponseTimes = $results | ForEach-Object { $_.ResponseTimes } | Where-Object { $_ -gt 0 }
    
    $avgResponseTime = if ($allResponseTimes.Count -gt 0) { 
        ($allResponseTimes | Measure-Object -Average).Average 
    } else { 0 }
    
    return @{
        TotalRequests = $totalSuccess + $totalFailures
        SuccessCount = $totalSuccess
        FailureCount = $totalFailures
        SuccessRate = if (($totalSuccess + $totalFailures) -gt 0) { 
            [math]::Round(($totalSuccess / ($totalSuccess + $totalFailures)) * 100, 1) 
        } else { 0 }
        TotalDuration = $totalDuration.TotalSeconds
        AvgResponseTime = [math]::Round($avgResponseTime, 2)
        RequestsPerSecond = if ($totalDuration.TotalSeconds -gt 0) { 
            [math]::Round($totalSuccess / $totalDuration.TotalSeconds, 2) 
        } else { 0 }
    }
}

function Test-ThreadPoolEfficiency {
    Write-Log "Testing thread pool efficiency vs sequential processing"
    
    # Test 1: Sequential baseline (1 connection at a time)
    Write-Log "Baseline Test: Sequential processing"
    $sequentialResults = Measure-ConcurrentPerformance -ConnectionCount 1 -RequestsPerConnection 20
    
    Write-Log "Sequential Results:"
    Write-Log "  Success Rate: $($sequentialResults.SuccessRate)%"
    Write-Log "  Avg Response Time: $($sequentialResults.AvgResponseTime)ms"
    Write-Log "  Requests/sec: $($sequentialResults.RequestsPerSecond)"
    Write-Log "  Total Duration: $($sequentialResults.TotalDuration)s"
    
    # Test 2: Concurrent processing (thread pool)
    Write-Log "Performance Test: Concurrent processing with thread pool"
    $concurrentResults = Measure-ConcurrentPerformance -ConnectionCount $ConcurrentConnections -RequestsPerConnection ([math]::Floor(20 / $ConcurrentConnections))
    
    Write-Log "Concurrent Results:"
    Write-Log "  Success Rate: $($concurrentResults.SuccessRate)%"
    Write-Log "  Avg Response Time: $($concurrentResults.AvgResponseTime)ms"
    Write-Log "  Requests/sec: $($concurrentResults.RequestsPerSecond)"
    Write-Log "  Total Duration: $($concurrentResults.TotalDuration)s"
    
    # Calculate improvement
    $throughputImprovement = if ($sequentialResults.RequestsPerSecond -gt 0) {
        [math]::Round($concurrentResults.RequestsPerSecond / $sequentialResults.RequestsPerSecond, 2)
    } else { 0 }
    
    $timeImprovement = if ($concurrentResults.TotalDuration -gt 0) {
        [math]::Round($sequentialResults.TotalDuration / $concurrentResults.TotalDuration, 2)
    } else { 0 }
    
    Write-Log "Performance Improvements:"
    Write-Log "  Throughput: ${throughputImprovement}x faster"
    Write-Log "  Time to completion: ${timeImprovement}x faster"
    
    return @{
        SequentialRequestsPerSec = $sequentialResults.RequestsPerSecond
        ConcurrentRequestsPerSec = $concurrentResults.RequestsPerSecond
        ThroughputImprovement = $throughputImprovement
        TimeImprovement = $timeImprovement
        ConcurrentSuccessRate = $concurrentResults.SuccessRate
        ThreadPoolEffective = ($throughputImprovement -ge 2.0)  # Expect at least 2x improvement
    }
}

function Test-ThreadPoolStability {
    Write-Log "Testing thread pool stability under sustained load"
    
    $sustainedResults = @()
    $rounds = 5
    
    for ($round = 1; $round -le $rounds; $round++) {
        Write-Log "Sustained load round $round/$rounds"
        $result = Measure-ConcurrentPerformance -ConnectionCount 5 -RequestsPerConnection 4
        $sustainedResults += $result
        
        Write-Log "  Round $round - Success Rate: $($result.SuccessRate)%, RPS: $($result.RequestsPerSecond)"
        Start-Sleep -Seconds 2  # Brief pause between rounds
    }
    
    # Check for stability (success rate shouldn't degrade significantly)
    $avgSuccessRate = ($sustainedResults | Measure-Object -Property SuccessRate -Average).Average
    $minSuccessRate = ($sustainedResults | Measure-Object -Property SuccessRate -Minimum).Minimum
    $maxSuccessRate = ($sustainedResults | Measure-Object -Property SuccessRate -Maximum).Maximum
    
    $successRateVariance = $maxSuccessRate - $minSuccessRate
    $isStable = ($avgSuccessRate -ge 80) -and ($successRateVariance -le 20)
    
    Write-Log "Stability Analysis:"
    Write-Log "  Average Success Rate: $([math]::Round($avgSuccessRate, 1))%"
    Write-Log "  Success Rate Range: $([math]::Round($minSuccessRate, 1))% - $([math]::Round($maxSuccessRate, 1))%"
    Write-Log "  Variance: $([math]::Round($successRateVariance, 1))%"
    Write-Log "  Stable: $(if ($isStable) { 'Yes' } else { 'No' })"
    
    return @{
        AverageSuccessRate = $avgSuccessRate
        SuccessRateVariance = $successRateVariance
        IsStable = $isStable
    }
}

# Main execution
Write-Log "Phase 2 Thread Pool Test Suite Starting"
Write-Log "Proxy Port: $ProxyPort"
Write-Log "Concurrent Connections: $ConcurrentConnections"
Write-Log "Total Requests: $TotalRequests"
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
Write-Log "Starting thread pool efficiency tests..."
$efficiencyResults = Test-ThreadPoolEfficiency

Write-Log "Starting thread pool stability tests..."
$stabilityResults = Test-ThreadPoolStability

# Summary
Write-Log "================================================"
Write-Log "PHASE 2 THREAD POOL TEST RESULTS"
Write-Log "================================================"

$tests = @{
    "Thread Pool Performance Improvement" = ($efficiencyResults.ThroughputImprovement -ge 2.0)
    "Concurrent Success Rate > 80%" = ($efficiencyResults.ConcurrentSuccessRate -ge 80)
    "System Stability Under Load" = $stabilityResults.IsStable
    "Thread Pool Efficiency" = $efficiencyResults.ThreadPoolEffective
}

$passCount = 0
foreach ($test in $tests.Keys) {
    $status = if ($tests[$test]) { "✅ PASS" } else { "❌ FAIL" }
    Write-Log "$test`: $status"
    if ($tests[$test]) { $passCount++ }
}

$passRate = [math]::Round(($passCount / $tests.Count) * 100, 1)

Write-Log "Key Metrics:"
Write-Log "  Throughput Improvement: $($efficiencyResults.ThroughputImprovement)x"
Write-Log "  Time Improvement: $($efficiencyResults.TimeImprovement)x"
Write-Log "  Concurrent Success Rate: $($efficiencyResults.ConcurrentSuccessRate)%"
Write-Log "  System Stability: $(if ($stabilityResults.IsStable) { 'Stable' } else { 'Unstable' })"

Write-Log "Overall Pass Rate: $passCount/$($tests.Count) ($passRate%)"

if ($passRate -ge 75) {
    Write-Log "🎉 PHASE 2 THREAD POOL: PASSED" "SUCCESS"
    if ($efficiencyResults.ThroughputImprovement -ge 3.0) {
        Write-Log "🚀 Exceptional performance improvement!" "SUCCESS"
    }
} else {
    Write-Log "⚠️  PHASE 2 THREAD POOL: NEEDS ATTENTION" "WARN"
}

Write-Log "Test results saved to: $logFile"
Write-Host "`n📊 Check $logFile for detailed results" -ForegroundColor Cyan
