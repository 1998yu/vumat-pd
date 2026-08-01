subroutine vumat_adapter_call(nblock, ndir, nshr, nstatev, nfieldv, nprops, lanneal, &
                              stepTime, totalTime, dt, cmname_c, coordMp, charLength, &
                              props, density, strainInc, relSpinInc, tempOld, stretchOld, &
                              defgradOld, fieldOld, stressOld, stateOld, enerInternOld, &
                              enerInelasOld, tempNew, stretchNew, defgradNew, fieldNew, &
                              stressNew, stateNew, enerInternNew, enerInelasNew) bind(C, name="vumat_adapter_call")
  use iso_c_binding
  implicit none

  integer(c_int), intent(inout) :: nblock, ndir, nshr, nstatev, nfieldv, nprops, lanneal
  real(c_double), intent(inout) :: stepTime, totalTime, dt
  character(kind=c_char), intent(in) :: cmname_c(80)
  real(c_double), intent(inout) :: coordMp(*), charLength(*), props(*), density(*), strainInc(*)
  real(c_double), intent(inout) :: relSpinInc(*), tempOld(*), stretchOld(*), defgradOld(*), fieldOld(*)
  real(c_double), intent(inout) :: stressOld(*), stateOld(*), enerInternOld(*), enerInelasOld(*)
  real(c_double), intent(inout) :: tempNew(*), stretchNew(*), defgradNew(*), fieldNew(*)
  real(c_double), intent(inout) :: stressNew(*), stateNew(*), enerInternNew(*), enerInelasNew(*)

  character(len=80) :: cmname
  integer :: i
  integer(c_int), allocatable :: jInfoArray(:)
  external :: vumat

  cmname = ' '
  do i = 1, 80
    cmname(i:i) = cmname_c(i)
  end do

  allocate(jInfoArray(6 + nblock))
  jInfoArray = 0
  jInfoArray(1) = lanneal
  jInfoArray(6) = 7
  do i = 1, nblock
    jInfoArray(6 + i) = i
  end do

  call vumat(nblock, ndir, nshr, nstatev, nfieldv, nprops, jInfoArray, &
             stepTime, totalTime, dt, cmname, coordMp, charLength, &
             props, density, strainInc, relSpinInc, tempOld, stretchOld, &
             defgradOld, fieldOld, stressOld, stateOld, enerInternOld, &
             enerInelasOld, tempNew, stretchNew, defgradNew, fieldNew, &
             stressNew, stateNew, enerInternNew, enerInelasNew)
  deallocate(jInfoArray)
end subroutine vumat_adapter_call
