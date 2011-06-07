/*  dynamo:- Event driven molecular dynamics simulator 
    http://www.marcusbannerman.co.uk/dynamo
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

#include "simulation.hpp"
#include "../dynamics/include.hpp"
#include "../schedulers/scheduler.hpp"
#include "../dynamics/include.hpp"
#include "../dynamics/interactions/intEvent.hpp"
#include "../outputplugins/outputplugin.hpp"
#include "../dynamics/liouvillean/liouvillean.hpp"
#include "../dynamics/systems/system.hpp"
#include "../dynamics/NparticleEventData.hpp"
#include "../outputplugins/tickerproperty/ticker.hpp"
#include "../dynamics/systems/sysTicker.hpp"
#include <magnet/exception.hpp>
#include <boost/foreach.hpp>
#include <iomanip>

Simulation::Simulation():
  Base("Simulation")
{}

void 
Simulation::setTickerPeriod(double nP)
{
  CSTicker* ptr = dynamic_cast<CSTicker*>(getSystem("SystemTicker"));
  if (ptr == NULL)
    M_throw() << "Could not find system ticker (maybe not required?)";

  ptr->setTickerPeriod(nP * dynamics.units().unitTime());
}

void 
Simulation::scaleTickerPeriod(double nP)
{
  CSTicker* ptr = dynamic_cast<CSTicker*>(getSystem("SystemTicker"));

  if (ptr == NULL)
    M_throw() << "Could not find system ticker (maybe not required?)";

  ptr->setTickerPeriod(nP * ptr->getPeriod());
}

System* 
Simulation::getSystem(std::string name)
{
  BOOST_FOREACH(magnet::ClonePtr<System>& sysPtr, dynamics.getSystemEvents())
    if (sysPtr->getName() == name)
      return sysPtr.get_ptr();
  
  return NULL;
}

void 
Simulation::addGlobal(Global* tmp)
{
  if (tmp == NULL)
    M_throw() << "Adding a NULL global";

  if (status != CONFIG_LOADED)
    M_throw() << "Cannot add global events now its initialised";

  dynamics.addGlobal(tmp);
}

void 
Simulation::addSystem(System* tmp)
{
  if (tmp == NULL)
    M_throw() << "Adding a NULL systemEvent";

  if (status != CONFIG_LOADED)
    M_throw() << "Cannot add system events now it is initialised";

  dynamics.addSystem(tmp);
}

void 
Simulation::addOutputPlugin(std::string Name)
{
  if (status >= INITIALISED)
    M_throw() << "Cannot add plugins now";
  
  I_cout() << "Loading output plugin string " << Name;

  magnet::ClonePtr<OutputPlugin> tempPlug(OutputPlugin::getPlugin(Name, this));
  outputPlugins.push_back(tempPlug);
}

void 
Simulation::setRandSeed(unsigned int x)
{ ranGenerator.seed(x); }

void 
Simulation::setnPrint(unsigned long long newnPrint)
{ 
  I_cout() << "Periodic output length set to " << newnPrint << " collisions";
  eventPrintInterval = newnPrint; 
}

void 
Simulation::simShutdown()
{ nextPrintEvent = endEventCount = eventCount; }

void 
Simulation::setTrajectoryLength(unsigned long long newMaxColl)
{ 
  //I_cout() << "Trajectory length set to " << newMaxColl << " collisions";
  endEventCount = newMaxColl; 
}

void
Simulation::initialise()
{
  I_cout() << "Sorting the Output Plugins";

  std::sort(outputPlugins.begin(), outputPlugins.end());
  
  bool needTicker = false;
  
  BOOST_FOREACH(magnet::ClonePtr<OutputPlugin> & Ptr, outputPlugins)
    if (dynamic_cast<OPTicker*>(Ptr.get_ptr()) != NULL)
      {
	needTicker = true; 
	break;
      }

  if (needTicker)
    dynamics.addSystemTicker();

  if (status != CONFIG_LOADED)
    M_throw() << "Sim initialised at wrong time";

  I_cout() << "Initialising Components";  

  if (ptrScheduler == NULL)
    M_throw() << "The scheduler has not been set!";      
  
  I_cout() << "Initialising the dynamics";
  dynamics.initialise();

  ensemble->initialise();
    
  fflush(stdout);

  if (endEventCount) //Only initialise the scheduler if we're simulating
    {
      I_cout() << "Initialising the scheduler";
      ptrScheduler->initialise();
    }
  else
    I_cout() << "Skipping initialisation of the Scheduler";
  
  I_cout() << "Initialising the output plugins";
  BOOST_FOREACH(magnet::ClonePtr<OutputPlugin> & Ptr, outputPlugins)
    Ptr->initialise();

  I_cout() << "System initialised";

  status = INITIALISED;
}

void
Simulation::runSimulation(bool silentMode)
{
  if (status != INITIALISED && status != PRODUCTION)
    M_throw() << "Bad state for runSimulation()";

  status = PRODUCTION;

  size_t lastprint = eventCount + eventPrintInterval;

  for (; eventCount < endEventCount;)
    try
      {
	ptrScheduler->runNextEvent();
	
	//Periodic work
	if ((eventCount > lastprint)
	    && !silentMode
	    && outputPlugins.size())
	  {
	    std::cout << "\n";
	    //Print the screen data plugins
	    BOOST_FOREACH( magnet::ClonePtr<OutputPlugin> & Ptr, 
			   outputPlugins)
	      Ptr->periodicOutput();

	    lastprint = eventCount + eventPrintInterval;
	    std::cout.flush();
	  }
      }
    catch (std::exception &cep)
      {
	M_throw() << "\nWhile executing event "
		  << eventCount << cep.what();
      }
}

void 
Simulation::configLoaded()
{
  //Handled by an input plugin
  if (status != START)
    M_throw() << "Loading config at wrong time";
  
  status = CONFIG_LOADED;
}

void
Simulation::outputData(std::string filename)
{
  if (status < INITIALISED || status == ERROR)
    M_throw() << "Cannot output data when not initialised!";

  namespace io = boost::iostreams;
  io::filtering_ostream coutputFile;
  
  if (std::string(filename.end()-4, filename.end()) == ".bz2")
    coutputFile.push(io::bzip2_compressor());
  
  coutputFile.push(io::file_sink(filename));
  
  xml::XmlStream XML(coutputFile);
  XML.setFormatXML(true);
  
  XML << std::setprecision(std::numeric_limits<double>::digits10)
      << xml::prolog() << xml::tag("OutputData");
  
  //Output the data and delete the outputplugins
  BOOST_FOREACH( magnet::ClonePtr<OutputPlugin> & Ptr, outputPlugins)
    Ptr->output(XML);
  
  XML << xml::endtag("OutputData");

  I_cout() << "Output written to " << filename;
}

long double 
Simulation::getSysTime()
{ return dSysTime / dynamics.units().unitTime(); }