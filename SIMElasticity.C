// $Id$
//==============================================================================
//!
//! \file SIMElasticity.C
//!
//! \date Dec 04 2010
//!
//! \author Knut Morten Okstad / SINTEF
//!
//! \brief Solution driver for NURBS-based linear elastic FEM analysis.
//!
//==============================================================================

#include "SIMElasticity.h"
#include "SIM2D.h"
#include "SIM3D.h"

//! Plane strain/stress option for 2D problems.
template<> bool SIMElasticity<SIM2D>::planeStrain = false;
//! Axisymmtry option for 2D problems.
template<> bool SIMElasticity<SIM2D>::axiSymmetry = false;
//! Option for output of Gauss points to VTF for 2D problems.
template<> bool SIMElasticity<SIM2D>::GIpointsVTF = false;

//! Dummy option for 3D problems.
template<> bool SIMElasticity<SIM3D>::planeStrain = false;
//! Dummy option for 3D problems.
template<> bool SIMElasticity<SIM3D>::axiSymmetry = false;
//! Option for output of Gauss points to VTF for 3D problems.
template<> bool SIMElasticity<SIM3D>::GIpointsVTF = false;


void ElasticityUtils::printNorms (const Vector& gNorm, const Vector& rNorm,
                                  const std::string& name, const SIMbase* model)
{
  const char* uRef = model->haveAnaSol() ? "|u|)   : " : "|u^r|) : ";
  double Rel = 100.0/(model->haveAnaSol() ? rNorm(3) : gNorm(1));

  IFEM::cout <<"\n\n>>> Error estimates based on "<< name <<" <<<"
             <<"\nEnergy norm |u^r| = a(u^r,u^r)^0.5   : "<< gNorm(1)
             <<"\nError norm a(e,e)^0.5, e=u^r-u^h     : "<< gNorm(2)
             <<"\n- relative error (% of "<< uRef << gNorm(2)*Rel;

  bool haveResErr = gNorm.size() >= (model->haveAnaSol() ? 8 : 5);
  if (haveResErr)
  {
    IFEM::cout <<"\nResidual error (r(u^r) + J(u^r))^0.5 : "<< gNorm(5)
               <<"\n- relative error (% of "<< uRef << gNorm(5)*Rel;
    if (gNorm.size() >= 9)
      IFEM::cout <<"\nJump term J(u^r)^0.5          : "<< gNorm(6)
                 <<"\n- relative error (% of "<< uRef << gNorm(6)*Rel;
  }

  if (model->haveAnaSol())
  {
    double exaErr = gNorm(gNorm.size() - (haveResErr ? 2 : 1));
    IFEM::cout <<"\nExact error a(e,e)^0.5, e=u-u^r      : "<< exaErr
               <<"\n- relative error (% of "<< uRef << exaErr*Rel
               <<"\nEffectivity index             : "<< gNorm(2)/rNorm(4);
    if (haveResErr)
      IFEM::cout <<"\nEffectivity index, theta^EX          : "
                 << (gNorm(2)+exaErr)/rNorm(4)
                 <<"\nEffectivity index, theta^RES         : "
                 << (gNorm(2)+gNorm(5))/rNorm(4);
  }
}
