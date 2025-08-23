# PHASE 6: Modular Architecture Tests
# Tests code organization, maintainability, and functionality

param(
    [string]$ProxyPort = "8080",
    [switch]$Verbose
)

Write-Host "📁 PHASE 6: Modular Architecture Tests" -ForegroundColor Magenta
Write-Host "=======================================" -ForegroundColor Magenta

$testDir = Split-Path $MyInvocation.MyCommand.Path
$rootDir = Split-Path $testDir
$resultsDir = Join-Path $testDir "test_results"
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$logFile = Join-Path $resultsDir "phase6_modular_$timestamp.log"

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

function Test-ModularCompilation {
    Write-Log "Testing modular compilation"
    
    $makefileExists = Test-Path (Join-Path $rootDir "Makefile_modular")
    if (-not $makefileExists) {
        Write-Log "Makefile_modular not found" "ERROR"
        return $false
    }
    
    # Check if all header files exist
    $requiredHeaders = @(
        "include/platform.h",
        "include/http_parser.h", 
        "include/thread_pool.h",
        "include/connection_pool.h",
        "include/cache.h",
        "include/proxy_server.h"
    )
    
    $missingHeaders = @()
    foreach ($header in $requiredHeaders) {
        $headerPath = Join-Path $rootDir $header
        if (-not (Test-Path $headerPath)) {
            $missingHeaders += $header
        }
    }
    
    if ($missingHeaders.Count -gt 0) {
        Write-Log "Missing header files: $($missingHeaders -join ', ')" "ERROR"
        return $false
    }
    
    # Check if all source files exist
    $requiredSources = @(
        "src/platform.c",
        "src/http_parser.c",
        "src/thread_pool.c", 
        "src/connection_pool.c",
        "src/cache.c",
        "src/proxy_server.c",
        "src/proxy_server_modular.c"
    )
    
    $missingSources = @()
    foreach ($source in $requiredSources) {
        $sourcePath = Join-Path $rootDir $source
        if (-not (Test-Path $sourcePath)) {
            $missingSources += $source
        }
    }
    
    if ($missingSources.Count -gt 0) {
        Write-Log "Missing source files: $($missingSources -join ', ')" "ERROR"
        return $false
    }
    
    Write-Log "All modular files present: $($requiredHeaders.Count + $requiredSources.Count) files"
    return $true
}

function Test-CodeOrganization {
    Write-Log "Testing code organization and modularity"
    
    $organizationScore = 0
    $maxScore = 6
    
    # Test 1: Platform abstraction
    $platformHeaderPath = Join-Path $rootDir "include/platform.h"
    if (Test-Path $platformHeaderPath) {
        $platformContent = Get-Content $platformHeaderPath -Raw
        if ($platformContent -match "#ifdef _WIN32" -and $platformContent -match "void platform_init") {
            $organizationScore++
            Write-Log "  Platform abstraction: Well organized"
        }
    }
    
    # Test 2: Thread pool separation
    $threadPoolPath = Join-Path $rootDir "src/thread_pool.c"
    if (Test-Path $threadPoolPath) {
        $threadContent = Get-Content $threadPoolPath -Raw
        if ($threadContent -match "thread_pool_create" -and $threadContent -match "worker_thread") {
            $organizationScore++
            Write-Log "  Thread pool module: Well separated"
        }
    }
    
    # Test 3: Cache module isolation
    $cachePath = Join-Path $rootDir "src/cache.c"
    if (Test-Path $cachePath) {
        $cacheContent = Get-Content $cachePath -Raw
        if ($cacheContent -match "cache_hash" -and $cacheContent -match "cache_create") {
            $organizationScore++
            Write-Log "  Cache module: Properly isolated"
        }
    }
    
    # Test 4: Connection pool module
    $connPoolPath = Join-Path $rootDir "src/connection_pool.c"
    if (Test-Path $connPoolPath) {
        $connContent = Get-Content $connPoolPath -Raw
        if ($connContent -match "connection_pool_get" -and $connContent -match "connection_pool_return") {
            $organizationScore++
            Write-Log "  Connection pool module: Well structured"
        }
    }
    
    # Test 5: Main server logic separation
    $mainServerPath = Join-Path $rootDir "src/proxy_server.c"
    if (Test-Path $mainServerPath) {
        $serverContent = Get-Content $mainServerPath -Raw
        if ($serverContent -match "proxy_server_init" -and $serverContent -match "handle_client_request") {
            $organizationScore++
            Write-Log "  Main server logic: Cleanly separated"
        }
    }
    
    # Test 6: Header inclusion structure
    $modularMainPath = Join-Path $rootDir "src/proxy_server_modular.c"
    if (Test-Path $modularMainPath) {
        $mainContent = Get-Content $modularMainPath -Raw
        if ($mainContent -match '#include "../include/proxy_server.h"' -and $mainContent.Length -lt 2000) {
            $organizationScore++
            Write-Log "  Main file: Clean and modular"
        }
    }
    
    $organizationPercentage = [math]::Round(($organizationScore / $maxScore) * 100, 1)
    Write-Log "Code organization score: $organizationScore/$maxScore ($organizationPercentage%)"
    
    return @{
        Score = $organizationScore
        MaxScore = $maxScore
        Percentage = $organizationPercentage
        WellOrganized = ($organizationPercentage -ge 80)
    }
}

function Test-ModularFunctionality {
    Write-Log "Testing modular version functionality"
    
    # Try to build the modular version
    Write-Log "Attempting to build modular version..."
    
    $originalLocation = Get-Location
    try {
        Set-Location $rootDir
        
        # Build using Windows gcc (since we're on Windows)
        $buildCommand = "gcc -Wall -Wextra -pthread -std=c99 -Iinclude src/platform.c src/http_parser.c src/thread_pool.c src/connection_pool.c src/cache.c src/proxy_server.c src/proxy_server_modular.c -lpthread -lws2_32 -o proxy_server_modular.exe"
        
        Write-Log "Build command: $buildCommand"
        
        $buildResult = cmd /c "$buildCommand 2>&1"
        $buildExitCode = $LASTEXITCODE
        
        if ($buildExitCode -eq 0) {
            Write-Log "Modular version built successfully"
            
            # Check if executable exists
            if (Test-Path "proxy_server_modular.exe") {
                Write-Log "Executable created: proxy_server_modular.exe"
                return @{
                    BuildSuccessful = $true
                    ExecutableExists = $true
                    BuildOutput = $buildResult
                }
            } else {
                Write-Log "Build reported success but executable not found" "WARN"
                return @{
                    BuildSuccessful = $true
                    ExecutableExists = $false
                    BuildOutput = $buildResult
                }
            }
        } else {
            Write-Log "Build failed with exit code: $buildExitCode" "ERROR"
            Write-Log "Build output: $buildResult" "ERROR"
            return @{
                BuildSuccessful = $false
                ExecutableExists = $false
                BuildOutput = $buildResult
            }
        }
    }
    finally {
        Set-Location $originalLocation
    }
}

function Test-CodeReadability {
    Write-Log "Testing code readability improvements"
    
    # Compare line counts
    $originalFile = Join-Path $rootDir "src/proxy_server_with_cache.c"
    $modularMain = Join-Path $rootDir "src/proxy_server_modular.c"
    
    $readabilityMetrics = @{
        OriginalLines = 0
        ModularMainLines = 0
        TotalModularLines = 0
        LineReduction = 0
        FilesCreated = 0
    }
    
    if (Test-Path $originalFile) {
        $originalContent = Get-Content $originalFile
        $readabilityMetrics.OriginalLines = $originalContent.Count
    }
    
    if (Test-Path $modularMain) {
        $modularContent = Get-Content $modularMain
        $readabilityMetrics.ModularMainLines = $modularContent.Count
    }
    
    # Count total lines in all modular files
    $modularFiles = @(
        "src/platform.c",
        "src/http_parser.c",
        "src/thread_pool.c",
        "src/connection_pool.c", 
        "src/cache.c",
        "src/proxy_server.c",
        "src/proxy_server_modular.c"
    )
    
    foreach ($file in $modularFiles) {
        $filePath = Join-Path $rootDir $file
        if (Test-Path $filePath) {
            $content = Get-Content $filePath
            $readabilityMetrics.TotalModularLines += $content.Count
            $readabilityMetrics.FilesCreated++
        }
    }
    
    if ($readabilityMetrics.OriginalLines -gt 0) {
        $readabilityMetrics.LineReduction = [math]::Round(
            (1 - ($readabilityMetrics.ModularMainLines / $readabilityMetrics.OriginalLines)) * 100, 1
        )
    }
    
    Write-Log "Readability Analysis:"
    Write-Log "  Original monolithic file: $($readabilityMetrics.OriginalLines) lines"
    Write-Log "  New main file: $($readabilityMetrics.ModularMainLines) lines"
    Write-Log "  Total modular code: $($readabilityMetrics.TotalModularLines) lines"
    Write-Log "  Main file size reduction: $($readabilityMetrics.LineReduction)%"
    Write-Log "  Number of modules created: $($readabilityMetrics.FilesCreated)"
    
    $readabilityImproved = ($readabilityMetrics.LineReduction -ge 80) -and ($readabilityMetrics.FilesCreated -ge 6)
    
    return @{
        Metrics = $readabilityMetrics
        ReadabilityImproved = $readabilityImproved
    }
}

# Main execution
Write-Log "Phase 6 Modular Architecture Test Suite Starting"
Write-Log "Results will be saved to: $logFile"

# Run tests
Write-Log "Testing modular compilation setup..."
$compilationTest = Test-ModularCompilation

Write-Log "Testing code organization..."
$organizationTest = Test-CodeOrganization

Write-Log "Testing modular functionality..."
$functionalityTest = Test-ModularFunctionality

Write-Log "Testing code readability improvements..."
$readabilityTest = Test-CodeReadability

# Summary
Write-Log "================================================"
Write-Log "PHASE 6 MODULAR ARCHITECTURE TEST RESULTS"
Write-Log "================================================"

$tests = @{
    "Modular File Structure" = $compilationTest
    "Code Organization" = $organizationTest.WellOrganized
    "Build Functionality" = $functionalityTest.BuildSuccessful
    "Readability Improvement" = $readabilityTest.ReadabilityImproved
}

$passCount = 0
foreach ($test in $tests.Keys) {
    $status = if ($tests[$test]) { "✅ PASS" } else { "❌ FAIL" }
    Write-Log "$test`: $status"
    if ($tests[$test]) { $passCount++ }
}

$passRate = [math]::Round(($passCount / $tests.Count) * 100, 1)

Write-Log "Key Metrics:"
Write-Log "  Code Organization Score: $($organizationTest.Score)/$($organizationTest.MaxScore)"
Write-Log "  Main File Size Reduction: $($readabilityTest.Metrics.LineReduction)%"
Write-Log "  Modules Created: $($readabilityTest.Metrics.FilesCreated)"
Write-Log "  Build Status: $(if ($functionalityTest.BuildSuccessful) { 'Success' } else { 'Failed' })"

Write-Log "Overall Pass Rate: $passCount/$($tests.Count) ($passRate%)"

if ($passRate -ge 75) {
    Write-Log "🎉 PHASE 6 MODULAR ARCHITECTURE: PASSED" "SUCCESS"
    if ($readabilityTest.Metrics.LineReduction -ge 90) {
        Write-Log "🚀 Excellent code organization and readability!" "SUCCESS"
    }
} else {
    Write-Log "⚠️  PHASE 6 MODULAR ARCHITECTURE: NEEDS ATTENTION" "WARN"
}

Write-Log "Test results saved to: $logFile"
Write-Host "`n📊 Check $logFile for detailed results" -ForegroundColor Cyan
