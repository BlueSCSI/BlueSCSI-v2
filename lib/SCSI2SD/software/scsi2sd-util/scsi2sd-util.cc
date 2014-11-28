//	Copyright (C) 2014 Michael McMaster <michael@codesrc.com>
//
//	This file is part of SCSI2SD.
//
//	SCSI2SD is free software: you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//
//	SCSI2SD is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with SCSI2SD.  If not, see <http://www.gnu.org/licenses/>.


// For compilers that support precompilation, includes "wx/wx.h".
#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/filedlg.h>
#include <wx/notebook.h>
#include <wx/progdlg.h>
#include <wx/utils.h>
#include <wx/windowptr.h>
#include <wx/thread.h>

#include "ConfigUtil.hh"
#include "TargetPanel.hh"
#include "SCSI2SD_Bootloader.hh"
#include "SCSI2SD_HID.hh"
#include "Firmware.hh"

#include <algorithm>
#include <vector>
#include <set>
#include <sstream>

#if __cplusplus >= 201103L
#include <cstdint>
#include <memory>
using std::shared_ptr;
#else
#include <stdint.h>
#include <tr1/memory>
using std::tr1::shared_ptr;
#endif


using namespace SCSI2SD;

class ProgressWrapper
{
public:
	void setProgressDialog(
		const wxWindowPtr<wxGenericProgressDialog>& dlg,
		size_t maxRows)
	{
		myProgressDialog = dlg;
		myMaxRows = maxRows;
		myNumRows = 0;
	}

	void clearProgressDialog()
	{
		myProgressDialog.reset();
	}

	void update(unsigned char arrayId, unsigned short rowNum)
	{
		if (!myProgressDialog) return;

		myNumRows++;

		std::stringstream ss;
		ss << "Writing flash array " <<
			static_cast<int>(arrayId) << " row " <<
			static_cast<int>(rowNum);
		std::clog << ss.str() << std::endl;
		myProgressDialog->Update(myNumRows, ss.str());
	}

private:
	wxWindowPtr<wxGenericProgressDialog> myProgressDialog;
	size_t myMaxRows;
	size_t myNumRows;
};
static ProgressWrapper TheProgressWrapper;

extern "C"
void ProgressUpdate(unsigned char arrayId, unsigned short rowNum)
{
	TheProgressWrapper.update(arrayId, rowNum);
}

namespace
{

class TimerLock
{
public:
	TimerLock(wxTimer* timer) :
		myTimer(timer),
		myInterval(myTimer->GetInterval())
	{
		myTimer->Stop();
	};

	virtual ~TimerLock()
	{
		if (myTimer && myInterval > 0)
		{
			myTimer->Start(myInterval);
		}
	}
private:
	wxTimer* myTimer;
	int myInterval;
};

class AppFrame : public wxFrame
{
public:
	AppFrame() :
		wxFrame(NULL, wxID_ANY, "scsi2sd-util", wxPoint(50, 50), wxSize(600, 650)),
		myInitialConfig(false),
		myTickCounter(0)
	{
		wxMenu *menuFile = new wxMenu();
		menuFile->Append(
			ID_ConfigDefaults,
			"Load &Defaults",
			"Load default configuration options.");
		menuFile->Append(
			ID_Firmware,
			"&Upgrade Firmware...",
			"Upgrade or inspect device firmware version.");
		menuFile->AppendSeparator();
		menuFile->Append(wxID_EXIT);
		wxMenu *menuHelp = new wxMenu();
		menuHelp->Append(wxID_ABOUT);
		wxMenuBar *menuBar = new wxMenuBar();
		menuBar->Append( menuFile, "&File" );
		menuBar->Append( menuHelp, "&Help" );
		SetMenuBar( menuBar );

		CreateStatusBar();
		SetStatusText( "Searching for SCSI2SD" );

		{
			wxPanel* cfgPanel = new wxPanel(this);
			wxFlexGridSizer *fgs = new wxFlexGridSizer(2, 1, 5, 5);
			wxNotebook* tabs = new wxNotebook(cfgPanel, ID_Notebook);

			for (int i = 0; i < MAX_SCSI_TARGETS; ++i)
			{
				TargetPanel* target =
					new TargetPanel(tabs, ConfigUtil::Default(i));
				myTargets.push_back(target);
				std::stringstream ss;
				ss << "Device " << (i + 1);
				tabs->AddPage(target, ss.str());
			}
			fgs->Add(tabs);


			wxPanel* btnPanel = new wxPanel(cfgPanel);
			wxFlexGridSizer *btnFgs = new wxFlexGridSizer(1, 2, 5, 5);
			btnFgs->Add(new wxButton(btnPanel, ID_BtnLoad, wxT("Load from device")));
			mySaveButton =
				new wxButton(btnPanel, ID_BtnSave, wxT("Save to device"));
			btnFgs->Add(mySaveButton);
			{
				wxBoxSizer* hbox = new wxBoxSizer(wxHORIZONTAL);
				hbox->Add(btnFgs, 1, wxALL | wxEXPAND, 15);
				btnPanel->SetSizer(hbox);
				fgs->Add(btnPanel);
			}

			{
				wxBoxSizer* hbox = new wxBoxSizer(wxHORIZONTAL);
				hbox->Add(fgs, 1, wxALL | wxEXPAND, 15);
				cfgPanel->SetSizer(hbox);
			}
		}

		myTimer = new wxTimer(this, ID_Timer);
		myTimer->Start(1000); //ms

	}
private:
	std::vector<TargetPanel*> myTargets;
	wxButton* mySaveButton;
	wxTimer* myTimer;
	shared_ptr<HID> myHID;
	shared_ptr<Bootloader> myBootloader;
	bool myInitialConfig;

	uint8_t myTickCounter;

	void onConfigChanged(wxCommandEvent& event)
	{
		evaluate();
	}

	void evaluate()
	{
		bool valid = true;

		// Check for duplicate SCSI IDs
		std::set<uint8_t> enabledID;

		// Check for overlapping SD sectors.
		std::vector<std::pair<uint32_t, uint64_t> > sdSectors;

		bool isTargetEnabled = false; // Need at least one enabled

		for (size_t i = 0; i < myTargets.size(); ++i)
		{
			valid = myTargets[i]->evaluate() && valid;

			if (myTargets[i]->isEnabled())
			{
				isTargetEnabled = true;
				uint8_t scsiID = myTargets[i]->getSCSIId();
				if (enabledID.find(scsiID) != enabledID.end())
				{
					myTargets[i]->setDuplicateID(true);
					valid = false;
				}
				else
				{
					enabledID.insert(scsiID);
					myTargets[i]->setDuplicateID(false);
				}

				auto sdSectorRange = myTargets[i]->getSDSectorRange();
				for (auto it(sdSectors.begin()); it != sdSectors.end(); ++it)
				{
					if (sdSectorRange.first < it->second &&
						sdSectorRange.second > it->first)
					{
						valid = false;
						myTargets[i]->setSDSectorOverlap(true);
					}
					else
					{
						myTargets[i]->setSDSectorOverlap(false);
					}
				}
				sdSectors.push_back(sdSectorRange);
			}
			else
			{
				myTargets[i]->setDuplicateID(false);
				myTargets[i]->setSDSectorOverlap(false);
			}
		}

		valid = valid && isTargetEnabled; // Need at least one.

		mySaveButton->Enable(valid && myHID);
	}


	enum
	{
		ID_ConfigDefaults = wxID_HIGHEST + 1,
		ID_Firmware,
		ID_Timer,
		ID_Notebook,
		ID_BtnLoad,
		ID_BtnSave
	};

	void OnID_ConfigDefaults(wxCommandEvent& event)
	{
		for (size_t i = 0; i < myTargets.size(); ++i)
		{
			myTargets[i]->setConfig(ConfigUtil::Default(i));
		}
	}

	void OnID_Firmware(wxCommandEvent& event)
	{
		TimerLock lock(myTimer);
		doFirmwareUpdate();
	}

	void doFirmwareUpdate()
	{
		wxFileDialog dlg(
			this,
			"Load firmware file",
			"",
			"",
			"SCSI2SD Firmware files (*.cyacd)|*.cyacd",
			wxFD_OPEN | wxFD_FILE_MUST_EXIST);
		if (dlg.ShowModal() == wxID_CANCEL) return;

		std::string filename(dlg.GetPath());

		try
		{
			if (!myHID) myHID.reset(HID::Open());
			if (myHID)
			{
				std::clog << "Resetting SCSI2SD into bootloader" << std::endl;

				myHID->enterBootloader();
				myHID.reset();
			}

			if (myBootloader)
			{
				// Verify the USB HID connection is valid
				if (!myBootloader->ping())
				{
					myBootloader.reset();
				}
			}

			for (int i = 0; !myBootloader && (i < 10); ++i)
			{
				std::clog << "Searching for bootloader" << std::endl;
				myBootloader.reset(Bootloader::Open());
				wxMilliSleep(100);
			}
		}
		catch (std::exception& e)
		{
			std::clog << e.what() << std::endl;

			SetStatusText(e.what());
			myHID.reset();
			myBootloader.reset();
		}

		if (!myBootloader)
		{
			wxMessageBox(
				"SCSI2SD not found",
				"Device not ready",
				wxOK | wxICON_ERROR);

			return;
		}

		if (!myBootloader->isCorrectFirmware(filename))
		{
			// TODO allow "force" option
			wxMessageBox(
				"Wrong filename",
				"Wrong filename",
				wxOK | wxICON_ERROR);
			return;
		}

		int totalFlashRows = 0;
		try
		{
			Firmware firmware(filename);
			totalFlashRows = firmware.totalFlashRows();
		}
		catch (std::exception& e)
		{
			SetStatusText(e.what());
			std::stringstream msg;
			msg << "Could not open firmware file: " << e.what();
			wxMessageBox(
				msg.str(),
				"Bad file",
				wxOK | wxICON_ERROR);
			return;
		}

		{
			wxWindowPtr<wxGenericProgressDialog> progress(
				new wxGenericProgressDialog(
					"Loading firmware",
					"Loading firmware",
					totalFlashRows,
					this,
					wxPD_APP_MODAL)
					);
			TheProgressWrapper.setProgressDialog(progress, totalFlashRows);
		}

		std::clog << "Upgrading firmware from file: " << filename << std::endl;

		try
		{
			myBootloader->load(filename, &ProgressUpdate);
			TheProgressWrapper.clearProgressDialog();

			wxMessageBox(
				"Firmware update successful",
				"Firmware OK",
				wxOK);
			SetStatusText("Firmware update successful");


			myHID.reset();
			myBootloader.reset();
		}
		catch (std::exception& e)
		{
			TheProgressWrapper.clearProgressDialog();
			SetStatusText(e.what());
			myHID.reset();
			myBootloader.reset();

			wxMessageBox(
				"Firmware Update Failed",
				e.what(),
				wxOK | wxICON_ERROR);
		}
	}

	void OnID_Timer(wxTimerEvent& event)
	{
		// Check if we are connected to the HID device.
		// AND/or bootloader device.
		try
		{
			if (myBootloader)
			{
				// Verify the USB HID connection is valid
				if (!myBootloader->ping())
				{
					myBootloader.reset();
				}
			}

			if (!myBootloader)
			{
				myBootloader.reset(Bootloader::Open());

				if (myBootloader)
				{
					SetStatusText(wxT("SCSI2SD Bootloader Ready"));
				}
			}

			if (myHID)
			{
				// Verify the USB HID connection is valid
				if (!myHID->ping())
				{
					myHID.reset();
				}
			}

			if (!myHID)
			{
				myHID.reset(HID::Open());
				if (myHID)
				{
					uint16_t version = myHID->getFirmwareVersion();
					if (version == 0)
					{
						// Oh dear, old firmware
						SetStatusText(wxT("Firmware update required"));
						myHID.reset();
					}
					else
					{
						SetStatusText(wxT("SCSI2SD Ready"));

						if (!myInitialConfig)
						{
							wxCommandEvent loadEvent(wxEVT_NULL, ID_BtnLoad);
							GetEventHandler()->AddPendingEvent(loadEvent);
						}
					}
				}
				else
				{
					char ticks[] = {'/', '-', '\\', '|'};
					std::stringstream ss;
					ss << "Searching for SCSI2SD device " << ticks[myTickCounter % sizeof(ticks)];
					myTickCounter++;
					SetStatusText(ss.str());
				}
			}
		}
		catch (std::runtime_error& e)
		{
			std::clog << e.what() << std::endl;
			SetStatusText(e.what());
		}

		evaluate();
	}

	void doLoad(wxCommandEvent& event)
	{
		TimerLock lock(myTimer);
		if (!myHID) return;

		std::clog << "Loading configuration" << std::endl;

		wxWindowPtr<wxGenericProgressDialog> progress(
			new wxGenericProgressDialog(
				"Load config settings",
				"Loading config settings",
				100,
				this,
				wxPD_APP_MODAL | wxPD_CAN_ABORT)
				);

		int flashRow = SCSI_CONFIG_0_ROW;
		int currentProgress = 0;
		int totalProgress = myTargets.size() * SCSI_CONFIG_ROWS;
		for (size_t i = 0;
			i < myTargets.size();
			++i, flashRow += SCSI_CONFIG_ROWS)
		{
			std::vector<uint8_t> raw(sizeof(TargetConfig));

			for (size_t j = 0; j < SCSI_CONFIG_ROWS; ++j)
			{
				std::stringstream ss;
				ss << "Reading flash array " << SCSI_CONFIG_ARRAY <<
					" row " << (flashRow + j);
				SetStatusText(ss.str());
				std::clog << ss.str() << std::endl;
				currentProgress += 1;
				if (!progress->Update(
						(100 * currentProgress) / totalProgress,
						ss.str()
						)
					)
				{
					goto abort;
				}

				std::vector<uint8_t> flashData;

				try
				{
					myHID->readFlashRow(
						SCSI_CONFIG_ARRAY, flashRow + j, flashData);

				}
				catch (std::runtime_error& e)
				{
					SetStatusText(e.what());
					goto err;
				}

				std::copy(
					flashData.begin(),
					flashData.end(),
					&raw[j * SCSI_CONFIG_ROW_SIZE]);
			}
			myTargets[i]->setConfig(ConfigUtil::fromBytes(&raw[0]));
		}

		myInitialConfig = true;
		SetStatusText("Load Complete");
		std::clog << "Load Complete" << std::endl;
		while (progress->Update(100, "Load Complete"))
		{
			// Wait for the user to click "Close"
			wxMilliSleep(50);
		}
		goto out;

	err:
		SetStatusText("Load failed");
		std::clog << "Load failed" << std::endl;
		while (progress->Update(100, "Load failed"))
		{
			// Wait for the user to click "Close"
			wxMilliSleep(50);
		}
		goto out;

	abort:
		SetStatusText("Load Aborted");
		std::clog << "Load Aborted" << std::endl;

	out:
		return;
	}

	void doSave(wxCommandEvent& event)
	{
		TimerLock lock(myTimer);
		if (!myHID) return;

		std::clog << "Saving configuration" << std::endl;

		wxWindowPtr<wxGenericProgressDialog> progress(
			new wxGenericProgressDialog(
				"Save config settings",
				"Saving config settings",
				100,
				this,
				wxPD_APP_MODAL | wxPD_CAN_ABORT)
				);

		int flashRow = SCSI_CONFIG_0_ROW;
		int currentProgress = 0;
		int totalProgress = myTargets.size() * SCSI_CONFIG_ROWS;
		for (size_t i = 0;
			i < myTargets.size();
			++i, flashRow += SCSI_CONFIG_ROWS)
		{
			TargetConfig config(myTargets[i]->getConfig());
			std::vector<uint8_t> raw(ConfigUtil::toBytes(config));

			for (size_t j = 0; j < SCSI_CONFIG_ROWS; ++j)
			{
				std::stringstream ss;
				ss << "Programming flash array " << SCSI_CONFIG_ARRAY <<
					" row " << (flashRow + j);
				SetStatusText(ss.str());
				std::clog << ss.str() << std::endl;
				currentProgress += 1;
				if (!progress->Update(
						(100 * currentProgress) / totalProgress,
						ss.str()
						)
					)
				{
					goto abort;
				}

				std::vector<uint8_t> flashData(SCSI_CONFIG_ROW_SIZE, 0);
				std::copy(
					&raw[j * SCSI_CONFIG_ROW_SIZE],
					&raw[(1+j) * SCSI_CONFIG_ROW_SIZE],
					flashData.begin());
				try
				{
					myHID->writeFlashRow(
						SCSI_CONFIG_ARRAY, flashRow + j, flashData);
				}
				catch (std::runtime_error& e)
				{
					std::clog << e.what() << std::endl;
					SetStatusText(e.what());
					goto err;
				}
			}
		}

		// Reboot so new settings take effect.
		myHID->enterBootloader();
		myHID.reset();

		SetStatusText("Save Complete");
		std::clog << "Save Complete" << std::endl;
		while (progress->Update(100, "Save Complete"))
		{
			// Wait for the user to click "Close"
			wxMilliSleep(50);
		}
		goto out;

	err:
		SetStatusText("Save failed");
		std::clog << "Save failed" << std::endl;
		while (progress->Update(100, "Save failed"))
		{
			// Wait for the user to click "Close"
			wxMilliSleep(50);
		}
		goto out;

	abort:
		SetStatusText("Save Aborted");
		std::clog << "Save Aborted" << std::endl;

	out:
		(void) true; // empty statement.
	}

	void OnExit(wxCommandEvent& event)
	{
		Close(true);
	}
	void OnAbout(wxCommandEvent& event)
	{
		wxMessageBox(
			"SCSI2SD (scsi2sd-util)\n"
			"Copyright (C) 2014 Michael McMaster <michael@codesrc.com>\n"
			"\n"
"This program is free software: you can redistribute it and/or modify\n"
"it under the terms of the GNU General Public License as published by\n"
"the Free Software Foundation, either version 3 of the License, or\n"
"(at your option) any later version.\n"
"\n"
"This program is distributed in the hope that it will be useful,\n"
"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
"GNU General Public License for more details.\n"
"\n"
"You should have received a copy of the GNU General Public License\n"
"along with this program.  If not, see <http://www.gnu.org/licenses/>.\n",

			"About scsi2sd-util", wxOK | wxICON_INFORMATION );
	}

	wxDECLARE_EVENT_TABLE();
};

wxBEGIN_EVENT_TABLE(AppFrame, wxFrame)
	EVT_MENU(AppFrame::ID_ConfigDefaults, AppFrame::OnID_ConfigDefaults)
	EVT_MENU(AppFrame::ID_Firmware, AppFrame::OnID_Firmware)
	EVT_MENU(wxID_EXIT, AppFrame::OnExit)
	EVT_MENU(wxID_ABOUT, AppFrame::OnAbout)

	EVT_TIMER(AppFrame::ID_Timer, AppFrame::OnID_Timer)

	EVT_COMMAND(wxID_ANY, ConfigChangedEvent, AppFrame::onConfigChanged)

	EVT_BUTTON(ID_BtnSave, AppFrame::doSave)
	EVT_BUTTON(ID_BtnLoad, AppFrame::doLoad)


wxEND_EVENT_TABLE()



class App : public wxApp
{
public:
	virtual bool OnInit()
	{
		AppFrame* frame = new AppFrame();
		frame->Show(true);
		SetTopWindow(frame);
		return true;
	}
};
} // namespace

// Main Method
wxIMPLEMENT_APP(App);


