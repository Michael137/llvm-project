! RUN: %python %S/../test_errors.py %s %flang -fopenmp -fopenmp-version=52

! Check OpenMP declarative directives

! 2.4 requires

subroutine requires_1(a)
  real(8), intent(inout) :: a
  !$omp requires reverse_offload, unified_shared_memory, atomic_default_mem_order(relaxed)
  a = a + 0.01
end subroutine requires_1

subroutine requires_2(a)
  real(8), intent(inout) :: a
  !$omp requires unified_address
  a = a + 0.01
end subroutine requires_2

! 2.8.2 declare-simd

subroutine declare_simd_1(a, b)
  real(8), intent(inout) :: a, b
  !ERROR: 'a' in ALIGNED clause must be of type C_PTR, POINTER or ALLOCATABLE
  !$omp declare simd(declare_simd_1) aligned(a)
  a = 3.14 + b
end subroutine declare_simd_1

module m1
  abstract interface
     subroutine sub(x,y)
       integer, intent(in)::x
       integer, intent(in)::y
     end subroutine sub
  end interface
end module m1

subroutine declare_simd_2
  use m1
  procedure (sub) sub1
  !ERROR: NOTINBRANCH and INBRANCH clauses are mutually exclusive and may not appear on the same DECLARE SIMD directive
  !$omp declare simd(sub1) inbranch notinbranch
  procedure (sub), pointer::p
  p=>sub1
  call p(5,10)
end subroutine declare_simd_2

subroutine sub1 (x,y)
  integer, intent(in)::x, y
  print *, x+y
end subroutine sub1

! 2.10.6 declare-target
! 2.15.2 threadprivate

module m2
contains
  subroutine foo
    !$omp declare target
    !WARNING: The entity with PARAMETER attribute is used in a DECLARE TARGET directive [-Wopen-mp-usage]
    !WARNING: The entity with PARAMETER attribute is used in a DECLARE TARGET directive [-Wopen-mp-usage]
    !$omp declare target (foo, N, M)
    !WARNING: The usage of TO clause on DECLARE TARGET directive has been deprecated. Use ENTER clause instead. [-Wopen-mp-usage]
    !ERROR: A variable that appears in a DECLARE TARGET directive must be declared in the scope of a module or have the SAVE attribute, either explicitly or implicitly
    !ERROR: A variable that appears in a DECLARE TARGET directive must be declared in the scope of a module or have the SAVE attribute, either explicitly or implicitly
    !ERROR: A variable that appears in a DECLARE TARGET directive must be declared in the scope of a module or have the SAVE attribute, either explicitly or implicitly
    !$omp declare target to(Q, S) link(R)
    !ERROR: A variable that appears in a DECLARE TARGET directive must be declared in the scope of a module or have the SAVE attribute, either explicitly or implicitly
    !ERROR: A variable that appears in a DECLARE TARGET directive must be declared in the scope of a module or have the SAVE attribute, either explicitly or implicitly
    !ERROR: A variable that appears in a DECLARE TARGET directive must be declared in the scope of a module or have the SAVE attribute, either explicitly or implicitly
    !$omp declare target enter(Q, S) link(R)
    !WARNING: The usage of TO clause on DECLARE TARGET directive has been deprecated. Use ENTER clause instead. [-Wopen-mp-usage]
    !ERROR: A variable that appears in a DECLARE TARGET directive must be declared in the scope of a module or have the SAVE attribute, either explicitly or implicitly
    !ERROR: MAP clause is not allowed on the DECLARE TARGET directive
    !$omp declare target to(Q) map(from:Q)
    !ERROR: A variable that appears in a DECLARE TARGET directive must be declared in the scope of a module or have the SAVE attribute, either explicitly or implicitly
    !ERROR: MAP clause is not allowed on the DECLARE TARGET directive
    !$omp declare target enter(Q) map(from:Q)
    integer, parameter :: N=10000, M=1024
    integer :: i
    real :: Q(N, N), R(N,M), S(M,M)
    !ERROR: A variable that appears in a THREADPRIVATE directive must be declared in the scope of a module or have the SAVE attribute, either explicitly or implicitly
    !$omp threadprivate(i)
  end subroutine foo
end module m2

! 2.16 declare-reduction

subroutine declare_red_1()
  integer :: my_var
  !$omp declare reduction (my_add_red : integer : omp_out = omp_out + omp_in) initializer (omp_priv=0)
  my_var = 0
  !$omp parallel reduction (my_add_red : my_var) num_threads(4)
  my_var = 1
  !$omp end parallel
  print *, "sum of thread numbers is ", my_var
end subroutine declare_red_1

end
