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
 * \file dialog.cpp
 * \brief The user data entry screen, and the thread launcher.
 */

#include "stipple.hpp"

bool Cancel;

static GtkWidget *dialog, *ProgressLabel,
*TopLayer, *BottomLayer, *BothLayers, *SelectedPolygons, *DeletePolygons,
*TopTraceEdit, *TopPitchEdit,
*BottomTraceEdit, *BottomPitchEdit, *PercentFillMessage;

static GtkProgressBar *ProgressBar;

static float percent_progress = 0.0;
static string ProgressMessage = "Stipple Progress";

void
StippleDialog::Progress(float progress, string Message)
{
	percent_progress = progress;
	ProgressMessage = Message;
}

gboolean
StippleDialog::UpdateProgress(GtkProgressBar *PB)
{
	if (Cancel || 2.0 == percent_progress)  {
		if (dialog != NULL) gtk_widget_destroy (dialog);
		dialog = NULL;
		return FALSE;
	}

	gtk_label_set_text ((GtkLabel *)ProgressLabel, ProgressMessage.c_str());

	if (percent_progress > 1.0) percent_progress = 1.0;
	if (percent_progress < 0.0) percent_progress = 0.0;
	gtk_progress_bar_set_fraction (PB, percent_progress);

	return TRUE;
}

void
StippleDialog::KeyPress( GtkButton *widget, gpointer data )
{
	string Buffer;

	try	{
	Buffer = gtk_editable_get_chars (GTK_EDITABLE (TopTraceEdit), 0, -1);
	ComponentTrace = boost::lexical_cast<int>(Buffer);
	Buffer = gtk_editable_get_chars (GTK_EDITABLE (TopPitchEdit), 0, -1);
	ComponentPitch = boost::lexical_cast<int>(Buffer);
	Buffer = gtk_editable_get_chars (GTK_EDITABLE (BottomTraceEdit), 0, -1);
	SolderTrace = boost::lexical_cast<int>(Buffer);
	Buffer = gtk_editable_get_chars (GTK_EDITABLE (BottomPitchEdit), 0, -1);
	SolderPitch = boost::lexical_cast<int>(Buffer);
	}
	catch(boost::bad_lexical_cast &) {
		cout << "Garbage in, Garbage Out" << endl;
	}

	Buffer = str( boost::format(
			"Comp. Fill=%2.0f%%, Solder Fill=%2.0f%%") %
			StippleDialog::PercentFill(ComponentTrace, ComponentPitch) %
			StippleDialog::PercentFill(SolderTrace, SolderPitch));

	gtk_label_set_text((GtkLabel *)PercentFillMessage, Buffer.c_str());
}

/// "C" thunk for threaded signals.
void
ButtonPressCallback( GtkButton *widget, gpointer data )  {
	StippleDialog::ButtonPress(widget, data);
}

void
StippleDialog::ButtonPress( GtkButton *widget, gpointer data )
{
	string Buffer;

	if (!strcmp("gtk-ok", gtk_button_get_label (widget)))  {

		gtk_button_set_label (widget, GTK_STOCK_CANCEL);

		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (TopLayer)))  {
			MakeLayers = MakeTopLayer;
		} else if (gtk_toggle_button_get_active(
				GTK_TOGGLE_BUTTON (BottomLayer)))  {
			MakeLayers = MakeBottomLayer;
		} else if (gtk_toggle_button_get_active(
				GTK_TOGGLE_BUTTON (BothLayers)))  {
			MakeLayers = MakeBothLayers;
		} else if (gtk_toggle_button_get_active(
				GTK_TOGGLE_BUTTON (SelectedPolygons)))  {
			MakeLayers = MakeSelected;
		} else if (gtk_toggle_button_get_active(
				GTK_TOGGLE_BUTTON (DeletePolygons)))  {
			MakeLayers = MakeDelete;
		} else {
			Log("No Layer Specification\n");
		}

		MakeLayerNames.clear();
		switch (MakeLayers)  {
		case MakeTopLayer:
			MakeLayerNames.push_back(component_perimeter);
			break;
		case MakeBottomLayer:
			MakeLayerNames.push_back(solder_perimeter);
			break;
		case MakeDelete:
		case MakeSelected:
		case MakeBothLayers:
			MakeLayerNames.push_back(component_perimeter);
			MakeLayerNames.push_back(solder_perimeter);
			break;
		}

		try  {
		Buffer = gtk_editable_get_chars (GTK_EDITABLE (TopTraceEdit), 0, -1);
		ComponentTrace = boost::lexical_cast<int>(Buffer);
		Buffer = gtk_editable_get_chars (GTK_EDITABLE (TopPitchEdit), 0, -1);
		ComponentPitch = boost::lexical_cast<int>(Buffer);
		Buffer = gtk_editable_get_chars (GTK_EDITABLE (BottomTraceEdit), 0, -1);
		SolderTrace = boost::lexical_cast<int>(Buffer);
		Buffer = gtk_editable_get_chars (GTK_EDITABLE (BottomPitchEdit), 0, -1);
		SolderPitch = boost::lexical_cast<int>(Buffer);
		}
		catch(boost::bad_lexical_cast &) {
			cout << "Bad Trace/Pitch parameter input" << endl;
			Cancel = true;
			return;
		}

		ofstream WritePrefs
		  (string(getenv("HOME")+string("/.pcb/stipple_prefs")).data());
		if (WritePrefs.is_open())
		{
		  WritePrefs << "ComponentTrace = " << ComponentTrace << endl;
		  WritePrefs << "ComponentPitch = " << ComponentPitch << endl;
		  WritePrefs << "SolderTrace = " << SolderTrace << endl;
		  WritePrefs << "SolderPitch = " << SolderPitch << endl;
		  WritePrefs << "DefaultAction = 1\n";
		  WritePrefs.close();

		}  else  {
		  cout << "Unable to write prefs file" << endl;
		}

		ComponentTrace	= ComponentTrace 	* MilToNanometer;
		SolderTrace		= SolderTrace 		* MilToNanometer;
		ComponentPitch	= ComponentPitch 	* MilToNanometer;
		SolderPitch		= SolderPitch 		* MilToNanometer;

		Cancel = false;
		g_timeout_add(500, (GSourceFunc)UpdateProgress, (gpointer)ProgressBar);
		g_thread_new("Stipple Thread", (GThreadFunc)MakeAllLayers, NULL);

	} else {
		Cancel = true;
	}
}

int
StippleDialog::PercentFill(double Trace, double Pitch)
{
	return (int)(100.0 * (1.0 -
	(pow(	((double)Pitch / sqrt(2.0)) -
			(sqrt(2.0) * (double)Trace / 2.0), 2.0) / 2.0) /
	(pow(	(double)Pitch / sqrt(2.0), 2.0) / 2.0)));
}

long
StippleDialog::ReadDefault(string File, string Key, Coord Default)
{
	string Number;
	size_t Idx = string::npos;

	if (string::npos != (Idx = File.find(Key)))  {
		if (string::npos != (Idx = File.find_first_of("-0123456789", Idx)))  {
			while ('-' == File[Idx] || isdigit(File[Idx]))  {
					Number.append(File.substr(Idx, 1));
					Idx++;
			}
		} else {
			return Default;
		}
		return atol(Number.c_str());
	} else {
		return Default;
	}
}

bool
StippleDialog::ReadDefaults()
{
	char *FileContents;
	long FileSize;
	string File;

	ifstream ReadPrefs
		(string(getenv("HOME")+string("/.pcb/stipple_prefs")).data(),
		 ios::in | ios::binary | ios::ate);

	if (ReadPrefs.is_open())
	{

		FileSize = ReadPrefs.tellg();
		FileContents = new char[FileSize];

		ReadPrefs.seekg(0, ios::beg);
		ReadPrefs.read(FileContents, FileSize);

		ReadPrefs.close();
		File = FileContents;

		ComponentTrace = ReadDefault(File, "ComponentTrace", 700);
		ComponentPitch = ReadDefault(File, "ComponentPitch", 4500);
		SolderTrace = ReadDefault(File, "SolderTrace", 700);
		SolderPitch = ReadDefault(File, "SolderPitch", 7000);

	}  else  {

		cout << getenv("HOME");
		cout << "Unable to read prefs file" << endl;
		ofstream WritePrefs
		  (string(getenv("HOME")+string("/.pcb/stipple_prefs")).data());
		if (WritePrefs.is_open())
		{
		  WritePrefs << "ComponentTrace = 700\n";
		  WritePrefs << "ComponentPitch = 4500\n";
		  WritePrefs << "SolderTrace = 700\n";
		  WritePrefs << "SolderPitch = 7000\n";
		  WritePrefs << "DefaultAction = 1\n";
		  WritePrefs.close();

		}  else  {
		  cout << "Unable to write prefs file" << endl;
		}
		ComponentTrace	= 700;
		ComponentPitch	= 4500;
		SolderTrace		= 700;
		SolderPitch		= 7000;
	}

	return 0;
}

void
StippleDialog::ParameterDialog()
{
	gint position;
	string Buffer;

	GtkWidget *content_area;
	GtkWidget *radio_vbox, *hbox, *vbox;

	GtkWidget *label;
	GtkWidget *separator, *button;
	GSList *group;

	Cancel = false;

	ReadDefaults();

	dialog = gtk_dialog_new();

	radio_vbox = gtk_vbox_new (FALSE, 4);
	gtk_container_set_border_width (GTK_CONTAINER (radio_vbox), 4);

	TopLayer = gtk_radio_button_new_with_label
			(NULL, "\"comp-perim\" to \"comp-stipple\"");
	gtk_box_pack_start (GTK_BOX (radio_vbox), TopLayer, TRUE, TRUE, 0);
	gtk_widget_show (TopLayer);

	group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (TopLayer));
	BottomLayer = gtk_radio_button_new_with_label
			(group, "\"solder-perim\" to \"solder-stipple\"");
	gtk_box_pack_start (GTK_BOX (radio_vbox), BottomLayer, TRUE, TRUE, 0);
	gtk_widget_show (BottomLayer);

	BothLayers = gtk_radio_button_new_with_label_from_widget
			(GTK_RADIO_BUTTON (BottomLayer), "Make all Stippled Polygons");
	gtk_box_pack_start (GTK_BOX (radio_vbox), BothLayers, TRUE, TRUE, 0);
	gtk_widget_show (BothLayers);

	DeletePolygons = gtk_radio_button_new_with_label_from_widget
			(GTK_RADIO_BUTTON (BothLayers), "Delete all Stippled Polygons");
	gtk_box_pack_start (GTK_BOX (radio_vbox), DeletePolygons, TRUE, TRUE, 0);
	gtk_widget_show (DeletePolygons);

	SelectedPolygons = gtk_radio_button_new_with_label_from_widget
			(GTK_RADIO_BUTTON (DeletePolygons), "Selected Polygons w/o Deletions");
	gtk_box_pack_start (GTK_BOX (radio_vbox), SelectedPolygons, TRUE, TRUE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (SelectedPolygons), TRUE);
	gtk_widget_show (SelectedPolygons);

	separator = gtk_hseparator_new ();
	gtk_box_pack_start (GTK_BOX (radio_vbox), separator, FALSE, TRUE, 0);
	gtk_widget_show (separator);

	Buffer = str( boost::format(
			"Comp. Fill=%d%%, Solder Fill=%d%%") %
			PercentFill(ComponentTrace, ComponentPitch) %
			PercentFill(SolderTrace, SolderPitch));

	PercentFillMessage = gtk_label_new (Buffer.c_str());
	gtk_box_pack_start (GTK_BOX (radio_vbox), PercentFillMessage, TRUE, TRUE, 0);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_container_add (GTK_CONTAINER (content_area), radio_vbox);

	position = 0;
	TopTraceEdit 	= gtk_entry_new ();
	BottomTraceEdit	= gtk_entry_new ();
	TopPitchEdit 	= gtk_entry_new ();
	BottomPitchEdit	= gtk_entry_new ();

	hbox = gtk_hbox_new (FALSE, 4);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 4);
	label = gtk_label_new ("Component Trace");
	gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
	Buffer = boost::lexical_cast<string>(ComponentTrace);
	gtk_editable_insert_text(GTK_EDITABLE (TopTraceEdit),
		(gchar *) Buffer.c_str(), Buffer.length(), &position);
	gtk_entry_set_activates_default (GTK_ENTRY (TopTraceEdit), TRUE);
	gtk_box_pack_start (GTK_BOX (hbox), TopTraceEdit, FALSE, FALSE, 0);
	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_container_add (GTK_CONTAINER (content_area), hbox);

	hbox = gtk_hbox_new (FALSE, 4);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 4);
	label = gtk_label_new ("Component Pitch");
	gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
	Buffer = boost::lexical_cast<string>(ComponentPitch);
	gtk_editable_insert_text(GTK_EDITABLE (TopPitchEdit),
		(gchar *) Buffer.c_str(), Buffer.length(), &position);
	gtk_entry_set_activates_default (GTK_ENTRY (TopPitchEdit), TRUE);
	gtk_box_pack_start (GTK_BOX (hbox), TopPitchEdit, FALSE, FALSE, 0);
	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_container_add (GTK_CONTAINER (content_area), hbox);

	hbox = gtk_hbox_new (FALSE, 4);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 4);
	label = gtk_label_new ("Solder Trace");
	gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
	Buffer = boost::lexical_cast<string>(SolderTrace);
	gtk_editable_insert_text(GTK_EDITABLE (BottomTraceEdit),
		(gchar *) Buffer.c_str(), Buffer.length(), &position);
	gtk_entry_set_activates_default (GTK_ENTRY (BottomTraceEdit), TRUE);
	gtk_box_pack_start (GTK_BOX (hbox), BottomTraceEdit, FALSE, FALSE, 0);
	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_container_add (GTK_CONTAINER (content_area), hbox);

	hbox = gtk_hbox_new (FALSE, 4);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 4);
	label = gtk_label_new ("Solder Pitch");
	gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
	Buffer = boost::lexical_cast<string>(SolderPitch);
	gtk_editable_insert_text(GTK_EDITABLE (BottomPitchEdit),
		(gchar *) Buffer.c_str(), Buffer.length(), &position);
	gtk_entry_set_activates_default (GTK_ENTRY (BottomPitchEdit), TRUE);
	gtk_box_pack_start (GTK_BOX (hbox), BottomPitchEdit, FALSE, FALSE, 0);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_container_add (GTK_CONTAINER (content_area), hbox);

	vbox = gtk_vbox_new (FALSE, 4);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 4);
	separator = gtk_hseparator_new ();
	gtk_box_pack_start (GTK_BOX (vbox), separator, FALSE, TRUE, 0);
	gtk_widget_show (separator);
	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_container_add (GTK_CONTAINER (content_area), vbox);

	ProgressMessage = "Stipple Progress";
	hbox = gtk_hbox_new (FALSE, 4);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 4);
	ProgressLabel = gtk_label_new (ProgressMessage.c_str());
	gtk_box_pack_start (GTK_BOX (hbox), ProgressLabel, TRUE, TRUE, 0);
	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_container_add (GTK_CONTAINER (content_area), hbox);

	hbox = gtk_hbox_new (FALSE, 4);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 4);
	ProgressBar = (GtkProgressBar *) gtk_progress_bar_new ();
	gtk_progress_bar_set_fraction (ProgressBar, 0.0);
	gtk_box_pack_start (GTK_BOX (hbox), (GtkWidget *)ProgressBar, TRUE, TRUE, 0);
	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_container_add (GTK_CONTAINER (content_area), hbox);

	vbox = gtk_vbox_new (FALSE, 4);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 4);
	separator = gtk_hseparator_new ();
	gtk_box_pack_start (GTK_BOX (vbox), separator, FALSE, TRUE, 0);
	gtk_widget_show (separator);
	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_container_add (GTK_CONTAINER (content_area), vbox);

	vbox = gtk_hbutton_box_new ();
	gtk_button_box_set_layout (GTK_BUTTON_BOX (vbox), GTK_BUTTONBOX_END);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
	gtk_box_set_spacing (GTK_BOX (vbox), 20);
	button = gtk_button_new_from_stock (GTK_STOCK_OK);
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
			GTK_SIGNAL_FUNC (ButtonPress), NULL);
	gtk_box_pack_start (GTK_BOX (vbox), (GtkWidget *)button, FALSE, FALSE, 0);
	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_container_add (GTK_CONTAINER (content_area), vbox);
	gtk_widget_grab_focus(button);

	gtk_signal_connect (GTK_OBJECT (TopTraceEdit), "changed",
			GTK_SIGNAL_FUNC (KeyPress), NULL);
	gtk_signal_connect (GTK_OBJECT (BottomTraceEdit), "changed",
			GTK_SIGNAL_FUNC (KeyPress), NULL);
	gtk_signal_connect (GTK_OBJECT (TopPitchEdit), "changed",
			GTK_SIGNAL_FUNC (KeyPress), NULL);
	gtk_signal_connect (GTK_OBJECT (BottomPitchEdit), "changed",
			GTK_SIGNAL_FUNC (KeyPress), NULL);

	gtk_widget_show_all (dialog);
	gtk_dialog_run (GTK_DIALOG (dialog));
}
