# HTTP Proxy Server Tests

This directory contains comprehensive tests for each optimization phase of the HTTP proxy server.

## Test Structure

```
tests/
├── README.md                 # This file
├── phase1_memory_safety.ps1  # Phase 1: Memory safety tests
├── phase2_thread_pool.ps1    # Phase 2: Thread pool performance tests
├── phase3_platform.ps1      # Phase 3: Cross-platform compatibility tests
├── phase4_cache.ps1          # Phase 4: Cache optimization tests
├── phase5_connection_pool.ps1 # Phase 5: Connection pooling tests
├── phase6_modular.ps1        # Phase 6: Modular architecture tests
├── performance_benchmark.ps1  # Complete performance comparison
├── run_all_tests.ps1         # Master test runner
├── test_requests/            # Sample HTTP requests for testing
└── test_results/             # Test output logs
```

## Quick Start

1. **Build the modular proxy server:**
   ```powershell
   # Build modular version (recommended)
   make -f Makefile_modular
   
   # Or build original monolithic version
   gcc -o proxy_server src/proxy_server_with_cache.c -lpthread -lws2_32
   ```

2. **Run all tests:**
   ```powershell
   ./tests/performance_benchmark.ps1
   ```

3. **Run specific phase tests:**
   ```powershell
   ./tests/phase5_connection_pool.ps1
   ```

## Test Requirements

- Windows PowerShell 5.1+
- GCC compiler with pthread and ws2_32 support
- curl (for HTTP requests)
- Administrative privileges (for some network tests)

## Expected Results

- **Phase 1**: Memory safety validation (no crashes, proper cleanup)
- **Phase 2**: 3-4x performance improvement with thread pool
- **Phase 3**: Cross-platform compatibility verified
- **Phase 4**: 5-10x cache hit performance boost
- **Phase 5**: 5-10x connection reuse performance improvement
- **Phase 6**: Code modularization and readability improvements

## Architecture Validation

Each test validates the specific architectural improvements:

1. **Memory Safety**: Proper allocation/deallocation, bounds checking
2. **Threading**: Worker thread efficiency, queue management
3. **Platform**: Windows/Linux compatibility layer
4. **Caching**: O(1) hash table lookups vs O(n) linear search
5. **Connections**: Persistent connection reuse vs new connection per request


Example Usage:

```powershell
# Start your proxy server first
gcc -o proxy_server src/proxy_server_with_cache.c -lpthread -lws2_32
./proxy_server 8080

# Run all tests
./tests/run_all_tests.ps1

# Quick performance benchmark
./tests/performance_benchmark.ps1 -QuickMode

# Test specific optimization phase
./tests/phase5_connection_pool.ps1 -Verbose
```


What the Tests Validate:
✅ Memory Safety - No crashes, proper cleanup, buffer overflow protection
✅ Thread Pool - 3-4x concurrent performance improvement
✅ Cross-Platform - Windows/Linux compatibility (Phase 3 built-in)
✅ Cache Optimization - 5-10x speedup for repeated requests
✅ Connection Pooling - 5-10x improvement for same-server requests