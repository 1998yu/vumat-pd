C BSD-3-Clause example VUMAT for standalone VUMAT-PD verification.
C props(1) = Young's modulus; props(2) = Poisson's ratio.
      subroutine vumat(
     1 nblock,ndir,nshr,nstatev,nfieldv,nprops,jInfoArray,
     2 stepTime,totalTime,dtArray,cmname,coordMp,charLength,
     3 props,density,strainInc,relSpinInc,tempOld,stretchOld,
     4 defgradOld,fieldOld,stressOld,stateOld,enerInternOld,
     5 enerInelasOld,tempNew,stretchNew,defgradNew,fieldNew,
     6 stressNew,stateNew,enerInternNew,enerInelasNew)
      include 'vaba_param.inc'
      character*80 cmname
      dimension props(nprops),density(nblock),coordMp(nblock,*),
     1 charLength(nblock),dtArray(*),strainInc(nblock,ndir+nshr),
     2 relSpinInc(nblock,nshr),tempOld(nblock),
     3 stretchOld(nblock,ndir+nshr),
     4 defgradOld(nblock,ndir+2*nshr),fieldOld(nblock,nfieldv),
     5 stressOld(nblock,ndir+nshr),stateOld(nblock,nstatev),
     6 enerInternOld(nblock),enerInelasOld(nblock),tempNew(nblock),
     7 stretchNew(nblock,ndir+nshr),
     8 defgradNew(nblock,ndir+2*nshr),fieldNew(nblock,nfieldv),
     9 stressNew(nblock,ndir+nshr),stateNew(nblock,nstatev),
     1 enerInternNew(nblock),enerInelasNew(nblock),jInfoArray(*)
      dimension cmat(6,6)
      e=props(1)
      anu=props(2)
      alam=e*anu/((1.d0+anu)*(1.d0-2.d0*anu))
      amu=e/(2.d0*(1.d0+anu))
      do i=1,6
        do j=1,6
          cmat(i,j)=0.d0
        enddo
      enddo
      do i=1,3
        do j=1,3
          cmat(i,j)=alam
        enddo
        cmat(i,i)=alam+2.d0*amu
      enddo
      do i=4,6
        cmat(i,i)=2.d0*amu
      enddo
      do k=1,nblock
        do i=1,ndir+nshr
          stressNew(k,i)=stressOld(k,i)
          do j=1,ndir+nshr
            stressNew(k,i)=stressNew(k,i)+
     1        cmat(i,j)*strainInc(k,j)
          enddo
        enddo
        tempNew(k)=tempOld(k)
        enerInternNew(k)=enerInternOld(k)
        enerInelasNew(k)=enerInelasOld(k)
        do i=1,nstatev
          stateNew(k,i)=stateOld(k,i)
        enddo
      enddo
      return
      end
