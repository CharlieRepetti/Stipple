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

/*! \mainpage gEDA PCB Stipple Plugin

\section intro_sec Introduction
This <a href="http://sandpiper-inc.com/stipple/src">plugin</a>
is for use with
<a href="http://pcb.geda-project.org/">gEDA PCB</a>.  It provides
cross-hatching for ground planes.  When designing printed circuit
boards, there are two areas where such a feature is of interest.

- <B>Capacitive Sensors</B>   Buttons and Sliders which are etched on the
PCB are designed to measure the ~10pf additional capacitance
introduced by the human touch.   The discharge time of a copper area
is monitored and an event is triggered when the threshold of the
human touch is exceeded.   Having solid ground planes around the sensor
decreases the intrinsic capacitance of the button, reducing overall
discharge time, and thus reduces the signal to noise ratio of the
human touch component.  Hatched ground planes, especially when staggered
with different spacing over the two outer layers of the board, reduce
this effect, but at the expense of ground-return capacity.

- <B>Flexible Circuits</B>    Flexible circuits which contain solid copper
planes loose their flexibility, since the shape memory of the
copper overwhelms the flexibility of the flexible construction.
Hatched planes reduce this shape memory.

\section feature_sec Usage

\subsection Compiling Compiling the Plugin
The plugin is compiled with gcc, and works on Linux, OSX, and Windows.
See the README for details.

\subsection Usage Using the Plugin

\image html Screens.png

In the first screen above, an example of layer groupings is shown. The
"comp-stipple" layer is linked with the "component" layer, while the
"comp-perim" layer is independent.  A single simple polygon is illustrated on
the perimeter layer in this example, but normal use might include any number
of overlapping polygons of most any shape.  The stipple process
calculates insets for pins, pads, and lines, and for a border for the union
of any contiguous region.  Note that all of these structures are flashed,
and no painting of any kind is used.   The UCamco technical note
"Gerber Format Application Note - Painting Considered Harmful" explains the
problem with simply painting lots of lines to achieve the illustrated effect.

The plugin is started using a "sp"
command (after typing ":" to get a PCB command line).   The resulting
dialog box is accepted, and the process executes.  When complete, the
"comp-stipple" layer is populated as illustrated.  Note that the artifacts
that PCB shows along its polygon intersections do not in general appear
in finished boards.  One must examine a finished board under a microscope
to confirm this.   This is because the plugin uses the Boost library for
calculations, rather than the PCB internal polygon routines.

The polygons from a single layer of a pcb are used as a template for
stippling a region.  First, the intersecting polygons are merged, and a
bounding box is drawn around the set of intersected polygons.  This set
is inserted to a second (presumably empty) layer of the pcb.   Then, holes
are poked in the polygons, with bounding boxes around all vias, lines,
and element pads.

\subsection Polygon Rendering within PCB

\image html ButtonClip.png

The image at the left shows a slice taken out of the via anuulus in a PCB
rendered session.   The image on the right, however, is a picture of a board
made from Gerber files exported from this design.  Note that the flashing
does not occur in the finished board as pictured.   In general, the
polygon artifacts rendered by PCB do not necessarily distort the fabricated
board.

\subsection Calc Percent Fill Calculation Method

<img src="PercentFill.png" alt="Percent Fill"
align=left border=0 width=675 height=538>

Some sources (e.g. <a href="http://www.cypress.com/?docID=27113">
Cypress Semiconductor AN2292</a>) describe a hatch pattern
of 7 mil traces with a 70 mil pitch as a ten percent fill.  The method for
calculating the reported percent fill here is different.  Remembering the
formula for the area of a isosceles right triangle (and using symetry):
<div align="left">\f$
Percent\ Fill = 100 \times (1-
\frac{1}{2}(\frac{1}{\sqrt{2}}(Pitch)-\sqrt{2}(Trace))^2\div
\frac{1}{2}(\frac{1}{\sqrt{2}}(Pitch))^2)
\f$</div>

\subsection loop Ground Loops
Although one may construct ground loops through a set of adjoining polygons
with a large hole in the middle, this plugin will fill them.   If this is
really what is desired, one might construct a set of disjoint border polygons
connected with unstippled polygon slices to form an effective ground loop.
Note that in general, ground loops form an antenna of some geometry-dependent
resonant frequency, and are typically avoided in PCB design.

\section author Charles Repetti
This program was written by Charles Repetti, who can be reached on the
gEDA mailing list.
 */
//
// \image html PercentFill.png

/**
 * \file stipple.hpp
 * \brief Data definitions.
 */

#ifndef STIPPLE_HPP_
#define STIPPLE_HPP_

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

extern "C" {
#include "config.h"
#include "global.h"
#include "data.h"
#include "macro.h"
#include "create.h"
#include "remove.h"
#include "hid.h"
#include "error.h"
#include "rtree.h"
#include "polygon.h"
#include "polyarea.h"
#include "assert.h"
#include "strflags.h"
#include "find.h"
#include "misc.h"
#include "draw.h"
#include "undo.h"
}

#include <gtk/gtk.h>

#include <iostream>
#include <fstream>
#include <string>
#include <iterator>
#include <algorithm>
#include <deque>

#include <boost/math/constants/constants.hpp>
#include <boost/polygon/polygon.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/geometry/geometries/geometries.hpp>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>

/// Shorthand for a boost iterator.
#define foreach BOOST_FOREACH

/// Shorthand for a PCB Box.
#define BoxTypePtr 		BoxType *

/// Shorthand for a PCB Layer.
#define LayerTypePtr 	LayerType *

/// Shorthand for a PCB Point.
#define PointTypePtr 	PointType *

/// Shorthand for a PCB Polygon.
#define PolygonTypePtr 	PolygonType *

namespace gtl = boost::polygon;

using namespace std;
using namespace gtl;
using namespace boost::polygon::operators;

/// A shorthand for a set of coordinates from a boost polygon.
typedef gtl::polygon_with_holes_data<int> 						b_polygon;

/// A shorthand for boost points (coordinates).
typedef gtl::polygon_traits<b_polygon>::point_type 				b_point;

/// A shorthand for the boost polygon holes.
typedef gtl::polygon_with_holes_traits<b_polygon>::hole_type 	b_hole;

/// A shorthand for a boost collection of polygons.
typedef std::vector<b_polygon> 									b_polygon_set;

/// Used for rounding edges and drawing approximate circles.
const double PI = boost::math::constants::pi<double>();

/// the PCB name of the component perimeter layer.
const string component_perimeter = "comp-perim";

/// the PCB name of the component stipple layer.
const string component_stipple = "comp-stipple";

/// the PCB name of the sodler perimeter layer.
const string solder_perimeter = "solder-perim";

/// the PCB name of the solder stipple layer.
const string solder_stipple = "solder-stipple";

/// Unit translation: 1 nanometer = .00003... mills.
const double NanometerToMil = 3.93700787E10-5;

/// Unit translation: 1 mil (1/1000 of an inch) = 254 nanometers.
const Coord MilToNanometer = 254;

/// The dialog box is on its own thread, so a cancel request is signaled
/// by setting this variable.
extern bool Cancel;

extern Coord
	/// The size trace to be used in stipples on the component layer
	ComponentTrace,
	/// The size trace to be used in stipples on the solder layer
	SolderTrace,
	/// The pitch size (spacing) to be used on the component layer
	ComponentPitch,
	/// The pitch size (spacing) to be used on the solder layer
	SolderPitch;

/// These correspond to the work order filled in by the operator in the dialog.
enum MakeLayers_t
{ MakeTopLayer, MakeBottomLayer, MakeBothLayers, MakeSelected, MakeDelete };

/// The checkbox translation of the work order.
extern MakeLayers_t MakeLayers;

/// A list of layer names to be stippled.
extern vector<string> MakeLayerNames;

/// The user interface.
void ParameterDialog();

/// Since the dialog runs on the PCB GUI thread, a new spool thread is
/// used to delegate all of the worker layer threads.  This allows a
/// "cancel" button to remain active in the dialog as the sometimes
/// lengthy stipple threads do their work.
void MakeAllLayers();

/// Since Gnome threads can not use a C++ decorated function as an
/// entry point, this serves as a thunk to the Layer worker class
void LayerFactory(int i);

/// A simple log print to stout
void Log(const char *format, ...);

/// This loop replaces POLYGON_LOOP(layer);
/// The loop had to unrolled from its macro because of the
/// rvalue typecast on pcb_polygon assignment
#define POLYGON_LP(layer) do {                                      \
  GList *__iter, *__next;                                           \
  Cardinal n = 0;                                                   \
  for (__iter = (layer)->Polygon, __next = g_list_next (__iter);    \
       __iter != NULL;                                              \
       __iter = __next, __next = g_list_next (__iter), n++) {       \
    PolygonType *polygon = (PolygonType *)__iter->data;

/// This loop replaces POLYGONPOINT_LOOP(layer);
/// The loop had to unrolled from its macro because of the
/// rvalue typecast on pcb_polygon assignment
#define	POLYGONPOINT_LP(polygon) do	{			\
	int				n;							\
	PointTypePtr	point;						\
	for (n = (int)((polygon)->PointN) - 1; n != -1; n--)	\
	{											\
		point = &(polygon)->Points[n]

/// This loop replaces VIA_LOOP(layer);
/// The loop had to unrolled from its macro because of the
/// rvalue typecast on pcb_polygon assignment
#define VIA_LP(top) do {                                            \
  GList *__iter, *__next;                                           \
  Cardinal n = 0;                                                   \
  for (__iter = (top)->Via, __next = g_list_next (__iter);          \
       __iter != NULL;                                              \
       __iter = __next, __next = g_list_next (__iter), n++) {       \
    PinType *via = (PinType *)__iter->data;

/// This loop replaces LINE_LOOP(layer);
/// The loop had to unrolled from its macro because of the
/// rvalue typecast on pcb_polygon assignment
#define LINE_LP(layer) do {                                         \
  GList *__iter, *__next;                                           \
  Cardinal n = 0;                                                   \
  for (__iter = (layer)->Line, __next = g_list_next (__iter);       \
       __iter != NULL;                                              \
       __iter = __next, __next = g_list_next (__iter), n++) {       \
    LineType *line = (LineType *)__iter->data;

/// This loop replaces ELEMENT_LOOP(layer);
/// The loop had to unrolled from its macro because of the
/// rvalue typecast on pcb_polygon assignment
#define ELEMENT_LP(top) do {                                        \
  GList *__iter, *__next;                                           \
  Cardinal n = 0;                                                   \
  for (__iter = (top)->Element, __next = g_list_next (__iter);      \
       __iter != NULL;                                              \
       __iter = __next, __next = g_list_next (__iter), n++) {       \
    ElementType *element = (ElementType *)__iter->data;

/// This loop replaces PAD_LOOP(layer);
/// The loop had to unrolled from its macro because of the
/// rvalue typecast on pcb_polygon assignment
#define PAD_LP(element) do {                                        \
  GList *__iter, *__next;                                           \
  Cardinal n = 0;                                                   \
  for (__iter = (element)->Pad, __next = g_list_next (__iter);      \
       __iter != NULL;                                              \
       __iter = __next, __next = g_list_next (__iter), n++) {       \
    PadType *pad = (PadType *)__iter->data;

/// This loop replaces PIN_LOOP(layer);
/// The loop had to unrolled from its macro because of the
/// rvalue typecast on pcb_polygon assignment
#define PIN_LP(element) do {                                        \
  GList *__iter, *__next;                                           \
  Cardinal n = 0;                                                   \
  for (__iter = (element)->Pin, __next = g_list_next (__iter);      \
       __iter != NULL;                                              \
       __iter = __next, __next = g_list_next (__iter), n++) {       \
    PinType *pin = (PinType *)__iter->data;

#endif /* STIPPLE_HPP_ */

/// A single boost polygon with all of it's cutouts.
class StippledPolygon
{
	public:

		/// Store the perimeter of a stippled region
		b_polygon Outline;

		/// Cutouts are the holes in the regions
		b_polygon_set CutOuts;

		/// Overlays are solid shadows for lines, vias and pads
		/// which from (with clearance) featured borders within
		/// stippled areas.
		b_polygon_set Overlays;
};

/// The main user interface.
class StippleDialog  {

private:

	/// Return the last-used individual parameter.
	long ReadDefault(string File, string Key, Coord Default);

	/// Populate the dialog with sane values.
	bool ReadDefaults();

public:

	/// OK/Cancel listeners
	static void ButtonPress( GtkButton *widget, gpointer data );

	/// Each time a key is typed the percent fill is updated
	static void KeyPress( GtkButton *widget, gpointer data );

	/// Receive a percent complete message.
	static gboolean UpdateProgress(GtkProgressBar *PB);

	/// Interface for setting progress bar
	static void Progress(float progress, string Message);

	/// Return the percent fill represented by the input parameters
	static int PercentFill(double Trace, double Pitch);

	/// The single dialog this add-in uses for creating a work order.
	void ParameterDialog();

};

/// Worker thread for a single layer's stipple processing.
class Layer
{

protected:

	/// Return the angle between two points on a plane.
	double Angle2D(
			int X0, int Y0,
			int X1, int Y1);

	/// Loop through the layer names from the host program and find the one
	/// which matches the supplied name, or return NULL.
	LayerTypePtr FindLayerByName(string Name);

	/// Using a finite number of line segments, approximate a circle.
	b_polygon MakeCircularOverlay(
			Coord x, Coord y, Coord Radius, int SegmentCount = 24);

	/// Remove a square inset from a polygon.
	b_polygon MakeRectangularOverlay(
			Coord x0, Coord y0, Coord x1, Coord y1, Coord Thickness);

	/// Make a rounded rectangle, using line segments to approximate the
	/// rounded corners.
	b_polygon MakeRoundedRectangle(
			int x0, int y0, int x1, int y1, int Radius, int Smoothness);

	/// Read and store all polygons on the template layer.
	b_polygon_set ReadTemplatePolygons(LayerTypePtr layer);

	/// Read all the keep-out information for the layer, which are all pins,
	/// pads, vias and lines.
	b_polygon_set LoadPCB(string LayerName, Coord Trace);

	/// Form a minimum set of unions which cover all of the polygons from the
	/// template layer, and where each union is the largest island which can
	/// be formed.  Thus, the result is a set of possibly one non-contiguous
	/// polygons, which will be the target area for polygon subdivision.
	/// Then hatch the union(s), shrinking the edges for a border and intersecting
	/// each inset with the assembled union to allow for any shape of bounding
	/// region.  It is the intersection of each diamond inlay with its enclosing
	/// polygon union which accounts for the glacial run-time of this add-in.
	vector<StippledPolygon> CalculateStipples(
			LayerTypePtr layer, b_polygon_set Union,
			Coord Trace, Coord Pitch, int i);

	/// Once all of the new polygons have been calculated using Boost
	/// polygons, convert them to a PCB data structure.
	void InsertToPCB(
			LayerTypePtr layer, vector<StippledPolygon> StippledPolygons);

public:

	/// Insert the new stippled polygons into the PCB program using
	/// the published interface. A Gnome Mutex is used since each layer
	/// runs in its own thread, and insertion could result in collisions.
	void MakeLayer(int i);
};


