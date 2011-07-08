/*
 * Moon.cpp
 *
 * Part of Fly! Legacy project
 *
 * Copyright 2003 Chris Wallace
 *
 * Fly! Legacy is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 * Fly! Legacy is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 *   along with Fly! Legacy; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*! \file Moon.cpp
 *  \brief Implements CMoon ephemeris calculator for Earth's moon
 *
 *  Implements CMoon ephemeris calculator for Earth's moon.
 */


#include <math.h>
#include "../Include/Ephemeris.h"
#include "../Include/Utility.h"


// Default orbital elements
static SOrbitalElements first =
{
  125.1228,   // N, Longitude of the ascending node
  5.1454,     // i, Inclination to the ecliptic
  318.0634,   // w, Argument of perihelion
  60.266600,    // a, Semi-major axis
  0.054900,   // e, Eccentricity
  115.3654    // M, Mean anomaly
};

static SOrbitalElements second =
{
  -0.0529538083,  // N, Longitude of the ascending node
  0.0,      // i, Inclination to the ecliptic
  0.1643573223, // w, Argument of perihelion
  0.0,      // a, Semi-major axis
  0.0,      // e, Eccentricity
  13.0649929509 // M, Mean anomaly
};


/*************************************************************************
 * CMoon::CMoon(double mjd)
 * Public constructor for class CMoon. Initializes the orbital elements
 * Argument: The current time.
 * the hard coded default orbital elements for CMoon are passed to 
 * CelestialBody::CelestialBody();
 ************************************************************************/
CMoon::CMoon(double mjd) :
  CCelestialBody (first.N, first.i, first.w, first.a, first.e, first.M,
                second.N, second.i, second.w, second.a, second.e, second.M,
          mjd)
{
}

CMoon::CMoon() :
  CCelestialBody (first.N, first.i, first.w, first.a, first.e, first.M,
                second.N, second.i, second.w, second.a, second.e, second.M)
{
}

CMoon::~CMoon()
{
}


/*****************************************************************************
 * void CMoon::updatePosition(double mjd, CSol *ourSun)
 * this member function calculates the actual topocentric position (i.e.) 
 * the position of the moon as seen from the current position on the surface
 * of the earth.
 ****************************************************************************/
void CMoon::UpdatePosition (double mjd, double lst, double lat, CSol *ourSun)
{
  // Update orbital elements
  double 
    eccAnom, ecl, actTime,
    xv, yv, v, r, xh, yh, zh, xg, yg, zg, xe, ye, ze,
    Ls, Lm, D, F, mpar, gclat, rho, HA, g,
    geoRa, geoDec;
  
  UpdateOrbElements(mjd);
  actTime = CalcActTime(mjd);

  // calculate the angle between ecliptic and equatorial coordinate system
  // in Radians
  ecl = ((SGD_DEGREES_TO_RADIANS * 23.4393) - (SGD_DEGREES_TO_RADIANS * 3.563E-7) * actTime);  
  eccAnom = CalcEccAnom(M, e);  // Calculate the eccentric anomaly
  xv = a * (cos(eccAnom) - e);
  yv = a * (sqrt(1.0 - e*e) * sin(eccAnom));
  v = atan2(yv, xv);               // the moon's true anomaly
  r = sqrt (xv*xv + yv*yv);       // and its distance
  
  // estimate the geocentric rectangular coordinates here
  xh = r * (cos(N) * cos (v+w) - sin (N) * sin(v+w) * cos(i));
  yh = r * (sin(N) * cos (v+w) + cos (N) * sin(v+w) * cos(i));
  zh = r * (sin(v+w) * sin(i));

  // calculate the ecliptic latitude and longitude here
  lonEcl = atan2 (yh, xh);
  latEcl = atan2(zh, sqrt(xh*xh + yh*yh));

  /* Calculate a number of perturbatioin, i.e. disturbances caused by the 
   * gravitational infuence of the sun and the other major planets.
   * The largest of these even have a name */
  Ls = ourSun->getM() + ourSun->getw();
  Lm = M + w + N;
  D = Lm - Ls;
  F = Lm - N;
  
  lonEcl += SGD_DEGREES_TO_RADIANS * (-1.274 * sin (M - 2*D)
        +0.658 * sin (2*D)
        -0.186 * sin(ourSun->getM())
        -0.059 * sin(2*M - 2*D)
        -0.057 * sin(M - 2*D + ourSun->getM())
        +0.053 * sin(M + 2*D)
        +0.046 * sin(2*D - ourSun->getM())
        +0.041 * sin(M - ourSun->getM())
        -0.035 * sin(D)
        -0.031 * sin(M + ourSun->getM())
        -0.015 * sin(2*F - 2*D)
        +0.011 * sin(M - 4*D)
        );
  latEcl += SGD_DEGREES_TO_RADIANS * (-0.173 * sin(F-2*D)
        -0.055 * sin(M - F - 2*D)
        -0.046 * sin(M + F - 2*D)
        +0.033 * sin(F + 2*D)
        +0.017 * sin(2*M + F)
        );
  r += (-0.58 * cos(M - 2*D)
  -0.46 * cos(2*D)
  );

  xg = r * cos(lonEcl) * cos(latEcl);
  yg = r * sin(lonEcl) * cos(latEcl);
  zg = r *               sin(latEcl);
  
  xe = xg;
  ye = yg * cos(ecl) -zg * sin(ecl);
  ze = yg * sin(ecl) +zg * cos(ecl);

  geoRa  = atan2(ye, xe);
  geoDec = atan2(ze, sqrt(xe*xe + ye*ye));

  // Given the moon's geocentric ra and dec, calculate its 
  // topocentric ra and dec. i.e. the position as seen from the
  // surface of the earth, instead of the center of the earth

  // First calculate the moon's parallax, that is, the apparent size of the 
  // (equatorial) radius of the earth, as seen from the moon 
  mpar = asin ( 1 / r);

  gclat = lat - 0.003358 * 
      sin (2 * SGD_DEGREES_TO_RADIANS * lat );

  rho = 0.99883 + 0.00167 * cos(2 * SGD_DEGREES_TO_RADIANS * lat);
  
  if (geoRa < 0)
    geoRa += (2*SGD_PI);
  
  HA = lst - (3.8197186 * geoRa);

  g = atan (tan(gclat) / cos ((HA / 3.8197186)));

  rightAscension = geoRa - mpar * rho * cos(gclat) * sin(HA) / cos (geoDec);
  if (fabs(lat) > 0) {
      declination
    = geoDec - mpar * rho * sin (gclat) * sin (g - geoDec) / sin(g);
  } else {
      declination = geoDec;
  }

  // Update age (days since last new moon)
  age = (float)(LUNATION_DAYS * WrapTwoPi (GetLon() - ourSun->GetLon()) / (SGD_PI * 2.0));
}


float CMoon::GetAge (void)
{
  return age;
}

/**************************************************************************
 * moonpos.cxx
 * Written by Durk Talsma. Originally started October 1997, for distribution  
 * with the FlightGear project. Version 2 was written in August and 
 * September 1998. This code is based upon algorithms and data kindly 
 * provided by Mr. Paul Schlyter. (pausch@saaf.se). 
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA  02111-1307, USA.
 *
 **************************************************************************/

