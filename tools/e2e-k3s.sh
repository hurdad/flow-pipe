#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMAGE_NAMESPACE="${IMAGE_NAMESPACE:-ghcr.io/${USER:-local}}"
IMAGE_TAG="${IMAGE_TAG:-local-$(date +%s)}"
NAMESPACE="${NAMESPACE:-flow-pipe}"
K3S_IMAGE="${K3S_IMAGE:-rancher/k3s:v1.28.8-k3s1}"
KEEP_CLUSTER="${KEEP_CLUSTER:-}"
K3S_KUBECONFIG_DIR="${K3S_KUBECONFIG_DIR:-${REPO_ROOT}/.k3s-kubeconfig}"
COMPOSE_PROJECT_NAME="${COMPOSE_PROJECT_NAME:-flow-pipe}"
K3S_CONTAINER="${COMPOSE_PROJECT_NAME}-k3s-1"

info() { echo "[INFO] $*"; }
append_summary() {
  if [[ -n "${GITHUB_STEP_SUMMARY:-}" ]]; then
    printf '%s\n' "$*" >>"${GITHUB_STEP_SUMMARY}" || true
  fi
}

cleanup_cluster() {
  if [[ -n "${KEEP_CLUSTER}" ]]; then
    info "Skipping cluster deletion because KEEP_CLUSTER is set"
    return
  fi

  info "Stopping docker compose k3s cluster"
  COMPOSE_PROJECT_NAME="${COMPOSE_PROJECT_NAME}" docker compose -f "${REPO_ROOT}/tools/k3s-compose.yaml" down --volumes || true
}

cleanup_port_forward() {
  if [[ -n "${PORT_FORWARD_PID:-}" ]]; then
    info "Stopping API port-forward"
    kill "${PORT_FORWARD_PID}" >/dev/null 2>&1 || true
  fi
}

trap "cleanup_port_forward; cleanup_cluster" EXIT

info "Starting docker compose k3s cluster"
mkdir -p "${K3S_KUBECONFIG_DIR}"
K3S_IMAGE="${K3S_IMAGE}" K3S_KUBECONFIG_DIR="${K3S_KUBECONFIG_DIR}" COMPOSE_PROJECT_NAME="${COMPOSE_PROJECT_NAME}" \
  docker compose -f "${REPO_ROOT}/tools/k3s-compose.yaml" up -d

info "Waiting for k3s kubeconfig"
for _ in {1..60}; do
  if [[ -f "${K3S_KUBECONFIG_DIR}/kubeconfig" ]]; then
    break
  fi
  sleep 2
done

if [[ ! -f "${K3S_KUBECONFIG_DIR}/kubeconfig" ]]; then
  echo "k3s kubeconfig was not created" >&2
  exit 1
fi

export KUBECONFIG="${K3S_KUBECONFIG_DIR}/kubeconfig"
# The compose deployment advertises the apiserver as "localhost" in its certificate,
# so rewrite the generated kubeconfig host to avoid x509 hostname mismatches.
sed -i "s/127.0.0.1/localhost/g" "${KUBECONFIG}"

current_context=$(kubectl config current-context || true)
if [[ -n "${current_context}" ]]; then
  cluster_name=$(kubectl config view -o "jsonpath={.contexts[?(@.name==\"${current_context}\")].context.cluster}" || true)
  if [[ -n "${cluster_name}" ]]; then
    info "Marking cluster ${cluster_name} as insecure to tolerate self-signed compose certificates"
    kubectl config set-cluster "${cluster_name}" --kubeconfig "${KUBECONFIG}" --insecure-skip-tls-verify=true >/dev/null
  fi
fi

info "Waiting for k3s nodes to be ready"
for _ in {1..90}; do
  if kubectl get --raw='/readyz' >/dev/null 2>&1; then
    break
  fi
  sleep 2
done

if ! kubectl get --raw='/readyz' >/dev/null 2>&1; then
  echo "k3s API server did not become ready" >&2
  exit 1
fi

# Wait for node resources to exist before waiting on readiness.
for _ in {1..45}; do
  node_count=$(kubectl get nodes --no-headers 2>/dev/null | wc -l || true)
  if (( node_count > 0 )); then
    break
  fi
  sleep 2
done

if (( node_count == 0 )); then
  echo "k3s nodes did not register" >&2
  exit 1
fi

kubectl wait --for=condition=Ready node --all --timeout=180s
info "Cluster nodes"
kubectl get nodes

info "Building runtime image"
docker buildx build --load \
  -f "${REPO_ROOT}/runtime/docker/Dockerfile.runtime" \
  -t "${IMAGE_NAMESPACE}/flow-pipe-runtime:${IMAGE_TAG}" "${REPO_ROOT}"

info "Building base image"
docker buildx build --load \
  -f "${REPO_ROOT}/runtime/docker/Dockerfile.base" \
  --build-arg FLOW_PIPE_IMAGE_NAMESPACE="${IMAGE_NAMESPACE}" \
  --build-arg FLOW_PIPE_RUNTIME_TAG="${IMAGE_TAG}" \
  -t "${IMAGE_NAMESPACE}/flow-pipe-base:${IMAGE_TAG}" "${REPO_ROOT}"

info "Building dev image"
docker buildx build --load \
  -f "${REPO_ROOT}/runtime/docker/Dockerfile.dev" \
  --build-arg FLOW_PIPE_BASE_TAG="${IMAGE_TAG}" \
  -t "${IMAGE_NAMESPACE}/flow-pipe-dev:${IMAGE_TAG}" "${REPO_ROOT}"

info "Building controller and API images"
docker buildx build --load \
  -f "${REPO_ROOT}/controller/Dockerfile" \
  -t "${IMAGE_NAMESPACE}/flow-pipe-controller:${IMAGE_TAG}" "${REPO_ROOT}"

docker buildx build --load \
  -f "${REPO_ROOT}/api/Dockerfile" \
  -t "${IMAGE_NAMESPACE}/flow-pipe-api:${IMAGE_TAG}" "${REPO_ROOT}"

IMAGES=(
  "${IMAGE_NAMESPACE}/flow-pipe-runtime:${IMAGE_TAG}"
  "${IMAGE_NAMESPACE}/flow-pipe-base:${IMAGE_TAG}"
  "${IMAGE_NAMESPACE}/flow-pipe-dev:${IMAGE_TAG}"
  "${IMAGE_NAMESPACE}/flow-pipe-controller:${IMAGE_TAG}"
  "${IMAGE_NAMESPACE}/flow-pipe-api:${IMAGE_TAG}"
)

for image in "${IMAGES[@]}"; do
  info "Importing ${image} into compose k3s runtime"
  docker save "${image}" | docker exec -i "${K3S_CONTAINER}" ctr -n k8s.io images import -
done

info "Installing Helm chart"
info "Fetching Helm chart dependencies"
helm dependency build "${REPO_ROOT}/deploy/helm/flow-pipe"
helm upgrade --install flow-pipe "${REPO_ROOT}/deploy/helm/flow-pipe" \
  --namespace "${NAMESPACE}" \
  --create-namespace \
  --set controller.image="${IMAGE_NAMESPACE}/flow-pipe-controller:${IMAGE_TAG}" \
  --set api.image="${IMAGE_NAMESPACE}/flow-pipe-api:${IMAGE_TAG}" \
  --set observability.prometheus.enabled=true \
  --set observability.loki.enabled=true \
  --set observability.tempo.enabled=true \
  --set observability.grafana.enabled=true \
  --set observability.alloy.enabled=true

info "Waiting for control plane pods"
kubectl wait --namespace "${NAMESPACE}" --for=condition=Ready pod --selector=app=flow-pipe-etcd --timeout=300s
kubectl wait --namespace "${NAMESPACE}" --for=condition=Ready pod --selector=app=flow-pipe-controller --timeout=300s
kubectl wait --namespace "${NAMESPACE}" --for=condition=Ready pod --selector=app=flow-pipe-api --timeout=300s
kubectl wait --namespace "${NAMESPACE}" --for=condition=Ready pod --selector=app.kubernetes.io/instance=flow-pipe --timeout=600s
kubectl get pods -n "${NAMESPACE}"

info "Building flow-pipe-cli image"
docker buildx build --load \
  -f "${REPO_ROOT}/cli/Dockerfile" \
  -t "${IMAGE_NAMESPACE}/flow-pipe-cli:${IMAGE_TAG}" "${REPO_ROOT}"

info "Generating flow specs with runtime metadata"
FLOW_TMP_DIR="$(mktemp -d)"

cat >"${FLOW_TMP_DIR}/streaming.yaml" <<'FLOW'
name: noop-observability
runtime: FLOW_RUNTIME_BUILTIN
execution:
  mode: EXECUTION_MODE_STREAMING
queues:
  - name: q1
    capacity: 128
stages:
  - name: src
    type: noop_source
    threads: 1
    output_queue: q1
    config:
      delay_ms: 200
      message: "DEADBEEF"
      max_messages: 0
  - name: sink
    type: stdout_sink
    threads: 1
    input_queue: q1
observability:
  metrics_enabled: true
  tracing_enabled: true
  logs_enabled: true
FLOW

cat >"${FLOW_TMP_DIR}/job.yaml" <<'FLOW'
name: simple-pipeline-job
runtime: FLOW_RUNTIME_BUILTIN
execution:
  mode: EXECUTION_MODE_JOB
queues:
  - name: q1
    capacity: 256
  - name: q2
    capacity: 256
stages:
  - name: source
    type: noop_source
    threads: 1
    output_queue: q1
    config:
      delay_ms: 200
      message: "DEADBEEF"
      max_messages: 20
  - name: transform
    type: noop_transform
    threads: 2
    input_queue: q1
    output_queue: q2
    config:
      verbose: true
      delay_ms: 50
  - name: sink
    type: stdout_sink
    threads: 1
    input_queue: q2
observability:
  metrics_enabled: true
  tracing_enabled: true
  logs_enabled: true
FLOW

info "Port-forwarding API service for flowctl"
kubectl port-forward svc/flow-pipe-api -n "${NAMESPACE}" 9090:9090 >/tmp/flow-pipe-api-port-forward.log 2>&1 &
PORT_FORWARD_PID=$!
sleep 5

info "Submitting flows through API"
docker run --rm --network host -v "${FLOW_TMP_DIR}:/flows:ro" \
  "${IMAGE_NAMESPACE}/flow-pipe-cli:${IMAGE_TAG}" submit /flows/streaming.yaml --api localhost:9090
docker run --rm --network host -v "${FLOW_TMP_DIR}:/flows:ro" \
  "${IMAGE_NAMESPACE}/flow-pipe-cli:${IMAGE_TAG}" submit /flows/job.yaml --api localhost:9090

wait_for_runtime() {
  local name="$1"
  local kind="$2"
  local timeout_seconds=300
  local start_time=$(date +%s)

  while true; do
    if kubectl get "${kind}" -n "${NAMESPACE}" "${name}" >/dev/null 2>&1; then
      info "Found ${kind} ${name}, waiting for readiness"
      case "${kind}" in
        deployment/*)
          kubectl rollout status "${kind}" -n "${NAMESPACE}" --timeout=120s && return 0
          ;;
        job/*)
          kubectl wait --for=condition=complete "${kind}" -n "${NAMESPACE}" --timeout=120s && return 0
          ;;
      esac
    fi

    # Try label-based lookup as a fallback
    found=$(kubectl get "${kind%%/*}" -n "${NAMESPACE}" -l "flowpipe.io/flow-name=${name}" --no-headers 2>/dev/null | head -n 1 | awk '{print $1}')
    if [[ -n "${found}" ]]; then
      info "Found ${kind%%/*} ${found} via label for flow ${name}"
      case "${kind%%/*}" in
        deployment)
          kubectl rollout status "deployment/${found}" -n "${NAMESPACE}" --timeout=120s && return 0
          ;;
        job)
          kubectl wait --for=condition=complete "job/${found}" -n "${NAMESPACE}" --timeout=120s && return 0
          ;;
      esac
    fi

    if (( $(date +%s) - start_time > timeout_seconds )); then
      kubectl get pods -n "${NAMESPACE}" -o wide || true
      return 1
    fi

    sleep 5
  done
}

info "Waiting for controller-created runtimes"
wait_for_runtime "noop-observability" "deployment/noop-observability-runtime"
wait_for_runtime "simple-pipeline-job" "job/simple-pipeline-job"

info "Capturing runtime logs"
kubectl logs deployment/noop-observability-runtime -n "${NAMESPACE}" --tail=200 >"${REPO_ROOT}/stream.log"
kubectl logs job/simple-pipeline-job -n "${NAMESPACE}" >"${REPO_ROOT}/job.log"

if ! grep -q "DEADBEEF" "${REPO_ROOT}/stream.log"; then
  echo "streaming runtime did not emit expected payloads" >&2
  exit 1
fi

if ! grep -q "DEADBEEF" "${REPO_ROOT}/job.log"; then
  echo "job runtime did not emit expected payloads" >&2
  exit 1
fi
append_summary "### Flow runtime stream logs"
append_summary "\n\n\`\`\`"
if [[ -f "${REPO_ROOT}/stream.log" ]]; then
  append_summary "$(cat "${REPO_ROOT}/stream.log")"
fi
append_summary "\n\`\`\`"
append_summary "\n### Flow runtime job logs"
append_summary "\n\n\`\`\`"
if [[ -f "${REPO_ROOT}/job.log" ]]; then
  append_summary "$(cat "${REPO_ROOT}/job.log")"
fi
append_summary "\n\`\`\`"

info "Running observability queries"
cat <<MANIFEST | kubectl apply -f -
apiVersion: batch/v1
kind: Job
metadata:
  name: observability-queries
  namespace: ${NAMESPACE}
spec:
  backoffLimit: 0
  template:
    spec:
      restartPolicy: Never
      containers:
        - name: checks
          image: python:3.11-alpine
          command: ["/bin/sh", "-c"]
          args:
            - |
              set -euo pipefail
              apk add --no-cache curl >/dev/null
              cat <<'PY' >/tmp/checks.py
import json
import sys
import time
import urllib.parse
import urllib.request

PROM_BASE = "http://flow-pipe-prometheus-server/api/v1/query?query="
METRICS = [
    "flowpipe.queue.enqueue.count",
    "flowpipe.queue.dequeue.count",
    "flowpipe.stage.process.count",
]


def get_json(url: str):
    with urllib.request.urlopen(url) as resp:
        body = resp.read().decode()
        status = resp.status
    try:
        data = json.loads(body)
    except json.JSONDecodeError as exc:  # pragma: no cover - validation path
        raise SystemExit(f"invalid JSON from {url}: {exc}: {body}")
    return status, data, body


def require_metric(metric: str):
    url = PROM_BASE + urllib.parse.quote(metric, safe="")
    status, data, body = get_json(url)
    if status != 200:
        raise SystemExit(f"metric query failed ({status}) for {metric}: {body}")
    if data.get("status") != "success":
        raise SystemExit(f"metric query unsuccessful for {metric}: {body}")
    result = data.get("data", {}).get("result")
    if not result:
        raise SystemExit(f"metric {metric} returned no series: {body}")
    print(f"metric {metric} OK: {result}")


def require_loki(label_query: str, substring: str):
    url = "http://flow-pipe-loki:3100/loki/api/v1/query?query=" + urllib.parse.quote(label_query, safe="{}=\"\",")
    status, data, body = get_json(url)
    if status != 200:
        raise SystemExit(f"loki query failed ({status}): {body}")
    streams = data.get("data", {}).get("result") or []
    joined = "\n".join(
        entry for stream in streams for _, entry in stream.get("values", [])
    )
    if substring not in joined:
        raise SystemExit(f"log substring '{substring}' not found in Loki response")
    print(f"loki logs contain '{substring}'")


def require_http_ok(url: str):
    status, _, body = get_json(url)
    if status != 200:
        raise SystemExit(f"HTTP check failed ({status}) for {url}: {body}")
    print(f"HTTP {url} -> {status}")


time.sleep(20)
for metric in METRICS:
    require_metric(metric)

require_loki('{app="flow-runtime-stream"}', "DEADBEEF")
require_http_ok("http://flow-pipe-tempo:3200/status")

status, _, body = get_json("http://flow-pipe-grafana/login")
if status not in (200, 302):
    raise SystemExit(f"unexpected Grafana status {status}: {body}")
print(f"Grafana login endpoint status {status}")

print("all observability checks passed")
PY
              python /tmp/checks.py
MANIFEST

kubectl wait --for=condition=complete job/observability-queries -n "${NAMESPACE}" --timeout=300s
kubectl logs job/observability-queries -n "${NAMESPACE}" >"${REPO_ROOT}/observability.log"
append_summary "\n### Observability queries"
append_summary "\n\n\`\`\`"
if [[ -f "${REPO_ROOT}/observability.log" ]]; then
  append_summary "$(cat "${REPO_ROOT}/observability.log")"
fi
append_summary "\n\`\`\`"

append_summary "\n## Helm status"
append_summary "$(helm status flow-pipe -n "${NAMESPACE}" 2>/dev/null || true)"
append_summary "\n## Pods"
append_summary "$(kubectl get pods -n "${NAMESPACE}" -o wide 2>/dev/null || true)"
append_summary "\n## Services"
append_summary "$(kubectl get svc -n "${NAMESPACE}" 2>/dev/null || true)"
append_summary "\n## Events"
append_summary "$(kubectl get events -n "${NAMESPACE}" --sort-by=.metadata.creationTimestamp | tail -n 50 2>/dev/null || true)"

info "Cleanup helper jobs"
kubectl delete job/observability-queries -n "${NAMESPACE}" --ignore-not-found
kubectl delete job/flow-runtime-job -n "${NAMESPACE}" --ignore-not-found
kubectl delete deployment/flow-runtime-stream -n "${NAMESPACE}" --ignore-not-found

info "Script complete"
