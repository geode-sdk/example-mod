variable "MOD_ID" {}
variable "GEODE_VERSION" {}

target "_common" {
  context    = "."
  dockerfile = "docker/Dockerfile"

  args = {
    MOD_ID        = MOD_ID
    GEODE_VERSION = GEODE_VERSION
  }
}

target "android32" {
  inherits = ["_common"]
  target   = "export-android32"
  output   = ["type=local,dest=./bin/android32"]
}

target "android64" {
  inherits = ["_common"]
  target   = "export-android64"
  output   = ["type=local,dest=./bin/android64"]
}

target "ios" {
  inherits = ["_common"]
  target   = "export-ios"
  output   = ["type=local,dest=./bin/ios"]
}

target "macos" {
  inherits = ["_common"]
  target   = "export-macos"
  output   = ["type=local,dest=./bin/macos"]
}

target "windows" {
  inherits = ["_common"]
  target   = "export-windows"
  output   = ["type=local,dest=./bin/windows"]
}
