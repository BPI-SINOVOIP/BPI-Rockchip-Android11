#!/bin/bash

ANNOTATIONS=(
    org.checkerframework.checker.nullness.compatqual.MonotonicNonNullDecl
    org.checkerframework.checker.nullness.compatqual.NullableDecl
    org.checkerframework.checker.nullness.compatqual.NonNullDecl
    org.checkerframework.checker.nullness.qual.NonNull
    org.checkerframework.checker.nullness.qual.Nullable
    org.checkerframework.checker.nullness.qual.MonotonicNonNull
    org.codehaus.mojo.animal_sniffer.IgnoreJRERequirement
    com.google.j2objc.annotations.J2ObjCIncompatible
    com.google.j2objc.annotations.WeakOuter
    com.google.j2objc.annotations.Weak
    com.google.j2objc.annotations.ReflectionSupport
    com.google.j2objc.annotations.RetainedWith
)

for a in ${ANNOTATIONS[@]}; do
    package=${a%.*}
    class=${a##*.}
    dir=$(dirname $0)/src/${package//.//}
    file=${class}.java

    mkdir -p ${dir}
    sed -e"s/__PACKAGE__/${package}/" -e"s/__CLASS__/${class}/" tmpl.java > ${dir}/${file}
done

f=$(dirname $0)/src/com/google/j2objc/annotations/ReflectionSupport.java
head -n-1 ${f} > ${f}.tmp

cat >> ${f}.tmp <<EOF
public @interface ReflectionSupport {
  enum Level {
    FULL
  }

  Level value();
}
EOF

mv ${f}.tmp ${f}

