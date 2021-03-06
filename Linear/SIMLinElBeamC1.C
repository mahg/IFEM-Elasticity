// $Id$
//==============================================================================
//!
//! \file SIMLinElBeamC1.C
//!
//! \date Sep 16 2010
//!
//! \author Knut Morten Okstad / SINTEF
//!
//! \brief Solution driver for NURBS-based FE analysis of C1-continous beams.
//!
//==============================================================================

#include "SIMLinElBeamC1.h"
#include "KirchhoffLovePlate.h"
#include "ElasticityUtils.h"
#include "LinIsotropic.h"
#include "AnalyticSolutions.h"
#include "AlgEqSystem.h"
#include "ASMbase.h"
#include "SAM.h"
#include "Functions.h"
#include "Utilities.h"
#include "Property.h"
#include "AnaSol.h"
#include "Vec3Oper.h"
#include "IFEM.h"
#include "tinyxml.h"


bool SIMLinElBeamC1::parse (char* keyWord, std::istream& is)
{
  if (!myProblem)
    myProblem = new KirchhoffLovePlate(1);

  char* cline = 0;
  KirchhoffLovePlate* klp = dynamic_cast<KirchhoffLovePlate*>(myProblem);
  if (!klp) return false;

  if (!strncasecmp(keyWord,"GRAVITY",7))
  {
    double g = atof(strtok(keyWord+7," "));
    klp->setGravity(g);
    IFEM::cout <<"\nGravitation constant: "<< g << std::endl;
  }

  else if (!strncasecmp(keyWord,"ISOTROPIC",9))
  {
    int nmat = atoi(keyWord+10);
    IFEM::cout <<"\nNumber of isotropic materials: "<< nmat << std::endl;

    for (int i = 0; i < nmat && (cline = utl::readLine(is)); i++)
    {
      int code = atoi(strtok(cline," "));
      if (code > 0)
        this->setPropertyType(code,Property::MATERIAL,mVec.size());

      double E   = atof(strtok(NULL," "));
      double rho = (cline = strtok(NULL, " ")) ? atof(cline) : 0.0;
      double thk = (cline = strtok(NULL, " ")) ? atof(cline) : 0.0;
      mVec.push_back(new LinIsotropic(E,0.0,rho,true));
      tVec.push_back(thk);
      IFEM::cout <<"\tMaterial code "<< code <<": "
                 << E <<" "<< rho <<" "<< thk << std::endl;
    }

    if (!mVec.empty())
      klp->setMaterial(mVec.front());
    if (!tVec.empty() && tVec.front() != 0.0)
      klp->setThickness(tVec.front());
  }

  else if (!strncasecmp(keyWord,"POINTLOAD",9))
  {
    int nload = atoi(keyWord+9);
    IFEM::cout <<"\nNumber of point loads: "<< nload;

    myLoads.resize(nload);
    for (int i = 0; i < nload && (cline = utl::readLine(is)); i++)
    {
      myLoads[i].patch = atoi(strtok(cline," "));
      myLoads[i].xi    = atof(strtok(NULL," "));
      myLoads[i].pload = atof(strtok(NULL," "));
      IFEM::cout <<"\n\tPoint "<< i+1 <<": P"<< myLoads[i].patch
                 <<" xi = "<< myLoads[i].xi <<" load = "<< myLoads[i].pload;
    }
  }

  else if (!strncasecmp(keyWord,"PRESSURE",8))
  {
    int npres = atoi(keyWord+8);
    IFEM::cout <<"\nNumber of pressures: "<< npres << std::endl;

    for (int i = 0; i < npres && (cline = utl::readLine(is)); i++)
    {
      int code = atoi(strtok(cline," "));
      double p = atof(strtok(NULL," "));
      IFEM::cout <<"\tPressure code "<< code <<": ";
      cline = strtok(NULL," ");
      myScalars[code] = const_cast<RealFunc*>(utl::parseRealFunc(cline,p));
      IFEM::cout << std::endl;
      if (code > 0)
        this->setPropertyType(code,Property::BODYLOAD);
    }
  }

  else if (!strncasecmp(keyWord,"ANASOL",6))
  {
    cline = strtok(keyWord+6," ");
    if (!strncasecmp(cline,"EXPRESSION",10))
    {
      IFEM::cout <<"\nAnalytical solution: Expression"<< std::endl;
      int lines = (cline = strtok(NULL," ")) ? atoi(cline) : 0;
      if (!mySol)
        mySol = new AnaSol(is,lines,false);
    }
    else
    {
      std::cerr <<"  ** SIMLinElBeamC1::parse: Unknown analytical solution "
                << cline <<" (ignored)"<< std::endl;
      return true;
    }
  }

  else
    return this->SIM1D::parse(keyWord,is);

  return true;
}


bool SIMLinElBeamC1::parse (const TiXmlElement* elem)
{
  if (strcasecmp(elem->Value(),"eulerbernoulli"))
    return this->SIM1D::parse(elem);

  KirchhoffLovePlate* klp = dynamic_cast<KirchhoffLovePlate*>(myProblem);
  if (!klp)
  {
    int version = 1;
    utl::getAttribute(elem,"version",version);
    myProblem = klp = new KirchhoffLovePlate(1,version);
  }

  const TiXmlElement* child = elem->FirstChildElement();
  for (; child; child = child->NextSiblingElement())

    if (!strcasecmp(child->Value(),"gravity")) {
      double g = 0.0;
      utl::getAttribute(child,"g",g);
      klp->setGravity(g);
      IFEM::cout <<"\tGravitation constant: "<< g << std::endl;
    }

    else if (!strcasecmp(child->Value(),"isotropic")) {
      int code = this->parseMaterialSet(child,mVec.size());

      double E = 1000.0, rho = 1.0, thk = 0.1;
      utl::getAttribute(child,"E",E);
      utl::getAttribute(child,"rho",rho);
      utl::getAttribute(child,"thickness",thk);

      mVec.push_back(new LinIsotropic(E,0.0,rho,true));
      tVec.push_back(thk);
      IFEM::cout <<"\tMaterial code "<< code <<": "
                 << E <<" "<< rho <<" "<< thk << std::endl;
      klp->setMaterial(mVec.front());
      if (tVec.front() != 0.0)
        klp->setThickness(tVec.front());
    }

    else if (!strcasecmp(child->Value(),"pointload")) {
      PointLoad load; int patch;
      utl::getAttribute(child,"patch",patch);
      if (child->FirstChild()) {
        load.patch = patch;
        load.pload = atof(child->FirstChild()->Value());
        utl::getAttribute(child,"xi",load.xi);
        IFEM::cout <<"\tPoint: P"<< load.patch <<" xi = "<< load.xi
                   <<" load = "<< load.pload << std::endl;
        myLoads.push_back(load);
      }
    }

    else if (!strcasecmp(child->Value(),"pressure")) {
      std::string set, type;
      utl::getAttribute(child,"set",set);
      int code = this->getUniquePropertyCode(set,1);
      if (code == 0) utl::getAttribute(child,"code",code);

      if (child->FirstChild() && code > 0) {
        utl::getAttribute(child,"type",type,true);
        IFEM::cout <<"\tPressure code "<< code;
        if (!type.empty()) IFEM::cout <<" ("<< type <<")";
        myScalars[code] = utl::parseRealFunc(child->FirstChild()->Value(),type);
        this->setPropertyType(code,Property::BODYLOAD);
        IFEM::cout << std::endl;
      }
    }

    else if (!strcasecmp(child->Value(),"anasol")) {
      std::string type;
      utl::getAttribute(child,"type",type,true);
      if (type == "expression") {
        IFEM::cout <<"\tAnalytical solution: Expression"<< std::endl;
        if (!mySol)
          mySol = new AnaSol(child);
      }
      else
        std::cerr <<"  ** SIMLinElKL::parse: Unknown analytical solution "
                  << type <<" (ignored)"<< std::endl;
    }

  return true;
}


bool SIMLinElBeamC1::initMaterial (size_t propInd)
{
  if (propInd >= mVec.size()) propInd = mVec.size()-1;

  KirchhoffLovePlate* klp = dynamic_cast<KirchhoffLovePlate*>(myProblem);
  if (!klp) return false;

  klp->setMaterial(mVec[propInd]);
  if (tVec[propInd] != 0.0)
    klp->setThickness(tVec[propInd]);

  return true;
}


bool SIMLinElBeamC1::initBodyLoad (size_t patchInd)
{
  KirchhoffLovePlate* klp = dynamic_cast<KirchhoffLovePlate*>(myProblem);
  if (!klp) return false;

  SclFuncMap::const_iterator it = myScalars.find(0);
  for (size_t i = 0; i < myProps.size(); i++)
    if (myProps[i].pcode == Property::BODYLOAD && myProps[i].patch == patchInd)
      if ((it = myScalars.find(myProps[i].pindx)) != myScalars.end()) break;

  klp->setPressure(it == myScalars.end() ? 0 : it->second);
  return true;
}


bool SIMLinElBeamC1::preprocessB ()
{
  // Preprocess the nodal point loads
  for (PloadVec::iterator p = myLoads.begin(); p != myLoads.end();)
    if ((p->inod = this->evalPoint(&p->xi,p->X,nullptr,p->patch,true)) < 1)
    {
      p = myLoads.erase(p);
      std::cerr <<"  ** SIMLinElBeamC1::preprocess: Load point ("<< p->xi
                <<") on patch #"<< p->patch <<" is not a nodal point (ignored)."
                << std::endl;
    }
    else
    {
      int ipt = 1 + (int)(p-myLoads.begin());
      if (ipt == 1) IFEM::cout <<'\n';
      IFEM::cout <<"Load point #"<< ipt <<": patch #"<< p->patch
                 <<" u=("<< p->xi <<"), node #"<< p->inod <<", X = "<< p->X
                 << std::endl;
      ++p;
    }

  return true;
}


bool SIMLinElBeamC1::assembleDiscreteTerms (const IntegrandBase*,
                                            const TimeDomain&)
{
  SystemVector* b = myEqSys->getVector();
  for (size_t i = 0; i < myLoads.size() && b; i++)
    if (!mySam->assembleSystem(*b,&myLoads[i].pload,myLoads[i].inod))
      return false;

  return true;
}


double SIMLinElBeamC1::externalEnergy (const Vectors& psol) const
{
  double energy = this->SIMbase::externalEnergy(psol);

  // External energy from the nodal point loads
  for (size_t i = 0; i < myLoads.size(); i++)
    energy += myLoads[i].pload * psol.front()(myLoads[i].inod);

  return energy;
}


void SIMLinElBeamC1::printNormGroup (const Vector& gNorm, const Vector& rNorm,
                                     const std::string& prjName) const
{
  ElasticityUtils::printNorms(gNorm,rNorm,prjName,this);
}
