! Test-only ABI probe. Not a physical constitutive model.
! Component-dependent stress increments deliberately expose shear permutations.
subroutine vumat(nblock, ndir, nshr, nstatev, nfieldv, nprops, jInfoArray, &
                 stepTime, totalTime, dtArray, cmname, coordMp, charLength, &
                 props, density, strainInc, relSpinInc, tempOld, stretchOld, &
                 defgradOld, fieldOld, stressOld, stateOld, enerInternOld, &
                 enerInelasOld, tempNew, stretchNew, defgradNew, fieldNew, &
                 stressNew, stateNew, enerInternNew, enerInelasNew)
  implicit none
  integer :: nblock, ndir, nshr, nstatev, nfieldv, nprops, jInfoArray(*), k, c
  character(len=80) :: cmname
  double precision :: stepTime,totalTime,dtArray(*),coordMp(nblock,*),charLength(nblock)
  double precision :: props(*),density(nblock),strainInc(nblock,6),relSpinInc(nblock,3)
  double precision :: tempOld(nblock),stretchOld(nblock,6),defgradOld(nblock,9)
  double precision :: fieldOld(nblock,*),stressOld(nblock,6),stateOld(nblock,nstatev)
  double precision :: enerInternOld(nblock),enerInelasOld(nblock),tempNew(nblock)
  double precision :: stretchNew(nblock,6),defgradNew(nblock,9),fieldNew(nblock,*)
  double precision :: stressNew(nblock,6),stateNew(nblock,nstatev)
  double precision :: enerInternNew(nblock),enerInelasNew(nblock)
  do k=1,nblock
    do c=1,6
      stressNew(k,c)=stressOld(k,c)+dble(c)*strainInc(k,c)
    end do
    stateNew(k,1:9)=defgradOld(k,1:9)
    stateNew(k,10:18)=defgradNew(k,1:9)
    stateNew(k,19:24)=stretchOld(k,1:6)
    stateNew(k,25:30)=stretchNew(k,1:6)
    stateNew(k,31:33)=relSpinInc(k,1:3)
    stateNew(k,34)=stateOld(k,34)+1.0d0
    stateNew(k,35)=density(k)
    stateNew(k,36)=charLength(k)
    stateNew(k,37)=dble(nblock)
    stateNew(k,38)=stepTime
    stateNew(k,39)=totalTime
    tempNew(k)=tempOld(k)+stateOld(k,34)+dtArray(1)
    enerInternNew(k)=enerInternOld(k)+sum(strainInc(k,:))
    enerInelasNew(k)=enerInelasOld(k)+2.0d0*sum(strainInc(k,:))
  end do
end subroutine vumat
