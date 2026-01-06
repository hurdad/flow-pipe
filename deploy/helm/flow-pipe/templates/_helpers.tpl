{{- define "flowpipe.name" -}}flow-pipe{{- end }}

{{- define "flowpipe.namespace" -}}
{{- if .Values.namespace.create -}}
{{ .Values.namespace.name }}
{{- else -}}
{{ .Release.Namespace }}
{{- end }}
{{- end }}

{{- define "flow-pipe.prometheus.endpoint" -}}
{{- if .Values.observability.prometheus.endpoint -}}
{{ .Values.observability.prometheus.endpoint }}
{{- else if .Values.observability.prometheus.enabled -}}
http://{{ .Release.Name }}-prometheus-server
{{- end -}}
{{- end }}

{{- define "flow-pipe.loki.endpoint" -}}
{{- if .Values.observability.loki.endpoint -}}
{{ .Values.observability.loki.endpoint }}
{{- else if .Values.observability.loki.enabled -}}
http://{{ .Release.Name }}-loki:3100
{{- end -}}
{{- end }}

{{- define "flow-pipe.tempo.endpoint" -}}
{{- if .Values.observability.tempo.endpoint -}}
{{ .Values.observability.tempo.endpoint }}
{{- else if .Values.observability.tempo.enabled -}}
http://{{ .Release.Name }}-tempo:3200
{{- end -}}
{{- end }}

{{- define "flow-pipe.grafana.endpoint" -}}
{{- if .Values.observability.grafana.endpoint -}}
{{ .Values.observability.grafana.endpoint }}
{{- else if .Values.observability.grafana.enabled -}}
{{- if and .Values.grafana .Values.grafana.ingress .Values.grafana.ingress.enabled (gt (len .Values.grafana.ingress.hosts) 0) -}}
{{- $scheme := ternary "https" "http" (and .Values.grafana.ingress.tls (gt (len .Values.grafana.ingress.tls) 0)) -}}
{{ printf "%s://%s" $scheme (index .Values.grafana.ingress.hosts 0) }}
{{- else -}}
http://{{ .Release.Name }}-grafana
{{- end -}}
{{- end -}}
{{- end }}

{{- define "flow-pipe.alloy.endpoint" -}}
{{- if .Values.observability.alloy.endpoint -}}
{{ .Values.observability.alloy.endpoint }}
{{- else if .Values.observability.alloy.enabled -}}
http://{{ .Release.Name }}-alloy:4317
{{- end -}}
{{- end }}

{{- define "flow-pipe.alloy.river" -}}
{{- $prom := include "flow-pipe.prometheus.endpoint" . -}}
{{- $loki := include "flow-pipe.loki.endpoint" . -}}
{{- $tempo := include "flow-pipe.tempo.endpoint" . -}}
{{- $exporters := dict "prom" "" "loki" "" "tempo" "" -}}
{{- if .Values.observability.alloy.exporters.prometheus.endpoint -}}
{{- $_ := set $exporters "prom" .Values.observability.alloy.exporters.prometheus.endpoint -}}
{{- else if $prom -}}
{{- $_ := set $exporters "prom" (printf "%s/api/v1/write" $prom) -}}
{{- end -}}
{{- if .Values.observability.alloy.exporters.loki.endpoint -}}
{{- $_ := set $exporters "loki" .Values.observability.alloy.exporters.loki.endpoint -}}
{{- else if $loki -}}
{{- $_ := set $exporters "loki" (printf "%s/loki/api/v1/push" $loki) -}}
{{- end -}}
{{- if .Values.observability.alloy.exporters.tempo.endpoint -}}
{{- $_ := set $exporters "tempo" .Values.observability.alloy.exporters.tempo.endpoint -}}
{{- else if $tempo -}}
{{- $_ := set $exporters "tempo" $tempo -}}
{{- end -}}
logging {
  level = "info"
}

otel {
  receiver "otlp" {
    protocols = {
      grpc = {
        endpoint = "{{ .Values.observability.alloy.receivers.otlp.grpcEndpoint }}"
      },
      http = {
        endpoint = "{{ .Values.observability.alloy.receivers.otlp.httpEndpoint }}"
      }
    }
  }
{{- if (get $exporters "prom") }}
  exporter "prometheusremotewrite" {
    endpoint = "{{ get $exporters "prom" }}"
  }
{{- end }}
{{- if (get $exporters "loki") }}
  exporter "loki" {
    endpoint = "{{ get $exporters "loki" }}"
  }
{{- end }}
{{- if (get $exporters "tempo") }}
  exporter "otlp" {
    client {
      endpoint = "{{ get $exporters "tempo" }}"
    }
  }
{{- end }}
  processor "batch" {}
{{- if (get $exporters "prom") }}
  pipeline "metrics" {
    receivers  = [otel.receiver.otlp]
    processors = [otel.processor.batch]
    exporters  = [otel.exporter.prometheusremotewrite]
  }
{{- end }}
{{- if (get $exporters "loki") }}
  pipeline "logs" {
    receivers  = [otel.receiver.otlp]
    processors = [otel.processor.batch]
    exporters  = [otel.exporter.loki]
  }
{{- end }}
{{- if (get $exporters "tempo") }}
  pipeline "traces" {
    receivers  = [otel.receiver.otlp]
    processors = [otel.processor.batch]
    exporters  = [otel.exporter.otlp]
  }
{{- end }}
}
{{- end }}
