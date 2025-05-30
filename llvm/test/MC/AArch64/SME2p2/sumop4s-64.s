// RUN: llvm-mc -triple=aarch64 -show-encoding -mattr=+sme-mop4,+sme-i16i64 < %s \
// RUN:        | FileCheck %s --check-prefixes=CHECK-ENCODING,CHECK-INST
// RUN: not llvm-mc -triple=aarch64 -show-encoding < %s 2>&1 \
// RUN:        | FileCheck %s --check-prefix=CHECK-ERROR
// RUN: llvm-mc -triple=aarch64 -filetype=obj -mattr=+sme-mop4,+sme-i16i64 < %s \
// RUN:        | llvm-objdump -d --mattr=+sme-mop4,+sme-i16i64 - | FileCheck %s --check-prefix=CHECK-INST
// RUN: llvm-mc -triple=aarch64 -filetype=obj -mattr=+sme-mop4,+sme-i16i64 < %s \
// RUN:        | llvm-objdump -d --mattr=-sme-mop4 - | FileCheck %s --check-prefix=CHECK-UNKNOWN
// Disassemble encoding and check the re-encoding (-show-encoding) matches.
// RUN: llvm-mc -triple=aarch64 -show-encoding -mattr=+sme-mop4,+sme-i16i64 < %s \
// RUN:        | sed '/.text/d' | sed 's/.*encoding: //g' \
// RUN:        | llvm-mc -triple=aarch64 -mattr=+sme-mop4,+sme-i16i64 -disassemble -show-encoding \
// RUN:        | FileCheck %s --check-prefixes=CHECK-ENCODING,CHECK-INST

sumop4s za0.d, z0.h, z16.h  // 10100000-11100000-00000000-00011000
// CHECK-INST: sumop4s za0.d, z0.h, z16.h
// CHECK-ENCODING: [0x18,0x00,0xe0,0xa0]
// CHECK-ERROR: instruction requires: sme-i16i64 sme-mop4
// CHECK-UNKNOWN: a0e00018 <unknown>

sumop4s za5.d, z10.h, z20.h  // 10100000-11100100-00000001-01011101
// CHECK-INST: sumop4s za5.d, z10.h, z20.h
// CHECK-ENCODING: [0x5d,0x01,0xe4,0xa0]
// CHECK-ERROR: instruction requires: sme-i16i64 sme-mop4
// CHECK-UNKNOWN: a0e4015d <unknown>

sumop4s za7.d, z14.h, z30.h  // 10100000-11101110-00000001-11011111
// CHECK-INST: sumop4s za7.d, z14.h, z30.h
// CHECK-ENCODING: [0xdf,0x01,0xee,0xa0]
// CHECK-ERROR: instruction requires: sme-i16i64 sme-mop4
// CHECK-UNKNOWN: a0ee01df <unknown>

sumop4s za0.d, z0.h, {z16.h-z17.h}  // 10100000-11110000-00000000-00011000
// CHECK-INST: sumop4s za0.d, z0.h, { z16.h, z17.h }
// CHECK-ENCODING: [0x18,0x00,0xf0,0xa0]
// CHECK-ERROR: instruction requires: sme-i16i64 sme-mop4
// CHECK-UNKNOWN: a0f00018 <unknown>

sumop4s za5.d, z10.h, {z20.h-z21.h}  // 10100000-11110100-00000001-01011101
// CHECK-INST: sumop4s za5.d, z10.h, { z20.h, z21.h }
// CHECK-ENCODING: [0x5d,0x01,0xf4,0xa0]
// CHECK-ERROR: instruction requires: sme-i16i64 sme-mop4
// CHECK-UNKNOWN: a0f4015d <unknown>

sumop4s za7.d, z14.h, {z30.h-z31.h}  // 10100000-11111110-00000001-11011111
// CHECK-INST: sumop4s za7.d, z14.h, { z30.h, z31.h }
// CHECK-ENCODING: [0xdf,0x01,0xfe,0xa0]
// CHECK-ERROR: instruction requires: sme-i16i64 sme-mop4
// CHECK-UNKNOWN: a0fe01df <unknown>

sumop4s za0.d, {z0.h-z1.h}, z16.h  // 10100000-11100000-00000010-00011000
// CHECK-INST: sumop4s za0.d, { z0.h, z1.h }, z16.h
// CHECK-ENCODING: [0x18,0x02,0xe0,0xa0]
// CHECK-ERROR: instruction requires: sme-i16i64 sme-mop4
// CHECK-UNKNOWN: a0e00218 <unknown>

sumop4s za5.d, {z10.h-z11.h}, z20.h  // 10100000-11100100-00000011-01011101
// CHECK-INST: sumop4s za5.d, { z10.h, z11.h }, z20.h
// CHECK-ENCODING: [0x5d,0x03,0xe4,0xa0]
// CHECK-ERROR: instruction requires: sme-i16i64 sme-mop4
// CHECK-UNKNOWN: a0e4035d <unknown>

sumop4s za7.d, {z14.h-z15.h}, z30.h  // 10100000-11101110-00000011-11011111
// CHECK-INST: sumop4s za7.d, { z14.h, z15.h }, z30.h
// CHECK-ENCODING: [0xdf,0x03,0xee,0xa0]
// CHECK-ERROR: instruction requires: sme-i16i64 sme-mop4
// CHECK-UNKNOWN: a0ee03df <unknown>

sumop4s za0.d, {z0.h-z1.h}, {z16.h-z17.h}  // 10100000-11110000-00000010-00011000
// CHECK-INST: sumop4s za0.d, { z0.h, z1.h }, { z16.h, z17.h }
// CHECK-ENCODING: [0x18,0x02,0xf0,0xa0]
// CHECK-ERROR: instruction requires: sme-i16i64 sme-mop4
// CHECK-UNKNOWN: a0f00218 <unknown>

sumop4s za5.d, {z10.h-z11.h}, {z20.h-z21.h}  // 10100000-11110100-00000011-01011101
// CHECK-INST: sumop4s za5.d, { z10.h, z11.h }, { z20.h, z21.h }
// CHECK-ENCODING: [0x5d,0x03,0xf4,0xa0]
// CHECK-ERROR: instruction requires: sme-i16i64 sme-mop4
// CHECK-UNKNOWN: a0f4035d <unknown>

sumop4s za7.d, {z14.h-z15.h}, {z30.h-z31.h}  // 10100000-11111110-00000011-11011111
// CHECK-INST: sumop4s za7.d, { z14.h, z15.h }, { z30.h, z31.h }
// CHECK-ENCODING: [0xdf,0x03,0xfe,0xa0]
// CHECK-ERROR: instruction requires: sme-i16i64 sme-mop4
// CHECK-UNKNOWN: a0fe03df <unknown>
