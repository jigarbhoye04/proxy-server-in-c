# HTTP Proxy Server - System Architecture

This document contains the comprehensive system architecture diagram using Mermaid.

## System Architecture Diagram

![System Architecture Diagram](/Public/System%20Architecture%20Diagram.png)
<details>
<summary>Show/Hide Mermaid Diagram Code</summary>

```mermaid
graph LR

    %% ========== Client Layer ==========

    subgraph ClientLayer["Client Layer"]
        direction TB
        C1[Web Browser]
        C2[curl/wget]
        C3[Applications]
    end

    %% ========== Proxy Core ==========

    subgraph ProxyCore["Proxy Server Core"]
        direction TB
        
        subgraph MainProcess["Main Process"]
            direction TB
            MAIN["proxy_server.c<br/>Main Entry Point"]
            SIGNAL["Signal Handlers<br/>Graceful Shutdown"]
        end

        subgraph NetworkLayer["Network Layer"]
            direction TB
            SOCKET["Server Socket<br/>Port 8080"]
            ACCEPT["Accept Loop<br/>Client Connections"]
            PLATFORM["platform.c<br/>Cross-Platform Socket API"]
        end

        subgraph ThreadMgmt["Thread Management"]
            direction TB
            THREADPOOL["thread_pool.c<br/>4 Worker Threads"]
            TASKQUEUE["Task Queue<br/>FIFO Processing"]
            SEMAPHORE["Semaphore<br/>Connection Limiting"]
            MUTEX["Mutex Locks<br/>Thread Synchronization"]
        end

        subgraph RequestProc["Request Processing"]
            direction TB
            PARSER["http_parser.c<br/>HTTP Request Parser"]
            VALIDATOR["Request Validator<br/>Security & Format"]
            EXTRACTOR["Host/Port Extractor<br/>URL Processing"]
        end

        subgraph CacheSystem["Cache System"]
            direction TB
            HASHTABLE["Hash Table<br/>O(1) Lookup"]
            LRU["LRU Algorithm<br/>Eviction Policy"]
            CACHEMGMT["cache.c<br/>Cache Management"]
            EXPIRE["Expiry Handler<br/>TTL Management"]
        end

        subgraph ConnMgmt["Connection Management"]
            direction TB
            CONNPOOL["connection_pool.c<br/>Connection Pool"]
            KEEPALIVE["Keep-Alive Handler<br/>HTTP/1.1 Persistence"]
            TIMEOUT["Timeout Manager<br/>Connection Cleanup"]
            REUSE["Connection Reuse<br/>Performance Optimization"]
        end

        subgraph ResponseHandler["Response Handling"]
            direction TB
            FORWARD["Request Forwarder<br/>Proxy Logic"]
            RESPONSE["Response Handler<br/>Data Processing"]
            ERROR["Error Handler<br/>HTTP Status Codes"]
        end
    end

    %% ========== Target Servers ==========

    subgraph Servers["Target Servers"]
        direction TB
        TS1[example.com]
        TS2[httpbin.org]
        TS3[Any HTTP Server]
    end

    %% ========== Data Flow (separate clean lane) ==========

    subgraph Flow["Data Flow"]
        direction TB
        DF1[1. Client Request]
        DF2[2. Thread Assignment]
        DF3[3. Cache Check]
        DF4[4. Connection Pool]
        DF5[5. Forward Request]
        DF6[6. Cache Response]
        DF7[7. Return to Client]
    end

    %% ================== Connections ==================
    %% Client connections
    C1 --> SOCKET
    C2 --> SOCKET  
    C3 --> SOCKET

    %% Core flow
    MAIN --> SOCKET
    SOCKET --> ACCEPT
    ACCEPT --> THREADPOOL
    THREADPOOL --> TASKQUEUE
    TASKQUEUE --> PARSER
    PARSER --> VALIDATOR
    VALIDATOR --> EXTRACTOR

    %% Cache integration
    EXTRACTOR --> HASHTABLE --> LRU --> CACHEMGMT --> EXPIRE
    CACHEMGMT -. Cache Miss .-> FORWARD

    %% Connection management
    EXTRACTOR --> CONNPOOL --> KEEPALIVE --> TIMEOUT --> REUSE --> FORWARD

    %% Request forwarding
    FORWARD --> RESPONSE --> ERROR
    FORWARD --> TS1
    FORWARD --> TS2
    FORWARD --> TS3
    TS1 --> RESPONSE
    TS2 --> RESPONSE
    TS3 --> RESPONSE

    %% Platform abstraction
    SOCKET --> PLATFORM
    CONNPOOL --> PLATFORM

    %% Synchronization
    THREADPOOL --> SEMAPHORE
    THREADPOOL --> MUTEX
    CACHEMGMT --> MUTEX
    CONNPOOL --> MUTEX

    %% Clean vertical data flow
    DF1 --> DF2 --> DF3 --> DF4 --> DF5 --> DF6 --> DF7

    %% ================== Styling ==================
    classDef clientClass fill:#e3f2fd,stroke:#1976d2,stroke-width:2px
    classDef coreClass fill:#f3e5f5,stroke:#7b1fa2,stroke-width:2px
    classDef cacheClass fill:#e8f5e8,stroke:#388e3c,stroke-width:2px
    classDef connClass fill:#fff3e0,stroke:#f57c00,stroke-width:2px
    classDef serverClass fill:#fce4ec,stroke:#c2185b,stroke-width:2px
    classDef flowClass fill:#f1f8e9,stroke:#689f38,stroke-width:2px

    %% Assign styles
    class C1,C2,C3 clientClass
    class MAIN,SOCKET,ACCEPT,THREADPOOL,PARSER,VALIDATOR,EXTRACTOR,FORWARD,RESPONSE,ERROR coreClass
    class HASHTABLE,LRU,CACHEMGMT,EXPIRE cacheClass
    class CONNPOOL,KEEPALIVE,TIMEOUT,REUSE connClass
    class TS1,TS2,TS3 serverClass
    class DF1,DF2,DF3,DF4,DF5,DF6,DF7 flowClass

```
</details>



## Component Dependencies

![Component Dependencies](/Public/Component%20Dependencies.png)

<details>
<summary>Show/Hide Mermaid Diagram Code</summary>

```mermaid
graph LR
    
    %% ================= Headers =================
    subgraph Headers["Header Dependencies"]
        direction TB
        
        %% External headers
        subgraph ExtHeaders["External Headers"]
            PH[proxy_parse.h<br/>External HTTP Parser]
            PTH[pthread.h<br/>Threading Library]
        end
        
        %% Custom headers
        subgraph CustHeaders["Custom Headers"]
            PLATFORMH[platform.h<br/>Cross-Platform API]
            HTTPH[http_parser.h<br/>Request Processing]
            THREADH[thread_pool.h<br/>Thread Management]
            CACHEH[cache.h<br/>Caching System]
            CONNH[connection_pool.h<br/>Connection Management]
            PROXYH[proxy_server.h<br/>Core Server Logic]
        end
    end
    
    %% ================= Implementation =================
    subgraph Impl["Implementation Files"]
        direction TB
        PLATFORMC[platform.c<br/>Socket Abstraction]
        HTTPC[http_parser.c<br/>HTTP Processing]
        THREADC[thread_pool.c<br/>Worker Threads]
        CACHEC[cache.c<br/>Hash Table Cache]
        CONNC[connection_pool.c<br/>Connection Pool]
        PROXYC[proxy_server.c<br/>Core Logic]
        MAINC[main.c<br/>Entry Point]
    end
    
    %% ================= Dependencies =================
    %% Core header dependencies
    PROXYH --> PLATFORMH
    PROXYH --> HTTPH
    PROXYH --> THREADH
    PROXYH --> CACHEH
    PROXYH --> CONNH
    
    %% External dependencies
    HTTPH --> PH
    THREADH --> PTH
    CACHEH --> PTH
    CONNH --> PTH
    
    %% Implementation dependencies
    PLATFORMC --> PLATFORMH
    HTTPC --> HTTPH
    THREADC --> THREADH
    CACHEC --> CACHEH
    CONNC --> CONNH
    PROXYC --> PROXYH
    MAINC --> PROXYH
    
    %% ================= Styling =================
    classDef headerClass fill:#e1f5fe,stroke:#0277bd,stroke-width:2px
    classDef implClass fill:#f3e5f5,stroke:#7b1fa2,stroke-width:2px
    classDef externalClass fill:#fff8e1,stroke:#f57c00,stroke-width:2px
    
    class PLATFORMH,HTTPH,THREADH,CACHEH,CONNH,PROXYH headerClass
    class PLATFORMC,HTTPC,THREADC,CACHEC,CONNC,PROXYC,MAINC implClass
    class PH,PTH externalClass
```

</details>