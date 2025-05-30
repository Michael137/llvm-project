// RUN: mlir-opt -finalize-memref-to-llvm -split-input-file %s | FileCheck %s

// CHECK-LABEL: func @zero_d_alloc()
func.func @zero_d_alloc() -> memref<f32> {
// CHECK: %[[one:.*]] = llvm.mlir.constant(1 : index) : i64
// CHECK: %[[null:.*]] = llvm.mlir.zero : !llvm.ptr
// CHECK: %[[gep:.*]] = llvm.getelementptr %[[null]][%[[one]]] : (!llvm.ptr, i64) -> !llvm.ptr, f32
// CHECK: %[[size_bytes:.*]] = llvm.ptrtoint %[[gep]] : !llvm.ptr to i64
// CHECK: %[[ptr:.*]] = llvm.call @malloc(%[[size_bytes]]) : (i64) -> !llvm.ptr
// CHECK: llvm.mlir.poison : !llvm.struct<(ptr, ptr, i64)>
// CHECK: llvm.insertvalue %[[ptr]], %{{.*}}[0] : !llvm.struct<(ptr, ptr, i64)>
// CHECK: llvm.insertvalue %[[ptr]], %{{.*}}[1] : !llvm.struct<(ptr, ptr, i64)>
// CHECK: %[[c0:.*]] = llvm.mlir.constant(0 : index) : i64
// CHECK: llvm.insertvalue %[[c0]], %{{.*}}[2] : !llvm.struct<(ptr, ptr, i64)>
// CHECK: unrealized_conversion_cast %{{.*}}

  %0 = memref.alloc() : memref<f32>
  return %0 : memref<f32>
}

// -----

// CHECK-LABEL: func @zero_d_dealloc
func.func @zero_d_dealloc(%arg0: memref<f32>) {
// CHECK: unrealized_conversion_cast
// CHECK: %[[ptr:.*]] = llvm.extractvalue %{{.*}}[0] : !llvm.struct<(ptr, ptr, i64)>
// CHECK: llvm.call @free(%[[ptr]]) : (!llvm.ptr) -> ()

  memref.dealloc %arg0 : memref<f32>
  return
}

// -----

// CHECK-LABEL: func @aligned_1d_alloc(
func.func @aligned_1d_alloc() -> memref<42xf32> {
// CHECK: %[[sz1:.*]] = llvm.mlir.constant(42 : index) : i64
// CHECK: %[[st1:.*]] = llvm.mlir.constant(1 : index) : i64
// CHECK: %[[null:.*]] = llvm.mlir.zero : !llvm.ptr
// CHECK: %[[gep:.*]] = llvm.getelementptr %[[null]][%[[sz1]]] : (!llvm.ptr, i64) -> !llvm.ptr, f32
// CHECK: %[[size_bytes:.*]] = llvm.ptrtoint %[[gep]] : !llvm.ptr to i64
// CHECK: %[[alignment:.*]] = llvm.mlir.constant(8 : index) : i64
// CHECK: %[[allocsize:.*]] = llvm.add %[[size_bytes]], %[[alignment]] : i64
// CHECK: %[[ptr:.*]] = llvm.call @malloc(%[[allocsize]]) : (i64) -> !llvm.ptr
// CHECK: %[[allocatedAsInt:.*]] = llvm.ptrtoint %[[ptr]] : !llvm.ptr to i64
// CHECK: %[[one_1:.*]] = llvm.mlir.constant(1 : index) : i64
// CHECK: %[[bump:.*]] = llvm.sub %[[alignment]], %[[one_1]] : i64
// CHECK: %[[bumped:.*]] = llvm.add %[[allocatedAsInt]], %[[bump]] : i64
// CHECK: %[[mod:.*]] = llvm.urem %[[bumped]], %[[alignment]] : i64
// CHECK: %[[aligned:.*]] = llvm.sub %[[bumped]], %[[mod]] : i64
// CHECK: %[[alignedBitCast:.*]] = llvm.inttoptr %[[aligned]] : i64 to !llvm.ptr
// CHECK: llvm.mlir.poison : !llvm.struct<(ptr, ptr, i64, array<1 x i64>, array<1 x i64>)>
// CHECK: llvm.insertvalue %[[ptr]], %{{.*}}[0] : !llvm.struct<(ptr, ptr, i64, array<1 x i64>, array<1 x i64>)>
// CHECK: llvm.insertvalue %[[alignedBitCast]], %{{.*}}[1] : !llvm.struct<(ptr, ptr, i64, array<1 x i64>, array<1 x i64>)>
// CHECK: %[[c0:.*]] = llvm.mlir.constant(0 : index) : i64
// CHECK: llvm.insertvalue %[[c0]], %{{.*}}[2] : !llvm.struct<(ptr, ptr, i64, array<1 x i64>, array<1 x i64>)>
  %0 = memref.alloc() {alignment = 8} : memref<42xf32>
  return %0 : memref<42xf32>
}

// -----

// CHECK-LABEL: func @static_alloc()
func.func @static_alloc() -> memref<32x18xf32> {
// CHECK: %[[num_elems:.*]] = llvm.mlir.constant(576 : index) : i64
// CHECK: %[[null:.*]] = llvm.mlir.zero : !llvm.ptr
// CHECK: %[[gep:.*]] = llvm.getelementptr %[[null]][%[[num_elems]]] : (!llvm.ptr, i64) -> !llvm.ptr, f32
// CHECK: %[[size_bytes:.*]] = llvm.ptrtoint %[[gep]] : !llvm.ptr to i64
// CHECK: llvm.call @malloc(%[[size_bytes]]) : (i64) -> !llvm.ptr
 %0 = memref.alloc() : memref<32x18xf32>
 return %0 : memref<32x18xf32>
}

// -----

// CHECK-LABEL: func @static_alloca()
func.func @static_alloca() -> memref<32x18xf32> {
// CHECK: %[[sz1:.*]] = llvm.mlir.constant(32 : index) : i64
// CHECK: %[[sz2:.*]] = llvm.mlir.constant(18 : index) : i64
// CHECK: %[[st2:.*]] = llvm.mlir.constant(1 : index) : i64
// CHECK: %[[num_elems:.*]] = llvm.mlir.constant(576 : index) : i64
// CHECK: %[[allocated:.*]] = llvm.alloca %[[num_elems]] x f32 : (i64) -> !llvm.ptr
 %0 = memref.alloca() : memref<32x18xf32>

 // Test with explicitly specified alignment. llvm.alloca takes care of the
 // alignment. The same pointer is thus used for allocation and aligned
 // accesses.
 // CHECK: %[[alloca_aligned:.*]] = llvm.alloca %{{.*}} x f32 {alignment = 32 : i64} : (i64) -> !llvm.ptr
 // CHECK: %[[desc:.*]] = llvm.mlir.poison : !llvm.struct<(ptr, ptr, i64, array<2 x i64>, array<2 x i64>)>
 // CHECK: %[[desc1:.*]] = llvm.insertvalue %[[alloca_aligned]], %[[desc]][0] : !llvm.struct<(ptr, ptr, i64, array<2 x i64>, array<2 x i64>)>
 // CHECK: llvm.insertvalue %[[alloca_aligned]], %[[desc1]][1] : !llvm.struct<(ptr, ptr, i64, array<2 x i64>, array<2 x i64>)>
 memref.alloca() {alignment = 32} : memref<32x18xf32>
 return %0 : memref<32x18xf32>
}

// -----

// CHECK-LABEL: func @static_alloca_zero()
func.func @static_alloca_zero() -> memref<32x0x18xf32> {
// CHECK: %[[sz1:.*]] = llvm.mlir.constant(32 : index) : i64
// CHECK: %[[sz2:.*]] = llvm.mlir.constant(0 : index) : i64
// CHECK: %[[sz3:.*]] = llvm.mlir.constant(18 : index) : i64
// CHECK: %[[st1:.*]] = llvm.mlir.constant(1 : index) : i64
// CHECK: %[[st2:.*]] = llvm.mlir.constant(0 : index) : i64
// CHECK: %[[num_elems:.*]] = llvm.mlir.constant(0 : index) : i64
// CHECK: %[[allocated:.*]] = llvm.alloca %[[num_elems]] x f32 : (i64) -> !llvm.ptr
 %0 = memref.alloca() : memref<32x0x18xf32>

 return %0 : memref<32x0x18xf32>
}

// -----

// CHECK-LABEL: func @static_dealloc
func.func @static_dealloc(%static: memref<10x8xf32>) {
// CHECK: %[[ptr:.*]] = llvm.extractvalue %{{.*}}[0] : !llvm.struct<(ptr, ptr, i64, array<2 x i64>, array<2 x i64>)>
// CHECK: llvm.call @free(%[[ptr]]) : (!llvm.ptr) -> ()
  memref.dealloc %static : memref<10x8xf32>
  return
}

// -----

// CHECK-LABEL: func @zero_d_load
func.func @zero_d_load(%arg0: memref<f32>) -> f32 {
// CHECK: %[[ptr:.*]] = llvm.extractvalue %{{.*}}[1] : !llvm.struct<(ptr, ptr, i64)>
// CHECK: %{{.*}} = llvm.load %[[ptr]] : !llvm.ptr -> f32
  %0 = memref.load %arg0[] : memref<f32>
  return %0 : f32
}

// -----

// CHECK-LABEL: func @static_load(
// CHECK-SAME:    %[[MEMREF:.*]]: memref<10x42xf32>, %[[I:.*]]: index, %[[J:.*]]: index)
func.func @static_load(%static : memref<10x42xf32>, %i : index, %j : index) {
// CHECK-DAG:  %[[II:.*]] = builtin.unrealized_conversion_cast %[[I]]
// CHECK-DAG:  %[[JJ:.*]] = builtin.unrealized_conversion_cast %[[J]]
// CHECK:  %[[ptr:.*]] = llvm.extractvalue %{{.*}}[1] : !llvm.struct<(ptr, ptr, i64, array<2 x i64>, array<2 x i64>)>
// CHECK:  %[[st0:.*]] = llvm.mlir.constant(42 : index) : i64
// CHECK:  %[[offI:.*]] = llvm.mul %[[II]], %[[st0]] overflow<nsw, nuw> : i64
// CHECK:  %[[off1:.*]] = llvm.add %[[offI]], %[[JJ]] overflow<nsw, nuw> : i64
// CHECK:  %[[addr:.*]] = llvm.getelementptr inbounds|nuw %[[ptr]][%[[off1]]] : (!llvm.ptr, i64) -> !llvm.ptr, f32
// CHECK:  llvm.load %[[addr]] : !llvm.ptr -> f32
  %0 = memref.load %static[%i, %j] : memref<10x42xf32>
  return
}

// -----

// CHECK-LABEL: func @zero_d_store
func.func @zero_d_store(%arg0: memref<f32>, %arg1: f32) {
// CHECK: %[[ptr:.*]] = llvm.extractvalue %[[ld:.*]][1] : !llvm.struct<(ptr, ptr, i64)>
// CHECK: llvm.store %{{.*}}, %[[ptr]] : f32, !llvm.ptr
  memref.store %arg1, %arg0[] : memref<f32>
  return
}

// -----

// CHECK-LABEL: func @static_store
// CHECK:         %[[MEMREF:.*]]: memref<10x42xf32>,
// CHECK-SAME:    %[[I:.*]]: index, %[[J:.*]]: index,
func.func @static_store(%static : memref<10x42xf32>, %i : index, %j : index, %val : f32) {
// CHECK-DAG: %[[II:.*]] = builtin.unrealized_conversion_cast %[[I]]
// CHECK-DAG: %[[JJ:.*]] = builtin.unrealized_conversion_cast %[[J]]
// CHECK: %[[ptr:.*]] = llvm.extractvalue %{{.*}}[1] : !llvm.struct<(ptr, ptr, i64, array<2 x i64>, array<2 x i64>)>
// CHECK: %[[st0:.*]] = llvm.mlir.constant(42 : index) : i64
// CHECK: %[[offI:.*]] = llvm.mul %[[II]], %[[st0]] overflow<nsw, nuw> : i64
// CHECK: %[[off1:.*]] = llvm.add %[[offI]], %[[JJ]] overflow<nsw, nuw> : i64
// CHECK: %[[addr:.*]] = llvm.getelementptr inbounds|nuw %[[ptr]][%[[off1]]] : (!llvm.ptr, i64) -> !llvm.ptr, f32
// CHECK: llvm.store %{{.*}}, %[[addr]] : f32, !llvm.ptr

  memref.store %val, %static[%i, %j] : memref<10x42xf32>
  return
}

// -----

// CHECK-LABEL: func @static_memref_dim
func.func @static_memref_dim(%static : memref<42x32x15x13x27xf32>) {
// CHECK:  llvm.mlir.constant(42 : index) : i64
  %c0 = arith.constant 0 : index
  %0 = memref.dim %static, %c0 : memref<42x32x15x13x27xf32>
// CHECK:  llvm.mlir.constant(32 : index) : i64
  %c1 = arith.constant 1 : index
  %1 = memref.dim %static, %c1 : memref<42x32x15x13x27xf32>
// CHECK:  llvm.mlir.constant(15 : index) : i64
  %c2 = arith.constant 2 : index
  %2 = memref.dim %static, %c2 : memref<42x32x15x13x27xf32>
// CHECK:  llvm.mlir.constant(13 : index) : i64
  %c3 = arith.constant 3 : index
  %3 = memref.dim %static, %c3 : memref<42x32x15x13x27xf32>
// CHECK:  llvm.mlir.constant(27 : index) : i64
  %c4 = arith.constant 4 : index
  %4 = memref.dim %static, %c4 : memref<42x32x15x13x27xf32>
  return
}

// -----

// CHECK-LABEL: func @static_out_of_bound_memref_dim
func.func @static_out_of_bound_memref_dim(%static : memref<42x32x15x13x27xf32>) -> index {
// CHECK: %[[C_MINUS_7:.*]] = arith.constant -7 : index
// CHECK: %[[C_MINUS_7_I64:.*]] = builtin.unrealized_conversion_cast %[[C_MINUS_7]] : index to i64
// CHECK: %[[UB_IDX:.*]] = llvm.getelementptr %{{.*}}[0, %[[C_MINUS_7_I64]]] : (!llvm.ptr, i64) -> !llvm.ptr
// CHECK: %[[UB_DIM_I64:.*]] = llvm.load %[[UB_IDX]] : !llvm.ptr
// CHECK: %[[UB_DIM:.*]] = builtin.unrealized_conversion_cast %[[UB_DIM_I64]] : i64 to index
// CHECK: return %[[UB_DIM]] : index
  %c-7 = arith.constant -7 : index
  %1 = memref.dim %static, %c-7 : memref<42x32x15x13x27xf32>
  return %1 : index
}
// -----

// Check that consistent types are emitted in address arithemic in presence of
// a data layout specification.
module attributes { dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<index, 32>> } {
  func.func @address() {
    %c1 = arith.constant 1 : index
    %0 = memref.alloc(%c1) : memref<? x vector<2xf32>>
    // CHECK-DAG: %[[CST_S:.*]] = arith.constant 1 : index
    // CHECK: %[[CST:.*]] = builtin.unrealized_conversion_cast
    // CHECK: llvm.mlir.zero
    // CHECK: llvm.getelementptr %{{.*}}[[CST]]
    // CHECK: llvm.ptrtoint %{{.*}} : !llvm.ptr to i32
    // CHECK: llvm.ptrtoint %{{.*}} : !llvm.ptr to i32
    // CHECK: llvm.add %{{.*}} : i32
    // CHECK: llvm.call @malloc(%{{.*}}) : (i32) -> !llvm.ptr
    // CHECK: llvm.ptrtoint %{{.*}} : !llvm.ptr to i32
    // CHECK: llvm.sub {{.*}} : i32
    // CHECK: llvm.add {{.*}} : i32
    // CHECK: llvm.urem {{.*}} : i32
    // CHECK: llvm.sub {{.*}} : i32
    // CHECK: llvm.inttoptr %{{.*}} : i32 to !llvm.ptr
    return
  }
}

// -----

memref.global "private" constant @__constant_3xi64 : memref<3xi64> = dense<[2, 6, 20]>

// CHECK-LABEL: func @memref.reshape
// CHECK-SAME:    %[[arg0:.*]]: memref<4x5x6xf32>) -> memref<2x6x20xf32>
func.func @memref.reshape(%arg0: memref<4x5x6xf32>) -> memref<2x6x20xf32> {
  // CHECK: %[[cast0:.*]] = builtin.unrealized_conversion_cast %arg0 : memref<4x5x6xf32> to !llvm.struct<(ptr, ptr, i64, array<3 x i64>, array<3 x i64>)>
  %0 = memref.get_global @__constant_3xi64 : memref<3xi64>

  // CHECK: %[[undef:.*]] = llvm.mlir.poison : !llvm.struct<(ptr, ptr, i64, array<3 x i64>, array<3 x i64>)>
  // CHECK: %[[elem0:.*]] = llvm.extractvalue %[[cast0]][0] : !llvm.struct<(ptr, ptr, i64, array<3 x i64>, array<3 x i64>)>
  // CHECK: %[[elem1:.*]] = llvm.extractvalue %[[cast0]][1] : !llvm.struct<(ptr, ptr, i64, array<3 x i64>, array<3 x i64>)>
  // CHECK: %[[insert0:.*]] = llvm.insertvalue %[[elem0]], %[[undef]][0] : !llvm.struct<(ptr, ptr, i64, array<3 x i64>, array<3 x i64>)>
  // CHECK: %[[insert1:.*]] = llvm.insertvalue %[[elem1]], %[[insert0:.*]][1] : !llvm.struct<(ptr, ptr, i64, array<3 x i64>, array<3 x i64>)>

  // CHECK: %[[zero:.*]] = llvm.mlir.constant(0 : index) : i64
  // CHECK: %[[insert2:.*]] = llvm.insertvalue %[[zero]], %[[insert1]][2] : !llvm.struct<(ptr, ptr, i64, array<3 x i64>, array<3 x i64>)>

  // CHECK: %[[one:.*]] = llvm.mlir.constant(1 : index) : i64
  // CHECK: %[[twenty0:.*]] = llvm.mlir.constant(20 : index) : i64
  // CHECK: %[[insert3:.*]] = llvm.insertvalue %[[twenty0]], %[[insert2]][3, 2] : !llvm.struct<(ptr, ptr, i64, array<3 x i64>, array<3 x i64>)>
  // CHECK: %[[insert4:.*]] = llvm.insertvalue %[[one]], %[[insert3]][4, 2] : !llvm.struct<(ptr, ptr, i64, array<3 x i64>, array<3 x i64>)>

  // CHECK: %[[twenty1:.*]] = llvm.mlir.constant(20 : index) : i64
  // CHECK: %[[six:.*]] = llvm.mlir.constant(6 : index) : i64
  // CHECK: %[[insert5:.*]] = llvm.insertvalue %[[six]], %[[insert4]][3, 1] : !llvm.struct<(ptr, ptr, i64, array<3 x i64>, array<3 x i64>)>
  // CHECK: %[[insert6:.*]] = llvm.insertvalue %[[twenty1]], %[[insert5]][4, 1] : !llvm.struct<(ptr, ptr, i64, array<3 x i64>, array<3 x i64>)>

  // CHECK: %[[hundred_and_twenty:.*]] = llvm.mlir.constant(120 : index) : i64
  // CHECK: %[[two:.*]] = llvm.mlir.constant(2 : index) : i64
  // CHECK: %[[insert7:.*]] = llvm.insertvalue %[[two]], %[[insert6]][3, 0] : !llvm.struct<(ptr, ptr, i64, array<3 x i64>, array<3 x i64>)>
  // CHECK: %[[insert8:.*]] = llvm.insertvalue %[[hundred_and_twenty]], %[[insert7]][4, 0] : !llvm.struct<(ptr, ptr, i64, array<3 x i64>, array<3 x i64>)>

  // CHECK: %[[cast1:.*]] = builtin.unrealized_conversion_cast %[[insert8]] : !llvm.struct<(ptr, ptr, i64, array<3 x i64>, array<3 x i64>)> to memref<2x6x20xf32>
  %1 = memref.reshape %arg0(%0) : (memref<4x5x6xf32>, memref<3xi64>) -> memref<2x6x20xf32>

  // CHECK: return %[[cast1]] : memref<2x6x20xf32>
  return %1 : memref<2x6x20xf32>
}

// -----

// CHECK-LABEL: func @memref.reshape.dynamic.dim
// CHECK-SAME:    %[[arg:.*]]: memref<?x?x?xf32>, %[[shape:.*]]: memref<4xi64>) -> memref<?x?x12x32xf32>
func.func @memref.reshape.dynamic.dim(%arg: memref<?x?x?xf32>, %shape: memref<4xi64>) -> memref<?x?x12x32xf32> {
  // CHECK-DAG: %[[arg_cast:.*]] = builtin.unrealized_conversion_cast %[[arg]] : memref<?x?x?xf32> to !llvm.struct<(ptr, ptr, i64, array<3 x i64>, array<3 x i64>)>
  // CHECK-DAG: %[[shape_cast:.*]] = builtin.unrealized_conversion_cast %[[shape]] : memref<4xi64> to !llvm.struct<(ptr, ptr, i64, array<1 x i64>, array<1 x i64>)>
  // CHECK: %[[undef:.*]] = llvm.mlir.poison : !llvm.struct<(ptr, ptr, i64, array<4 x i64>, array<4 x i64>)>
  // CHECK: %[[alloc_ptr:.*]] = llvm.extractvalue %[[arg_cast]][0] : !llvm.struct<(ptr, ptr, i64, array<3 x i64>, array<3 x i64>)>
  // CHECK: %[[align_ptr:.*]] = llvm.extractvalue %[[arg_cast]][1] : !llvm.struct<(ptr, ptr, i64, array<3 x i64>, array<3 x i64>)>
  // CHECK: %[[insert0:.*]] = llvm.insertvalue %[[alloc_ptr]], %[[undef]][0] : !llvm.struct<(ptr, ptr, i64, array<4 x i64>, array<4 x i64>)>
  // CHECK: %[[insert1:.*]] = llvm.insertvalue %[[align_ptr]], %[[insert0]][1] : !llvm.struct<(ptr, ptr, i64, array<4 x i64>, array<4 x i64>)>

  // CHECK: %[[zero0:.*]] = llvm.mlir.constant(0 : index) : i64
  // CHECK: %[[insert2:.*]] = llvm.insertvalue %[[zero0]], %[[insert1]][2] : !llvm.struct<(ptr, ptr, i64, array<4 x i64>, array<4 x i64>)>

  // CHECK: %[[one0:.*]] = llvm.mlir.constant(1 : index) : i64
  // CHECK: %[[thirty_two0:.*]] = llvm.mlir.constant(32 : index) : i64
  // CHECK: %[[insert3:.*]] = llvm.insertvalue %[[thirty_two0]], %[[insert2]][3, 3] : !llvm.struct<(ptr, ptr, i64, array<4 x i64>, array<4 x i64>)>
  // CHECK: %[[insert4:.*]] = llvm.insertvalue %[[one0]], %[[insert3]][4, 3] : !llvm.struct<(ptr, ptr, i64, array<4 x i64>, array<4 x i64>)>

  // CHECK: %[[thirty_two1:.*]] = llvm.mlir.constant(32 : index) : i64
  // CHECK: %[[twelve:.*]] = llvm.mlir.constant(12 : index) : i64
  // CHECK: %[[insert5:.*]] = llvm.insertvalue %[[twelve]], %[[insert4]][3, 2] : !llvm.struct<(ptr, ptr, i64, array<4 x i64>, array<4 x i64>)>
  // CHECK: %[[insert6:.*]] = llvm.insertvalue %[[thirty_two1]], %[[insert5]][4, 2] : !llvm.struct<(ptr, ptr, i64, array<4 x i64>, array<4 x i64>)>

  // CHECK: %[[three_hundred_and_eighty_four:.*]] = llvm.mlir.constant(384 : index) : i64
  // CHECK: %[[one1:.*]] = llvm.mlir.constant(1 : index) : i64
  // CHECK: %[[shape_ptr0:.*]] = llvm.extractvalue %[[shape_cast]][1] : !llvm.struct<(ptr, ptr, i64, array<1 x i64>, array<1 x i64>)>
  // CHECK: %[[shape_gep0:.*]] = llvm.getelementptr inbounds|nuw %[[shape_ptr0]][%[[one1]]] : (!llvm.ptr, i64) -> !llvm.ptr, i64
  // CHECK: %[[shape_load0:.*]] = llvm.load %[[shape_gep0]] : !llvm.ptr -> i64
  // CHECK: %[[insert7:.*]] = llvm.insertvalue %[[shape_load0]], %[[insert6]][3, 1] : !llvm.struct<(ptr, ptr, i64, array<4 x i64>, array<4 x i64>)>
  // CHECK: %[[insert8:.*]] = llvm.insertvalue %[[three_hundred_and_eighty_four]], %[[insert7]][4, 1] : !llvm.struct<(ptr, ptr, i64, array<4 x i64>, array<4 x i64>)>

  // CHECK: %[[mul:.*]] = llvm.mul %19, %23  : i64
  // CHECK: %[[zero1:.*]] = llvm.mlir.constant(0 : index) : i64
  // CHECK: %[[shape_ptr1:.*]] = llvm.extractvalue %[[shape_cast]][1] : !llvm.struct<(ptr, ptr, i64, array<1 x i64>, array<1 x i64>)>
  // CHECK: %[[shape_gep1:.*]] = llvm.getelementptr inbounds|nuw %[[shape_ptr1]][%[[zero1]]] : (!llvm.ptr, i64) -> !llvm.ptr, i64
  // CHECK: %[[shape_load1:.*]] = llvm.load %[[shape_gep1]] : !llvm.ptr -> i64
  // CHECK: %[[insert9:.*]] = llvm.insertvalue %[[shape_load1]], %[[insert8]][3, 0] : !llvm.struct<(ptr, ptr, i64, array<4 x i64>, array<4 x i64>)>
  // CHECK: %[[insert10:.*]] = llvm.insertvalue %[[mul]], %[[insert9]][4, 0] : !llvm.struct<(ptr, ptr, i64, array<4 x i64>, array<4 x i64>)>

  // CHECK: %[[result_cast:.*]] = builtin.unrealized_conversion_cast %[[insert10]] : !llvm.struct<(ptr, ptr, i64, array<4 x i64>, array<4 x i64>)> to memref<?x?x12x32xf32>
  %0 = memref.reshape %arg(%shape) : (memref<?x?x?xf32>, memref<4xi64>) -> memref<?x?x12x32xf32>

  return %0 : memref<?x?x12x32xf32>
  // CHECK: return %[[result_cast]] : memref<?x?x12x32xf32>
}

// -----

// CHECK-LABEL: func @memref.reshape_index
// CHECK-SAME:    %[[arg:.*]]: memref<?x?xi32>, %[[shape:.*]]: memref<1xindex>
func.func @memref.reshape_index(%arg0: memref<?x?xi32>, %shape: memref<1xindex>) ->  memref<?xi32> {
  // CHECK-DAG: %[[arg_cast:.*]] = builtin.unrealized_conversion_cast %[[arg]] : memref<?x?xi32> to !llvm.struct<(ptr, ptr, i64, array<2 x i64>, array<2 x i64>)>
  // CHECK-DAG: %[[shape_cast:.*]] = builtin.unrealized_conversion_cast %[[shape]] : memref<1xindex> to !llvm.struct<(ptr, ptr, i64, array<1 x i64>, array<1 x i64>)>
  // CHECK: %[[undef:.*]] = llvm.mlir.poison : !llvm.struct<(ptr, ptr, i64, array<1 x i64>, array<1 x i64>)>
  // CHECK: %[[alloc_ptr:.*]] = llvm.extractvalue %[[arg_cast]][0] : !llvm.struct<(ptr, ptr, i64, array<2 x i64>, array<2 x i64>)>
  // CHECK: %[[align_ptr:.*]] = llvm.extractvalue %[[arg_cast]][1] : !llvm.struct<(ptr, ptr, i64, array<2 x i64>, array<2 x i64>)>
  // CHECK: %[[insert0:.*]] = llvm.insertvalue %[[alloc_ptr]], %[[undef:.*]][0] : !llvm.struct<(ptr, ptr, i64, array<1 x i64>, array<1 x i64>)>
  // CHECK: %[[insert1:.*]] = llvm.insertvalue %[[align_ptr]], %[[insert0:.*]][1] : !llvm.struct<(ptr, ptr, i64, array<1 x i64>, array<1 x i64>)>

  // CHECK: %[[zero0:.*]] = llvm.mlir.constant(0 : index) : i64
  // CHECK: %[[insert2:.*]] = llvm.insertvalue %[[zero0]], %[[insert1:.*]][2] : !llvm.struct<(ptr, ptr, i64, array<1 x i64>, array<1 x i64>)>

  // CHECK: %[[one0:.*]] = llvm.mlir.constant(1 : index) : i64
  // CHECK: %[[zero1:.*]] = llvm.mlir.constant(0 : index) : i64

  // CHECK: %[[shape_ptr0:.*]] = llvm.extractvalue %[[shape_cast:.*]][1] : !llvm.struct<(ptr, ptr, i64, array<1 x i64>, array<1 x i64>)>
  // CHECK: %[[shape_gep0:.*]] = llvm.getelementptr inbounds|nuw %[[shape_ptr0:.*]][%[[zero1:.*]]] : (!llvm.ptr, i64) -> !llvm.ptr, i64
  // CHECK: %[[shape_load0:.*]] = llvm.load %[[shape_gep0:.*]] : !llvm.ptr -> i64
  // CHECK: %[[insert3:.*]] = llvm.insertvalue %[[shape_load0:.*]], %[[insert2:.*]][3, 0] : !llvm.struct<(ptr, ptr, i64, array<1 x i64>, array<1 x i64>)>
  // CHECK: %[[insert4:.*]] = llvm.insertvalue %[[one0:.*]], %[[insert3:.*]][4, 0] : !llvm.struct<(ptr, ptr, i64, array<1 x i64>, array<1 x i64>)>

  // CHECK: %[[result_cast:.*]] = builtin.unrealized_conversion_cast %[[insert4:.*]] : !llvm.struct<(ptr, ptr, i64, array<1 x i64>, array<1 x i64>)> to memref<?xi32>
  // CHECK: return %[[result_cast:.*]] : memref<?xi32>

  %1 = memref.reshape %arg0(%shape) : (memref<?x?xi32>, memref<1xindex>) -> memref<?xi32>
  return %1 : memref<?xi32>
}

// CHECK-LABEL: @memref_memory_space_cast
func.func @memref_memory_space_cast(%input : memref<?xf32>) -> memref<?xf32, 1> {
  %cast = memref.memory_space_cast %input : memref<?xf32> to memref<?xf32, 1>
  return %cast : memref<?xf32, 1>
}
// CHECK: [[INPUT:%.*]] = builtin.unrealized_conversion_cast %{{.*}}
//  CHECK: [[ALLOC:%.*]] = llvm.extractvalue [[INPUT]][0]
//  CHECK: [[ALIGN:%.*]] = llvm.extractvalue [[INPUT]][1]
// CHECK: [[OFFSET:%.*]] = llvm.extractvalue [[INPUT]][2]
//   CHECK: [[SIZE:%.*]] = llvm.extractvalue [[INPUT]][3, 0]
// CHECK: [[STRIDE:%.*]] = llvm.extractvalue [[INPUT]][4, 0]
// CHECK: [[CAST_ALLOC:%.*]] = llvm.addrspacecast [[ALLOC]] : !llvm.ptr to !llvm.ptr<1>
// CHECK: [[CAST_ALIGN:%.*]] = llvm.addrspacecast [[ALIGN]] : !llvm.ptr to !llvm.ptr<1>
// CHECK: [[RESULT_0:%.*]] = llvm.mlir.poison
// CHECK: [[RESULT_1:%.*]] = llvm.insertvalue [[CAST_ALLOC]], [[RESULT_0]][0]
// CHECK: [[RESULT_2:%.*]] = llvm.insertvalue [[CAST_ALIGN]], [[RESULT_1]][1]
// CHECK: [[RESULT_3:%.*]] = llvm.insertvalue [[OFFSET]], [[RESULT_2]][2]
// CHECK: [[RESULT_4:%.*]] = llvm.insertvalue [[SIZE]], [[RESULT_3]][3, 0]
// CHECK: [[RESULT_5:%.*]] = llvm.insertvalue [[STRIDE]], [[RESULT_4]][4, 0]
// CHECK: [[RESULT:%.*]] = builtin.unrealized_conversion_cast [[RESULT_5]] : {{.*}} to memref<?xf32, 1>
// CHECK: return [[RESULT]]
