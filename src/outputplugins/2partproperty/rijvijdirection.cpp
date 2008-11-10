/*  DYNAMO:- Event driven molecular dynamics simulator 
    http://www.marcusbannerman.co.uk/dynamo
    Copyright (C) 2008  Marcus N Campbell Bannerman <m.bannerman@gmail.com>

    This program is free software: you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    version 3 as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "rijvijdirection.hpp"
#include "../../dynamics/include.hpp"
#include <boost/foreach.hpp>
#include "../../base/is_simdata.hpp"
#include "../0partproperty/collMatrix.hpp"

COPRijVij::COPRijVij(const DYNAMO::SimData* tmp, const XMLNode&):
  COutputPlugin(tmp, "RdotV")
{}

void 
COPRijVij::initialise()
{}

void 
COPRijVij::eventUpdate(const CIntEvent& iEvent, const C2ParticleData& pDat)
{
  mapdata& ref = 
    rvdotacc[mapKey(iEvent.getType(), getClassKey(iEvent.getInteraction()))];

  CVector<> rijnorm(pDat.rij.unitVector());
  CVector<> vijnorm(pDat.vijold.unitVector());

  for (size_t iDim(0); iDim < NDIM; ++iDim)
    {
      ref.rij[iDim].addVal(rijnorm[iDim]);
      ref.vij[iDim].addVal(vijnorm[iDim]);
    }
}

void 
COPRijVij::eventUpdate(const CGlobEvent& globEvent, const CNParticleData& SDat)
{
  BOOST_FOREACH(const C2ParticleData& pDat, SDat.L2partChanges)
    {
      mapdata& ref = rvdotacc[mapKey(globEvent.getType(), 
				     getClassKey(globEvent))];

      CVector<> rijnorm(pDat.rij.unitVector());
      CVector<> vijnorm(pDat.vijold.unitVector());
      
      for (size_t iDim(0); iDim < NDIM; ++iDim)
	{
	  ref.rij[iDim].addVal(rijnorm[iDim]);
	  ref.vij[iDim].addVal(vijnorm[iDim]);
	}      
    }
}

void 
COPRijVij::eventUpdate(const CLocalEvent& localEvent, const CNParticleData& SDat)
{
  BOOST_FOREACH(const C2ParticleData& pDat, SDat.L2partChanges)
    {
      mapdata& ref = rvdotacc[mapKey(localEvent.getType(), 
				     getClassKey(localEvent))];
      
      CVector<> rijnorm(pDat.rij.unitVector());
      CVector<> vijnorm(pDat.vijold.unitVector());
      
      for (size_t iDim(0); iDim < NDIM; ++iDim)
	{
	  ref.rij[iDim].addVal(rijnorm[iDim]);
	  ref.vij[iDim].addVal(vijnorm[iDim]);
	}      
    }
}

void
COPRijVij::eventUpdate(const CSystem& sysEvent, const CNParticleData& SDat, const Iflt&)
{
  BOOST_FOREACH(const C2ParticleData& pDat, SDat.L2partChanges)
    {
      mapdata& ref 
	= rvdotacc[mapKey(sysEvent.getType(), getClassKey(sysEvent))];
      
      CVector<> rijnorm(pDat.rij.unitVector());
      CVector<> vijnorm(pDat.vijold.unitVector());
      
      for (size_t iDim(0); iDim < NDIM; ++iDim)
	{
	  ref.rij[iDim].addVal(rijnorm[iDim]);
	  ref.vij[iDim].addVal(vijnorm[iDim]);
	}      
    } 
}

void
COPRijVij::output(xmlw::XmlStream &XML)
{
  XML << xmlw::tag("RijVijComponents");
  
  typedef std::pair<const mapKey, mapdata> mappair;

  BOOST_FOREACH(const mappair& pair1, rvdotacc)
    {
      XML << xmlw::tag("Element")
	  << xmlw::attr("Type") 
	  << CIntEvent::getCollEnumName(pair1.first.first)
	  << xmlw::attr("EventName") 
	  << getName(pair1.first.second, Sim);

      
      for (size_t iDim(0); iDim < NDIM; ++iDim)
	{
	  XML << xmlw::tag("Rij")
	      << xmlw::attr("dimension")
	      << iDim;

	  pair1.second.rij[iDim].outputHistogram(XML, 1.0);

	  XML << xmlw::endtag("Rij");
	}

      for (size_t iDim(0); iDim < NDIM; ++iDim)
	{
	  XML << xmlw::tag("Vij")
	      << xmlw::attr("dimension")
	      << iDim;

	  pair1.second.vij[iDim].outputHistogram(XML, 1.0);

	  XML << xmlw::endtag("Vij");
	}
      
      XML << xmlw::endtag("Element");
    }
  
  
    XML << xmlw::endtag("RijVijComponents");
}
