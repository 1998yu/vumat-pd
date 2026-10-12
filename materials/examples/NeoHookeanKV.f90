subroutine vumat(nblock, ndir, nshr, nstatev, nfieldv, nprops, jInfoArray, &
                 stepTime, totalTime, dtArray, cmname, coordMp, charLength, &
                 props, density, strainInc, relSpinInc, tempOld, stretchOld, &
                 defgradOld, fieldOld, stressOld, stateOld, enerInternOld, &
                 enerInelasOld, tempNew, stretchNew, defgradNew, fieldNew, &
                 stressNew, stateNew, enerInternNew, enerInelasNew)
  implicit none
  integer, intent(in) :: nblock, ndir, nshr, nstatev, nfieldv, nprops
  integer, intent(in) :: jInfoArray(*)
  double precision, intent(in) :: stepTime, totalTime, dtArray(*)
  character(len=80), intent(in) :: cmname
  double precision, intent(in) :: coordMp(nblock,*), charLength(nblock)
  double precision, intent(in) :: props(nprops), density(nblock)
  double precision, intent(in) :: strainInc(nblock,ndir+nshr)
  double precision, intent(in) :: relSpinInc(nblock,nshr), tempOld(nblock)
  double precision, intent(in) :: stretchOld(nblock,ndir+nshr)
  double precision, intent(in) :: defgradOld(nblock,ndir+nshr+nshr)
  double precision, intent(in) :: fieldOld(nblock,nfieldv)
  double precision, intent(in) :: stressOld(nblock,ndir+nshr)
  double precision, intent(in) :: stateOld(nblock,nstatev)
  double precision, intent(in) :: enerInternOld(nblock), enerInelasOld(nblock)
  double precision, intent(out) :: tempNew(nblock)
  double precision, intent(out) :: stretchNew(nblock,ndir+nshr)
  double precision, intent(inout) :: defgradNew(nblock,ndir+nshr+nshr)
  double precision, intent(out) :: fieldNew(nblock,nfieldv)
  double precision, intent(out) :: stressNew(nblock,ndir+nshr)
  double precision, intent(out) :: stateNew(nblock,nstatev)
  double precision, intent(out) :: enerInternNew(nblock), enerInelasNew(nblock)

  integer :: k, i
  double precision :: mu, bulk, etaShear, etaBulk, dt
  double precision :: f11,f12,f13,f21,f22,f23,f31,f32,f33,jac
  double precision :: b11,b22,b33,b12,b13,b23,i1,jm53
  double precision :: d11,d22,d33,d12,d13,d23,trd
  double precision :: sd11,sd22,sd33,sd12,sd13,sd23
  double precision :: sv11,sv22,sv33,sv12,sv13,sv23
  double precision :: wiso,wvol,viscPower,cumulativeDiss

  mu = props(1)
  bulk = props(2)
  etaShear = props(3)
  etaBulk = props(4)
  dt = dtArray(1)

  do k = 1, nblock
    f11=defgradNew(k,1); f22=defgradNew(k,2); f33=defgradNew(k,3)
    f12=defgradNew(k,4); f23=defgradNew(k,5); f31=defgradNew(k,6)
    f21=defgradNew(k,7); f32=defgradNew(k,8); f13=defgradNew(k,9)
    jac=f11*(f22*f33-f23*f32)-f12*(f21*f33-f23*f31) &
        +f13*(f21*f32-f22*f31)
    jac=max(jac,1.0d-10)

    b11=f11*f11+f12*f12+f13*f13
    b22=f21*f21+f22*f22+f23*f23
    b33=f31*f31+f32*f32+f33*f33
    b12=f11*f21+f12*f22+f13*f23
    b13=f11*f31+f12*f32+f13*f33
    b23=f21*f31+f22*f32+f23*f33
    i1=b11+b22+b33
    jm53=jac**(-5.0d0/3.0d0)

    sd11=mu*jm53*(b11-i1/3.0d0)+bulk*(jac-1.0d0)
    sd22=mu*jm53*(b22-i1/3.0d0)+bulk*(jac-1.0d0)
    sd33=mu*jm53*(b33-i1/3.0d0)+bulk*(jac-1.0d0)
    sd12=mu*jm53*b12
    sd13=mu*jm53*b13
    sd23=mu*jm53*b23

    d11=strainInc(k,1)/dt; d22=strainInc(k,2)/dt
    d33=strainInc(k,3)/dt; d23=strainInc(k,4)/dt
    d13=strainInc(k,5)/dt; d12=strainInc(k,6)/dt
    trd=d11+d22+d33
    sv11=2.0d0*etaShear*(d11-trd/3.0d0)+etaBulk*trd
    sv22=2.0d0*etaShear*(d22-trd/3.0d0)+etaBulk*trd
    sv33=2.0d0*etaShear*(d33-trd/3.0d0)+etaBulk*trd
    sv12=2.0d0*etaShear*d12
    sv13=2.0d0*etaShear*d13
    sv23=2.0d0*etaShear*d23

    stressNew(k,1)=sd11+sv11; stressNew(k,2)=sd22+sv22
    stressNew(k,3)=sd33+sv33; stressNew(k,4)=sd23+sv23
    stressNew(k,5)=sd13+sv13; stressNew(k,6)=sd12+sv12

    wiso=0.5d0*mu*(jac**(-2.0d0/3.0d0)*i1-3.0d0)
    wvol=0.5d0*bulk*(jac-1.0d0)*(jac-1.0d0)
    viscPower=sv11*d11+sv22*d22+sv33*d33 &
              +2.0d0*(sv12*d12+sv13*d13+sv23*d23)
    cumulativeDiss=stateOld(k,4)+jac*max(viscPower,0.0d0)*dt

    stateNew(k,1)=jac
    stateNew(k,2)=wiso+wvol
    stateNew(k,3)=max(viscPower,0.0d0)
    stateNew(k,4)=cumulativeDiss
    tempNew(k)=tempOld(k)
    do i=1,ndir+nshr
      stretchNew(k,i)=stretchOld(k,i)
    end do
    do i=1,nfieldv
      fieldNew(k,i)=fieldOld(k,i)
    end do
    enerInternNew(k)=(wiso+wvol+cumulativeDiss)/max(density(k),1.0d-12)
    enerInelasNew(k)=cumulativeDiss/max(density(k),1.0d-12)
  end do
end subroutine vumat
