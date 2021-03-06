#include "DefoConfig.h"
#include "DefoRecoSurface.h"


///
///
///
DefoRecoSurface::DefoRecoSurface(QObject *parent)
  :QObject(parent)
{

  // read parameters
  spacingEstimate_ = DefoConfig::instance()->getValue<int>( "SPACING_ESTIMATE" );
  searchPathHalfWidth_ = DefoConfig::instance()->getValue<int>( "SEARCH_PATH_HALF_WIDTH" );
  nominalGridDistance_ = DefoConfig::instance()->getValue<double>( "NOMINAL_GRID_DISTANCE" );
  nominalCameraDistance_ = DefoConfig::instance()->getValue<double>( "NOMINAL_CAMERA_DISTANCE" );
  nominalViewingAngle_ = DefoConfig::instance()->getValue<double>( "NOMINAL_VIEWING_ANGLE" );
  pitchX_= DefoConfig::instance()->getValue<double>( "PIXEL_PITCH_X" );
  pitchY_= DefoConfig::instance()->getValue<double>( "PIXEL_PITCH_Y" );
  focalLength_= DefoConfig::instance()->getValue<double>( "LENS_FOCAL_LENGTH" );
  debugLevel_ = DefoConfig::instance()->getValue<unsigned int>( "DEBUG_LEVEL" );

  // to be called after cfg reading
  calculateHelpers();
}

///
/// calculate some helper variables
///
void DefoRecoSurface::calculateHelpers( void ) {

  heightAboveSensor_ = nominalCameraDistance_ * sin( nominalViewingAngle_ );
  if( tan( nominalViewingAngle_ ) != 0. ) horizontalDistanceToSensor_ = heightAboveSensor_ / tan( nominalViewingAngle_ );
  else {
    std::cerr << " [DefoRecoSurface::calculateHelpers] ** ERROR: tan(delta) is zero, no chance for proper reconstruction. Check parameters in configuration file. Abort." << std::endl;
    throw;
  }

}

///
/// perform the reconstruction based on two point collections:
/// a) reconstructed points of current image
/// b) reconstructed points of reference image
///
const DefoSurface DefoRecoSurface::reconstruct( DefoPointCollection& currentPoints, DefoPointCollection& referencePoints ) {

  DefoSurface theSurface;

  // create raw z splines for surface reconstruction
  DefoSplineField currentZSplineField = createZSplines( currentPoints, referencePoints );
  emit incrementRecoProgress();

  // connect x and y splines
  mountZSplines( currentZSplineField );
  emit incrementRecoProgress();

  // correct for global offsets
  removeGlobalOffset( currentZSplineField );
  emit incrementRecoProgress();

  // attach the spline field
  theSurface.setSplineField( currentZSplineField );
  emit incrementRecoProgress();

  theSurface.setPoints( referencePoints ); // ref points for the moment because createZSplines uses them
  emit incrementRecoProgress();

  theSurface.createPointFields(); // create matrix of points (internal)
  emit incrementRecoProgress();

  theSurface.makeSummary();

  return theSurface;
}



///
/// create z splines from difference in point positions
/// NEW VERSION based on indexed points
///
const DefoSplineField DefoRecoSurface::createZSplines( DefoPointCollection const& currentPoints, DefoPointCollection const& referencePoints ) {

  if( debugLevel_ >= 3 ) std::cout << " [DefoRecoSurface::createZSplines] =3= Starting" << std::endl;

  DefoSplineField theOutput;


  // x,y correction factors: see Diss. S. Koenig, p. 100;
  const std::pair<double,double> correctionFactors = std::pair<double,double> (
    pitchX_ / focalLength_ * ( nominalGridDistance_ + nominalCameraDistance_ ) / 2. / nominalGridDistance_,
    pitchY_ / focalLength_ * ( nominalGridDistance_ + nominalCameraDistance_ ) / 2. / nominalGridDistance_
  );

  if( debugLevel_ >= 2 ) std::cout << " [DefoRecoSurface::createZSplines] =2= Will apply nominal correction factors: "
				   << correctionFactors.first << " , " << correctionFactors.second << std::endl;

  // determine index ranges in current points
  // (could also take ref points)
  std::pair<int,int> indexRangeX = std::pair<int,int>( 0, 0 );
  std::pair<int,int> indexRangeY = std::pair<int,int>( 0, 0 );

  // here we assume that there is at least one point right/left/above/below the blue one, resp., in the image;
  // otherwise the reconstruction will probably crash later
  for( DefoPointCollection::const_iterator it = currentPoints.begin(); it < currentPoints.end(); ++it ) {
    if( it->getIndex().first  < indexRangeX.first )  indexRangeX.first = it->getIndex().first;
    if( it->getIndex().first  > indexRangeX.second ) indexRangeX.second = it->getIndex().first;
    if( it->getIndex().second < indexRangeY.first )  indexRangeY.first  = it->getIndex().second;
    if( it->getIndex().second > indexRangeY.second ) indexRangeY.second = it->getIndex().second;
  }
  
  if( debugLevel_ >= 2 ) std::cout << " [DefoRecoSurface::createZSplines] =2= Found index range: x: " << indexRangeX.first
				   << " .. " << indexRangeX.second << " y: " << indexRangeY.first << " .. " << indexRangeY.second << std::endl;


  // we need the blue point (from *ref*) as geom. reference, it always has index 0,0
  std::pair<bool,DefoPointCollection::const_iterator> bluePointByIndex = findPointByIndex( referencePoints, std::pair<int,int>( 0, 0 ) );


  // now attach the points to the spline sets according to their indices
  std::pair<int,int> index = std::pair<int,int>( indexRangeX.first, indexRangeY.first );

  // first along-y
  for( ; index.first <= indexRangeX.second; ++index.first ) {

    // create a spline set along-y (a "column") .. 
    DefoSplineSetY aSplineSet;
    index.second = indexRangeY.first; // reset

    for( ; index.second <= indexRangeY.second; ++index.second ) {
      
      std::pair<bool,DefoPointCollection::const_iterator> currentPointByIndex   = findPointByIndex( currentPoints, index );
      std::pair<bool,DefoPointCollection::const_iterator> referencePointByIndex = findPointByIndex( referencePoints, index );
      
      // check if a point with that index exists in both images
      // (it should then have been a reflection from the same source)
      if( currentPointByIndex.first && referencePointByIndex.first ) {

	// this point is abstract and lives where the *ref* point is on the module
	// (make a copy)
	DefoPoint aPoint = DefoPoint( *(referencePointByIndex.second) );

	// the attached slope (= tan(alpha)) is derived from the difference in y position
	aPoint.setSlope( correctionFactors.second * ( *(currentPointByIndex.second) - *(referencePointByIndex.second) ).getY() ); // ##### check

	// convert from pixel units to real units on module

	// set blue point at x=0,y=0
	aPoint.setPosition( ( aPoint.getX() - bluePointByIndex.second->getX() ) * pitchX_ * nominalCameraDistance_ / focalLength_ ,
		      ( aPoint.getY() - bluePointByIndex.second->getY() ) * pitchY_ * nominalCameraDistance_ / focalLength_ );

// 	aPoint.setXY( aPoint.getX() * pitchX_ * nominalCameraDistance_ / focalLength_ , // old version
// 		      aPoint.getY() * pitchY_ * nominalCameraDistance_ / focalLength_ );


	aSplineSet.addPoint( aPoint );

	if( debugLevel_ >= 3 ) std::cout << " [DefoRecoSurface::createZSplines] =3= Found shared point along-y with indices: "
					 << index.first << " , " << index.second << std::endl;

      }

      else {
	if( debugLevel_ >= 2 ) std::cout << " [DefoRecoSurface::createZSplines] =2= Non-shared point along-y in current image with indices: "
					 << index.first << " , " << index.second << std::endl;
      }

    }
    
    // check if there are enough points attached to the set (min 2)
    if( 2 > aSplineSet.getNPoints() ) continue;

    // do the fit
    aSplineSet.doFitZ();
    
    // attach to output field (as *second*!!)
    theOutput.second.push_back( aSplineSet );

  }




  // then along-x
  index = std::pair<int,int>( indexRangeX.first, indexRangeY.first );

  for( ; index.second <= indexRangeY.second; ++index.second ) {

    // create a spline set along-x (a "row") .. 
    DefoSplineSetX aSplineSet;
    index.first = indexRangeX.first; // reset

    for( ; index.first <= indexRangeX.second; ++index.first ) {
      
      std::pair<bool,DefoPointCollection::const_iterator> currentPointByIndex   = findPointByIndex( currentPoints, index );
      std::pair<bool,DefoPointCollection::const_iterator> referencePointByIndex = findPointByIndex( referencePoints, index );
      
      // check if a point with that index exists in both images
      // (it should then have been a reflection from the same source)
      if( currentPointByIndex.first && referencePointByIndex.first ) {

	// this point is abstract and lives where the ref point is on the module
	// (make a copy)
	DefoPoint aPoint = DefoPoint( *(referencePointByIndex.second) );

	// the attached slope (= tan(alpha)) is derived from the difference in y position
	aPoint.setSlope( correctionFactors.first * ( *(currentPointByIndex.second) - *(referencePointByIndex.second) ).getX() ); // ##### check
	
	// convert from pixel units to real units on module

	// 	aPoint.setXY( aPoint.getX() * pitchX_ * nominalCameraDistance_ / focalLength_ , // old version
	// 		      aPoint.getY() * pitchY_ * nominalCameraDistance_ / focalLength_ );

	// blue point at x=0,y=0
	aPoint.setPosition( ( aPoint.getX() - bluePointByIndex.second->getX() ) * pitchX_ * nominalCameraDistance_ / focalLength_ ,
		      ( aPoint.getY() - bluePointByIndex.second->getY() ) * pitchY_ * nominalCameraDistance_ / focalLength_ );

	aSplineSet.addPoint( aPoint );

	if( debugLevel_ >= 3 ) std::cout << " [DefoRecoSurface::createZSplines] =3= Found shared point along-x with indices: "
					 << index.first << " , " << index.second << std::endl;

      }

      else {
	if( debugLevel_ >= 2 ) std::cout << " [DefoRecoSurface::createZSplines] =2= non-shared point along-x in current image with indices: "
					 << index.first << " , " << index.second << std::endl;
      }

    }

    // check if there are enought points attached to the set (min 2)
    if( 2 > aSplineSet.getNPoints() ) continue;

    // do the fit
    aSplineSet.doFitZ();
    
    // attach to output field (as *second*!!)
    theOutput.first.push_back( aSplineSet );

  }


  // c'est tout
  if( debugLevel_ >= 3 ) std::cout << " [DefoRecoSurface::createZSplines] =3= Done" << std::endl;
  return theOutput;

}



///
/// create z splines from difference of point positions
/// OLD symmetric version, to be deprecated
///
const DefoSplineField DefoRecoSurface::createZSplinesOld( DefoPointCollection const& currentPoints, DefoPointCollection const& referencePoints ) {

  DefoSplineField theOutput;

  // group & sort the points.
  // these are supposed to match 1<>1 in structure,
  // so no points must "vanish" from the DUT by deformation..
  const std::pair<std::vector<DefoPointCollection>,std::vector<DefoPointCollection> > currentGroups = groupPointsSorted( currentPoints );
  const std::pair<std::vector<DefoPointCollection>,std::vector<DefoPointCollection> > referenceGroups = groupPointsSorted( referencePoints );

  // check if we have the same number of rows & columns
  // in current image and reference image
  if( currentGroups.first.size() != referenceGroups.first.size()   ||
      currentGroups.second.size() != referenceGroups.second.size()    ) {
    std::cerr << " [DefoRecoSurface::createZSplinesOld] ** ERROR: Size mismatch in sorted point groups, empty output." << std::endl;
    return theOutput;
  }

  // x,y correction factors: see Diss. S. Koenig, p. 100
  const std::pair<double,double> correctionFactors (
    pitchX_ / focalLength_ * ( nominalGridDistance_ + nominalCameraDistance_ ) / 2. / nominalGridDistance_,
    pitchY_ / focalLength_ * ( nominalGridDistance_ + nominalCameraDistance_ ) / 2. / nominalGridDistance_
  );

  // first along y = SAME-X = pointGroups.first
  std::vector<DefoPointCollection>::const_iterator currentCollIt, referenceCollIt;
  for( currentCollIt = currentGroups.first.begin(), referenceCollIt = referenceGroups.first.begin();
       currentCollIt < currentGroups.first.end(); ++currentCollIt, ++referenceCollIt ) {
    
    // create a spline set (a "column") .. 
    DefoSplineSetY aSplineSet;

    // .. and attach the points
    DefoPointCollection::const_iterator currentPointIt, referencePointIt;
    for( currentPointIt = currentCollIt->begin(), referencePointIt = referenceCollIt->begin();
	 currentPointIt < currentCollIt->end(); ++currentPointIt, ++referencePointIt ) {

      // this point is abstract and lives where the ref point is on the module
      // (make a copy)
      DefoPoint aPoint = DefoPoint( *referencePointIt );

      // the attached slope (= tan(alpha)) is derived from the difference in x position
      aPoint.setSlope( correctionFactors.first * (*currentPointIt - *referencePointIt).getY() );

      // convert from pixel units to real units on module
      aPoint.setPosition( aPoint.getX() * pitchX_ * nominalCameraDistance_ / focalLength_ , 
 		    aPoint.getY() * pitchY_ * nominalCameraDistance_ / focalLength_ );

      aSplineSet.addPoint( aPoint );

    }

    // do the fit
    aSplineSet.doFitZ();

    // attach to output field (as *second*!!)
    theOutput.second.push_back( aSplineSet );

  }


  // then along x = SAME-Y = pointGroups.second
  for( currentCollIt = currentGroups.second.begin(), referenceCollIt = referenceGroups.second.begin();
       currentCollIt < currentGroups.second.end(); ++currentCollIt, ++referenceCollIt ) {
    
    // create a spline set (a "row") .. 
    DefoSplineSetX aSplineSet;
    
    // .. and attach the points
    DefoPointCollection::const_iterator currentPointIt, referencePointIt;
    for( currentPointIt = currentCollIt->begin(), referencePointIt = referenceCollIt->begin();
	 currentPointIt < currentCollIt->end(); ++currentPointIt, ++referencePointIt ) {

      // this point is abstract and lives where the ref point is
      // (make a copy)
      DefoPoint aPoint = DefoPoint( *referencePointIt );

      // the attached slope (= tan(alpha)) is derived from the difference in x position
      aPoint.setSlope( correctionFactors.first * (*currentPointIt - *referencePointIt).getX() );

      // convert from pixel units to real units
      aPoint.setPosition( aPoint.getX() * pitchX_ * nominalCameraDistance_ / focalLength_ , 
 		    aPoint.getY() * pitchY_ * nominalCameraDistance_ / focalLength_   );

      aSplineSet.addPoint( aPoint );

    }

    // do the fit
    aSplineSet.doFitZ();

    // attach to output field (as *first*!!)
    theOutput.first.push_back( aSplineSet );
  }

  return theOutput;

}



///
///
///
void DefoRecoSurface::mountZSplines( DefoSplineField& splineField ) const {

  // the numbers of point rows/columns
  std::pair<unsigned int, unsigned int> nSplines
    = std::pair<unsigned int, unsigned int>( splineField.first.size(), splineField.second.size() );
  
  // this container stores a double per splineset;
  // used to sum up the relative heights of the splines from different mountings
  // for later averaging
  DefoSplineFieldOffsets theOffsets;
  theOffsets.first.resize( nSplines.first, 0. );
  theOffsets.second.resize( nSplines.second, 0. );

  // this container stores and int per splineset, counting the mounting processes,
  // used to properly calculate the average
  DefoSplineFieldCounters theCounters;
  theCounters.first.resize( nSplines.first, 0 );
  theCounters.second.resize( nSplines.second, 0 );


  // make a working copy for temporary values
  DefoSplineField cSplineField( splineField );


  // loop all "along-x" splinesets as height reference for the orthogonal "along-y" splines
  for( DefoSplineSetXCollection::const_iterator itX = cSplineField.first.begin(); itX < cSplineField.first.end(); ++itX ) {

    // loop all the points in this reference
    for( DefoPointCollection::const_iterator itPX = itX->getPoints().begin(); itPX < itX->getPoints().end(); ++itPX ) {
      
      // find the corresponding along-y spline which itX shares this point with
      for( DefoSplineSetYCollection::iterator itY = cSplineField.second.begin(); itY < cSplineField.second.end(); ++itY ) {
	for( DefoPointCollection::const_iterator itPY = itY->getPoints().begin(); itPY < itY->getPoints().end(); ++itPY ) {

	  if( itPX->getIndex() == itPY->getIndex() ) { // sharing?

	    // evaluate the height of the splineset at the corresponding point
	    const double heightOfSpline = itY->eval( itPY->getY() );
	    
	    // evaluate the height of the reference splineset at that point
	    const double heightOfReference = itX->eval( itPX->getX() );
	    
	    // determine the offset and apply
	    itY->offset( heightOfReference - heightOfSpline );

	  }

	} // itPY
      } // itY

    } // itPX
    
    // now fill the offset container with the results
    DefoSplineSetYCollection::const_iterator itY2 = cSplineField.second.begin();
    std::vector<double>::iterator itOY = theOffsets.second.begin();
    for( ; itY2 < cSplineField.second.end(); ++itY2, ++itOY ) {
      *itOY += itY2->eval( itY2->getPoints().front().getY() );
    }
    
  } // itX


  // now "backpropagate" this to the "along-x" splines
  // using the first "along-y" spline as reference

  // fresh working copy
  cSplineField = DefoSplineField( splineField );
  

  // loop all "along-y" splinesets as height reference for the orthogonal "along-x" splines
  for( DefoSplineSetYCollection::const_iterator itY = cSplineField.second.begin(); itY < cSplineField.second.end(); ++itY ) {

    // loop all the points in this reference
    for( DefoPointCollection::const_iterator itPY = itY->getPoints().begin(); itPY < itY->getPoints().end(); ++itPY ) {
      
      // find the corresponding along-y spline which itX shares this point with
      for( DefoSplineSetXCollection::iterator itX = cSplineField.first.begin(); itX < cSplineField.first.end(); ++itX ) {
	for( DefoPointCollection::const_iterator itPX = itX->getPoints().begin(); itPX < itX->getPoints().end(); ++itPX ) {

	  if( itPY->getIndex() == itPX->getIndex() ) { // sharing?

	    // evaluate the height of the splineset at the corresponding point
	    const double heightOfSpline = itX->eval( itPX->getX() );

	    // evaluate the height of the reference splineset at that point
	    const double heightOfReference = itY->eval( itPY->getY() );
	    
	    // determine the offset and apply
	    itX->offset( heightOfReference - heightOfSpline );

	  }

	} // itPX
      } // itX

    } // itPY

    // now fill the offset container with the results
    DefoSplineSetXCollection::const_iterator itX2 = cSplineField.first.begin();
    std::vector<double>::iterator itOX = theOffsets.first.begin();
    for( ; itX2 < cSplineField.first.end(); ++itX2, ++itOX ) {
      *itOX += itX2->eval( itX2->getPoints().front().getX() );
    }

  } // itY
  



  // average the offsets
  for( std::vector<double>::iterator itX = theOffsets.first.begin(); itX < theOffsets.first.end(); ++itX ) {
    *itX /= nSplines.first;
  }
  for( std::vector<double>::iterator itY = theOffsets.second.begin(); itY < theOffsets.second.end(); ++itY ) {
    *itY /= nSplines.second;
  }
  
  
  // apply the average to the original input spline field
  DefoSplineSetXCollection::iterator itX = splineField.first.begin();
  DefoSplineSetYCollection::iterator itY = splineField.second.begin();
  std::vector<double>::const_iterator itOX = theOffsets.first.begin();
  std::vector<double>::const_iterator itOY = theOffsets.second.begin();
  
  for( ; itX < splineField.first.end(); ++itX, ++itOX ) {
    itX->offset( *itOX - itX->eval( itX->getPoints().front().getX() ) );
  }
  
  for( ; itY < splineField.second.end(); ++itY, ++itOY ) {
    itY->offset( *itOY - itY->eval( itY->getPoints().front().getY() ) );
  }

}



///
/// combine z-splines along x and y
/// by mounting them on each other
/// OLD symmetric version, to be deprecated
///
void DefoRecoSurface::mountZSplinesOld( DefoSplineField& splineField ) const {

  // the numbers of point rows/columns
  std::pair<unsigned int, unsigned int> nSplines
    = std::pair<unsigned int, unsigned int>( splineField.first.size(), splineField.second.size() );

  // this container stores a double per splineset;
  // used to sum up the relative heights of the splines from different mountings
  // for later averaging
  DefoSplineFieldOffsets theOffsets;
  theOffsets.first.resize( nSplines.first, 0. );
  theOffsets.second.resize( nSplines.second, 0. );


  
  // loop all "along-x" splinesets as reference
  for( DefoSplineSetXCollection::const_iterator itX = splineField.first.begin(); itX < splineField.first.end(); ++itX ) {

    // make a working copy
    DefoSplineField cSplineField( splineField );
    
    // the current reference "along-x" spline
    // on which the along-y splines are mounted
    DefoSplineSetX const& theReferenceSplineSetX = *itX;
    
    // all the "along-y" splines
    // and the corresponding points that are shared with the reference "along-x" spline
    DefoPointCollection::const_iterator pIt = theReferenceSplineSetX.getPoints().begin();
    for( DefoSplineSetYCollection::iterator itY = cSplineField.second.begin();
	 itY < cSplineField.second.end(); ++itY, ++pIt ) {
      
      // evaluate the height of the splineset at the corresponding point
      const double heightOfSpline = itY->eval( pIt->getY() );

      // evaluate the height of the reference splineset at that point
      const double heightOfReference = theReferenceSplineSetX.eval( pIt->getX() );

      // determine the offset and apply
      itY->offset( heightOfReference - heightOfSpline );

    }

    // now fill the offset container with the results
    DefoSplineSetYCollection::const_iterator itY2 = cSplineField.second.begin();
    std::vector<double>::iterator itOY = theOffsets.second.begin();
    for( ; itY2 < cSplineField.second.end(); ++itY2, ++itOY ) {
      *itOY += itY2->eval( itY2->getPoints().front().getY() );
    }


  }

  

  // now "backpropagate" this to the "along-x" splines
  // using the first "along-y" spline as reference


  // loop all "along-y" splinesets as reference
  for( DefoSplineSetYCollection::const_iterator itY = splineField.second.begin(); itY < splineField.second.end(); ++itY ) {

    // make a working copy
    DefoSplineField cSplineField( splineField );

    // the reference "along-y" spline
    // on which the along-x splines are mounted
    DefoSplineSetY const& theReferenceSplineSetY = *itY;

    // all the "along-x" splines
    // and the corresponding points that are shared with the reference "along-y" spline
    DefoPointCollection::const_iterator pIt = theReferenceSplineSetY.getPoints().begin();
    for( DefoSplineSetXCollection::iterator itX = cSplineField.first.begin();
	 itX < cSplineField.first.end(); ++itX, ++pIt ) {
      
      // evaluate the height of the splineset at the first point
      const double heightOfSpline = itX->eval( pIt->getX() );
      
      // evaluate the height of the reference splineset at that point
      const double heightOfReference = theReferenceSplineSetY.eval( pIt->getY() );
      
      // determine the offset and apply
      itX->offset( heightOfReference - heightOfSpline );
      
    }
    
    
    // now fill the offset container with the results
    DefoSplineSetXCollection::const_iterator itX2 = cSplineField.first.begin();
    std::vector<double>::iterator itOX = theOffsets.first.begin();
    
    for( ; itX2 < cSplineField.first.end(); ++itX2, ++itOX ) {
      *itOX += itX2->eval( itX2->getPoints().front().getX() );
    }
    
  }



  // average the offsets
  for( std::vector<double>::iterator itX = theOffsets.first.begin(); itX < theOffsets.first.end(); ++itX ) {
    *itX /= nSplines.first;
  }
  for( std::vector<double>::iterator itY = theOffsets.second.begin(); itY < theOffsets.second.end(); ++itY ) {
    *itY /= nSplines.second;
  }


  // apply the average to the original input spline field
  DefoSplineSetXCollection::iterator itX = splineField.first.begin();
  DefoSplineSetYCollection::iterator itY = splineField.second.begin();
  std::vector<double>::const_iterator itOX = theOffsets.first.begin();
  std::vector<double>::const_iterator itOY = theOffsets.second.begin();

  for( ; itX < splineField.first.end(); ++itX, ++itOX ) {
    itX->offset( *itOX - itX->eval( itX->getPoints().front().getX() ) );
  }

  for( ; itY < splineField.second.end(); ++itY, ++itOY ) {
    itY->offset( *itOY - itY->eval( itY->getPoints().front().getY() ) );
  }
    
}



///
/// create x(y) and y(x) spline field from the points
/// this is for displaying the point grouping results only,
/// and has no effect on surface reco
///
const DefoSplineField DefoRecoSurface::createXYSplines( DefoPointCollection const& points ) {

  DefoSplineField theOutput;

  // group & sort the points
  const std::pair<std::vector<DefoPointCollection>,std::vector<DefoPointCollection> > pointGroups = groupPointsSorted( points );


  // first along y = SAME-X = pointGroups.first
  for( std::vector<DefoPointCollection>::const_iterator collIt = pointGroups.first.begin();
       collIt < pointGroups.first.end(); ++collIt ) {

    // create a spline set (a "row") .. 
    DefoSplineSetY aSplineSet;

    // .. and attach the points
    for( DefoPointCollection::const_iterator pointIt = collIt->begin(); pointIt < collIt->end(); ++pointIt ) {
      aSplineSet.addPoint( *pointIt );
    }

    // fit the splineset
    if( !aSplineSet.doFitXY() ) {
      std::cerr << " [DefoRecoSurface::createXYSplines] ** ERROR: Failed to fit splineset along Y, return empty output." << std::endl;
      return theOutput;
    }

    // attach to output field (as *second*!!)
    theOutput.second.push_back( aSplineSet );

  }


  // then along x = SAME-Y = pointGroups.second
  for( std::vector<DefoPointCollection>::const_iterator collIt = pointGroups.second.begin();
       collIt < pointGroups.second.end(); ++collIt ) {

    // create a spline set (a "row") .. 
    DefoSplineSetX aSplineSet;

    // .. and attach the points
    for( DefoPointCollection::const_iterator pointIt = collIt->begin(); pointIt < collIt->end(); ++pointIt ) {
      aSplineSet.addPoint( *pointIt );
    }

    // fit the splineset
    if( !aSplineSet.doFitXY() ) {
      std::cerr << " [DefoRecoSurface::createXYSplines] ** ERROR: Failed to fit splineset along X, return empty output." << std::endl;
      return theOutput;
    }

    // attach to output field (as *first*!!)
    theOutput.first.push_back( aSplineSet );

  }

  return theOutput;
    
}



///
/// group points in two sets of PointCollections (1 for x and 1 for y),
/// such that points in each collection are within a search path
/// according to x,y coordinates
/// the pair<> is for groups with "~SAME" x and y, resp.:
/// first = "~SAME" X, second = "~SAME" Y
///
const std::pair<std::vector<DefoPointCollection>,std::vector<DefoPointCollection> >
DefoRecoSurface::groupPointsSorted( DefoPointCollection const& thePoints ) {

  // group the points according to their x,y coordinates,
  // assuming they are lined up within a path of
  // +- searchPathHalfWidth_ pixels
  std::pair<std::vector<DefoPointCollection>, std::vector<DefoPointCollection> >  theOutput;

  // loop all points and group simultaneously
  for( DefoPointCollection::const_iterator it = thePoints.begin(); it < thePoints.end(); ++it ) {
    
    // add a point if it complies with the last point in a group
    // according to the searchPathHalfWidth_
    std::pair<bool,bool> isAssigned = std::pair<bool,bool>( false, false );

    // first: groups with "~SAME" X
    for( std::vector<DefoPointCollection>::iterator collIt = theOutput.first.begin();
	 collIt < theOutput.first.end(); ++collIt ) {

      if( fabs( it->getX() - collIt->back().getX() ) < searchPathHalfWidth_ ) { 
	collIt->push_back( *it );
	isAssigned.first = true;
      }
      
    }
    
    // if there is no match, create a new point group
    // (applies also for the very first point)
    if( !isAssigned.first ) {
      DefoPointCollection aPointCollection;
      aPointCollection.push_back( *it );
      theOutput.first.push_back( aPointCollection );
    }

    // then groups with "~SAME" Y
    for( std::vector<DefoPointCollection>::iterator collIt = theOutput.second.begin();
	 collIt < theOutput.second.end(); ++collIt ) {
      
      if( fabs( it->getY() - collIt->back().getY() ) < searchPathHalfWidth_ ) { 
	collIt->push_back( *it );
	isAssigned.second = true;
      }
      
    }
    
    // if there is no match, create a new point group
    // (applies also for the very first point)
    if( !isAssigned.second ) {
      DefoPointCollection aPointCollection;
      aPointCollection.push_back( *it );
      theOutput.second.push_back( aPointCollection );
    }
    
  } // point loop


  // sort the output

  // first: points with "~SAME" X are sorted according to Y
  for( std::vector<DefoPointCollection>::iterator collIt = theOutput.first.begin();
       collIt < theOutput.first.end(); ++collIt ) {
    std::sort( collIt->begin(), collIt->end(), DefoPointYPredicate );
  }

  // first: points with "~SAME" Y are sorted according to X
  for( std::vector<DefoPointCollection>::iterator collIt = theOutput.second.begin();
       collIt < theOutput.second.end(); ++collIt ) {
    std::sort( collIt->begin(), collIt->end(), DefoPointXPredicate );
  }

  // then sort the vectors of groups, the "~SAME X" groups according to their average X ..
  std::sort( theOutput.first.begin(), theOutput.first.end(), DefoPointCollectionAverageXPredicate );
  
  // .. and the "~SAME Y" groups according to their average Y
  std::sort( theOutput.second.begin(), theOutput.second.end(), DefoPointCollectionAverageYPredicate );

  return theOutput;

}



///
/// find an estimate for the spacing between the points
/// along x and y, resp.
/// from averaging
///
const std::pair<double,double> DefoRecoSurface::determineAverageSpacing( DefoPointCollection const& points ) const {

  // refill coordinates separately for x,y
  // into vectors container
  std::pair<std::vector<double>,std::vector<double> > coordinates;

  // fill the container
  for( DefoPointCollection::const_iterator it = points.begin(); it < points.end(); ++it ) {
    coordinates.first.push_back(  it->getX() );
    coordinates.second.push_back( it->getY() );
  }

  // sort the vectors
  sort( coordinates.first.begin(),  coordinates.first.end() );
  sort( coordinates.second.begin(), coordinates.second.end() );

    
  // walk along, look for spacings > spacingEstimate_
  // and average them
  // (assuming iterators are parallel)
  std::pair<double,double> sumOfSpacings;
  std::pair<unsigned int,unsigned int> nSpacings = std::pair<unsigned int,unsigned int>( 0, 0 );

  for( std::vector<double>::const_iterator it1 = coordinates.first.begin() + 1, it2 = coordinates.second.begin() + 1;
       it1 < coordinates.first.end(); ++it1, ++it2 ) {
    
    const std::pair<double,double> aSpacing = std::pair<double,double>( fabs( *it1 - *(it1 - 1) ), fabs( *it2 - *(it2 - 1) ) );
    if( aSpacing.first  > spacingEstimate_ ) { sumOfSpacings.first  += aSpacing.first;  ++nSpacings.first; }
    if( aSpacing.second > spacingEstimate_ ) { sumOfSpacings.second += aSpacing.second; ++nSpacings.second; }

  }

  if( !( nSpacings.first && nSpacings.second ) ) {
    std::cerr << " [DefoRecoSurface::determineAverageSpacing] ** ERROR: unable to determine spacing, trying default (" 
	      <<  spacingEstimate_ << " pix)." << std::endl;
    return std::pair<double,double>( spacingEstimate_, spacingEstimate_ );
  }

  // average
  return std::pair<double,double>( sumOfSpacings.first / nSpacings.first, sumOfSpacings.second / nSpacings.second );

}

///
/// in the collection "points", find the one which is closest to "aPoint";
/// this closest point must not be further away from aPoint than (SPACING_ESTIMATE,SPACING_ESTIMATE),
/// otherwise return false.
///
std::pair<bool,DefoPointCollection::iterator> DefoRecoSurface::findClosestPoint( DefoPoint const& aPoint, DefoPointCollection& points ) const {
  
  DefoPoint ref( std::numeric_limits<double>::max(), std::numeric_limits<double>::max() );
  DefoPoint max( spacingEstimate_, spacingEstimate_ );

  std::vector<std::pair<DefoPointCollection::iterator,DefoPoint> > pointsAndDistances;

  // loop points, compute distance
  for( DefoPointCollection::iterator it = points.begin(); it < points.end(); ++it ) {
    const DefoPoint distance = aPoint - *it;
    pointsAndDistances.push_back( std::pair<DefoPointCollection::iterator,DefoPoint>( it, distance ) );
  }

  // check, sort & get smallest
  DefoPointCollection::iterator result;
  if( 0 == pointsAndDistances.size() ) return( std::pair<bool,DefoPointCollection::iterator>( false, result ) ); // ?
  std::sort( pointsAndDistances.begin(), pointsAndDistances.end(), DefoPointPairSecondAbsPredicate );
  result = pointsAndDistances.at( 0 ).first;

  ///////////////////////////////////////////////////////
//   std::cout << "~~~~~~~~~~~~~~~~~~~~~" << std::endl;
//   for( unsigned int i = 0; i < 5 && i < pointsAndDistances.size(); ++i ) {
//     std::cout << "PPP: " << i << " " << pointsAndDistances.at( i ).first->getX() << " " 
// 	      << pointsAndDistances.at( i ).first->getY() << " " << pointsAndDistances.at( i ).second.abs() << std::endl;
//   }
//   std::cout << "~~~~~~~~~~~~~~~~~~~~~" << std::endl;
  ///////////////////////////////////////////////////////

  // point not too far away from requested position aPoint?
  bool isFound = false;
  if( *result - aPoint < max ) isFound = true;

  return std::pair<bool,DefoPointCollection::iterator>( isFound, result );

}



///
/// in the collection "points", find the one which is closest to "aPoint"
/// but is different from excludedPoint;
/// this closest point must not be further away from aPoint than (SPACING_ESTIMATE,SPACING_ESTIMATE),
/// otherwise return false.
///
std::pair<bool,DefoPointCollection::iterator> 
DefoRecoSurface::findClosestPointExcluded( DefoPoint const& aPoint, DefoPointCollection& points, DefoPoint const& excludedPoint ) const {
  
  DefoPoint ref( std::numeric_limits<double>::max(), std::numeric_limits<double>::max() );
  DefoPoint max( spacingEstimate_, spacingEstimate_ );

  std::vector<std::pair<DefoPointCollection::iterator,DefoPoint> > pointsAndDistances;

  // loop points, compute distance
  for( DefoPointCollection::iterator it = points.begin(); it < points.end(); ++it ) {
    const DefoPoint distance = aPoint - *it;
    pointsAndDistances.push_back( std::pair<DefoPointCollection::iterator,DefoPoint>( it, distance ) );
  }

  // check, sort
  DefoPointCollection::iterator result;
  if( 0 == pointsAndDistances.size() ) return( std::pair<bool,DefoPointCollection::iterator>( false, result ) ); // ?
  std::sort( pointsAndDistances.begin(), pointsAndDistances.end(), DefoPointPairSecondAbsPredicate );

  // get point with smallest distance to aPoint, which is not excludedPoint
  std::vector<std::pair<DefoPointCollection::iterator,DefoPoint> >::const_iterator it = pointsAndDistances.begin();
  while( it < pointsAndDistances.end() ) {
    if( ! ( *(it->first) - excludedPoint ).abs() < 0.01 ) { result = it->first; break; }
    ++it;
  }



  // point not too far away from requested position aPoint?
  bool isFound = false;
  if( *result - aPoint < max ) isFound = true;

  return std::pair<bool,DefoPointCollection::iterator>( isFound, result );

} 



///
///
///  
const std::pair<bool,DefoPointCollection::const_iterator> 
DefoRecoSurface::findPointByIndex( DefoPointCollection const& points, std::pair<int,int> const& index ) const {

  DefoPointCollection::const_iterator it = points.begin();
  bool isFound = false;

  for( ; it < points.end(); ++it ) {
    if( index == it->getIndex() ) { isFound = true; break; }
  }

  return std::pair<bool,DefoPointCollection::const_iterator>( isFound, it );

}



///
/// determine and apply a common offset to all spline sets in the field
/// such that the lowermost point has height zero in the end
///
void DefoRecoSurface::removeGlobalOffset( DefoSplineField& splineField ) const {
  
  double minimalHeight = std::numeric_limits<double>::max();

  // first determine the height of the lowermost point
  for( DefoSplineSetXCollection::const_iterator itX = splineField.first.begin(); itX < splineField.first.end(); ++itX ) {
    for( DefoPointCollection::const_iterator itPX = itX->getPoints().begin(); itPX < itX->getPoints().end(); ++itPX ) {
      if( itX->eval( itPX->getX() ) < minimalHeight ) minimalHeight = itX->eval( itPX->getX() );
    }
  }

  // now adjust all the splinesets
  for( DefoSplineSetXCollection::iterator itX = splineField.first.begin(); itX < splineField.first.end(); ++itX ) {
    itX->offset( -1. * minimalHeight );
  }


  minimalHeight = std::numeric_limits<double>::max();
  
  // first determine the height of the lowermost point
  for( DefoSplineSetYCollection::const_iterator itY = splineField.second.begin(); itY < splineField.second.end(); ++itY ) {
    for( DefoPointCollection::const_iterator itPY = itY->getPoints().begin(); itPY < itY->getPoints().end(); ++itPY ) {
      if( itY->eval( itPY->getY() ) < minimalHeight ) minimalHeight = itY->eval( itPY->getY() );
    }
  }


  for( DefoSplineSetYCollection::iterator itY = splineField.second.begin(); itY < splineField.second.end(); ++itY ) {
    itY->offset( -1. * minimalHeight );
  }
  

}



///
/// remove a global tilt (along x or y) of the spline field
///
void DefoRecoSurface::removeTilt( DefoSplineField& ) const {
}


void DefoRecoSurface::dump()
{
    std::cout << std::endl;
    std::cout << "spacingEstimate_ = " << spacingEstimate_ << std::endl;
    std::cout << "searchPathHalfWidth_ = " << searchPathHalfWidth_ << std::endl;
    std::cout << "nominalGridDistance_ = " << nominalGridDistance_ << std::endl;
    std::cout << "nominalCameraDistance_ = " << nominalCameraDistance_ << std::endl;
    std::cout << "nominalViewingAngle_ = " << nominalViewingAngle_ << std::endl;
    std::cout << "heightAboveSensor_ = " << heightAboveSensor_ << std::endl;
    std::cout << "horizontalDistanceToSensor_ = " << horizontalDistanceToSensor_ << std::endl;
    std::cout << "pitchX_ = " << pitchX_ << std::endl;
    std::cout << "pitchY_ = " << pitchY_ << std::endl;
    std::cout << "focalLength_ = " << focalLength_ << std::endl;
    std::cout << "debugLevel_ = " << debugLevel_ << std::endl;
    std::cout << "indexedPoints_ = " << indexedPoints_.size() << std::endl;
}
