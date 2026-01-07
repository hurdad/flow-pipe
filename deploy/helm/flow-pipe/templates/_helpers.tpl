{{- define "flowpipe.name" -}}flow-pipe{{- end }}

{{- define "flowpipe.namespace" -}}
{{- if .Values.namespace.create -}}
{{ .Values.namespace.name }}
{{- else -}}
{{ .Release.Namespace }}
{{- end }}
{{- end }}

{{- define "flow-pipe.prometheus.endpoint" -}}
{{- $global := default (dict) .Values.global -}}
{{- $observability := default (dict) (default $global.observability .Values.observability) -}}
{{- $prometheus := default (dict) $observability.prometheus -}}
{{- if $prometheus.endpoint -}}
{{ $prometheus.endpoint }}
{{- else if $prometheus.enabled -}}
http://{{ .Release.Name }}-prometheus-server
{{- end -}}
{{- end }}

{{- define "flow-pipe.loki.endpoint" -}}
{{- $global := default (dict) .Values.global -}}
{{- $observability := default (dict) (default $global.observability .Values.observability) -}}
{{- $loki := default (dict) $observability.loki -}}
{{- if $loki.endpoint -}}
{{ $loki.endpoint }}
{{- else if $loki.enabled -}}
http://{{ .Release.Name }}-loki:3100
{{- end -}}
{{- end }}

{{- define "flow-pipe.tempo.endpoint" -}}
{{- $global := default (dict) .Values.global -}}
{{- $observability := default (dict) (default $global.observability .Values.observability) -}}
{{- $tempo := default (dict) $observability.tempo -}}
{{- if $tempo.endpoint -}}
{{ $tempo.endpoint }}
{{- else if $tempo.enabled -}}
http://{{ .Release.Name }}-tempo:3200
{{- end -}}
{{- end }}

{{- define "flow-pipe.grafana.endpoint" -}}
{{- $global := default (dict) .Values.global -}}
{{- $observability := default (dict) (default $global.observability .Values.observability) -}}
{{- $grafanaObs := default (dict) $observability.grafana -}}
{{- if $grafanaObs.endpoint -}}
{{- $grafanaObs.endpoint -}}
{{- else if and $grafanaObs.enabled
              .Values.grafana
              .Values.grafana.ingress
              .Values.grafana.ingress.enabled -}}
{{- $scheme := ternary "https" "http" (gt (len .Values.grafana.ingress.tls) 0) -}}
{{- if gt (len .Values.grafana.ingress.hosts) 0 -}}
{{- printf "%s://%s" $scheme (index .Values.grafana.ingress.hosts 0) -}}
{{- else -}}
{{- printf "%s://%s-grafana" $scheme .Release.Name -}}
{{- end -}}
{{- end -}}
{{- end -}}


{{- define "flow-pipe.alloy.endpoint" -}}
{{- $global := default (dict) .Values.global -}}
{{- $observability := default (dict) (default $global.observability .Values.observability) -}}
{{- $alloy := default (dict) $observability.alloy -}}
{{- if $alloy.endpoint -}}
{{ $alloy.endpoint }}
{{- else if $alloy.enabled -}}
http://{{ .Release.Name }}-alloy:4317
{{- end -}}
{{- end }}

{{- define "flow-pipe.alloy.river" -}}
{{- $global := default (dict) .Values.global -}}
{{- $observability := default (dict) (default $global.observability .Values.observability) -}}
{{- $alloy := default (dict) $observability.alloy -}}
{{- $prom := include "flow-pipe.prometheus.endpoint" . -}}
{{- $loki := include "flow-pipe.loki.endpoint" . -}}
{{- $tempo := include "flow-pipe.tempo.endpoint" . -}}

logging {
  level = "info"
}

################################################################################
# OTLP Receiver
################################################################################
otelcol.receiver.otlp "default" {
  grpc {
    endpoint = "{{ default "0.0.0.0:4317" (dig "receivers" "otlp" "grpcEndpoint" "" $alloy) }}"
  }

  http {
    endpoint = "{{ default "0.0.0.0:4318" (dig "receivers" "otlp" "httpEndpoint" "" $alloy) }}"
  }

  output {
{{- if or $tempo $prom $loki }}
{{- if $tempo }}
    traces  = [otelcol.processor.batch.default.input]
{{- end }}
{{- if $prom }}
    metrics = [otelcol.processor.batch.default.input]
{{- end }}
{{- if $loki }}
    logs    = [otelcol.processor.batch.default.input]
{{- end }}
{{- else }}
    traces  = []
    metrics = []
    logs    = []
{{- end }}
  }
}

################################################################################
# Batch Processor
################################################################################
otelcol.processor.batch "default" {
  output {
  {{- if or $tempo $prom $loki }}
{{- if $tempo }}
    traces  = [otelcol.exporter.otlp.tempo.input]
{{- end }}
{{- if $prom }}
    metrics = [prometheus.remote_write.default.input]
{{- end }}
{{- if $loki }}
    logs    = [loki.write.default.input]
{{- end }}
{{- else }}
    traces  = []
    metrics = []
    logs    = []
{{- end }}
  }
}

################################################################################
# Tempo (Traces)
################################################################################
{{- if $tempo }}
otelcol.exporter.otlp "tempo" {
  client {
    endpoint = "{{ $tempo }}"
    tls {
      insecure = true
    }
  }
}
{{- end }}

################################################################################
# Prometheus Remote Write (Metrics)
################################################################################
{{- if $prom }}
prometheus.remote_write "default" {
  endpoint {
    url = "{{ printf "%s/api/v1/write" $prom }}"
  }
}
{{- end }}

################################################################################
# Loki (Logs)
################################################################################
{{- if $loki }}
loki.write "default" {
  endpoint {
    url = "{{ printf "%s/loki/api/v1/push" $loki }}"
  }
}
{{- end }}

{{- end }}
