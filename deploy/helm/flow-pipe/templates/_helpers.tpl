{{/* =========================================================
   Basic helpers
   ========================================================= */}}

{{- define "flowpipe.name" -}}
flow-pipe
{{- end }}

{{- define "flowpipe.namespace" -}}
{{- if .Values.namespace.create -}}
{{ .Values.namespace.name }}
{{- else -}}
{{ .Release.Namespace }}
{{- end }}
{{- end }}

{{/* =========================================================
   Endpoint helpers (nil-safe)
   ========================================================= */}}

{{- define "flow-pipe.prometheus.endpoint" -}}
{{- $obs := default dict .Values.observability -}}
{{- $p := default dict $obs.prometheus -}}
{{- if $p.endpoint -}}
{{ $p.endpoint }}
{{- else if $p.enabled -}}
http://{{ .Release.Name }}-prometheus-server
{{- end -}}
{{- end }}

{{- define "flow-pipe.loki.endpoint" -}}
{{- $obs := default dict .Values.observability -}}
{{- $l := default dict $obs.loki -}}
{{- if $l.endpoint -}}
{{ $l.endpoint }}
{{- else if $l.enabled -}}
http://{{ .Release.Name }}-loki:3100
{{- end -}}
{{- end }}

{{- define "flow-pipe.tempo.endpoint" -}}
{{- $obs := default dict .Values.observability -}}
{{- $t := default dict $obs.tempo -}}
{{- if $t.endpoint -}}
{{ $t.endpoint }}
{{- else if $t.enabled -}}
http://{{ .Release.Name }}-tempo:3200
{{- end -}}
{{- end }}

{{/* =========================================================
   Alloy River (NO early return, NO invalid funcs)
   ========================================================= */}}

{{- define "flow-pipe.alloy.river" -}}
{{- $obs := default dict .Values.observability -}}
{{- $alloy := default dict $obs.alloy -}}

{{- if $alloy.enabled }}

{{- $prom := include "flow-pipe.prometheus.endpoint" . -}}
{{- $loki := include "flow-pipe.loki.endpoint" . -}}
{{- $tempo := include "flow-pipe.tempo.endpoint" . -}}

logging {
  level = "info"
}

otel {
  receiver "otlp" {
    protocols = {
      grpc = { endpoint = "0.0.0.0:4317" }
      http = { endpoint = "0.0.0.0:4318" }
    }
  }

{{- if $prom }}
  exporter "prometheusremotewrite" {
    endpoint = "{{ $prom }}/api/v1/write"
  }
{{- end }}

{{- if $loki }}
  exporter "loki" {
    endpoint = "{{ $loki }}/loki/api/v1/push"
  }
{{- end }}

{{- if $tempo }}
  exporter "otlp" {
    client {
      endpoint = "{{ $tempo }}"
    }
  }
{{- end }}

  processor "batch" {}

{{- if $prom }}
  pipeline "metrics" {
    receivers  = [otel.receiver.otlp]
    processors = [otel.processor.batch]
    exporters  = [otel.exporter.prometheusremotewrite]
  }
{{- end }}

{{- if $loki }}
  pipeline "logs" {
    receivers  = [otel.receiver.otlp]
    processors = [otel.processor.batch]
    exporters  = [otel.exporter.loki]
  }
{{- end }}

{{- if $tempo }}
  pipeline "traces" {
    receivers  = [otel.receiver.otlp]
    processors = [otel.processor.batch]
    exporters  = [otel.exporter.otlp]
  }
{{- end }}
}

{{- end }}
{{- end }}
