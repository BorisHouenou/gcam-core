/*! 
* \file tran_sector.cpp
* \ingroup Objects
* \brief transporation technology class source file.
* \author Marshall Wise, Sonny Kim, Josh Lurz
* \date $Date$
* \version $Revision$
*/

#include "util/base/include/definitions.h"
#include <string>
#include <iostream>
#include <cassert>

#include "marketplace/include/marketplace.h"
#include "util/base/include/model_time.h"
#include "containers/include/scenario.h"
#include "sectors/include/tran_sector.h"
#include "sectors/include/tran_subsector.h"
#include "containers/include/gdp.h"

// xml headers
#include "util/base/include/xml_helper.h"
#include <xercesc/dom/DOMNode.hpp>

using namespace std;
using namespace xercesc;

extern Scenario* scenario;
// static initialize.
const string TranSector::XML_NAME = "tranSector";

//! Default constructor
TranSector::TranSector( const string regionName ): DemandSector( regionName ) {
    percentLicensed.resize( scenario->getModeltime()->getmaxper(), 1.0 );
    legacyDemand = true;
}

//! Destructor.
TranSector::~TranSector(){
}

/*! \brief Get the XML node name for output to XML.
*
* This public function accesses the private constant string, XML_NAME.
* This way the tag is always consistent for both read-in and output and can be easily changed.
* This function may be virtual to be overriden by derived class pointers.
* \author Josh Lurz, James Blackwood
* \return The constant XML_NAME.
*/
const std::string& TranSector::getXMLName() const {
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
const std::string& TranSector::getXMLNameStatic() {
	return XML_NAME;
}

//! Parses any input variables specific to derived classes
bool TranSector::XMLDerivedClassParse( const string& nodeName, const DOMNode* curr ) {
    // call the demand sector XML parse to fill demand sector attributes
    if( DemandSector::XMLDerivedClassParse( nodeName, curr ) ){
    }
    else if( nodeName == "percentLicensed" ) {
        XMLHelper<double>::insertValueIntoVector( curr, percentLicensed, scenario->getModeltime() );
    } 
    else if( nodeName == TranSubsector::getXMLNameStatic() ){
        parseContainerNode( curr, subsec, subSectorNameMap, new TranSubsector( regionName, name ) );
    }
    else {
        return false;
    }
    return true;
}

/*! \brief XML output stream for derived classes
*
* Function writes output due to any variables specific to derived classes to XML
* \author Josh Lurz
* \param out reference to the output stream
* \param tabs A tabs object responsible for printing the correct number of tabs. 
*/
void TranSector::toInputXMLDerived( ostream& out, Tabs* tabs ) const {
    // Write out parent class information.
    DemandSector::toInputXMLDerived( out, tabs );
    
    const Modeltime* modeltime = scenario->getModeltime();
    for( unsigned int i = 0; i < percentLicensed.size(); i++ ){
        XMLWriteElementCheckDefault( percentLicensed[ i ], "percentLicensed", out, tabs, 1.0, modeltime->getper_to_yr( i ) );
    }
}	


//! XML output for viewing.
void TranSector::toOutputXMLDerived( ostream& out, Tabs* tabs ) const {
    // Write out parent class information.
    DemandSector::toOutputXMLDerived( out, tabs );
    
    const Modeltime* modeltime = scenario->getModeltime();
    for( unsigned int i = 0; i < percentLicensed.size(); i++ ){
        XMLWriteElementCheckDefault( percentLicensed[ i ], "percentLicensed", out, tabs, 1.0, modeltime->getper_to_yr( i ) );
    }
}	

//! Write object to debugging xml output stream.
void TranSector::toDebugXMLDerived( const int period, ostream& out, Tabs* tabs ) const {
    // Write out parent class information.
    DemandSector::toDebugXMLDerived( period, out, tabs );
    XMLWriteElement( percentLicensed[ period ], "percentLicensed", out, tabs );
}

/*! \brief Perform any sector level calibration data consistancy checks
*
* Check to make sure that total calibrated outputs are equal to sector demand in base period.
* 
* \author Steve Smith
* \param period Model period
*/
void TranSector::checkSectorCalData( const int period ) {
// For any periods where inputs are calibrated, must make sure that read-in calibration is equal to service demand. 

   // Adjust aggregate demand to match calibrated outputs of all inputs to this sector are calibrated
  if ( inputsAllFixed( period, "allInputs" ) ) {
      double scaleFactor = getCalOutput( period ) / service[ period ];
      service[ period ] = getCalOutput( period );
      ILogger& mainLog = ILogger::getLogger( "main_log" );
      mainLog.setLevel( ILogger::DEBUG );
      mainLog << "Calibrated Demand Scaled by " << scaleFactor << " in region " << regionName << " sector " << name << endl;
   }
}

//! Aggrgate sector energy service demand function.
void TranSector::aggdemand( const GDP* gdp, const int period ) { 
      
    double scaledGdpPerCapita = gdp->getBestScaledGDPperCap(period); 
			 
	double gdp1 = gdp->getApproxScaledGDP(period); //gdp->getGDP(period); 
	
    const Modeltime* modeltime = scenario->getModeltime();
    double ser_dmd;
    
    /*!  Compute calibrating scaler if first period, otherwise use computed
    scaler in subsequent periods */
    
    // demand for service
    // reading in period 1 data so calibrate scaler to same for both periods
    if (period == 0 || period == 1) {
        priceRatio=1.0;
        priceRatioNotLic=1.0;
        
        // calculate base year scalers
        if (perCapitaBased) { // demand based on per capita GDP
            baseScaler = service[ period ]* percentLicensed[period] 
                * pow(scaledGdpPerCapita,-iElasticity[period])/(gdp1/scaledGdpPerCapita);
            baseScalerNotLic = service[ period ]* (1 - percentLicensed[period]) 
                * pow(scaledGdpPerCapita,-iElasticity[period])/(gdp1/scaledGdpPerCapita);
        }
        else {
            baseScaler = service[ period ]* percentLicensed[period] * pow(priceRatio,-pElasticity[period])
                * pow(gdp1,-iElasticity[period]);
            baseScalerNotLic = service[ period ]* (1 - percentLicensed[period]) * pow(priceRatioNotLic,-pElasticity[period])
                * pow(gdp1,-iElasticity[period]);
        }
        // base output is initialized by data
        ser_dmd = service[ period ]; 

        // Save the service demand without technical change applied for comparison with miniCAM.
        servicePreTechChange[ period ] = ser_dmd;
        service[period] = service[ period ];
    }
    else {
        // for non-base year
          // note normalized to previous year not base year
        // has implications for how technical change is applied
           priceRatio = sectorprice[period]/sectorprice[period-1];
           priceRatioNotLic = sectorprice[period]/sectorprice[period-1];
        // perCapitaBased is true or false
        if (perCapitaBased) { // demand based on per capita GDP
            ser_dmd = baseScaler*pow(priceRatio,pElasticity[period])*pow(scaledGdpPerCapita,iElasticity[period])
                + baseScalerNotLic*pow(priceRatioNotLic,pElasticity[period])*pow(scaledGdpPerCapita,iElasticity[period]);
            // need to multiply above by population ratio (current population/base year
            // population).  The gdp ratio provides the population ratio.
            ser_dmd *= gdp1/scaledGdpPerCapita;

        }
        else { // demand based on scale of GDP
            ser_dmd = baseScaler*pow(priceRatio,pElasticity[period])*pow(gdp1,iElasticity[period]);
        }
        // Save the service demand without technical change applied for comparison with miniCAM.
        servicePreTechChange[ period ] = ser_dmd;

        // adjust demand for AEEI, autonomous end-use energy intensity
        // note: not using cummulative technical change
        service[period] = ser_dmd/pow(1+aeei[period],modeltime->gettimestep(period));
    }
    
    output[period] = service[period];
    // sets subsector outputs, technology outputs, and market demands
    setoutput( service[ period ], period, gdp );
    sumOutput(period);
}

