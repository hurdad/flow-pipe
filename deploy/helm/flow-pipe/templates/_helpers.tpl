{{- define "flowpipe.name" -}}flow-pipe{{- end }}

{{- define "flowpipe.namespace" -}}
{{- if .Values.namespace.create -}}
{{ .Values.namespace.name }}
{{- else -}}
{{ .Release.Namespace }}
{{- end }}
{{- end }}

{{- define "flowpipe.etcdHeadlessName" -}}flow-pipe-etcd-headless{{- end }}

{{- define "flowpipe.etcdInitialCluster" -}}
{{- $replicas := int .Values.etcd.replicas -}}
{{- $headless := include "flowpipe.etcdHeadlessName" . -}}
{{- $items := list -}}
{{- range $i, $_ := until $replicas -}}
{{- $items = append $items (printf "flow-pipe-etcd-%d=http://flow-pipe-etcd-%d.%s:2380" $i $i $headless) -}}
{{- end -}}
{{- join "," $items -}}
{{- end }}
