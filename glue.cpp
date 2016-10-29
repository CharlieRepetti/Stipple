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
 * \file glue.cpp
 * \brief The functions which require undecorated "C" access.
 */

#include "stipple.hpp"

extern "C" {
	static int
	Stipple (int argc, char **argv, Coord x, Coord y)
	{
		Log("\nStipple Plugin Begins\n");

		StippleDialog Stippler;
		Stippler.ParameterDialog();
		return 0;
	}

	static HID_Action stipple_action_list[] = {
	  { (char *)"sp", "???", Stipple, NULL, NULL}
	};

	REGISTER_ACTIONS (stipple_action_list)

	/// The entry point for this addin.
	void pcb_plugin_init()
	{
	  register_stipple_action_list();
	}
}

void Log(const char *format, ...)
{
    char Message[128];

    va_list args;
    va_start(args, format);
    vsprintf(Message, format, args);
    va_end(args);
    printf("%s", Message);
}


void LayerFactory(int i)
{
	Layer L;
	L.MakeLayer(i);
}

void MakeAllLayers()
{
	time_t StartTime, EndTime, ElapsedTime;

	gpointer *LayerThreads;

	time(&StartTime);

	StippleDialog::Progress(0.05, "Polygon Stipple Begins...");
	if (Cancel)  {
		return;
	}

	if (NULL == PCB)  {
		Log("Error: No PCB Interface!\n");
		return;
	}

	LayerThreads = (gpointer *)
				malloc(MakeLayerNames.size() * sizeof(gpointer));

	for (unsigned long i = 0; i < MakeLayerNames.size(); i++)  {

		// Layer(i);
		LayerThreads[i] =
				g_thread_new("Stipple Thread",
						(GThreadFunc)LayerFactory, (gpointer)i);
	}

	for (unsigned long i = 0; i < MakeLayerNames.size(); i++)  {
		g_thread_join((GThread *)LayerThreads[i]);
	}
	free(LayerThreads);

	time(&EndTime);
	ElapsedTime = (long)difftime(EndTime, StartTime);
	Log("Stipple Plugin Ends: Elapsed Time is %02d:%02d:%02d\n",
		(ElapsedTime % (60 * 60 * 60)) / (60 * 60),
		(ElapsedTime % (60 * 60)) / 60,
		 ElapsedTime % 60);

	PCB->Changed = TRUE;
	StippleDialog::Progress(2.0, "Polygon Stipple Ends");
}
