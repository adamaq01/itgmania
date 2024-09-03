#ifndef INPUT_HANDLER_WIN32_EAMIO_H
#define INPUT_HANDLER_WIN32_EAMIO_H

#include "InputHandler.h"
#include "RageThreads.h"

#include <cstdint>
#include <vector>


static bool _eamiodll_loaded = false;

#define EAMIO_DEVICEID_P1 DEVICE_JOY5
#define EAMIO_DEVICEID_P2 DEVICE_JOY6

enum eam_io_keypad_scan_code {
    EAM_IO_KEYPAD_0 = 0,
    EAM_IO_KEYPAD_1 = 1,
    EAM_IO_KEYPAD_4 = 2,
    EAM_IO_KEYPAD_7 = 3,
    EAM_IO_KEYPAD_00 = 4,
    EAM_IO_KEYPAD_2 = 5,
    EAM_IO_KEYPAD_5 = 6,
    EAM_IO_KEYPAD_8 = 7,
    EAM_IO_KEYPAD_DECIMAL = 8,
    EAM_IO_KEYPAD_3 = 9,
    EAM_IO_KEYPAD_6 = 10,
    EAM_IO_KEYPAD_9 = 11,

    EAM_IO_KEYPAD_COUNT = 12, /* Not an actual scan code */
};

class InputHandler_Win32_eamio: public InputHandler
{
public:
	InputHandler_Win32_eamio();
	~InputHandler_Win32_eamio();

	RString GetDeviceSpecificInputString( const DeviceInput &di );
	void GetDevicesAndDescriptions( std::vector<InputDeviceInfo>& vDevicesOut );

private:
	RageThread InputThread;
	bool m_bShutdown;

	bool LoadDLL();
	bool MapFunctions();
	static int InputThread_Start( void *p );
	void InputThreadMain();

	void PushInputState(std::uint16_t newInput, bool isP1);
};

/*
 * (c) 2024 din, adamaq01
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, and/or sell copies of the Software, and to permit persons to
 * whom the Software is furnished to do so, provided that the above
 * copyright notice(s) and this permission notice appear in all copies of
 * the Software and that both the above copyright notice(s) and this
 * permission notice appear in supporting documentation.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF
 * THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS
 * INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */


#endif
