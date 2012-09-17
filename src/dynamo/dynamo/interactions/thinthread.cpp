/*  dynamo:- Event driven molecular dynamics simulator 
    http://www.dynamomd.org
    Copyright (C) 2011  Marcus N Campbell Bannerman <m.bannerman@gmail.com>

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

#include <dynamo/interactions/thinthread.hpp>
#include <dynamo/BC/BC.hpp>
#include <dynamo/units/units.hpp>
#include <dynamo/globals/global.hpp>
#include <dynamo/particle.hpp>
#include <dynamo/interactions/intEvent.hpp>
#include <dynamo/species/species.hpp>
#include <dynamo/2particleEventData.hpp>
#include <dynamo/dynamics/dynamics.hpp>
#include <dynamo/simulation.hpp>
#include <dynamo/schedulers/scheduler.hpp>
#include <dynamo/NparticleEventData.hpp>
#include <dynamo/outputplugins/outputplugin.hpp>
#include <magnet/xmlwriter.hpp>
#include <magnet/xmlreader.hpp>
#include <cmath>
#include <iomanip>

namespace dynamo {
  IThinThread::IThinThread(const magnet::xml::Node& XML, dynamo::Simulation* tmp):
    ISquareWell(tmp, NULL) //A temporary value!
  {
    operator<<(XML);
  }

  void 
  IThinThread::operator<<(const magnet::xml::Node& XML)
  {
    if (strcmp(XML.getAttribute("Type"),"ThinThread"))
      M_throw() << "Attempting to load ThinThread from non ThinThread entry";
  
    Interaction::operator<<(XML);
  
    try {
      _diameter = Sim->_properties.getProperty(XML.getAttribute("Diameter"),
					       Property::Units::Length());
      _lambda = Sim->_properties.getProperty(XML.getAttribute("Lambda"),
					     Property::Units::Dimensionless());
      _wellDepth = Sim->_properties.getProperty(XML.getAttribute("WellDepth"),
						Property::Units::Energy());

      if (XML.hasAttribute("Elasticity"))
	_e = Sim->_properties.getProperty(XML.getAttribute("Elasticity"),
					  Property::Units::Dimensionless());
      else
	_e = Sim->_properties.getProperty(1.0, Property::Units::Dimensionless());
      intName = XML.getAttribute("Name");
      ISingleCapture::loadCaptureMap(XML);   
    }
    catch (boost::bad_lexical_cast &)
      {
	M_throw() << "Failed a lexical cast in IThinThread";
      }
  }

  IntEvent
  IThinThread::getEvent(const Particle &p1, 
			const Particle &p2) const 
  {
#ifdef DYNAMO_DEBUG
    if (!Sim->dynamics->isUpToDate(p1))
      M_throw() << "Particle 1 is not up to date";
  
    if (!Sim->dynamics->isUpToDate(p2))
      M_throw() << "Particle 2 is not up to date";

    if (p1 == p2)
      M_throw() << "You shouldn't pass p1==p2 events to the interactions!";
#endif 

    double d = (_diameter->getProperty(p1.getID())
		+ _diameter->getProperty(p2.getID())) * 0.5;

    double l = (_lambda->getProperty(p1.getID())
		+ _lambda->getProperty(p2.getID())) * 0.5;

    IntEvent retval(p1, p2, HUGE_VAL, NONE, *this);

    if (isCaptured(p1, p2))
      {
	double dt = Sim->dynamics->SphereSphereInRoot(p1, p2, d);
	if (dt != HUGE_VAL)
	  {
#ifdef DYNAMO_OverlapTesting
	    if (Sim->dynamics->sphereOverlap(p1, p2, d))
	      M_throw() << "Overlapping particles found"
			<< ", particle1 " << p1.getID()
			<< ", particle2 " << p2.getID()
			<< "\nOverlap = " 
			<< Sim->dynamics.getDynamics()
		.sphereOverlap(p1, p2, d)
		/ Sim->units.unitLength();
#endif	    
	    retval = IntEvent(p1, p2, dt, CORE, *this);
	  }

	dt = Sim->dynamics->SphereSphereOutRoot(p1, p2, l * d);
	if (retval.getdt() > dt)
	  retval = IntEvent(p1, p2, dt, WELL_OUT, *this);
      }
    else
      {
	double dt = Sim->dynamics->SphereSphereInRoot(p1, p2, d);

	if (dt != HUGE_VAL)
	  {
#ifdef DYNAMO_OverlapTesting
	    if (Sim->dynamics->sphereOverlap(p1, p2, d))
	      M_throw() << "Overlapping cores (but not registerd as captured) particles found in ThinThread" 
			<< "\nparticle1 " << p1.getID() << ", particle2 " 
			<< p2.getID() << "\nOverlap = " 
			<< Sim->dynamics->sphereOverlap(p1, p2, d)
		/ Sim->units.unitLength();
#endif
	    retval = IntEvent(p1, p2, dt, WELL_IN, *this);
	  }
      }

    return retval;
  }

  void
  IThinThread::runEvent(Particle& p1, Particle& p2, const IntEvent& iEvent) const
  {
    ++Sim->eventCount;

    double d = (_diameter->getProperty(p1.getID())
		+ _diameter->getProperty(p2.getID())) * 0.5;
    double d2 = d * d;

    double e = (_e->getProperty(p1.getID())
		+ _e->getProperty(p2.getID())) * 0.5;

    double l = (_lambda->getProperty(p1.getID())
		+ _lambda->getProperty(p2.getID())) * 0.5;
    double ld2 = d * l * d * l;

    double wd = (_wellDepth->getProperty(p1.getID())
		 + _wellDepth->getProperty(p2.getID())) * 0.5;
    switch (iEvent.getType())
      {
      case CORE:
	{
	  PairEventData retVal(Sim->dynamics->SmoothSpheresColl(iEvent, e, d2, CORE));
	  Sim->signalParticleUpdate(retVal);
	
	  Sim->ptrScheduler->fullUpdate(p1, p2);
	
	  BOOST_FOREACH(shared_ptr<OutputPlugin> & Ptr, Sim->outputPlugins)
	    Ptr->eventUpdate(iEvent, retVal);

	  break;
	}
      case WELL_IN:
	{
	  PairEventData retVal(Sim->dynamics->SphereWellEvent(iEvent, 0, d2));
	
	  if (retVal.getType() != BOUNCE)
	    addToCaptureMap(p1, p2);      
	
	  Sim->ptrScheduler->fullUpdate(p1, p2);
	  Sim->signalParticleUpdate(retVal);
	
	  BOOST_FOREACH(shared_ptr<OutputPlugin> & Ptr, Sim->outputPlugins)
	    Ptr->eventUpdate(iEvent, retVal);


	  break;
	}
      case WELL_OUT:
	{
	  PairEventData retVal(Sim->dynamics->SphereWellEvent(iEvent, -wd, ld2));
	
	  if (retVal.getType() != BOUNCE)
	    removeFromCaptureMap(p1, p2);      

	  Sim->signalParticleUpdate(retVal);

	  Sim->ptrScheduler->fullUpdate(p1, p2);
	
	  BOOST_FOREACH(shared_ptr<OutputPlugin> & Ptr, Sim->outputPlugins)
	    Ptr->eventUpdate(iEvent, retVal);
	  break;
	}
      default:
	M_throw() << "Unknown collision type";
      } 
  }

  void
  IThinThread::checkOverlaps(const Particle& part1, const Particle& part2) const
  {
    Vector  rij = part1.getPosition() - part2.getPosition();
    Sim->BCs->applyBC(rij);
    double r2 = rij.nrm2();

    double d = (_diameter->getProperty(part1.getID())
		+ _diameter->getProperty(part2.getID())) * 0.5;

    double d2 = d * d;

    if (r2 < d2)
      derr << "Overlap error\n ID1=" << part1.getID() 
	   << ", ID2=" << part2.getID() << "\nR_ij^2=" 
	   << r2 / pow(Sim->units.unitLength(),2)
	   << "\n(d)^2=" 
	   << d2 / pow(Sim->units.unitLength(),2) << std::endl;
  }
  
  void 
  IThinThread::outputXML(magnet::xml::XmlStream& XML) const
  {
    XML << magnet::xml::attr("Type") << "ThinThread"
	<< magnet::xml::attr("Diameter") << _diameter->getName()
	<< magnet::xml::attr("Elasticity") << _e->getName()
	<< magnet::xml::attr("Lambda") << _lambda->getName()
	<< magnet::xml::attr("WellDepth") << _wellDepth->getName()
	<< magnet::xml::attr("Name") << intName
	<< *range;
  
    ISingleCapture::outputCaptureMap(XML);  
  }
}
