/* Copyright (C) 2001 to 2011 Chris Vine

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
  MA 02110-1301 USA

*/

#include "settings_help.h"

#ifdef ENABLE_NLS
#include <libintl.h>
#endif

std::string SettingsMessagesBase::get_caption(std::vector<std::string>::size_type index) {
  if (index > captions.size()) {
    write_error("Yikes, the message index is too large in SettingsMessagesBase::get_caption.\n"
		"Please report the bug to the author.\n");
    return std::string(); // return empty string
  }
  else return captions[index];
}

std::string SettingsMessagesBase::get_message(std::vector<std::string>::size_type index) {
  if (index > messages.size()) {
    write_error("Yikes, the message index is too large in SettingsMessagesBase::get_message.\n"
		"Please report the bug to the author.\n");
    return std::string(); // return empty string
  }
  else return messages[index];
}

IdentityMessages::IdentityMessages(void): SettingsMessagesBase(number + 1) {

  messages[name] = gettext("Put the name to appear on the fax top header on sent faxes "
			   "here.  It must be in plain ASCII characters.  If this is a "
			   "problem, leave it blank as the fax station number will always "
			   "be given on the top header");
  captions[name] = gettext("efax-gtk help: Name");

  messages[number] = gettext("Put the number to appear on the fax top header on sent faxes here.  "
			     "This will also comprise the fax station ID reported to the sending "
			     "fax machine when receiving faxes");
  captions[number] = gettext("efax-gtk help: Number");
}

ModemMessages::ModemMessages(void): SettingsMessagesBase(rings + 1) {

  messages[device] = gettext("Put the serial device to which the modem is connected "
			     "here (if none is given, the program defaults to "
			     "/dev/modem).  Do not include the `/dev/' part of "
			     "the device name -- ie state it as `ttyS1' or `cua2', etc.  "
			     "With Linux, ttyS0 equates to COM 1, ttyS1 to COM 2, and so on");
  captions[device] = gettext("efax-gtk help: Device");

  messages[lock] = gettext("Put the lock file directory here.  If none is specified, the program "
			   "defaults to /var/lock");
  captions[lock] = gettext("efax-gtk help: Lock File");

  messages[modem_class] = gettext("With efax-0.9 or higher you should usually pick \"Auto\", as then "
				  "efax will work out the class of the modem by itself, but you can "
				  "also force a class by picking one of the specified ones if you "
				  "want.  However, with efax-0.8 the program defaults to Class 2, "
				  "so if you are using a Class 1 modem with old versions of efax "
				  "specify the Class here");
  captions[modem_class] = gettext("efax-gtk help: Modem Class");

  messages[dialmode] = gettext("This specifies whether tone or pulse dialling will be used "
			       "when sending faxes");
  captions[dialmode] = gettext("efax-gtk help: Dial Mode");

  messages[capabilities] = gettext("This specifies the capabilities of the modem.  To see what "
				   "these mean do `man efax', and look at the operation of the "
				   "`-c' flag.  With efax-0.9 and a Class 2 modem, you usually "
				   "won't need to specify this and should leave it blank, as the "
				   "program will interrogate the modem.  If using an older version "
				   "of efax or a different class of modem, values which will work with "
				   "practically any 14,400 bps modem are 1,5,0,2,0,0,0,0, or "
				   "1,3,0,2,0,0,0,0 for slower ones");
  captions[capabilities] = gettext("efax-gtk help: Capabilities");

  messages[rings] = gettext("This defines the number of rings the modem allows to pass before "
			    "answering the telephone when in Standby mode.  Acceptable values "
			    "are 1 to 9.  If none is specified, the program defaults to 1");
  captions[rings] = gettext("efax-gtk help: Rings");
}

ParmsMessages::ParmsMessages(void): SettingsMessagesBase(extra_parms + 1) {

  messages[init] = gettext("This specifies the initializing `AT' commands for the modem when "
			   "in fax mode.  Specify as many of these as are needed, separated by "
			   "spaces for commands which need to be separated, but without a prepended "
			   "`AT'.  If none is specified, the program will default to "
			   "`Z &FE&D2S7=120 &C0 M1L0', which will be correct for practically all "
			   "modems");
  captions[init] = gettext("efax-gtk help: Initialization Parameters");

  messages[reset] = gettext("This specifies the `AT' commands that will reset the modem.  Specify "
			    "as many commands as are needed, separated by spaces for commands "
			    "which need to be separated, but without a prepended `AT'.  If none is "
			    "specified, the program will default to `Z', which will be correct "
			    "for practically all modems");
  captions[reset] = gettext("efax-gtk help: Reset Parameters");

  messages[extra_parms] = gettext("This can be used to pass any other parameter to efax (do "
				  "`man efax' to see what is available).  Specify as many of these as "
				  "are needed, separated by spaces for different parameters -- eg "
				  "include `-or' to do a bit reversal during receive for Multitech "
				  "modems which require it.  Unless you have an unusual modem, leave "
				  "this blank");
  captions[extra_parms] = gettext("efax-gtk help: Other Parameters");
}

PrintMessages::PrintMessages(void): SettingsMessagesBase(popup + 1) {

  messages[gtkprint] = gettext("If this is checked, the program will use the GTK+ print system "
			       "rather than the command line argument specified below to print "
			       "faxes");
  captions[gtkprint] = gettext("efax-gtk help: Use GTK+ print system");

  messages[command] = gettext("This is the command which prints and takes postscript input "
			      "on standard input.  If none is specified, the program will default "
			      "to `lpr'.  This has no effect if the \"Use GTK+ Print System\" button "
			      "is checked");
  captions[command] = gettext("efax-gtk help: Print Program");

  messages[shrink] = gettext("This will determine the extent to which a printed page will "
			     "be reduced to fit within a printer's print area.  It is a percentage -- "
			     "eg 98 will shrink the page to 98 per cent of its size.  If none is "
			     "specified no shrink will take place (ie you can, but there is no need "
			     "to, specify 100).  This has no effect if the \"Use GTK+ Print System\" "
			     "button is checked");
  captions[shrink] = gettext("efax-gtk help: Print Shrink");

  messages[popup] = gettext("This specifies whether a confirmatory pop-up dialog is to appear before "
			    "printing whenever the \"Print selected fax\" button is pressed in the "
			    "Received Faxes list or Sent Faxes list.  Uncheck the box if you don't "
			    "want a dialog (if you have specified an external print manager such as "
			    "'kprinter' in the Print Program box, then you will not want this checked).  "
			    "This has no effect if the \"Use GTK+ Print System\" button is checked");
  captions[popup] = gettext("efax-gtk help: Popup");
}

ViewMessages::ViewMessages(void): SettingsMessagesBase(ps_view_command + 1) {

  messages[ps_view_command] = gettext("A postscript viewer is used to view sent and received faxes via "
				      "the fax lists, and also to view faxes to be sent identified via "
				      "the \"Files to fax\" file selector dialog.  Specify here the "
				      "command to be used to view postscript files.  If none is specified, "
				      "the program will default to 'gv'.  Other possibilities you may want "
				      "to specify are 'evince' (the Gnome image viewer) or 'kghostview' "
				      "(the KDE postscript viewer). If using gv, you may need to use the "
				      "'-media A4' or '-media Letter' option to select correct paper size.  "
				      "evince and kghostview do this automatically");
  captions[ps_view_command] = gettext("efax-gtk help: Postscript Viewer Program");
}

SockMessages::SockMessages(void): SettingsMessagesBase(ip_family + 1) {

  messages[run_server] = gettext("This specifies whether a socket server is to be run for CUPS or  "
				 "some other print system.  Check the box if it is to run");
  captions[run_server] = gettext("efax-gtk help: Run socket server");

  messages[popup] = gettext("This specifies whether a pop-up dialog is to appear whenever a fax "
			    "is received by the socket server from the print system.  Check the box "
			    "if you want a dialog (this has no effect unless you have also checked "
			    "the box for the socket server to run).  If the program is inactive or "
			    "is standing by to receive faxes, the fax can be sent directly from "
			    "this dialog");
  captions[popup] = gettext("efax-gtk help: Popup");

  messages[port] = gettext("This specifies the port number on which the socket server is to "
			   "listen.  It should be between 1024 and 65535.");
  captions[port] = gettext("efax-gtk help: Port");

  messages[client_address] = gettext("This determines whether addresses other than localhost and your machine's own "
				     "host name will be allowed to make a connection to the socket. Unless you have "
				     "unusual requirements (that is, unless the CUPS or lpd daemons are running on "
				     "another machine), you should choose localhost.  If you want other machines to "
				     "be able to connect, pick \"other\" and specify the hostnames of the computers "
				     "which may connect (separating different hostnames by spaces, but you do not "
				     "need to specify localhost and your machine's hostname, as those are always "
				     "permitted).  Such hostnames may be specified as an ordinary host name or in "
				     "numeric notation.  Numeric notation may have * as a trailing wildcard, so "
				     "for example 10.1.* will permit access for addresses 10.1.0.0 to 10.1.255.255.  "
				     "IPv6 numeric addresses may also have a trailing wildcard, but if so '::' may "
				     "not be used in the address (but other forms of 0 suppression are acceptable).  "
				     "You should run efax-gtk behind a firewall if you choose \"other\".");
  captions[client_address] = gettext("efax-gtk help: Socket connections allowed");
  messages[ip_family] = gettext("This specifies the IP family to be used by the socket server.");
  captions[ip_family] = gettext("efax-gtk help: IP family");
}

ReceiveMessages::ReceiveMessages(void): SettingsMessagesBase(exec + 1) {

  messages[popup] = gettext("This parameter specifies whether a pop-up dialog will appear when "
			    "a fax is received from a modem. Check the box if you want a pop-up.");
  captions[popup] = gettext("efax-gtk help: Receive pop-up");

  messages[exec] = gettext("This specifies whether a program or script is to be executed when a "
			   "fax is received from a modem.  Check the box if you want to execute a "
			   "program, and enter the program name.  efax-gtk comes with two scripts, "
                           "print_fax and mail_fax, which can be used automatically to print a "
                           "received fax or to mail it to a user (so to mail a fax you can enter "
                           "`mail_fax' here).  See the README file for further details.");
  captions[exec] = gettext("efax-gtk help: Execute program when fax received");
}

SendMessages::SendMessages(void): SettingsMessagesBase(dial_prefix + 1) {

  messages[res] = gettext("This specifies the resolution to which faxes are sent.  \"Standard\" "
			  "gives 204x98 and \"Fine\" gives 204x196");
  captions[res] = gettext("efax-gtk help: Sent Fax Resolution");

  messages[header] = gettext("This specifies whether the destination fax number is included in the "
			     "top fax header line for sent faxes.  Uncheck the box if you do not "
			     "want this to be shown (say because it includes a pay card access "
			     "number)");
  captions[header] = gettext("efax-gtk help: Fax top header line");

  messages[redial] = gettext("This specifies whether the program is to redial automatically when "
                             "sending a fax if the modem is in use or the recipient is busy.  "
			     "Check the box if you want want automatic redial");
  captions[redial] = gettext("efax-gtk help: Redial");

  messages[dial_prefix] = gettext("This specifies a prefix to be prepended to any dialed number - for "
				  "example you can specify '9,' here to dial through a switchboard which "
				  "needs 9 to obtain an outside line followed by a delay.");
  captions[dial_prefix] = gettext("efax-gtk help: Dial Prefix");
}

LoggingMessages::LoggingMessages(void): SettingsMessagesBase(logfile + 1) {

  messages[logfile] = gettext("This parameter specifies a log file to which progress on negotiations "
			      "and fax status, and errors and warnings, are logged.  If none is specified, "
			      "then no log file will be maintained.  (A log file can also be kept by "
			      "redirecting stdout and stderr -- see the README file for further details)");
  captions[logfile] = gettext("efax-gtk help: Log File");
}

PageMessages::PageMessages(void): SettingsMessagesBase(page + 1) {

  messages[page] = gettext("Specify the page size for faxes here");
  captions[page] = gettext("efax-gtk help: Page Size");
}


void SettingsHelpDialogCB::settings_help_dialog_button_clicked(GtkWidget*, void* data) {
  static_cast<SettingsHelpDialog*>(data)->close();
}


SettingsHelpDialog::SettingsHelpDialog(const int standard_size, const std::string& text,
				       const std::string& caption, GtkWindow* parent_p):
                                            WinBase(caption.c_str(),
						    prog_config.window_icon_h,
						    true, parent_p) {

  close_button_p = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
  GtkWidget* button_box_p = gtk_hbutton_box_new();
  GtkWidget* label_p = gtk_label_new(text.c_str());
  GtkTable* table_p = GTK_TABLE(gtk_table_new(2, 1, false));
  
  gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box_p), GTK_BUTTONBOX_SPREAD);
  gtk_container_add(GTK_CONTAINER(button_box_p), close_button_p);

  gtk_label_set_line_wrap(GTK_LABEL(label_p), true);
#if GTK_CHECK_VERSION(2,99,0)
  gtk_widget_set_size_request(label_p, 15 * standard_size, -1);
#endif
  
  gtk_table_attach(table_p, label_p,
		   0, 1, 0, 1,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GtkAttachOptions(GTK_FILL | GTK_EXPAND),
		   standard_size/2, standard_size/4);

  gtk_table_attach(table_p, button_box_p,
		   0, 1, 1, 2,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   standard_size/2, standard_size/4);
  
  g_signal_connect(G_OBJECT(close_button_p), "clicked",
		   G_CALLBACK(SettingsHelpDialogCB::settings_help_dialog_button_clicked), this);

#if GTK_CHECK_VERSION(2,20,0)
  gtk_widget_set_can_default(close_button_p, true);
#else
  GTK_WIDGET_SET_FLAGS(close_button_p, GTK_CAN_DEFAULT);
#endif

  gtk_container_set_border_width(GTK_CONTAINER(get_win()), standard_size/4);
  gtk_container_add(GTK_CONTAINER(get_win()), GTK_WIDGET(table_p));
  gtk_window_set_position(get_win(), GTK_WIN_POS_CENTER_ON_PARENT);
  gtk_window_set_type_hint(get_win(), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_resizable(get_win(), false);

  gtk_widget_show_all(GTK_WIDGET(get_win()));
}
