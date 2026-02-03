# Deployment Guide

> Version: 0.1.0.0
> Last Updated: 2026-02-03
> Status: Foundation Reference

## Overview

This document provides comprehensive deployment guidance for the unified game server, including Docker configuration, Kubernetes manifests, monitoring setup, and operational procedures.

---

## Table of Contents

1. [Deployment Architecture](#deployment-architecture)
2. [Docker Configuration](#docker-configuration)
3. [Kubernetes Deployment](#kubernetes-deployment)
4. [Service Mesh](#service-mesh)
5. [Monitoring & Observability](#monitoring--observability)
6. [Scaling Strategies](#scaling-strategies)
7. [Disaster Recovery](#disaster-recovery)
8. [Operational Procedures](#operational-procedures)

---

## Deployment Architecture

### Production Architecture

```
                           ┌─────────────────────────────────────────┐
                           │            Load Balancer               │
                           │         (AWS ALB / GCP LB)             │
                           └─────────────┬───────────────────────────┘
                                         │
              ┌──────────────────────────┼──────────────────────────┐
              │                          │                          │
    ┌─────────▼─────────┐    ┌───────────▼──────────┐    ┌─────────▼─────────┐
    │   Gateway Pod 1   │    │   Gateway Pod 2      │    │   Gateway Pod N   │
    │   (WebSocket)     │    │   (WebSocket)        │    │   (WebSocket)     │
    └─────────┬─────────┘    └───────────┬──────────┘    └─────────┬─────────┘
              │                          │                          │
              └──────────────────────────┼──────────────────────────┘
                                         │
                           ┌─────────────▼───────────────────────────┐
                           │          Service Mesh (Istio)           │
                           └─────────────┬───────────────────────────┘
                                         │
         ┌───────────────┬───────────────┼───────────────┬───────────────┐
         │               │               │               │               │
    ┌────▼────┐    ┌─────▼─────┐   ┌─────▼─────┐   ┌─────▼─────┐   ┌─────▼─────┐
    │  Auth   │    │   Game    │   │   World   │   │  Lobby    │   │  Social   │
    │ Service │    │  Service  │   │  Service  │   │  Service  │   │  Service  │
    └────┬────┘    └─────┬─────┘   └─────┬─────┘   └─────┬─────┘   └─────┬─────┘
         │               │               │               │               │
         └───────────────┴───────────────┴───────────────┴───────────────┘
                                         │
              ┌──────────────────────────┼──────────────────────────┐
              │                          │                          │
    ┌─────────▼─────────┐    ┌───────────▼──────────┐    ┌─────────▼─────────┐
    │   PostgreSQL      │    │      Redis           │    │   Kafka/NATS     │
    │   (Primary/Rep)   │    │    (Cluster)         │    │   (Messaging)    │
    └───────────────────┘    └──────────────────────┘    └───────────────────┘
```

### Deployment Environments

| Environment | Purpose | Scale | Update Policy |
|-------------|---------|-------|---------------|
| Development | Developer testing | 1 pod each | Continuous |
| Staging | QA/Integration | 2 pods each | Daily |
| Production | Live game | Auto-scaled | Scheduled/Blue-Green |

---

## Docker Configuration

### Multi-Stage Dockerfile

```dockerfile
# Dockerfile
# =============================================================================
# Game Server Multi-Stage Build
# =============================================================================

# -----------------------------------------------------------------------------
# Stage 1: Build Environment
# -----------------------------------------------------------------------------
FROM ubuntu:22.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    g++-13 \
    git \
    libpq-dev \
    libssl-dev \
    libboost-all-dev \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# Set compiler
ENV CC=gcc-13
ENV CXX=g++-13

WORKDIR /build

# Copy source
COPY . .

# Build
RUN cmake -B build \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTS=OFF \
    && cmake --build build --parallel

# -----------------------------------------------------------------------------
# Stage 2: Runtime Environment
# -----------------------------------------------------------------------------
FROM ubuntu:22.04 AS runtime

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libpq5 \
    libssl3 \
    libboost-system1.74.0 \
    libboost-thread1.74.0 \
    ca-certificates \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Create non-root user
RUN useradd -r -s /bin/false gameserver

WORKDIR /app

# Copy binary and assets
COPY --from=builder /build/build/bin/game_server .
COPY --from=builder /build/config ./config
COPY --from=builder /build/data ./data

# Set permissions
RUN chown -R gameserver:gameserver /app

USER gameserver

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:9090/health || exit 1

# Expose ports
EXPOSE 7777 7778/udp 8080 9090

# Entry point
ENTRYPOINT ["./game_server"]
CMD ["--config", "/app/config/production.yaml"]
```

### Docker Compose for Local Development

```yaml
# docker-compose.yml
version: '3.8'

services:
  game-server:
    build:
      context: .
      dockerfile: Dockerfile
      target: runtime
    ports:
      - "7777:7777"      # Game TCP
      - "7778:7778/udp"  # Game UDP
      - "8080:8080"      # WebSocket
      - "9090:9090"      # Admin/Metrics
    environment:
      - GAME_SERVER_ENV=development
      - DB_HOST=postgres
      - DB_PORT=5432
      - DB_NAME=game_db
      - DB_USER=game
      - DB_PASSWORD=development_password
      - REDIS_HOST=redis
      - REDIS_PORT=6379
      - LOG_LEVEL=debug
    depends_on:
      postgres:
        condition: service_healthy
      redis:
        condition: service_healthy
    volumes:
      - ./config:/app/config:ro
      - ./data:/app/data:ro
      - game-logs:/var/log/game-server
    networks:
      - game-network
    restart: unless-stopped

  postgres:
    image: postgres:15-alpine
    environment:
      POSTGRES_USER: game
      POSTGRES_PASSWORD: development_password
      POSTGRES_DB: game_db
    ports:
      - "5432:5432"
    volumes:
      - postgres-data:/var/lib/postgresql/data
      - ./migrations:/docker-entrypoint-initdb.d:ro
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U game -d game_db"]
      interval: 10s
      timeout: 5s
      retries: 5
    networks:
      - game-network

  redis:
    image: redis:7-alpine
    ports:
      - "6379:6379"
    volumes:
      - redis-data:/data
    command: redis-server --appendonly yes
    healthcheck:
      test: ["CMD", "redis-cli", "ping"]
      interval: 10s
      timeout: 5s
      retries: 5
    networks:
      - game-network

  prometheus:
    image: prom/prometheus:latest
    ports:
      - "9091:9090"
    volumes:
      - ./monitoring/prometheus.yml:/etc/prometheus/prometheus.yml:ro
      - prometheus-data:/prometheus
    command:
      - '--config.file=/etc/prometheus/prometheus.yml'
      - '--storage.tsdb.path=/prometheus'
    networks:
      - game-network

  grafana:
    image: grafana/grafana:latest
    ports:
      - "3000:3000"
    volumes:
      - ./monitoring/grafana/provisioning:/etc/grafana/provisioning:ro
      - grafana-data:/var/lib/grafana
    environment:
      - GF_SECURITY_ADMIN_PASSWORD=admin
    depends_on:
      - prometheus
    networks:
      - game-network

volumes:
  postgres-data:
  redis-data:
  game-logs:
  prometheus-data:
  grafana-data:

networks:
  game-network:
    driver: bridge
```

---

## Kubernetes Deployment

### Namespace and RBAC

```yaml
# k8s/namespace.yaml
apiVersion: v1
kind: Namespace
metadata:
  name: game-server
  labels:
    name: game-server
    istio-injection: enabled

---
# k8s/rbac.yaml
apiVersion: v1
kind: ServiceAccount
metadata:
  name: game-server
  namespace: game-server

---
apiVersion: rbac.authorization.k8s.io/v1
kind: Role
metadata:
  name: game-server-role
  namespace: game-server
rules:
  - apiGroups: [""]
    resources: ["configmaps", "secrets"]
    verbs: ["get", "list", "watch"]
  - apiGroups: [""]
    resources: ["pods"]
    verbs: ["get", "list", "watch"]

---
apiVersion: rbac.authorization.k8s.io/v1
kind: RoleBinding
metadata:
  name: game-server-rolebinding
  namespace: game-server
subjects:
  - kind: ServiceAccount
    name: game-server
    namespace: game-server
roleRef:
  kind: Role
  name: game-server-role
  apiGroup: rbac.authorization.k8s.io
```

### ConfigMap and Secrets

```yaml
# k8s/configmap.yaml
apiVersion: v1
kind: ConfigMap
metadata:
  name: game-server-config
  namespace: game-server
data:
  production.yaml: |
    server:
      environment: production
      cluster:
        enabled: true
        discovery:
          type: kubernetes
          namespace: game-server

    limits:
      max_players: 5000

    performance:
      game_tick_rate: 30

    logging:
      level: info
      outputs:
        console:
          enabled: true
          format: json
        remote:
          enabled: true
          type: loki
          endpoint: http://loki:3100

---
# k8s/secrets.yaml
apiVersion: v1
kind: Secret
metadata:
  name: game-server-secrets
  namespace: game-server
type: Opaque
stringData:
  DB_PASSWORD: "${DB_PASSWORD}"
  REDIS_PASSWORD: "${REDIS_PASSWORD}"
  JWT_SECRET: "${JWT_SECRET}"
  ENCRYPTION_KEY: "${ENCRYPTION_KEY}"
```

### Game Server Deployment

```yaml
# k8s/game-server-deployment.yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: game-server
  namespace: game-server
  labels:
    app: game-server
    version: v1
spec:
  replicas: 3
  selector:
    matchLabels:
      app: game-server
  strategy:
    type: RollingUpdate
    rollingUpdate:
      maxSurge: 1
      maxUnavailable: 0
  template:
    metadata:
      labels:
        app: game-server
        version: v1
      annotations:
        prometheus.io/scrape: "true"
        prometheus.io/port: "9090"
        prometheus.io/path: "/metrics"
    spec:
      serviceAccountName: game-server
      terminationGracePeriodSeconds: 60

      # Anti-affinity to spread across nodes
      affinity:
        podAntiAffinity:
          preferredDuringSchedulingIgnoredDuringExecution:
            - weight: 100
              podAffinityTerm:
                labelSelector:
                  matchExpressions:
                    - key: app
                      operator: In
                      values:
                        - game-server
                topologyKey: kubernetes.io/hostname

      containers:
        - name: game-server
          image: gameserver/game-server:latest
          imagePullPolicy: Always

          ports:
            - name: game-tcp
              containerPort: 7777
              protocol: TCP
            - name: game-udp
              containerPort: 7778
              protocol: UDP
            - name: websocket
              containerPort: 8080
              protocol: TCP
            - name: metrics
              containerPort: 9090
              protocol: TCP

          env:
            - name: GAME_SERVER_ENV
              value: "production"
            - name: POD_NAME
              valueFrom:
                fieldRef:
                  fieldPath: metadata.name
            - name: POD_NAMESPACE
              valueFrom:
                fieldRef:
                  fieldPath: metadata.namespace
            - name: NODE_NAME
              valueFrom:
                fieldRef:
                  fieldPath: spec.nodeName
            - name: DB_HOST
              value: "postgres-primary.database.svc.cluster.local"
            - name: DB_PORT
              value: "5432"
            - name: DB_NAME
              value: "game_db"
            - name: DB_USER
              value: "game_user"
            - name: DB_PASSWORD
              valueFrom:
                secretKeyRef:
                  name: game-server-secrets
                  key: DB_PASSWORD
            - name: REDIS_HOST
              value: "redis-cluster.cache.svc.cluster.local"
            - name: REDIS_PASSWORD
              valueFrom:
                secretKeyRef:
                  name: game-server-secrets
                  key: REDIS_PASSWORD
            - name: JWT_SECRET
              valueFrom:
                secretKeyRef:
                  name: game-server-secrets
                  key: JWT_SECRET

          resources:
            requests:
              cpu: "1000m"
              memory: "2Gi"
            limits:
              cpu: "4000m"
              memory: "8Gi"

          livenessProbe:
            httpGet:
              path: /health/live
              port: metrics
            initialDelaySeconds: 30
            periodSeconds: 10
            timeoutSeconds: 5
            failureThreshold: 3

          readinessProbe:
            httpGet:
              path: /health/ready
              port: metrics
            initialDelaySeconds: 10
            periodSeconds: 5
            timeoutSeconds: 3
            failureThreshold: 3

          startupProbe:
            httpGet:
              path: /health/startup
              port: metrics
            initialDelaySeconds: 5
            periodSeconds: 5
            failureThreshold: 30

          volumeMounts:
            - name: config
              mountPath: /app/config
              readOnly: true
            - name: data
              mountPath: /app/data
              readOnly: true

          lifecycle:
            preStop:
              exec:
                command:
                  - /bin/sh
                  - -c
                  - |
                    # Signal server to stop accepting new connections
                    curl -X POST http://localhost:9090/admin/drain
                    # Wait for existing connections to drain
                    sleep 30

      volumes:
        - name: config
          configMap:
            name: game-server-config
        - name: data
          persistentVolumeClaim:
            claimName: game-data-pvc

---
# k8s/game-server-service.yaml
apiVersion: v1
kind: Service
metadata:
  name: game-server
  namespace: game-server
  labels:
    app: game-server
spec:
  type: ClusterIP
  ports:
    - name: game-tcp
      port: 7777
      targetPort: 7777
      protocol: TCP
    - name: game-udp
      port: 7778
      targetPort: 7778
      protocol: UDP
    - name: websocket
      port: 8080
      targetPort: 8080
      protocol: TCP
    - name: metrics
      port: 9090
      targetPort: 9090
      protocol: TCP
  selector:
    app: game-server

---
# k8s/game-server-hpa.yaml
apiVersion: autoscaling/v2
kind: HorizontalPodAutoscaler
metadata:
  name: game-server-hpa
  namespace: game-server
spec:
  scaleTargetRef:
    apiVersion: apps/v1
    kind: Deployment
    name: game-server
  minReplicas: 3
  maxReplicas: 50
  metrics:
    - type: Resource
      resource:
        name: cpu
        target:
          type: Utilization
          averageUtilization: 70
    - type: Resource
      resource:
        name: memory
        target:
          type: Utilization
          averageUtilization: 80
    - type: Pods
      pods:
        metric:
          name: game_server_player_count
        target:
          type: AverageValue
          averageValue: "1000"
  behavior:
    scaleUp:
      stabilizationWindowSeconds: 60
      policies:
        - type: Pods
          value: 4
          periodSeconds: 60
    scaleDown:
      stabilizationWindowSeconds: 300
      policies:
        - type: Percent
          value: 10
          periodSeconds: 60
```

### Gateway Service

```yaml
# k8s/gateway-deployment.yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: gateway
  namespace: game-server
spec:
  replicas: 2
  selector:
    matchLabels:
      app: gateway
  template:
    metadata:
      labels:
        app: gateway
    spec:
      containers:
        - name: gateway
          image: gameserver/gateway:latest
          ports:
            - containerPort: 443
              name: https
            - containerPort: 8443
              name: wss
          env:
            - name: GAME_SERVER_ENDPOINTS
              value: "game-server.game-server.svc.cluster.local:7777"
          resources:
            requests:
              cpu: "500m"
              memory: "512Mi"
            limits:
              cpu: "2000m"
              memory: "2Gi"

---
apiVersion: v1
kind: Service
metadata:
  name: gateway
  namespace: game-server
  annotations:
    service.beta.kubernetes.io/aws-load-balancer-type: "nlb"
    service.beta.kubernetes.io/aws-load-balancer-cross-zone-load-balancing-enabled: "true"
spec:
  type: LoadBalancer
  externalTrafficPolicy: Local
  ports:
    - name: https
      port: 443
      targetPort: 443
    - name: wss
      port: 8443
      targetPort: 8443
  selector:
    app: gateway
```

---

## Service Mesh

### Istio Configuration

```yaml
# k8s/istio/virtual-service.yaml
apiVersion: networking.istio.io/v1beta1
kind: VirtualService
metadata:
  name: game-server-vs
  namespace: game-server
spec:
  hosts:
    - game-server
  http:
    - match:
        - headers:
            x-game-version:
              prefix: "2.0"
      route:
        - destination:
            host: game-server
            subset: v2
          weight: 100
    - route:
        - destination:
            host: game-server
            subset: v1
          weight: 100

---
apiVersion: networking.istio.io/v1beta1
kind: DestinationRule
metadata:
  name: game-server-dr
  namespace: game-server
spec:
  host: game-server
  trafficPolicy:
    connectionPool:
      tcp:
        maxConnections: 10000
        connectTimeout: 10s
      http:
        h2UpgradePolicy: UPGRADE
    loadBalancer:
      simple: LEAST_REQUEST
    outlierDetection:
      consecutive5xxErrors: 5
      interval: 30s
      baseEjectionTime: 30s
      maxEjectionPercent: 50
  subsets:
    - name: v1
      labels:
        version: v1
    - name: v2
      labels:
        version: v2

---
# k8s/istio/circuit-breaker.yaml
apiVersion: networking.istio.io/v1beta1
kind: DestinationRule
metadata:
  name: database-circuit-breaker
  namespace: game-server
spec:
  host: postgres-primary.database.svc.cluster.local
  trafficPolicy:
    connectionPool:
      tcp:
        maxConnections: 100
    outlierDetection:
      consecutiveErrors: 3
      interval: 10s
      baseEjectionTime: 30s
      maxEjectionPercent: 100
```

---

## Monitoring & Observability

### Prometheus Configuration

```yaml
# monitoring/prometheus.yml
global:
  scrape_interval: 15s
  evaluation_interval: 15s

rule_files:
  - /etc/prometheus/rules/*.yaml

alerting:
  alertmanagers:
    - static_configs:
        - targets:
            - alertmanager:9093

scrape_configs:
  - job_name: 'game-server'
    kubernetes_sd_configs:
      - role: pod
        namespaces:
          names:
            - game-server
    relabel_configs:
      - source_labels: [__meta_kubernetes_pod_annotation_prometheus_io_scrape]
        action: keep
        regex: true
      - source_labels: [__meta_kubernetes_pod_annotation_prometheus_io_path]
        action: replace
        target_label: __metrics_path__
        regex: (.+)
      - source_labels: [__address__, __meta_kubernetes_pod_annotation_prometheus_io_port]
        action: replace
        regex: ([^:]+)(?::\d+)?;(\d+)
        replacement: $1:$2
        target_label: __address__

  - job_name: 'kubernetes-pods'
    kubernetes_sd_configs:
      - role: pod
    relabel_configs:
      - source_labels: [__meta_kubernetes_pod_annotation_prometheus_io_scrape]
        action: keep
        regex: true
```

### Alert Rules

```yaml
# monitoring/rules/game-server-alerts.yaml
groups:
  - name: game-server-alerts
    rules:
      # High player load
      - alert: HighPlayerCount
        expr: game_server_player_count > 4500
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "High player count on {{ $labels.instance }}"
          description: "Player count is {{ $value }}, approaching limit of 5000"

      # Server overload
      - alert: ServerOverload
        expr: game_server_player_count > 4900
        for: 1m
        labels:
          severity: critical
        annotations:
          summary: "Server {{ $labels.instance }} at capacity"
          description: "Player count {{ $value }} is at limit"

      # High latency
      - alert: HighLatency
        expr: histogram_quantile(0.99, game_server_request_duration_seconds_bucket) > 0.1
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "High latency on {{ $labels.instance }}"
          description: "P99 latency is {{ $value }}s"

      # Error rate
      - alert: HighErrorRate
        expr: rate(game_server_errors_total[5m]) > 10
        for: 5m
        labels:
          severity: critical
        annotations:
          summary: "High error rate on {{ $labels.instance }}"
          description: "Error rate is {{ $value }}/s"

      # Memory pressure
      - alert: HighMemoryUsage
        expr: game_server_memory_usage_bytes / game_server_memory_limit_bytes > 0.9
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "High memory usage on {{ $labels.instance }}"
          description: "Memory usage is {{ $value | humanizePercentage }}"

      # Pod restart
      - alert: PodRestarting
        expr: increase(kube_pod_container_status_restarts_total{namespace="game-server"}[1h]) > 3
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "Pod {{ $labels.pod }} is restarting frequently"
          description: "{{ $value }} restarts in the last hour"
```

### Grafana Dashboards

```json
{
  "dashboard": {
    "title": "Game Server Overview",
    "panels": [
      {
        "title": "Player Count",
        "type": "stat",
        "targets": [
          {
            "expr": "sum(game_server_player_count)",
            "legendFormat": "Total Players"
          }
        ]
      },
      {
        "title": "Players per Server",
        "type": "graph",
        "targets": [
          {
            "expr": "game_server_player_count",
            "legendFormat": "{{pod}}"
          }
        ]
      },
      {
        "title": "Request Latency (P99)",
        "type": "graph",
        "targets": [
          {
            "expr": "histogram_quantile(0.99, rate(game_server_request_duration_seconds_bucket[5m]))",
            "legendFormat": "P99"
          }
        ]
      },
      {
        "title": "Error Rate",
        "type": "graph",
        "targets": [
          {
            "expr": "rate(game_server_errors_total[5m])",
            "legendFormat": "{{error_type}}"
          }
        ]
      },
      {
        "title": "CPU Usage",
        "type": "graph",
        "targets": [
          {
            "expr": "rate(container_cpu_usage_seconds_total{namespace=\"game-server\"}[5m])",
            "legendFormat": "{{pod}}"
          }
        ]
      },
      {
        "title": "Memory Usage",
        "type": "graph",
        "targets": [
          {
            "expr": "container_memory_usage_bytes{namespace=\"game-server\"}",
            "legendFormat": "{{pod}}"
          }
        ]
      }
    ]
  }
}
```

---

## Scaling Strategies

### Horizontal Pod Autoscaler

```yaml
# k8s/hpa-advanced.yaml
apiVersion: autoscaling/v2
kind: HorizontalPodAutoscaler
metadata:
  name: game-server-hpa
  namespace: game-server
spec:
  scaleTargetRef:
    apiVersion: apps/v1
    kind: Deployment
    name: game-server
  minReplicas: 3
  maxReplicas: 100
  metrics:
    # CPU based scaling
    - type: Resource
      resource:
        name: cpu
        target:
          type: Utilization
          averageUtilization: 70

    # Memory based scaling
    - type: Resource
      resource:
        name: memory
        target:
          type: Utilization
          averageUtilization: 80

    # Custom metric: player count
    - type: Pods
      pods:
        metric:
          name: game_server_player_count
        target:
          type: AverageValue
          averageValue: "800"

    # Custom metric: connection count
    - type: Pods
      pods:
        metric:
          name: game_server_connection_count
        target:
          type: AverageValue
          averageValue: "1000"

  behavior:
    scaleUp:
      stabilizationWindowSeconds: 30
      policies:
        - type: Percent
          value: 100
          periodSeconds: 30
        - type: Pods
          value: 10
          periodSeconds: 30
      selectPolicy: Max

    scaleDown:
      stabilizationWindowSeconds: 300
      policies:
        - type: Percent
          value: 10
          periodSeconds: 60
      selectPolicy: Min
```

### Vertical Pod Autoscaler

```yaml
# k8s/vpa.yaml
apiVersion: autoscaling.k8s.io/v1
kind: VerticalPodAutoscaler
metadata:
  name: game-server-vpa
  namespace: game-server
spec:
  targetRef:
    apiVersion: apps/v1
    kind: Deployment
    name: game-server
  updatePolicy:
    updateMode: "Auto"
  resourcePolicy:
    containerPolicies:
      - containerName: game-server
        minAllowed:
          cpu: "500m"
          memory: "1Gi"
        maxAllowed:
          cpu: "8000m"
          memory: "16Gi"
        controlledResources: ["cpu", "memory"]
```

---

## Disaster Recovery

### Backup Strategy

```yaml
# k8s/backup/backup-cronjob.yaml
apiVersion: batch/v1
kind: CronJob
metadata:
  name: database-backup
  namespace: game-server
spec:
  schedule: "0 */4 * * *"  # Every 4 hours
  concurrencyPolicy: Forbid
  successfulJobsHistoryLimit: 3
  failedJobsHistoryLimit: 3
  jobTemplate:
    spec:
      template:
        spec:
          containers:
            - name: backup
              image: postgres:15
              command:
                - /bin/sh
                - -c
                - |
                  TIMESTAMP=$(date +%Y%m%d_%H%M%S)
                  BACKUP_FILE="/backups/game_db_${TIMESTAMP}.sql.gz"

                  pg_dump -h $DB_HOST -U $DB_USER -d $DB_NAME | gzip > $BACKUP_FILE

                  # Upload to S3
                  aws s3 cp $BACKUP_FILE s3://${BACKUP_BUCKET}/database/

                  # Cleanup old local backups
                  find /backups -mtime +7 -delete
              env:
                - name: DB_HOST
                  value: "postgres-primary.database.svc.cluster.local"
                - name: DB_USER
                  value: "game_user"
                - name: PGPASSWORD
                  valueFrom:
                    secretKeyRef:
                      name: game-server-secrets
                      key: DB_PASSWORD
                - name: BACKUP_BUCKET
                  value: "game-server-backups"
              volumeMounts:
                - name: backup-volume
                  mountPath: /backups
          volumes:
            - name: backup-volume
              persistentVolumeClaim:
                claimName: backup-pvc
          restartPolicy: OnFailure
```

### Recovery Procedure

```bash
#!/bin/bash
# scripts/disaster-recovery.sh

set -euo pipefail

# Configuration
BACKUP_BUCKET="game-server-backups"
DB_HOST="postgres-primary.database.svc.cluster.local"
DB_NAME="game_db"
DB_USER="game_user"

# Functions
restore_database() {
    local backup_file=$1

    echo "Downloading backup from S3..."
    aws s3 cp "s3://${BACKUP_BUCKET}/database/${backup_file}" /tmp/restore.sql.gz

    echo "Stopping game servers..."
    kubectl scale deployment game-server --replicas=0 -n game-server
    sleep 30

    echo "Restoring database..."
    gunzip -c /tmp/restore.sql.gz | psql -h "$DB_HOST" -U "$DB_USER" -d "$DB_NAME"

    echo "Starting game servers..."
    kubectl scale deployment game-server --replicas=3 -n game-server

    echo "Waiting for pods to be ready..."
    kubectl wait --for=condition=ready pod -l app=game-server -n game-server --timeout=300s

    echo "Recovery complete!"
}

# Main
case "${1:-}" in
    restore)
        restore_database "$2"
        ;;
    list)
        aws s3 ls "s3://${BACKUP_BUCKET}/database/"
        ;;
    *)
        echo "Usage: $0 {restore <backup_file>|list}"
        exit 1
        ;;
esac
```

---

## Operational Procedures

### Deployment Checklist

```markdown
## Pre-Deployment Checklist

- [ ] All tests passing in CI/CD
- [ ] Database migrations tested in staging
- [ ] Config changes reviewed and applied
- [ ] Monitoring alerts configured
- [ ] Rollback plan documented
- [ ] Team notified of deployment window

## Deployment Steps

1. [ ] Create deployment branch
2. [ ] Run staging deployment
3. [ ] Verify staging metrics
4. [ ] Create production PR
5. [ ] Deploy to production (blue-green)
6. [ ] Monitor for 30 minutes
7. [ ] Complete or rollback

## Post-Deployment

- [ ] Verify all pods healthy
- [ ] Check error rates
- [ ] Verify player connections stable
- [ ] Update deployment documentation
```

### Runbook

```markdown
# Game Server Runbook

## High Player Load

### Symptoms
- Player queue forming
- Login times increasing
- Server CPU > 80%

### Actions
1. Check current pod count: `kubectl get pods -n game-server`
2. Check HPA status: `kubectl get hpa -n game-server`
3. If HPA not scaling, manually scale:
   ```bash
   kubectl scale deployment game-server --replicas=<N> -n game-server
   ```
4. Check for any failing pods:
   ```bash
   kubectl get pods -n game-server | grep -v Running
   ```
5. If needed, increase HPA limits temporarily

## Database Connection Issues

### Symptoms
- Connection timeout errors
- Slow queries
- Database alerts firing

### Actions
1. Check database pod status:
   ```bash
   kubectl get pods -n database
   ```
2. Check connection pool:
   ```bash
   kubectl exec -it postgres-primary-0 -n database -- \
     psql -U game_user -c "SELECT count(*) FROM pg_stat_activity;"
   ```
3. Check for long-running queries:
   ```bash
   kubectl exec -it postgres-primary-0 -n database -- \
     psql -U game_user -c "SELECT pid, now() - pg_stat_activity.query_start AS duration, query FROM pg_stat_activity WHERE state != 'idle' ORDER BY duration DESC LIMIT 10;"
   ```
4. If needed, kill problematic queries
5. Consider scaling read replicas

## Memory Pressure

### Symptoms
- OOMKilled pods
- High memory alerts
- Slow GC (if applicable)

### Actions
1. Check memory usage:
   ```bash
   kubectl top pods -n game-server
   ```
2. Check for memory leaks in metrics
3. If immediate issue, restart affected pods:
   ```bash
   kubectl delete pod <pod-name> -n game-server
   ```
4. Consider increasing memory limits if persistent
```

---

*This document provides the complete deployment and operations guide for the unified game server.*
