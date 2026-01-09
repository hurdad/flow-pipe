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
BIN_DIR="${REPO_ROOT}/.bin"
HELM_VERSION="${HELM_VERSION:-v3.14.4}"

info() { echo "[INFO] $*"; }
append_summary() {
  if [[ -n "${GITHUB_STEP_SUMMARY:-}" ]]; then
    printf '%s\n' "$*" >>"${GITHUB_STEP_SUMMARY}" || true
  fi
}

require_command() {
  local cmd="$1"
  if command -v "${cmd}" >/dev/null 2>&1; then
    return 0
  fi

  case "${cmd}" in
    kubectl)
      install_kubectl
      ;;
    helm)
      install_helm
      ;;
    *)
      echo "Required command '${cmd}' is missing" >&2
      return 1
      ;;
  esac
}

detect_platform() {
  local os arch
  os="$(uname -s | tr '[:upper:]' '[:lower:]')"
  arch="$(uname -m)"

  case "${os}" in
    linux|darwin) ;;
    *)
      echo "Unsupported OS for auto-install: ${os}" >&2
      return 1
      ;;
  esac

  case "${arch}" in
    x86_64|amd64) arch="amd64" ;;
    arm64|aarch64) arch="arm64" ;;
    *)
      echo "Unsupported architecture for auto-install: ${arch}" >&2
      return 1
      ;;
  esac

  printf '%s/%s' "${os}" "${arch}"
}

install_kubectl() {
  info "kubectl not found; installing locally"
  local platform
  platform="$(detect_platform)"
  local os="${platform%/*}"
  local arch="${platform#*/}"
  local version
  version="$(curl -fsSL https://dl.k8s.io/release/stable.txt)"
  mkdir -p "${BIN_DIR}"
  curl -fsSL -o "${BIN_DIR}/kubectl" "https://dl.k8s.io/release/${version}/bin/${os}/${arch}/kubectl"
  chmod +x "${BIN_DIR}/kubectl"
  export PATH="${BIN_DIR}:${PATH}"
}

install_helm() {
  info "Helm not found; installing locally"
  local platform
  platform="$(detect_platform)"
  local os="${platform%/*}"
  local arch="${platform#*/}"
  local tarball="helm-${HELM_VERSION}-${os}-${arch}.tar.gz"
  local tmp_dir
  tmp_dir="$(mktemp -d)"
  mkdir -p "${BIN_DIR}"
  curl -fsSL -o "${tmp_dir}/${tarball}" "https://get.helm.sh/${tarball}"
  tar -xzf "${tmp_dir}/${tarball}" -C "${tmp_dir}"
  mv "${tmp_dir}/${os}-${arch}/helm" "${BIN_DIR}/helm"
  chmod +x "${BIN_DIR}/helm"
  rm -rf "${tmp_dir}"
  export PATH="${BIN_DIR}:${PATH}"
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

require_command kubectl
require_command helm

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
  -f "${REPO_ROOT}/runtime/docker/Dockerfile" \
  --target runtime \
  -t "${IMAGE_NAMESPACE}/flow-pipe-runtime:${IMAGE_TAG}" "${REPO_ROOT}"

info "Building base image"
docker buildx build --load \
  -f "${REPO_ROOT}/runtime/docker/Dockerfile" \
  --target base \
  -t "${IMAGE_NAMESPACE}/flow-pipe-base:${IMAGE_TAG}" "${REPO_ROOT}"

info "Building dev image"
docker buildx build --load \
  -f "${REPO_ROOT}/runtime/docker/Dockerfile" \
  --target dev \
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
info "Ensuring Helm repositories"
helm repo add grafana https://grafana.github.io/helm-charts --force-update >/dev/null
helm repo update >/dev/null
info "Fetching Helm chart dependencies"
helm dependency build "${REPO_ROOT}/deploy/helm/flow-pipe"
OBS_EXPORT_ENDPOINT="http://observability-backend:4317"
helm upgrade --install flow-pipe "${REPO_ROOT}/deploy/helm/flow-pipe" \
  --namespace "${NAMESPACE}" \
  --create-namespace \
  --set controller.image="${IMAGE_NAMESPACE}/flow-pipe-controller:${IMAGE_TAG}" \
  --set api.image="${IMAGE_NAMESPACE}/flow-pipe-api:${IMAGE_TAG}" \
  --set runtime.image="${IMAGE_NAMESPACE}/flow-pipe-runtime:${IMAGE_TAG}" \
  --set observability.alloy.enabled=true \
  --set observability.alloy.exporters.metrics.endpoint="${OBS_EXPORT_ENDPOINT}" \
  --set observability.alloy.exporters.traces.endpoint="${OBS_EXPORT_ENDPOINT}" \
  --set observability.alloy.exporters.logs.endpoint="${OBS_EXPORT_ENDPOINT}"

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
FLOW_KUBERNETES_CONFIG=$(cat <<FLOW
kubernetes:
  image: ${IMAGE_NAMESPACE}/flow-pipe-runtime:${IMAGE_TAG}
  image_pull_policy: IMAGE_PULL_POLICY_IF_NOT_PRESENT
  restart_policy: RESTART_POLICY_ALWAYS
FLOW
)

cat >"${FLOW_TMP_DIR}/streaming.yaml" <<FLOW
name: noop-observability
runtime: FLOW_RUNTIME_BUILTIN
image: ${IMAGE_NAMESPACE}/flow-pipe-runtime:${IMAGE_TAG}
execution:
  mode: EXECUTION_MODE_STREAMING
${FLOW_KUBERNETES_CONFIG}
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

cat >"${FLOW_TMP_DIR}/streaming-update.yaml" <<FLOW
name: noop-observability
runtime: FLOW_RUNTIME_BUILTIN
image: ${IMAGE_NAMESPACE}/flow-pipe-runtime:${IMAGE_TAG}
execution:
  mode: EXECUTION_MODE_STREAMING
${FLOW_KUBERNETES_CONFIG}
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
      message: "FEEDBEEF"
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

cat >"${FLOW_TMP_DIR}/job.yaml" <<FLOW
name: simple-pipeline-job
runtime: FLOW_RUNTIME_BUILTIN
image: ${IMAGE_NAMESPACE}/flow-pipe-runtime:${IMAGE_TAG}
execution:
  mode: EXECUTION_MODE_JOB
${FLOW_KUBERNETES_CONFIG}
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

chmod -R a+rX "${FLOW_TMP_DIR}"

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

info "Updating flow with flowctl"
docker run --rm --network host -v "${FLOW_TMP_DIR}:/flows:ro" \
  "${IMAGE_NAMESPACE}/flow-pipe-cli:${IMAGE_TAG}" update /flows/streaming-update.yaml --api localhost:9090
wait_for_runtime "noop-observability" "deployment/noop-observability-runtime"

info "Rolling back flow with flowctl"
docker run --rm --network host \
  "${IMAGE_NAMESPACE}/flow-pipe-cli:${IMAGE_TAG}" rollback noop-observability --version 1 --api localhost:9090
wait_for_runtime "noop-observability" "deployment/noop-observability-runtime"

info "Stopping flows with flowctl"
docker run --rm --network host \
  "${IMAGE_NAMESPACE}/flow-pipe-cli:${IMAGE_TAG}" stop noop-observability --api localhost:9090
docker run --rm --network host \
  "${IMAGE_NAMESPACE}/flow-pipe-cli:${IMAGE_TAG}" stop simple-pipeline-job --api localhost:9090
info "Waiting for flow resources to be removed"
kubectl wait --for=delete deployment/noop-observability-runtime -n "${NAMESPACE}" --timeout=120s
kubectl wait --for=delete job/simple-pipeline-job -n "${NAMESPACE}" --timeout=120s
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

info "Checking observability configuration"
metrics_endpoint=$(kubectl get configmap flow-pipe-observability -n "${NAMESPACE}" -o jsonpath='{.data.metricsEndpoint}')
traces_endpoint=$(kubectl get configmap flow-pipe-observability -n "${NAMESPACE}" -o jsonpath='{.data.tracesEndpoint}')
logs_endpoint=$(kubectl get configmap flow-pipe-observability -n "${NAMESPACE}" -o jsonpath='{.data.logsEndpoint}')
alloy_endpoint=$(kubectl get configmap flow-pipe-observability -n "${NAMESPACE}" -o jsonpath='{.data.alloyEndpoint}')

{
  echo "metricsEndpoint: ${metrics_endpoint}"
  echo "tracesEndpoint: ${traces_endpoint}"
  echo "logsEndpoint: ${logs_endpoint}"
  echo "alloyEndpoint: ${alloy_endpoint}"
} >"${REPO_ROOT}/observability.log"

if [[ "${metrics_endpoint}" != "${OBS_EXPORT_ENDPOINT}" ]]; then
  echo "metrics endpoint mismatch: ${metrics_endpoint}" >&2
  exit 1
fi
if [[ "${traces_endpoint}" != "${OBS_EXPORT_ENDPOINT}" ]]; then
  echo "traces endpoint mismatch: ${traces_endpoint}" >&2
  exit 1
fi
if [[ "${logs_endpoint}" != "${OBS_EXPORT_ENDPOINT}" ]]; then
  echo "logs endpoint mismatch: ${logs_endpoint}" >&2
  exit 1
fi
if [[ -z "${alloy_endpoint}" ]]; then
  echo "alloy endpoint missing from observability config" >&2
  exit 1
fi
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
kubectl delete job/flow-runtime-job -n "${NAMESPACE}" --ignore-not-found
kubectl delete deployment/flow-runtime-stream -n "${NAMESPACE}" --ignore-not-found

info "Script complete"
