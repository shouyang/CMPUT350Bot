#pragma once

namespace sc2 
{
	Point2D getMapCenter(const ObservationInterface* obs);
	Point2D pointTowards(Point2D cur, Point2D dest);
	double distanceTo(Point2D cur, Point2D& dest);

}

