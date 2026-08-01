subroutine xplb_exit()
  use iso_fortran_env, only: error_unit
  implicit none
  write(error_unit, *) 'XPLB_EXIT called by VUMAT plugin.'
  error stop
end subroutine xplb_exit
