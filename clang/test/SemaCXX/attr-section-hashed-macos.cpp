__attribute__((section("__RODATA,SectOK")))
int bar;

__attribute__((section("__RODATA,ThisSectionNameIsTooLong")))
int foo;

void baz() {
  extern int start_foo[] __asm("section$start$__RODATA$ThisSectionNameIsTooLong");
  extern int end_foo[] __asm("section$end$__RODATA$ThisSectionNameIsTooLong");

  const int *bim = start_foo;
  const int *bap = end_foo;
}

#pragma clang attribute section("__RODATA,ThisSectionNameIsTooLong")

// RUN: %clang_cc1 -fhash-section-names=16 -emit-llvm -o - %s | FileCheck %s
// REQUIRES: system-darwin
// CHECK: @bar = global i32 0, section "__RODATA,SectOK"
// CHECK: @foo = global i32 0, section "__RODATA,ip9RNVxH27rCS+Ix"
// CHECK: store i32* getelementptr inbounds ([0 x i32], [0 x i32]* @"\01section$start$__RODATA$ip9RNVxH27rCS+Ix"
// CHECK: store i32* getelementptr inbounds ([0 x i32], [0 x i32]* @"\01section$end$__RODATA$ip9RNVxH27rCS+Ix"

// TODO: test for #pragma
