# Deployment

flow-pipe is deployed exclusively via Helm.

Argo CD is the recommended installation mechanism, but Helm can also be used directly.

- `helm/flow-pipe` contains the platform chart
- `argocd/` contains Argo CD Application definitions
