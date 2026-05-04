terraform {
  required_version = ">= 1.6.0"
}

locals {
  common_labels = {
    "orbitdesktop.io/environment-id" = var.environment_id
    "orbitdesktop.io/owner"          = var.owner
    "orbitdesktop.io/template"       = "sample"
    "orbitdesktop.io/ttl-hours"      = tostring(var.ttl_hours)
  }
}

# Portfolio placeholder:
# In a real cluster, use the kubernetes provider to create namespaces,
# secrets, config maps, quotas, and service accounts.
output "namespace" {
  value = "preview-${var.environment_id}"
}

output "app_url" {
  value = "https://${var.environment_id}.preview.example.com"
}

output "labels" {
  value = local.common_labels
}

