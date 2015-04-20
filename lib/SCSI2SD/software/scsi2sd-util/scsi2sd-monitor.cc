//	Copyright (C) 2015 Michael McMaster <michael@codesrc.com>
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
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/log.h>
#include <wx/notebook.h>
#include <wx/progdlg.h>
#include <wx/utils.h>
#include <wx/windowptr.h>
#include <wx/thread.h>

#include <zipper.hh>

#include "ConfigUtil.hh"
#include "TargetPanel.hh"
#include "SCSI2SD_Bootloader.hh"
#include "SCSI2SD_HID.hh"
#include "Firmware.hh"

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

#define MIN_FIRMWARE_VERSION 0x0400

using namespace SCSI2SD;

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
		wxFrame(NULL, wxID_ANY, "scsi2sd-monitor", wxPoint(50, 50), wxSize(250, 150))
	{
		wxFlexGridSizer *fgs = new wxFlexGridSizer(3, 2, 9, 25);

		fgs->Add(new wxStaticText(this, wxID_ANY, wxT("SCSI2SD Device:")));
		myBoardText = new wxStaticText(this, wxID_ANY, wxT(""));
		fgs->Add(myBoardText);
		fgs->Add(new wxStaticText(this, wxID_ANY, wxT("SD Test:")));
		mySDText = new wxStaticText(this, wxID_ANY, wxT(""));
		fgs->Add(mySDText);
		fgs->Add(new wxStaticText(this, wxID_ANY, wxT("SCSI Test:")));
		mySCSIText = new wxStaticText(this, wxID_ANY, wxT(""));
		fgs->Add(mySCSIText);

		wxBoxSizer* hbox = new wxBoxSizer(wxHORIZONTAL);
		hbox->Add(fgs, 1, wxALL | wxEXPAND, 15);
		this->SetSizer(hbox);
		Centre();

		//Fit(); // Needed to reduce window size on Windows
		//FitInside(); // Needed on Linux to prevent status bar overlap

		myTimer = new wxTimer(this, ID_Timer);
		myTimer->Start(1000);
	}

private:
	wxTimer* myTimer;
	shared_ptr<HID> myHID;
	shared_ptr<Bootloader> myBootloader;

	wxStaticText* myBoardText;
	wxStaticText* mySDText;
	wxStaticText* mySCSIText;

	enum
	{
		ID_ConfigDefaults = wxID_HIGHEST + 1,
		ID_Timer
	};

	void evaluate()
	{
		if (myHID)
		{
			std::stringstream msg;
			msg << "Ready, " <<
				myHID->getFirmwareVersionStr();
			myBoardText->SetLabelText(msg.str());


			std::vector<uint8_t> csd(myHID->getSD_CSD());
			std::vector<uint8_t> cid(myHID->getSD_CID());

			bool sdGood = false;
			for (size_t i = 0; i < 16; ++i)
			{
				if (csd[i] != 0)
				{
					sdGood = true;
					//break;
				}
			}

			sdGood = sdGood &&
				(sdCrc7(&csd[0], 15, 0) == (csd[15] >> 1)) &&
				(sdCrc7(&cid[0], 15, 0) == (cid[15] >> 1));

			if (sdGood)
			{
				mySDText->SetLabelText("OK");
			}
			else
			{
				mySDText->SetLabelText("FAIL");
			}

			if (myHID->scsiSelfTest())
			{
				mySCSIText->SetLabelText("OK");
			}
			else
			{
				mySCSIText->SetLabelText("FAIL");
			}
		}
		else
		{
			if (myBootloader)
			{
				myBoardText->SetLabelText("Bootloader");
			}
			else
			{
				myBoardText->SetLabelText("Missing");
			}
			mySDText->SetLabelText("-");
			mySCSIText->SetLabelText("-");
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
			}

			if (myHID && !myHID->ping())
			{
				// Verify the USB HID connection is valid
std::cerr << "RESET!" << std::endl;
				myHID.reset();
			}

			if (!myHID)
			{
				myHID.reset(HID::Open());

			}
		}
		catch (std::runtime_error& e)
		{
			std::cerr << e.what() << std::endl;
		}

		evaluate();
	}

	// Note: Don't confuse this with the wxApp::OnExit virtual method
	void OnExitEvt(wxCommandEvent& event)
	{
		Close(true);
	}

	wxDECLARE_EVENT_TABLE();
};

wxBEGIN_EVENT_TABLE(AppFrame, wxFrame)
	EVT_MENU(wxID_EXIT, AppFrame::OnExitEvt)

	EVT_TIMER(AppFrame::ID_Timer, AppFrame::OnID_Timer)

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


