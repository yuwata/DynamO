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

#pragma once
#include <magnet/intersection/polynomial.hpp>
#include <magnet/math/vector.hpp>

namespace magnet {
  namespace intersection {
    namespace detail {
      struct QuarticFunc
      {
      public:
	inline double operator()(double t) const
	{
	  return (((coeffs[0] * t + coeffs[1]) * t + coeffs[2]) * t + coeffs[3]) * t + coeffs[4];
	}
	
	double coeffs[5];
      };
    }

    /*! \brief A parabolic(ray)-sphere intersection test with backface culling.
      
      \param T The origin of the ray relative to the sphere center.
      \param D The direction/velocity of the ray.
      \param A The acceleration of the ray.
      \param r The radius of the sphere.
      \return The time until the intersection, or HUGE_VAL if no intersection.
    */
    template<bool inverse = false>
    inline double parabola_sphere(const math::Vector& R, const math::Vector& V, const math::Vector& A, const double& r)
    {
      const double f0 = R.nrm2() - r * r;
      const double f1 = 2 * (V | R);
      const double f2 = 2 * (V.nrm2() + (A | R));
      const double f3 = 6 * (A | V);
      const double f4 = 6 * A.nrm2();
      if (inverse)
	return detail::fourthOrder(-f0, -f1, -f2, -f3, -f4, r * r);
      else
	return detail::fourthOrder(+f0, +f1, +f2, +f3, +f4, r * r);
    }
  }
}
