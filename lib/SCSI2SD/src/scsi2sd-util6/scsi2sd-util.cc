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

#include <wx/app.h>
#include <wx/filedlg.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/log.h>
#include <wx/notebook.h>
#include <wx/progdlg.h>
#include <wx/utils.h>
#include <wx/wfstream.h>
#include <wx/windowptr.h>
#include <wx/thread.h>
#include <wx/txtstrm.h>

#include <zipper.hh>

#include "ConfigUtil.hh"
#include "BoardPanel.hh"
#include "TargetPanel.hh"
#include "SCSI2SD_HID.hh"
//#include "Dfu.hh"

#include <algorithm>
#include <iomanip>
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
		myProgressDialog->Show(false);
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
		wxLogMessage("%s", ss.str());
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

static uint8_t sdCrc7(uint8_t* chr, uint8_t cnt, uint8_t crc)
{
	uint8_t a;
	for(a = 0; a < cnt; a++)
	{
		uint8_t data = chr[a];
		uint8_t i;
		for(i = 0; i < 8; i++)
		{
			crc <<= 1;
			if ((data & 0x80) ^ (crc & 0x80))
			{
				crc ^= 0x09;
			}
			data <<= 1;
		}
	}
	return crc & 0x7F;
}

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
		wxFrame(NULL, wxID_ANY, "scsi2sd-util6", wxPoint(50, 50), wxSize(600, 700)),
		myInitialConfig(false),
		myTickCounter(0),
		myLastPollTime(0)
	{
		wxMenu *menuFile = new wxMenu();
		menuFile->Append(
			ID_SaveFile,
			_("&Save to file..."),
			_("Save settings to local file."));
		menuFile->Append(
			ID_OpenFile,
			_("&Open file..."),
			_("Load settings from local file."));
		menuFile->AppendSeparator();
		menuFile->Append(
			ID_ConfigDefaults,
			_("Load &Defaults"),
			_("Load default configuration options."));
		menuFile->Append(
			ID_Firmware,
			_("&Upgrade Firmware..."),
			_("Upgrade or inspect device firmware version."));
		menuFile->Append(wxID_EXIT);

		wxMenu *menuWindow= new wxMenu();
		menuWindow->Append(
			ID_LogWindow,
			_("Show &Log"),
			_("Show debug log window"));

		wxMenu *menuDebug = new wxMenu();
		mySCSILogChk = menuDebug->AppendCheckItem(
			ID_SCSILog,
			_("Log SCSI data"),
			_("Log SCSI commands"));

		mySelfTestChk = menuDebug->AppendCheckItem(
			ID_SelfTest,
			_("SCSI Standalone Self-Test"),
			_("SCSI Standalone Self-Test"));

		wxMenu *menuHelp = new wxMenu();
		menuHelp->Append(wxID_ABOUT);

		wxMenuBar *menuBar = new wxMenuBar();
		menuBar->Append( menuFile, _("&File") );
		menuBar->Append( menuDebug, _("&Debug") );
		menuBar->Append( menuWindow, _("&Window") );
		menuBar->Append( menuHelp, _("&Help") );
		SetMenuBar( menuBar );

		CreateStatusBar();

		{
			wxPanel* cfgPanel = new wxPanel(this);
			wxFlexGridSizer *fgs = new wxFlexGridSizer(3, 1, 15, 15);
			cfgPanel->SetSizer(fgs);

			// Empty space below menu bar.
			fgs->Add(5, 5, wxALL);

			wxNotebook* tabs = new wxNotebook(cfgPanel, ID_Notebook);
			myBoardPanel = new BoardPanel(tabs, ConfigUtil::DefaultBoardConfig());
			tabs->AddPage(myBoardPanel, _("General Settings"));
			for (int i = 0; i < S2S_MAX_TARGETS; ++i)
			{
				TargetPanel* target =
					new TargetPanel(tabs, ConfigUtil::Default(i));
				myTargets.push_back(target);
				std::stringstream ss;
				ss << "Device " << (i + 1);
				tabs->AddPage(target, ss.str());
				target->Fit();
			}
			tabs->Fit();
			fgs->Add(tabs);


			wxPanel* btnPanel = new wxPanel(cfgPanel);
			wxFlexGridSizer *btnFgs = new wxFlexGridSizer(1, 2, 5, 5);
			btnPanel->SetSizer(btnFgs);
			myLoadButton =
				new wxButton(btnPanel, ID_BtnLoad, _("Load from device"));
			btnFgs->Add(myLoadButton);
			mySaveButton =
				new wxButton(btnPanel, ID_BtnSave, _("Save to device"));
			btnFgs->Add(mySaveButton);
			fgs->Add(btnPanel);

			btnPanel->Fit();
			cfgPanel->Fit();
		}
		//Fit(); // Needed to reduce window size on Windows
		FitInside(); // Needed on Linux to prevent status bar overlap

		myLogWindow = new wxLogWindow(this, _("scsi2sd-util6 debug log"), true);
		myLogWindow->PassMessages(false); // Prevent messagebox popups

		myTimer = new wxTimer(this, ID_Timer);
		myTimer->Start(64); //ms, suitable for scsi debug logging
	}

private:
	//Dfu myDfu;
	wxLogWindow* myLogWindow;
	BoardPanel* myBoardPanel;
	std::vector<TargetPanel*> myTargets;
	wxButton* myLoadButton;
	wxButton* mySaveButton;
	wxMenuItem* mySCSILogChk;
	wxMenuItem* mySelfTestChk;
	wxTimer* myTimer;
	shared_ptr<HID> myHID;
	bool myInitialConfig;

	uint8_t myTickCounter;

	time_t myLastPollTime;

	void mmLogStatus(const std::string& msg)
	{
		// We set PassMessages to false on our log window to prevent popups, but
		// this also prevents wxLogStatus from updating the status bar.
		SetStatusText(msg);
		wxLogMessage(this, "%s", msg.c_str());
	}

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
		uint32_t autoStartSector = 0;
		for (size_t i = 0; i < myTargets.size(); ++i)
		{
			myTargets[i]->setAutoStartSector(autoStartSector);
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
				autoStartSector = sdSectorRange.second;
			}
			else
			{
				myTargets[i]->setDuplicateID(false);
				myTargets[i]->setSDSectorOverlap(false);
			}
		}

		valid = valid && isTargetEnabled; // Need at least one.

		mySaveButton->Enable(valid && myHID);

		myLoadButton->Enable(static_cast<bool>(myHID));
	}


	enum
	{
		ID_ConfigDefaults = wxID_HIGHEST + 1,
		ID_Firmware,
		ID_Timer,
		ID_Notebook,
		ID_BtnLoad,
		ID_BtnSave,
		ID_LogWindow,
		ID_SCSILog,
		ID_SelfTest,
		ID_SaveFile,
		ID_OpenFile
	};

	void OnID_ConfigDefaults(wxCommandEvent& event)
	{
		myBoardPanel->setConfig(ConfigUtil::DefaultBoardConfig());
		for (size_t i = 0; i < myTargets.size(); ++i)
		{
			myTargets[i]->setConfig(ConfigUtil::Default(i));
		}
	}

	void OnID_SaveFile(wxCommandEvent& event)
	{
		TimerLock lock(myTimer);



		wxFileDialog dlg(
			this,
			"Save config settings",
			"",
			"",
			"XML files (*.xml)|*.xml",
			wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
		if (dlg.ShowModal() == wxID_CANCEL) return;

		wxFileOutputStream file(dlg.GetPath());
		if (!file.IsOk())
		{
			wxLogError("Cannot save settings to file '%s'.", dlg.GetPath());
			return;
		}

		wxTextOutputStream s(file);

		s << "<SCSI2SD>\n";

		s << ConfigUtil::toXML(myBoardPanel->getConfig());
		for (size_t i = 0; i < myTargets.size(); ++i)
		{
			s << ConfigUtil::toXML(myTargets[i]->getConfig());
		}

		s << "</SCSI2SD>\n";
	}

	void OnID_OpenFile(wxCommandEvent& event)
	{
		TimerLock lock(myTimer);

		wxFileDialog dlg(
			this,
			"Load config settings",
			"",
			"",
			"XML files (*.xml)|*.xml",
			wxFD_OPEN | wxFD_FILE_MUST_EXIST);
		if (dlg.ShowModal() == wxID_CANCEL) return;

		try
		{
			std::pair<S2S_BoardCfg, std::vector<S2S_TargetCfg>> configs(
				ConfigUtil::fromXML(std::string(dlg.GetPath())));

			myBoardPanel->setConfig(configs.first);

			size_t i;
			for (i = 0; i < configs.second.size() && i < myTargets.size(); ++i)
			{
				myTargets[i]->setConfig(configs.second[i]);
			}

			for (; i < myTargets.size(); ++i)
			{
				myTargets[i]->setConfig(ConfigUtil::Default(i));
			}
		}
		catch (std::exception& e)
		{
			wxLogError(
				"Cannot load settings from file '%s'.\n%s",
				dlg.GetPath(),
				e.what());

			wxMessageBox(
				e.what(),
				"Load error",
				wxOK | wxICON_ERROR);
		}
	}

	void OnID_Firmware(wxCommandEvent& event)
	{
		TimerLock lock(myTimer);
		doFirmwareUpdate();
	}

	void OnID_LogWindow(wxCommandEvent& event)
	{
		myLogWindow->Show();
	}

	void doFirmwareUpdate()
	{
		wxFileDialog dlg(
			this,
			"Load firmware file",
			"",
			"",
			"SCSI2SD Firmware files (*.dfu)|*.dfu",
			wxFD_OPEN | wxFD_FILE_MUST_EXIST);
		if (dlg.ShowModal() == wxID_CANCEL) return;

		std::string filename(dlg.GetPath());

		wxWindowPtr<wxGenericProgressDialog> progress(
			new wxGenericProgressDialog(
				"Searching for bootloader",
				"Searching for bootloader",
				100,
				this,
				wxPD_AUTO_HIDE | wxPD_CAN_ABORT)
				);
		mmLogStatus("Searching for bootloader");
		while (true)
		{
			try
			{
				if (!myHID) myHID.reset(HID::Open());
				if (myHID)
				{
					mmLogStatus("Resetting SCSI2SD into bootloader");

					myHID->enterBootloader();
					myHID.reset();
				}


/*
				if (myDfu.hasDevice())
				{
					mmLogStatus("STM DFU Bootloader found");
					progress->Show(0);
					doDFUUpdate(filename);
					return;
				}
*/
			}
			catch (std::exception& e)
			{
				mmLogStatus(e.what());
				myHID.reset();
			}
			wxMilliSleep(100);
			if (!progress->Pulse())
			{
				return; // user cancelled.
			}
		}
	}

	void doDFUUpdate(const std::string& filename)
	{
		if (filename.find(".dfu") == std::string::npos)
		{
			wxMessageBox(
				"Wrong filename",
				"SCSI2SD V6 requires a .dfu file",
				wxOK | wxICON_ERROR);
			return;
		}

		std::stringstream ss;
		ss << "dfu-util --download \""
			<< filename.c_str() << "\" --alt 0 --reset";


		std::string cmd = ss.str();
		int result = system(cmd.c_str());
#ifdef WIN32
		if (result != 0)
#else
		if (WEXITSTATUS(result) != 0)
#endif
		{
			wxMessageBox(
				"Update failed",
				"Firmware update failed. Command = " + cmd,
				wxOK | wxICON_ERROR);
			return;
		}
	}

	void dumpSCSICommand(std::vector<uint8_t> buf)
        {
		std::stringstream msg;
		msg << std::hex;
		for (size_t i = 0; i < 32 && i < buf.size(); ++i)
		{
			msg << std::setfill('0') << std::setw(2) <<
			static_cast<int>(buf[i]) << ' ';
		}
		wxLogMessage(this, msg.str().c_str());
        }

	void logSCSI()
	{
		if (!mySCSILogChk->IsChecked() ||
			!myHID)
		{
			return;
		}
		try
		{
			std::vector<uint8_t> info;
			if (myHID->readSCSIDebugInfo(info))
			{
				dumpSCSICommand(info);
			}
		}
		catch (std::exception& e)
		{
			wxLogWarning(this, e.what());
			myHID.reset();
		}
	}

	void OnID_Timer(wxTimerEvent& event)
	{
		logSCSI();
		time_t now = time(NULL);
		if (now == myLastPollTime) return;
		myLastPollTime = now;

		// Check if we are connected to the HID device.
		try
		{
			if (myHID && !myHID->ping())
			{
				// Verify the USB HID connection is valid
				myHID.reset();
			}

			if (!myHID)
			{
				myHID.reset(HID::Open());
				if (myHID)
				{
					std::stringstream msg;
					msg << "SCSI2SD Ready, firmware version " <<
						myHID->getFirmwareVersionStr();
					mmLogStatus(msg.str());

					std::vector<uint8_t> csd(myHID->getSD_CSD());
					std::vector<uint8_t> cid(myHID->getSD_CID());
					std::stringstream sdinfo;
					sdinfo << "SD Capacity (512-byte sectors): " <<
						myHID->getSDCapacity() << std::endl;

					sdinfo << "SD CSD Register: ";
					if (sdCrc7(&csd[0], 15, 0) != (csd[15] >> 1))
					{
						sdinfo << "BADCRC ";
					}
					for (size_t i = 0; i < csd.size(); ++i)
					{
						sdinfo <<
							std::hex << std::setfill('0') << std::setw(2) <<
							static_cast<int>(csd[i]);
					}
					sdinfo << std::endl;
					sdinfo << "SD CID Register: ";
					if (sdCrc7(&cid[0], 15, 0) != (cid[15] >> 1))
					{
						sdinfo << "BADCRC ";
					}
					for (size_t i = 0; i < cid.size(); ++i)
					{
						sdinfo <<
							std::hex << std::setfill('0') << std::setw(2) <<
							static_cast<int>(cid[i]);
					}

					wxLogMessage(this, "%s", sdinfo.str());

					if (mySelfTestChk->IsChecked())
					{
						std::stringstream scsiInfo;
						scsiInfo << "SCSI Self-Test: " <<
							(myHID->scsiSelfTest() ? "Passed" : "FAIL");
						wxLogMessage(this, "%s", scsiInfo.str());
					}

					if (!myInitialConfig)
					{
/* This doesn't work properly, and causes crashes.
						wxCommandEvent loadEvent(wxEVT_NULL, ID_BtnLoad);
						GetEventHandler()->AddPendingEvent(loadEvent);
*/
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
			std::cerr << e.what() << std::endl;
			mmLogStatus(e.what());
		}

		evaluate();
	}

	void doLoad(wxCommandEvent& event)
	{
		TimerLock lock(myTimer);
		if (!myHID) return;

		mmLogStatus("Loading configuration");

		wxWindowPtr<wxGenericProgressDialog> progress(
			new wxGenericProgressDialog(
				"Load config settings",
				"Loading config settings",
				100,
				this,
				wxPD_CAN_ABORT | wxPD_REMAINING_TIME)
				);

		int currentProgress = 0;
		int totalProgress = 2;

		std::vector<uint8_t> cfgData(S2S_CFG_SIZE);
		uint32_t sector = myHID->getSDCapacity() - 2;
		for (size_t i = 0; i < 2; ++i)
		{
			std::stringstream ss;
			ss << "Reading sector " << sector;
			mmLogStatus(ss.str());
			currentProgress += 1;
			if (currentProgress == totalProgress)
			{
				ss.str("Load Complete.");
				mmLogStatus("Load Complete.");
			}

			if (!progress->Update(
					(100 * currentProgress) / totalProgress,
					ss.str()
					)
				)
			{
				goto abort;
			}

			std::vector<uint8_t> sdData;

			try
			{
				myHID->readSector(sector++, sdData);
			}
			catch (std::runtime_error& e)
			{
				mmLogStatus(e.what());
				goto err;
			}

			std::copy(
				sdData.begin(),
				sdData.end(),
				&cfgData[i * 512]);
		}

		myBoardPanel->setConfig(ConfigUtil::boardConfigFromBytes(&cfgData[0]));
		for (int i = 0; i < S2S_MAX_TARGETS; ++i)
		{
			myTargets[i]->setConfig(
				ConfigUtil::fromBytes(
					&cfgData[sizeof(S2S_BoardCfg) + i * sizeof(S2S_TargetCfg)]
					)
				);
		}

		myInitialConfig = true;
		goto out;

	err:
		mmLogStatus("Load failed");
		progress->Update(100, "Load failed");
		goto out;

	abort:
		mmLogStatus("Load Aborted");

	out:
		return;

	}

	void doSave(wxCommandEvent& event)
	{
		TimerLock lock(myTimer);
		if (!myHID) return;

		mmLogStatus("Saving configuration");

		wxWindowPtr<wxGenericProgressDialog> progress(
			new wxGenericProgressDialog(
				"Save config settings",
				"Saving config settings",
				100,
				this,
				wxPD_CAN_ABORT | wxPD_REMAINING_TIME)
				);


		int currentProgress = 0;
		int totalProgress = 2;

		std::vector<uint8_t> cfgData(
			ConfigUtil::boardConfigToBytes(myBoardPanel->getConfig())
			);
		for (int i = 0; i < S2S_MAX_TARGETS; ++i)
		{
			std::vector<uint8_t> raw(
				ConfigUtil::toBytes(myTargets[i]->getConfig())
				);
			cfgData.insert(cfgData.end(), raw.begin(), raw.end());
		}

		uint32_t sector = myHID->getSDCapacity() - 2;

		for (size_t i = 0; i < 2; ++i)
		{
			std::stringstream ss;
			ss << "Programming sector flash array " << sector;
			mmLogStatus(ss.str());
			currentProgress += 1;

			if (currentProgress == totalProgress)
			{
				ss.str("Save Complete.");
				mmLogStatus("Save Complete.");
			}
			if (!progress->Update(
					(100 * currentProgress) / totalProgress,
					ss.str()
					)
				)
			{
				goto abort;
			}

			try
			{
				std::vector<uint8_t> buf;
				buf.insert(buf.end(), &cfgData[i * 512], &cfgData[(i+1) * 512]);
				myHID->writeSector(sector++, buf);
			}
			catch (std::runtime_error& e)
			{
				mmLogStatus(e.what());
				goto err;
			}
		}

		myHID.reset();

		goto out;

	err:
		mmLogStatus("Save failed");
		progress->Update(100, "Save failed");
		goto out;

	abort:
		mmLogStatus("Save Aborted");

	out:
		return;

	}

	// Note: Don't confuse this with the wxApp::OnExit virtual method
	void OnExitEvt(wxCommandEvent& event);

	void OnCloseEvt(wxCloseEvent& event);

	void OnAbout(wxCommandEvent& event)
	{
		wxMessageBox(
			"SCSI2SD (scsi2sd-util6)\n"
			"Copyright (C) 2014-2016 Michael McMaster <michael@codesrc.com>\n"
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

			"About scsi2sd-util6", wxOK | wxICON_INFORMATION );
	}

	wxDECLARE_EVENT_TABLE();
};

wxBEGIN_EVENT_TABLE(AppFrame, wxFrame)
	EVT_MENU(AppFrame::ID_ConfigDefaults, AppFrame::OnID_ConfigDefaults)
	EVT_MENU(AppFrame::ID_Firmware, AppFrame::OnID_Firmware)
	EVT_MENU(AppFrame::ID_LogWindow, AppFrame::OnID_LogWindow)
	EVT_MENU(AppFrame::ID_SaveFile, AppFrame::OnID_SaveFile)
	EVT_MENU(AppFrame::ID_OpenFile, AppFrame::OnID_OpenFile)
	EVT_MENU(wxID_EXIT, AppFrame::OnExitEvt)
	EVT_MENU(wxID_ABOUT, AppFrame::OnAbout)

	EVT_TIMER(AppFrame::ID_Timer, AppFrame::OnID_Timer)

	EVT_COMMAND(wxID_ANY, ConfigChangedEvent, AppFrame::onConfigChanged)

	EVT_BUTTON(ID_BtnSave, AppFrame::doSave)
	EVT_BUTTON(ID_BtnLoad, AppFrame::doLoad)

	EVT_CLOSE(AppFrame::OnCloseEvt)

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

void
AppFrame::OnExitEvt(wxCommandEvent& event)
{
	wxGetApp().ExitMainLoop();
}

void
AppFrame::OnCloseEvt(wxCloseEvent& event)
{
	wxGetApp().ExitMainLoop();
}

