variable "environment_id" {
  description = "Unique preview environment identifier."
  type        = string
}

variable "image_tag" {
  description = "Container image tag created by CI."
  type        = string
}

variable "ttl_hours" {
  description = "Time to live for the preview environment."
  type        = number
  default     = 2
}

variable "owner" {
  description = "Developer or team responsible for the environment."
  type        = string
  default     = "local"
}

