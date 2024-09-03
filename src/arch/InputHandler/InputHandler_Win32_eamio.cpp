#include "global.h"
#include "InputHandler_Win32_eamio.h"

#include "RageLog.h"
#include "RageUtil.h"
#include "RageInputDevice.h"
#include "PrefsManager.h"

#include <cstdint>
#include <vector>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>

typedef int (*thread_create_t)(
	int (*proc)(void*), void* ctx, std::uint32_t stack_sz, unsigned int priority);
typedef void (*thread_join_t)(int thread_id, int* result);
typedef void (*thread_destroy_t)(int thread_id);

typedef void (*log_formatter_t)(const char* module, const char* fmt, ...);

typedef bool (*EAMIO_IO_INIT)(thread_create_t thread_create,
	thread_join_t thread_join,
	thread_destroy_t thread_destroy
	);
static EAMIO_IO_INIT eamio_io_init;

typedef std::uint16_t (*EAMIO_READ_PAD)(std::uint8_t unit_no);
static EAMIO_READ_PAD eamio_get_keypad_state;

typedef int (*EAMIO_FINI)();
static EAMIO_FINI eamio_fini;

HINSTANCE hEAMIOdll = nullptr;

REGISTER_INPUT_HANDLER_CLASS2( eamio, Win32_eamio );


struct shim_ctx {
	HANDLE barrier;
	int (*proc)(void*);
	void* ctx;
};

static unsigned int eamio_crt_thread_shim(void* outer_ctx)
{
	LOG->Trace("eamio_crt_thread_shim");

	struct shim_ctx* sctx = (struct shim_ctx*)outer_ctx;
	int (*proc)(void*);
	void* inner_ctx;

	proc = sctx->proc;
	inner_ctx = sctx->ctx;

	SetEvent(sctx->barrier);

	return proc(inner_ctx);
}


int eamio_crt_thread_create(
	int (*proc)(void*), void* ctx, std::uint32_t stack_sz, unsigned int priority)
{

	LOG->Trace("eamio_crt_thread_create");

	struct shim_ctx sctx;
	std::uintptr_t thread_id;

	sctx.barrier = CreateEvent(NULL, TRUE, FALSE, NULL);
	sctx.proc = proc;
	sctx.ctx = ctx;

	thread_id = _beginthreadex(NULL, stack_sz, eamio_crt_thread_shim, &sctx, 0, NULL);

	WaitForSingleObject(sctx.barrier, INFINITE);
	CloseHandle(sctx.barrier);

	LOG->Trace("eamio_crt_thread_create %d", thread_id);

	return (int)thread_id;

}

void eamio_crt_thread_destroy(int thread_id)
{
	LOG->Trace("eamio_crt_thread_destroy %d", thread_id);

	CloseHandle((HANDLE)(std::uintptr_t)thread_id);
}


void eamio_crt_thread_join(int thread_id, int* result)
{
	LOG->Trace("eamio_crt_thread_join %d", thread_id);

	WaitForSingleObject((HANDLE)(std::uintptr_t)thread_id, INFINITE);

	if (result)
	{
		GetExitCodeThread((HANDLE)(std::uintptr_t)thread_id, (DWORD*)result);
	}
}



int eamio_filter(unsigned int, struct _EXCEPTION_POINTERS*)
{
	return EXCEPTION_EXECUTE_HANDLER;
}

bool InputHandler_Win32_eamio::LoadDLL()
{
	hEAMIOdll = LoadLibrary("eamio.dll");

	if (hEAMIOdll == nullptr)
	{
		MessageBox(nullptr, "Could not load eamio.dll. Ensure it is in the Program directory, the proper C++ Visual C++ Redistributable Runtimes dependencies are installed, you have matched your architectures (x86/x64), and any additional dll dependencies of eamio.dll are available.", "ERROR", MB_OK);
		return false;
	}

	_eamiodll_loaded = true;

	return true;
}

bool InputHandler_Win32_eamio::MapFunctions()
{
	__try
	{
		//pull the functions we need out of the dll.
		eamio_io_init = (EAMIO_IO_INIT)GetProcAddress(hEAMIOdll, "eam_io_init");
		eamio_get_keypad_state = (EAMIO_READ_PAD)GetProcAddress(hEAMIOdll, "eam_io_get_keypad_state");
		eamio_fini = (EAMIO_FINI)GetProcAddress(hEAMIOdll, "eam_io_fini");
	}
	__except (eamio_filter(GetExceptionCode(), GetExceptionInformation()))
	{
		MessageBox(nullptr, "Could not map functions in eamio.dll", "ERROR", MB_OK);
		FreeLibrary(hEAMIOdll);
	}

	return true;
}

InputHandler_Win32_eamio::InputHandler_Win32_eamio()
{
	if (!_eamiodll_loaded && LoadDLL())
	{
		_eamiodll_loaded = MapFunctions();
	}

	bool eamInit = false;

	thread_create_t thread_impl_create = eamio_crt_thread_create;
	thread_join_t thread_impl_join = eamio_crt_thread_join;
	thread_destroy_t thread_impl_destroy = eamio_crt_thread_destroy;


	if (eamio_io_init != nullptr)
	{
		eamInit = eamio_io_init(thread_impl_create, thread_impl_join, thread_impl_destroy);
	}

	if (!eamInit)
	{
		LOG->Warn("Could not init eamio.dll");
	}

	m_bShutdown = false;

	if(_eamiodll_loaded && eamInit && PREFSMAN->m_bThreadedInput )
	{
		InputThread.SetName( "eamio thread" );
		InputThread.Create( InputThread_Start, this );
	}
}

InputHandler_Win32_eamio::~InputHandler_Win32_eamio()
{
	if( InputThread.IsCreated() )
	{
		m_bShutdown = true;
		LOG->Trace( "Shutting down eamio thread ..." );
		InputThread.Wait();
		LOG->Trace( "eamio thread shut down." );


		if (eamio_fini != nullptr)
		{
			LOG->Trace("calling eamio dll fini");
			eamio_fini();
			LOG->Trace("eamio dll complete");
		}

	}
}

RString InputHandler_Win32_eamio::GetDeviceSpecificInputString( const DeviceInput &di )
{
	switch (di.button)
	{
		case JOY_BUTTON_1:  return "eamio 0";
		case JOY_BUTTON_2:  return "eamio 1";
		case JOY_BUTTON_3:  return "eamio 4";
		case JOY_BUTTON_4:  return "eamio 7";
		case JOY_BUTTON_5:  return "eamio 00";
		case JOY_BUTTON_6:  return "eamio 2";
		case JOY_BUTTON_7:  return "eamio 5";
		case JOY_BUTTON_8:  return "eamio 8";
		case JOY_BUTTON_9:  return "eamio Decimal";
		case JOY_BUTTON_10: return "eamio 3";
		case JOY_BUTTON_11: return "eamio 6";
		case JOY_BUTTON_12: return "eamio 9";
		case JOY_BUTTON_13: return "eamio UNUSED 13";
		case JOY_BUTTON_14: return "eamio UNUSED 14";
		case JOY_BUTTON_15: return "eamio UNUSED 15";
		case JOY_BUTTON_16: return "eamio UNUSED 16";
		case JOY_BUTTON_17: return "eamio UNUSED 17";
		case JOY_BUTTON_18: return "eamio UNUSED 18";
		case JOY_BUTTON_19: return "eamio UNUSED 19";
		case JOY_BUTTON_20: return "eamio UNUSED 20";
		case JOY_BUTTON_21: return "eamio UNUSED 21";
		case JOY_BUTTON_22: return "eamio UNUSED 22";
		case JOY_BUTTON_23: return "eamio UNUSED 23";
		case JOY_BUTTON_24: return "eamio UNUSED 24";
		case JOY_BUTTON_25: return "eamio UNUSED 25";
		case JOY_BUTTON_26: return "eamio UNUSED 26";
		case JOY_BUTTON_27: return "eamio UNUSED 27";
		case JOY_BUTTON_28: return "eamio UNUSED 28";
		case JOY_BUTTON_29: return "eamio UNUSED 29";
		case JOY_BUTTON_30: return "eamio UNUSED 30";
		case JOY_BUTTON_31: return "eamio UNUSED 31";
		case JOY_BUTTON_32: return "eamio UNUSED 32";
	}

	return InputHandler::GetDeviceSpecificInputString(di);
}

void InputHandler_Win32_eamio::GetDevicesAndDescriptions( std::vector<InputDeviceInfo>& vDevicesOut )
{
	vDevicesOut.push_back(InputDeviceInfo(InputDevice(EAMIO_DEVICEID_P1), "eamio P1"));
	vDevicesOut.push_back(InputDeviceInfo(InputDevice(EAMIO_DEVICEID_P2), "eamio P2"));
}

int InputHandler_Win32_eamio::InputThread_Start( void *p )
{
	((InputHandler_Win32_eamio *) p)->InputThreadMain();
	return 0;
}

void InputHandler_Win32_eamio::InputThreadMain()
{
	std::uint16_t prevInputP1 = 0, newInputP1 = 0, prevInputP2 = 0, newInputP2 = 0;

	while (!m_bShutdown)
	{
		InputHandler::UpdateTimer();

		newInputP1 = eamio_get_keypad_state(0);
		newInputP2 = eamio_get_keypad_state(1);

		if (prevInputP1 != newInputP1)
		{
			PushInputState(newInputP1, true);
		}

		if (prevInputP2 != newInputP2)
		{
			PushInputState(newInputP2, false);
		}

		prevInputP1 = newInputP1;
		prevInputP2 = newInputP2;
	}
}

void InputHandler_Win32_eamio::PushInputState(std::uint16_t newInput, bool isP2)
{
	for (int i = 0; i < EAM_IO_KEYPAD_COUNT; i++)
	{
		bool pressed = (newInput & (1 << i));

		//return from eamio is active high.
		DeviceInput di(isP2 ? EAMIO_DEVICEID_P1 : EAMIO_DEVICEID_P2, enum_add2(JOY_BUTTON_1, i), pressed);

		//If we're in a thread, our timestamp is accurate.
		if (InputThread.IsCreated())
			di.ts.Touch();

		ButtonPressed(di);
	}
}




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

