__attribute__((section("__RODATA,SectOK")))
int bar;

__attribute__((section("__RODATA,ThisSectionNameIsTooLong")))
int foo;

#pragma clang attribute section("__RODATA,ThisSectionNameIsTooLong")

// RUN: %clang_cc1 -fhash-section-names=16 -emit-llvm -o - %s | FileCheck %s
// REQUIRES: system-darwin
// CHECK: @bar = global i32 0, section "__RODATA,SectOK"
// CHECK: @foo = global i32 0, section "__RODATA,ip9RNVxH27rCS+Ix"

// TODO: test for #pragma
