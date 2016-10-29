/*
 *                            COPYRIGHT
 *
 *  Stipple, cross hatching add-in for gEDA PCB
 *  Copyright (C) 2015 Charles Repetti
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
*/

/**
 * \file stipple.cpp
 * \brief The worker thread for processing an individual layer.
 */

#include "stipple.hpp"
#include <time.h>

Coord ComponentTrace, SolderTrace, ComponentPitch, SolderPitch;
MakeLayers_t MakeLayers;
vector<string> MakeLayerNames;

double
Layer::Angle2D(
		int X0, int Y0,
		int X1, int Y1)
{
	return atan2((double)(X1 - X0),  (double)(Y1 - Y0));
}

LayerTypePtr
Layer::FindLayerByName(string Name)  {

	LAYER_LOOP (PCB->Data, max_copper_layer);
	{
		for (unsigned int i = 0; i < MakeLayerNames.size(); i++)  {
			if (!Name.compare(layer->Name)) {
				return layer;
			}
		}
	}
	END_LOOP;
	return NULL;
}

b_polygon
Layer::MakeCircularOverlay(
		Coord x, Coord y, Coord Radius, int SegmentCount)
{
	double dTheta = PI/SegmentCount/2;
	deque<b_point> EdgeSet;
	b_polygon Overlay;

	EdgeSet.clear();
	for (double iTheta = -PI; iTheta <= PI; iTheta += dTheta)  {
		EdgeSet.push_back(gtl::construct<b_point>(
				x + Radius * cos(iTheta),
				y + Radius * sin(iTheta)));
	}
	Overlay.set(EdgeSet.begin(), EdgeSet.end());
	return Overlay;
}

b_polygon
Layer::MakeRectangularOverlay(
		Coord x0, Coord y0, Coord x1, Coord y1, Coord Thickness)
{

	b_polygon Overlay;
	deque<b_point> EdgeSet;

	double Theta = Angle2D(x0, y0, x1, y1);
	int dx = Thickness * sin(Theta + PI/2.0);
	int dy = Thickness * cos(Theta + PI/2.0);

	EdgeSet.clear();
	EdgeSet.push_back(gtl::construct<b_point>(x0 + dx, y0 + dy));
	EdgeSet.push_back(gtl::construct<b_point>(x0 - dx, y0 - dy));
	EdgeSet.push_back(gtl::construct<b_point>(x1 - dx, y1 - dy));
	EdgeSet.push_back(gtl::construct<b_point>(x1 + dx, y1 + dy));
	EdgeSet.push_back(gtl::construct<b_point>(x0 + dx, y0 + dy));

	Overlay.set(EdgeSet.begin(), EdgeSet.end());
	return Overlay;
}

b_polygon
Layer::MakeRoundedRectangle(
		int x0, int y0, int x1, int y1, int Radius, int Smoothness)
{
	b_polygon Overlay;
	deque<b_point> EdgeSet;

 	double dTheta = PI/Smoothness/8;

	// Top Edge
	EdgeSet.push_back(gtl::construct<b_point>(x0 + Radius, y0));
	EdgeSet.push_back(gtl::construct<b_point>(x1 - Radius, y0));

	// Top Right Corner
	for (double iTheta = PI/2; iTheta <= PI; iTheta += dTheta)  {
			EdgeSet.push_back(gtl::construct<b_point>(
					(x1 - Radius) - Radius * cos(iTheta),
					(y0 + Radius) - Radius * sin(iTheta)));
	}

	// Right Edge
	EdgeSet.push_back(gtl::construct<b_point>(x1, y0 + Radius));
	EdgeSet.push_back(gtl::construct<b_point>(x1, y1 - Radius));

	// Bottom Right Corner
	for (double iTheta = 0; iTheta >= -PI/2; iTheta -= dTheta)  {
			EdgeSet.push_back(gtl::construct<b_point>(
					(x1 - Radius) + Radius * cos(iTheta),
					(y1 - Radius) - Radius * sin(iTheta)));
	}

	// Bottom Edge
	EdgeSet.push_back(gtl::construct<b_point>(x1 - Radius, y1));
	EdgeSet.push_back(gtl::construct<b_point>(x0 + Radius, y1));

	// Bottom Left Corner
	for (double iTheta = -PI/2; iTheta <= 0; iTheta += dTheta)  {
		EdgeSet.push_back(gtl::construct<b_point>(
			(x0 + Radius) - Radius * cos(iTheta),
			(y1 - Radius) - Radius * sin(iTheta)));
	}

	EdgeSet.push_back(gtl::construct<b_point>(x0, y1 - Radius));
	EdgeSet.push_back(gtl::construct<b_point>(x0, y0 + Radius));

	// Top Left Corner
	for (double iTheta = 0; iTheta <= PI/2; iTheta += dTheta)  {
		EdgeSet.push_back(gtl::construct<b_point>(
			(x0 + Radius) - Radius * cos(iTheta),
			(y0 + Radius) - Radius * sin(iTheta)));
	}

	Overlay.set(EdgeSet.begin(), EdgeSet.end());
	return Overlay;
}

b_polygon_set
Layer::ReadTemplatePolygons(LayerTypePtr layer)
{
	bool FirstPoint;
	int PCnt = 0;
	Coord x0 = 0, y0 = 0;
	std::deque<b_point> EdgeSet;
	vector<b_polygon> PolygonSet;

	PolygonSet.clear();

	POLYGON_LP(layer);
	{
		if (Cancel)  {
			return PolygonSet;  }

		if (MakeSelected == MakeLayers &&
			!TEST_FLAG (SELECTEDFLAG, polygon))  {
			continue;
		}

		EdgeSet.clear();
		FirstPoint = true;

		POLYGONPOINT_LP(polygon);
		{
			if (FirstPoint)  {
				x0 = point->X; y0 = point->Y;
				FirstPoint = false;
			}

			// Insert to the head of the list to achieve
			// counterclockwise winding
			EdgeSet.push_back(gtl::construct
					<b_point>(point->X, point->Y));
		}
		END_LOOP;

		// Close the Polygon Set for correct Boost operation
		EdgeSet.push_back(gtl::construct<b_point>(x0, y0));

		b_polygon Polygon;
		Polygon.set(EdgeSet.begin(), EdgeSet.end());
		PolygonSet.push_back(Polygon);
		++PCnt;
	}
	END_LOOP;

	return PolygonSet;
}

b_polygon_set
Layer::LoadPCB(string LayerName, Coord Trace)
{
	LayerTypePtr layer;
	b_polygon_set OverlayEdgeSet;

	VIA_LP(PCB->Data);  {
		OverlayEdgeSet.push_back( MakeCircularOverlay(via->X, via->Y,
				Trace + (via->Thickness + via->Clearance)/(Coord)2));
	}
	END_LOOP;

	if	((( MakeTopLayer 	== MakeLayers ||
			MakeBothLayers 	== MakeLayers ||
			MakeSelected 	== MakeLayers)
			&& LayerName 	== component_stipple &&
			NULL != (layer = FindLayerByName("component"))) ||
		((	MakeBottomLayer	== MakeLayers ||
			MakeBothLayers 	== MakeLayers ||
			MakeSelected 	== MakeLayers)
			&& LayerName 	== solder_stipple &&
			NULL != (layer = FindLayerByName("solder"))))  {

		// Handle each line on the layer, as an area without holes
		LINE_LP(layer);
		{
			Coord Thickness  = Trace +
					(line->Thickness + line->Clearance)/ (Coord)2;

			// Add a bloated polygon hole right over the line...
			OverlayEdgeSet.push_back( MakeRectangularOverlay(
					line->Point1.X, line->Point1.Y,
					line->Point2.X, line->Point2.Y, Thickness));

			// ...and add two barbells at the ends of the line
			OverlayEdgeSet.push_back( MakeCircularOverlay(
					line->Point1.X, line->Point1.Y, Thickness));

			OverlayEdgeSet.push_back( MakeCircularOverlay(
					line->Point2.X, line->Point2.Y, Thickness));
		}
		END_LOOP;
	}

	// Each Pad's Coordinates are relative to the element's mark, which
	// is where the component was placed on the layout.
	gtl::rectangle_data<Coord> Extents;
	b_polygon_set Pads;

	// No holes may be placed in Elements
	ELEMENT_LP(PCB->Data);
	{
		if	((LayerName == component_stipple && FRONT(element)) ||
			 (LayerName == solder_stipple && !FRONT(element)))  {

			// Pads for this element
			PAD_LP(element);
			{
				Pads.clear();
				Coord Clear = Trace + pad->Thickness/2 + pad->Clearance/2;
				Pads += rectangle_data<Coord>(
						pad->Point1.X - Clear,
						pad->Point1.Y - Clear,
						pad->Point2.X + Clear,
						pad->Point2.Y + Clear);

				extents(Extents, Pads);

				OverlayEdgeSet.push_back(
						MakeRoundedRectangle(
							xl(Extents), yl(Extents), xh(Extents), yh(Extents),
							Trace + pad->Clearance/2, 8));
			}
			END_LOOP;
			Pads.clear();
		}

		// Pins for this element are on both sides
		PIN_LP(element);
		{
			OverlayEdgeSet.push_back( MakeCircularOverlay(pin->X, pin->Y,
					Trace + (pin->Thickness + pin->Clearance)/(Coord)2));
		}
		END_LOOP;
	}
	END_LOOP;
	return OverlayEdgeSet;
}

vector<StippledPolygon>
Layer::CalculateStipples(
		LayerTypePtr layer, b_polygon_set Union,
		Coord Trace, Coord Pitch, int i)
{
	int PCnt;
	vector<b_hole> H;
	b_polygon Diamond;
	b_polygon_set Stipple, Container, IntersectionSet, ComponentSet;

	// Cypress refers to a 7 mil line with a 7 mil spacing as a 10% fill
	Coord Dx_Line = Trace * sqrt(2);
	Coord Dx_Hole = (Pitch - Trace) * sqrt(2);

	gtl::rectangle_data<Coord> Extents;

	StippledPolygon AddStippledPolygon;
	vector<StippledPolygon> StippledPolygons;

	ComponentSet = LoadPCB(layer->Name, Trace);
	PCnt = 0;

	foreach(b_polygon ThisPolygon, Union) {

		string ProgressMessage;
		ProgressMessage = str( boost::format(
				"Area %d of %ld for \"%s\"...") %
				(PCnt+1) % Union.size() % layer->Name);

		bool EveryOther = true;
		boost::polygon::extents(Extents, ThisPolygon);
		Container += ThisPolygon;

		Coord Dx, Dy, X, Y;

		// Set up the bounding rectangle for the unionized set.
		// Shrink it to expose the perimeter and to expose a margin
		// around each cut-out used to outline the pattern.
		Container -= (int)Trace;
		Dx = Dx_Line + Dx_Hole;
		Dy = Dx;
		Y = Dy * (yl(Extents) / Dy);

		while (Y < yh(Extents) + Dy) {

			if (Cancel)  {
				return StippledPolygons;
			}

			// No look-ahead on the progress estimate, just a fraction of
			// the layers, polygons within the layers, and loop iteration.
			// About all that can be said in this expression's favor is that
			// it doesn't ever go backwards.
			StippleDialog::Progress(0.05 + (0.95 *
				(float)i / (float)MakeLayerNames.size() +
				1.0 / (float)MakeLayerNames.size() *
				(float)PCnt / (float)Union.size() +
				1.0 / (float)MakeLayerNames.size() * 1.0 / (float)Union.size() *
				((float)(Y - yl(Extents)) /
				((float)yh(Extents) - (float)yl(Extents) + (float)Dy))),
				ProgressMessage);

			// ping-pong to inset the squares to form a mosaic pattern
			X = Dx * (xl(Extents) / Dx);
			if (EveryOther) {
				X -= Dx / 2;
				EveryOther = false;
			} else {
				EveryOther = true;
			}
			while (X < xh(Extents) + Dx) {

				if (Cancel)  {
					return StippledPolygons;
				}

				b_point DiamondPoints[] = {
					gtl::construct<b_point>(X, Y-Dx_Hole / 2), // Top
					gtl::construct<b_point>(X+Dx_Hole/2, Y),   // Right
					gtl::construct<b_point>(X, Y+Dx_Hole/2),   // Bottom
					gtl::construct<b_point>(X-Dx_Hole/2, Y) }; // Left

				gtl::set_points(Diamond, DiamondPoints, DiamondPoints + 4);
				Stipple += Diamond;	// This is the expensive operation
				X += Dx;
			}
			Y += Dy/2;
		}

		// Intersect all the stipples with the container
		IntersectionSet += Stipple & Container;

		AddStippledPolygon.Outline = ThisPolygon;
		AddStippledPolygon.CutOuts = IntersectionSet;

		foreach(b_polygon ThisComponent, ComponentSet)  {
			AddStippledPolygon.Overlays += ThisComponent * ThisPolygon;
		}

		StippledPolygons.push_back(AddStippledPolygon);

		Stipple.clear();
		Container.clear();
		IntersectionSet.clear();
		++PCnt;
	}
	return StippledPolygons;
}

void
Layer::InsertToPCB(
		LayerTypePtr layer, vector<StippledPolygon> StippledPolygons)
{

	if (MakeSelected != MakeLayers)  {
		POLYGON_LP(layer);
		{
			ErasePolygon(polygon);
			MoveObjectToRemoveUndoList (POLYGON_TYPE, layer, polygon, polygon);
		}
		END_LOOP;
	}

	foreach(StippledPolygon ThisPolygon, StippledPolygons) {

		PolygonTypePtr NewPolygon =
				// FULLPOLYFLAG would make bisection of stippled areas occur.
				CreateNewPolygon (layer, MakeFlags(CLEARPOLYFLAG));

		// Skip the redundant start point boost required
		for (polygon_traits<b_polygon>::iterator_type iPoint =
				ThisPolygon.Outline.begin();
				iPoint+1 != ThisPolygon.Outline.end();
				++iPoint)  {
			CreateNewPointInPolygon (NewPolygon,
					gtl::x(*iPoint), gtl::y(*iPoint));
		}

		foreach(b_polygon Intersection, ThisPolygon.CutOuts) {

			CreateNewHoleInPolygon(NewPolygon);

			// The first point is repeated by the intersection
			// operator, so is not added in.
			for (polygon_traits<b_polygon>::iterator_type iPoint =
					Intersection.begin();
					iPoint != Intersection.end(); ++iPoint) {

				CreateNewPointInPolygon (NewPolygon,
						gtl::x(*iPoint), gtl::y(*iPoint));
			}
		}

		SetPolygonBoundingBox (NewPolygon);
		if (!layer->polygon_tree)
			layer->polygon_tree = r_create_tree (NULL, 0, 0);
		r_insert_entry (layer->polygon_tree,
				(BoxTypePtr) NewPolygon, 0);
		AddObjectToCreateUndoList (
				POLYGON_TYPE, layer, NewPolygon, NewPolygon);
	}

	// Again for overlays for lines, vias and pads.
	foreach(StippledPolygon ThisPolygon, StippledPolygons) {

		foreach(b_polygon Overlay, ThisPolygon.Overlays) {

			PolygonTypePtr NewPolygon =
					CreateNewPolygon (layer, MakeFlags(FULLPOLYFLAG | CLEARPOLYFLAG));

			// The first point is repeated by the intersection
			// operator, so is not added in.
			for (polygon_traits<b_polygon>::iterator_type iPoint =
					Overlay.begin();
					iPoint != Overlay.end(); ++iPoint) {

				CreateNewPointInPolygon (NewPolygon,
						gtl::x(*iPoint), gtl::y(*iPoint));
			}

			SetPolygonBoundingBox (NewPolygon);
			if (!layer->polygon_tree)
				layer->polygon_tree = r_create_tree (NULL, 0, 0);
			r_insert_entry (layer->polygon_tree,
				(BoxTypePtr) NewPolygon, 0);
			AddObjectToCreateUndoList (
					POLYGON_TYPE, layer, NewPolygon, NewPolygon);
		}
	}
}

void
Layer::MakeLayer(int i)
{
	Coord Trace, Pitch;
	LayerTypePtr layer;

	b_polygon_set Union;
	vector<b_polygon> PolygonSet;
	vector<StippledPolygon> StippledPolygons;

	static GMutex mutex;

	if (MakeDelete == MakeLayers)  {

		if (MakeLayerNames[i] == component_perimeter)  {
			MakeLayerNames[i] = component_stipple;
		} else if (MakeLayerNames[i] == solder_perimeter) {
			MakeLayerNames[i] = solder_stipple;
		}

		if	(NULL != (layer = FindLayerByName(MakeLayerNames[i])))  {
			POLYGON_LP(layer);
			{
				ErasePolygon(polygon);
				MoveObjectToRemoveUndoList (
						POLYGON_TYPE, layer, polygon, polygon);
			}
			END_LOOP;
		}
		return;
	}

	if	(NULL != (layer = FindLayerByName(MakeLayerNames[i])))  {

		PolygonSet.clear();
		Union.clear();
		StippledPolygons.clear();

		PolygonSet = ReadTemplatePolygons(layer);
		if (Cancel)  {
			return;
		}

		// Merge overlapping polygons so a perimeter may be drawn around
		// each individual island despite overlaps.
		foreach(b_polygon Polygon, PolygonSet) {
			Union |= Polygon;
		}

		if (MakeLayerNames[i] == component_perimeter)  {

			Trace = ComponentTrace;
			Pitch = ComponentPitch;
			MakeLayerNames[i] = component_stipple;

		} else if (MakeLayerNames[i] == solder_perimeter) {

			Trace = SolderTrace;
			Pitch = SolderPitch;
			MakeLayerNames[i] = solder_stipple;
		}

		if	(NULL != (layer = FindLayerByName(MakeLayerNames[i])))  {

			StippledPolygons =
					CalculateStipples(layer, Union, Trace, Pitch, i);

			g_mutex_lock (&mutex);
			InsertToPCB(layer, StippledPolygons);
			g_mutex_unlock (&mutex);
		}
	}
}

