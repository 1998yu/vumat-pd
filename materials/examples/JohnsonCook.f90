! SPDX-License-Identifier: BSD-3-Clause
!
! Independently authored Johnson-Cook VUMAT example for VUMAT-PD.
! The implementation follows the published Johnson-Cook flow-stress law and
! a small-strain J2 radial-return update.  It is supplied as a redistributable
! reference implementation; it is not copied from the restricted VUMAT used
! during early development of the project.
!
! props: E, nu, A, B, C, n, m, rate0, Tref, Tmelt, rho, beta, Cp, workToHeat
! state: initialized, equivalentPlasticStrain, temperature, yieldStress,
!        lastPlasticIncrement, cumulativeIterations
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
  double precision, intent(in) :: relSpinInc(nblock,nshr)
  double precision, intent(in) :: tempOld(nblock)
  double precision, intent(in) :: stretchOld(nblock,ndir+nshr)
  double precision, intent(in) :: defgradOld(nblock,ndir+2*nshr)
  double precision, intent(in) :: fieldOld(nblock,nfieldv)
  double precision, intent(in) :: stressOld(nblock,ndir+nshr)
  double precision, intent(in) :: stateOld(nblock,nstatev)
  double precision, intent(in) :: enerInternOld(nblock), enerInelasOld(nblock)
  double precision, intent(out) :: tempNew(nblock)
  double precision, intent(out) :: stretchNew(nblock,ndir+nshr)
  double precision, intent(out) :: defgradNew(nblock,ndir+2*nshr)
  double precision, intent(out) :: fieldNew(nblock,nfieldv)
  double precision, intent(out) :: stressNew(nblock,ndir+nshr)
  double precision, intent(out) :: stateNew(nblock,nstatev)
  double precision, intent(out) :: enerInternNew(nblock), enerInelasNew(nblock)

  integer :: k, i, iteration
  double precision :: young, poisson, a0, b0, rateC, hardN, thermalM
  double precision :: rate0, tref, tmelt, rho, beta, cp, workToHeat
  double precision :: lambda, mu, dt, traceInc, pressure, qTrial, qNew
  double precision :: epOld, epInc, epLower, epUpper, epRate
  double precision :: temperature, oldYield, yield, residual, tolerance
  double precision :: workInc, plasticWork, scale
  double precision :: trial(6), deviator(6), updated(6), oldStress(6)

  if (ndir /= 3 .or. nshr /= 3) stop "JohnsonCook VUMAT requires ndir=3,nshr=3"
  if (nprops < 14 .or. nstatev < 6) stop "JohnsonCook VUMAT requires 14 props and 6 states"

  young = props(1)
  poisson = props(2)
  a0 = props(3)
  b0 = props(4)
  rateC = props(5)
  hardN = props(6)
  thermalM = props(7)
  rate0 = props(8)
  tref = props(9)
  tmelt = props(10)
  rho = props(11)
  beta = props(12)
  cp = props(13)
  workToHeat = props(14)
  dt = dtArray(1)
  mu = young / (2.0d0 * (1.0d0 + poisson))
  lambda = young * poisson / ((1.0d0 + poisson) * (1.0d0 - 2.0d0 * poisson))
  tolerance = young * 1.0d-9

  do k = 1, nblock
    traceInc = strainInc(k,1) + strainInc(k,2) + strainInc(k,3)
    do i = 1, 3
      trial(i) = stressOld(k,i) + lambda * traceInc + 2.0d0 * mu * strainInc(k,i)
    end do
    do i = 4, 6
      trial(i) = stressOld(k,i) + 2.0d0 * mu * strainInc(k,i)
    end do

    updated = trial
    stateNew(k,:) = stateOld(k,:)
    tempNew(k) = tempOld(k)
    enerInternNew(k) = enerInternOld(k)
    enerInelasNew(k) = enerInelasOld(k)
    stretchNew(k,:) = stretchOld(k,:)
    defgradNew(k,:) = defgradOld(k,:)
    if (nfieldv > 0) fieldNew(k,:) = fieldOld(k,:)

    if (stateOld(k,1) == 0.0d0) then
      stateNew(k,1) = 1.0d0
      stateNew(k,2) = 0.0d0
      stateNew(k,3) = tref
      stateNew(k,4) = 0.0d0
      stateNew(k,5) = 0.0d0
      stateNew(k,6) = 1.0d0
      tempNew(k) = tref
      stressNew(k,:) = updated
      cycle
    end if

    oldStress = stressOld(k,:)
    epOld = stateOld(k,2)
    temperature = stateOld(k,3)
    oldYield = stateOld(k,4)
    pressure = (trial(1) + trial(2) + trial(3)) / 3.0d0
    deviator(1:3) = trial(1:3) - pressure
    deviator(4:6) = trial(4:6)
    qTrial = sqrt(1.5d0 * (deviator(1)**2 + deviator(2)**2 + deviator(3)**2 + &
             2.0d0 * (deviator(4)**2 + deviator(5)**2 + deviator(6)**2)))

    epInc = 0.0d0
    epLower = 0.0d0
    epUpper = qTrial / (2.0d0 * mu)
    yield = oldYield
    iteration = 0
    do while (iteration < 50)
      iteration = iteration + 1
      epRate = epInc / dt
      yield = jc_yield(a0, b0, rateC, hardN, thermalM, rate0, tref, tmelt, &
                       epOld + epInc, epRate, temperature)
      yield = max(yield, oldYield)
      qNew = max(0.0d0, qTrial - 3.0d0 * mu * epInc)
      residual = qNew - yield
      if (abs(residual) < tolerance) exit
      if (epInc == 0.0d0 .and. residual < 0.0d0) exit
      if (residual >= 0.0d0) then
        epLower = max(epLower, epInc)
      else
        epUpper = min(epUpper, epInc)
      end if
      epInc = 0.5d0 * (epLower + epUpper)
      if (iteration == 1) epInc = min(max(stateOld(k,5), epLower), epUpper)
    end do

    if (qTrial > 0.0d0) then
      scale = max(0.0d0, 1.0d0 - 3.0d0 * mu * epInc / qTrial)
      updated(1:3) = pressure + scale * deviator(1:3)
      updated(4:6) = scale * deviator(4:6)
    end if
    qNew = max(0.0d0, qTrial - 3.0d0 * mu * epInc)
    workInc = 0.5d0 * sum((oldStress + updated) * strainInc(k,:))
    plasticWork = 0.5d0 * epInc * qNew
    stressNew(k,:) = updated
    enerInternNew(k) = enerInternOld(k) + workInc / rho
    enerInelasNew(k) = enerInelasOld(k) + plasticWork / rho
    stateNew(k,1) = 1.0d0
    stateNew(k,2) = epOld + epInc
    stateNew(k,3) = temperature + workToHeat * beta * plasticWork / (rho * cp)
    stateNew(k,4) = yield
    stateNew(k,5) = epInc
    stateNew(k,6) = stateOld(k,6) + dble(iteration)
    tempNew(k) = stateNew(k,3)
  end do

contains

  double precision function jc_yield(a, b, c, n, m, refRate, refTemp, meltTemp, ep, rate, temp)
    implicit none
    double precision, intent(in) :: a, b, c, n, m, refRate, refTemp, meltTemp
    double precision, intent(in) :: ep, rate, temp
    double precision :: theta, effectiveRate
    theta = min(1.0d0, max(0.0d0, (temp - refTemp) / (meltTemp - refTemp)))
    effectiveRate = rate
    if (effectiveRate == 0.0d0) effectiveRate = refRate
    jc_yield = (a + b * ep**n) * (1.0d0 - theta**m) * &
               (1.0d0 + c * log(effectiveRate / refRate))
  end function jc_yield

end subroutine vumat
