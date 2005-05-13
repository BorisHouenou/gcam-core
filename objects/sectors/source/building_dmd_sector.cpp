/*! 
* \file building_dmd_sector.cpp
* \ingroup CIAM
* \brief The building demand sector
* \author Steve Smith
* \date $Date$
* \version $Revision$
*/

#include "util/base/include/definitions.h"
#include <string>
#include <iostream>
#include <cmath>
#include <xercesc/dom/DOMNode.hpp>
#include "util/base/include/xml_helper.h"
#include "util/base/include/model_time.h"
#include "marketplace/include/marketplace.h"
#include "containers/include/scenario.h"
#include "containers/include/gdp.h"
#include "util/base/include/model_time.h"
#include "marketplace/include/market_info.h"

#include "sectors/include/building_dmd_sector.h"
#include "sectors/include/building_dmd_subsector.h"

using namespace std;
using namespace xercesc;

extern Scenario* scenario;
// static initialize.
const string BuildingDemandSector::XML_NAME = "buildingdemandsector";

/*! \brief Default constructor.
*
* Constructor initializes member variables with default values, sets vector sizes, and sets value of debug flag.
*
* \author Sonny Kim, Steve Smith, Josh Lurz
*/
BuildingDemandSector::BuildingDemandSector( const string regionName ): DemandSector( regionName ){

    // resize vectors
    const Modeltime* modeltime = scenario->getModeltime();
    const int maxper = modeltime->getmaxper();

    baseScaler = -1;
    baseService.resize( maxper, -1 );
    
}

//! Default destructor
BuildingDemandSector::~BuildingDemandSector() {
}

/*! \brief Parses any attributes specific to derived classes
*
* Method parses any input data attributes (not child nodes, see XMLDerivedClassParse) that are specific to any classes derived from this class.
*
* \author Josh Lurz, Steve Smith
* \param nodeName The name of the curr node. 
* \param curr pointer to the current node in the XML input tree
* \return returns true if the node was parsed
*/
bool BuildingDemandSector::XMLDerivedClassParse( const string& nodeName, const DOMNode* curr ) {
    const Modeltime* modeltime = scenario->getModeltime();
            
    if ( DemandSector::XMLDerivedClassParse( nodeName, curr) ) {
    }   // if false, node was not parsed so far so try to parse here
    else if( nodeName == BuildingDemandSubSector::getXMLNameStatic() ){
        parseContainerNode( curr, subsec, subSectorNameMap, new BuildingDemandSubSector( regionName, name ) );
    }
    else if( nodeName == "baseservice" ){
        XMLHelper<double>::insertValueIntoVector( curr, baseService, modeltime );
    }
    else {
        return false;
    }
    // If was true somewhere above then noce was parsed
    return true;
}

/*! \brief XML output stream for derived classes
*
* Function writes output due to any variables specific to derived classes to XML
*
* \author Steve Smith, Josh Lurz
* \param out reference to the output stream
* \param tabs A tabs object responsible for printing the correct number of tabs. 
*/
void BuildingDemandSector::toInputXMLDerived( ostream& out, Tabs* tabs ) const {  
    const Modeltime* modeltime = scenario->getModeltime();
   
    DemandSector::toInputXMLDerived( out, tabs );
    XMLWriteVector( baseService, "baseservice", out, tabs, modeltime, 0.0 );
}	


//! XML output for viewing.
void BuildingDemandSector::toOutputXMLDerived( ostream& out, Tabs* tabs ) const {
    const Modeltime* modeltime = scenario->getModeltime();
    
    DemandSector::toOutputXMLDerived( out, tabs );

    // write the xml for the class members.
   XMLWriteVector( baseService, "baseservice", out, tabs, modeltime, 0.0 );
}

//! Write object to debugging xml output stream.
void BuildingDemandSector::toDebugXMLDerived( const int period, ostream& out, Tabs* tabs ) const {
    
    XMLWriteElement( baseService[ period ], "baseservice", out, tabs );
    DemandSector::toDebugXMLDerived( period, out, tabs );

}

/*! \brief Get the XML node name for output to XML.
*
* This public function accesses the private constant string, XML_NAME.
* This way the tag is always consistent for both read-in and output and can be easily changed.
* This function may be virtual to be overriden by derived class pointers.
* \author Josh Lurz, James Blackwood
* \return The constant XML_NAME.
*/
const std::string& BuildingDemandSector::getXMLName() const {
	return XML_NAME;
}

/*! \brief Get the XML node name in static form for comparison when parsing XML.
*
* This public function accesses the private constant string, XML_NAME.
* This way the tag is always consistent for both read-in and output and can be easily changed.
* The "==" operator that is used when parsing, required this second function to return static.
* \note A function cannot be static and virtual.
* \author Josh Lurz, James Blackwood
* \return The constant XML_NAME as a static.
*/
const std::string& BuildingDemandSector::getXMLNameStatic() {
	return XML_NAME;
}

/*! \brief Complete the initialization
* Save heating and cooling degree day information into market info so that this is available to all subsectors and demands
*
* \author Steve Smith
*/
void BuildingDemandSector::initCalc( const int period, const MarketInfo* aRegionInfo ) {

    // Add items from sectorInfo -- this needs to be done before control is passed to Sector:initCalc() so that information is available to subsector and technology initCalc() routines
    mSectorInfo->addItem( "heatingDegreeDays", aRegionInfo->getItemValue( "heatingDegreeDays" ) );
    mSectorInfo->addItem( "coolingDegreeDays", aRegionInfo->getItemValue( "coolingDegreeDays" ) );

    if ( baseScaler < 0 ) {
        ILogger& mainLog = ILogger::getLogger( "main_log" );
        mainLog.setLevel( ILogger::WARNING );
        mainLog << "WARNING: Building sector base demand service not set in period " << period 
                << " sector " << name << " region " << regionName 
                << ".  baseScaler being set to 1." << endl;
        baseScaler = 1;
    }
    
    Sector::initCalc( period, aRegionInfo );

}

/*! \brief Aggrgate sector energy service demand function
*
* Function calculates the aggregate demand for energy services and passes that down to the sub-sectors. 
* Demand is proportional to either GDP (to a power) or GDP per capita (to a power) times population.
*
* \author Sonny Kim
* \param gdp GDP object for calculating various types of gdps.
* \param period Model period
* \todo Sonny to add more to this description if necessary
* \pre Sector price attribute must have been previously calculated and set (via calcPrice)
*/
void BuildingDemandSector::aggdemand( const GDP* gdp, const int period ) {
      
    double scaledGdpPerCapita = gdp->getBestScaledGDPperCap(period); 
			 
	double scaledTotalGDP = gdp->getApproxScaledGDP(period); //gdp->getGDP(period); 
	
    double ser_dmd;
    
    // Prices are not calculated reliably until period 1 so do not use price ratio until after this
    // note normalized to previous year not base year (this is also done in detailed transportation)
    double priceRatio = ( period > 1 ) ? 
        sectorprice[ period ]/sectorprice[ period - 1 ] : 1;
     
    // demand for service
    // reading in period 1 data so calibrate scaler to same for both periods
    if ( baseService[ period ] >= 0 ) {
         
        // calculate base year scalers
        baseScaler = baseService[ period ] / pow( priceRatio, pElasticity[period] );
        
        if ( perCapitaBased ) { // demand based on per capita GDP times population
            baseScaler /= pow( scaledGdpPerCapita, iElasticity[period] ) * scaledTotalGDP/scaledGdpPerCapita;
        }
        else {
            baseScaler /= pow( scaledTotalGDP, iElasticity[period] );
        }
        ser_dmd = baseService[ period ];
    }
    else {      // for non-base year
        // perCapitaBased is true or false
        if ( perCapitaBased ) { // demand based on per capita GDP
            ser_dmd = baseScaler*pow( priceRatio, pElasticity[period] )*pow( scaledGdpPerCapita, iElasticity[period] );
            // need to multiply above by population ratio (current population/base year
            // population).  The gdp ratio provides the population ratio.
            ser_dmd *= scaledTotalGDP/scaledGdpPerCapita;
        }
        else { // demand based on scale of GDP
            ser_dmd = baseScaler*pow(priceRatio,pElasticity[period])*pow(scaledTotalGDP,iElasticity[period]);
        }
    }
    
    servicePreTechChange[ period ] = ser_dmd;
    service[period] = ser_dmd;
    setServiceDemand( service[ period ], period ); // sets the output

    // sets subsector outputs, technology outputs, and market demands
    setoutput( service[ period ], period, gdp );
    sumOutput(period);
}

