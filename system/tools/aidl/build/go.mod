module android/soong/aidl

require (
	android/soong v0.0.0
	github.com/google/blueprint v0.0.0
)

replace android/soong v0.0.0 => ../../../../build/soong

replace github.com/golang/protobuf v0.0.0 => ../../../../external/golang-protobuf

replace github.com/google/blueprint v0.0.0 => ../../../../build/blueprint

go 1.13
