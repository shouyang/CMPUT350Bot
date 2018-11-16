#include <math.h>

#include "sc2api/sc2_api.h"
#include "sc2lib/sc2_lib.h"



namespace sc2
{
	Point2D getMapCenter(const ObservationInterface* obs)
	{
		float width = obs->GetGameInfo().width / 2;
		float height = obs->GetGameInfo().height / 2;

		return Point2D(width, height);
	}

	Point2D pointTowards(Point2D cur, Point2D dest)
	{
		float x = dest.x - cur.x;
		float y = dest.y - cur.y;

		x = x / sqrt(x * x + y * y);
		y = y / sqrt(x * x + y * y);

		return Point2D(x, y);
	}

	float distanceTo(Point2D cur, Point2D& dest)
	{
		float x = dest.x - cur.x;
		float y = dest.y - cur.y;

		return sqrt(x * x + y * y);
	}

	

}
