#pragma once

#ifndef WINSIMPLE_VIDEO_HPP
#define WINSIMPLE_VIDEO_HPP


#ifndef WINVER
#define WINVER _WIN32_WINNT_WIN7
#endif

// Must be first: lets guiddef.h DEFINE all the GUIDs locally instead of
// declaring them extern. On MinGW this is required because the prebuilt
// libmfuuid.a / libuuid.a do not export every MF GUID (e.g. the video
// renderer service GUID).
#include <initguid.h>

#include <windows.h>

#include <shlwapi.h>
#include <mfapi.h>
#include <mfidl.h>

#ifdef __MINGW32__
// MinGW's mfidl.h is missing MF_SESSION_CLOCK.
// {8737B98D-814D-4CDD-A78C-7E2C6F4A8F9C}
DEFINE_GUID(MF_SESSION_CLOCK,
    0x8737b98d, 0x814d, 0x4cdd,
    0xa7, 0x8c, 0x7e, 0x2c, 0x6f, 0x4a, 0x8f, 0x9c);

// {bac0145b-5528-45fd-8ff2-9c5b814d4eee}
DEFINE_GUID(MF_TOPOLOGY_STATIC_PLAYBACK_OPTIMIZATIONS,
    0xbac0145b, 0x5528, 0x45fd,
    0x8f, 0xf2, 0x9c, 0x5b, 0x81, 0x4d, 0x4e, 0xee);

#endif


#include <mfobjects.h>
#include <mferror.h>
#include <evr.h>
#include <assert.h>

//-lmfplat -lmf -lmfuuid -levr -lshlwapi -lole32 -luuid
// MinGW ignores these; they're harmless and help if you ever build with MSVC.
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "evr.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")


namespace ws
{
	class InitIMF
	{
		public:
		InitIMF();
		~InitIMF();		
		HRESULT result;
	}initIMF;


	class Video : public IMFAsyncCallback
	{
	public:
		Video();
		~Video();

		// Non-copyable
		Video(const Video&) = delete;
		Video& operator=(const Video&) = delete;
		//moveable
		Video(Video&& other) noexcept;
		Video& operator=(Video&& other) noexcept; 


		bool open(std::string sURL, ws::Window &window);

		bool play();
		bool pause();
		bool stop();
		std::string getStatus()
		{return status;}
		double getProgress();
		bool setProgress(double seconds);
		double getLength();
		bool isFinished();
		void setLoop(bool loops = true);		
		void close();
		bool setVolume(int level);
		int getVolume();

		const UINT WS_VIDEO_EVENT = WM_APP + 1;
		
		private:

		std::string status = "stopped";
		bool m_loop = false;
		bool m_restartPending = false;
		bool m_pausePending = false;
		HRESULT HandleEvent(IMFMediaEvent* pEvent);
		void Shutdown();
		HANDLE m_hCloseEvent = nullptr;

		void updateVideoPosition();

		// ----- IUnknown (required by IMFAsyncCallback) -----
		STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
		STDMETHODIMP_(ULONG) AddRef() override;
		STDMETHODIMP_(ULONG) Release() override;

		// ----- IMFAsyncCallback -----
		STDMETHODIMP GetParameters(DWORD* pdwFlags, DWORD* pdwQueue) override;
		STDMETHODIMP Invoke(IMFAsyncResult* pResult) override;


		HRESULT CreateSession();
		HRESULT CreateMediaSource(const WCHAR* sURL);
		HRESULT CreateTopology(HWND hwndVideo);
		HRESULT AddSourceNode(
			IMFTopology* pTopology,
			IMFMediaSource* pSource,
			IMFPresentationDescriptor* pPD,
			IMFStreamDescriptor* pSD,
			IMFTopologyNode** ppNode);
		HRESULT AddOutputNode(
			IMFTopology* pTopology,
			IMFStreamDescriptor* pSD,
			IMFTopologyNode* pSourceNode,
			HWND hwndVideo);
		HRESULT StartPlayback();

		// Media Session event handlers
		HRESULT OnTopologyStatus(IMFMediaEvent* pEvent);
		HRESULT OnPresentationEnded(IMFMediaEvent* pEvent);
		HRESULT OnSessionStarted(IMFMediaEvent* pEvent);
		HRESULT OnSessionStopped(IMFMediaEvent* pEvent);
		
		// COM refcount
		LONG m_cRef;

		// Media Foundation objects
		IMFMediaSession*           m_pSession = nullptr;
		IMFSimpleAudioVolume*      m_pVolume = nullptr;
		IMFMediaSource*            m_pSource = nullptr;
		IMFPresentationDescriptor* m_pPD = nullptr;
		IMFVideoDisplayControl*    m_pDisplayControl = nullptr;
		
		ws::Window *windowPtr = nullptr; // Window to receive WS_VIDEO_PLAYER_EVENT
		int handler = -1;
	};
	
	#ifdef WINSIMPLE_VIDEO_IMPL

	InitIMF::InitIMF()
	{
		result = MFStartup(MF_VERSION);
	}
	InitIMF::~InitIMF()
	{
		if(SUCCEEDED(result))
			MFShutdown();		
	}	

	Video::Video()
		: m_cRef(1)
		, m_pSession(nullptr)
		, m_pSource(nullptr)
		, m_pPD(nullptr)
		, status("stopped")
		, windowPtr(nullptr)
		, handler(-1)
	{
		m_hCloseEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
	}

	Video::~Video()
	{
		Shutdown();
		if (m_hCloseEvent) { CloseHandle(m_hCloseEvent); m_hCloseEvent = nullptr; }
	}

	Video::Video(Video&& other) noexcept
		: status(std::move(other.status))
		, m_loop(other.m_loop)
		, m_restartPending(other.m_restartPending)
		, m_pausePending(other.m_pausePending)
		, m_hCloseEvent(other.m_hCloseEvent)
		, m_cRef(1)
		, m_pSession(other.m_pSession)
		, m_pVolume(other.m_pVolume)
		, m_pSource(other.m_pSource)
		, m_pPD(other.m_pPD)
		, windowPtr(other.windowPtr)
		, handler(other.handler)
	{
		other.m_hCloseEvent = nullptr;
		other.m_pSession = nullptr;
		other.m_pVolume  = nullptr;
		other.m_pSource  = nullptr;
		other.m_pPD      = nullptr;
		other.windowPtr = nullptr;
	}

	Video& Video::operator=(Video&& other) noexcept
	{
		if (this != &other) {
			Shutdown();
			if (m_hCloseEvent) { CloseHandle(m_hCloseEvent); m_hCloseEvent = nullptr; }

			status            = std::move(other.status);
			m_loop            = other.m_loop;
			m_restartPending  = other.m_restartPending;
			m_pausePending	  = other.m_pausePending;
			m_hCloseEvent     = other.m_hCloseEvent;
			m_pSession        = other.m_pSession;
			m_pVolume         = other.m_pVolume;
			m_pSource         = other.m_pSource;
			m_pPD             = other.m_pPD;
			windowPtr       = other.windowPtr;
			handler = other.handler;
			
			other.m_hCloseEvent = nullptr;
			other.m_pSession = nullptr;
			other.m_pVolume  = nullptr;
			other.m_pSource  = nullptr;
			other.m_pPD      = nullptr;
			other.windowPtr = nullptr;
		}
		return *this;
	}

	// ---------------------------------------------------------------------------
	// IUnknown
	// ---------------------------------------------------------------------------

	STDMETHODIMP Video::QueryInterface(REFIID riid, void** ppv)
	{
		static const QITAB qit[] = {
			QITABENT(Video, IMFAsyncCallback),
			{ 0 },
		};
		return QISearch(this, qit, riid, ppv);
	}

	STDMETHODIMP_(ULONG) Video::AddRef()
	{
		return InterlockedIncrement(&m_cRef);
	}

	STDMETHODIMP_(ULONG) Video::Release()
	{
		ULONG cRef = InterlockedDecrement(&m_cRef);
		if (cRef == 0)
			delete this;
		return cRef;
	}

	// ---------------------------------------------------------------------------
	// IMFAsyncCallback
	// ---------------------------------------------------------------------------

	STDMETHODIMP Video::GetParameters(DWORD* pdwFlags, DWORD* pdwQueue)
	{
		// Use the default work queue.
		*pdwFlags = 0;
		*pdwQueue = MFASYNC_CALLBACK_QUEUE_STANDARD;
		return S_OK;
	}

	STDMETHODIMP Video::Invoke(IMFAsyncResult* pResult)
	{
		MediaEventType meType = MEUnknown;
		IMFMediaEvent* pEvent = nullptr;

		HRESULT hr = m_pSession->EndGetEvent(pResult, &pEvent);
		if (SUCCEEDED(hr))                  // <-- was: if (FAILED(hr)) return hr;
		{
			if (SUCCEEDED(pEvent->GetType(&meType)))
			{
				if(PostMessage(windowPtr->hwnd, WS_VIDEO_EVENT, (WPARAM)pEvent, (LPARAM)meType))
					pEvent = nullptr;
			}
			if (pEvent) pEvent->Release();
		}

		if (meType == MESessionClosed)
			SetEvent(m_hCloseEvent);                      // wake Shutdown
		else
			m_pSession->BeginGetEvent(this, nullptr);     // keep listening

		return S_OK;
	}


	bool Video::open(std::string sURL, ws::Window &window)
	{
		if (sURL.empty() || !window.hwnd)
			return false;

		// Already open?
		if (m_pSession || m_pSource)
			Shutdown(); //close the old

		windowPtr = &window;   // we post events to the video window

		HRESULT hr = CreateSession();
		if (FAILED(hr)) goto done;

		hr = CreateMediaSource(ws::toUTF16(sURL).c_str());
		if (FAILED(hr)) goto done;

		hr = CreateTopology(window.hwnd);

	done:
		if (FAILED(hr))
		{
			Shutdown();
			return false;
		}
		
		if(handler == -1)
		{
			handler = window.addMessageHandler([this](MSG msg){
				
				if (msg.message != WS_VIDEO_EVENT) return 0;
				
				IMFMediaEvent* pEvent = reinterpret_cast<IMFMediaEvent*>(msg.wParam);
				if (!pEvent) return 0;

				if (windowPtr && m_pSession)
					HandleEvent(pEvent);

				pEvent->Release();  				
				return 1;
			});
		}
		
		return true;
	}

	bool Video::play()
	{
		if (status != "paused" && status != "stopped")
			return false;

		if (!m_pSession || !m_pSource)
			return false;

		return StartPlayback();
	}

	bool Video::pause()
	{
		if (status != "playing")
			return false;

		if (!m_pSession || !m_pSource)
			return false;

		HRESULT hr = m_pSession->Pause();
		if (SUCCEEDED(hr))
			status = "paused";
		else
			return false;
		return true;
	}

	bool Video::stop()
	{
		if (!m_pSession || !m_pSource)
			return false;

		HRESULT hr = m_pSession->Stop();
		if (SUCCEEDED(hr))
			status = "stopped";
		else
			return false;
		return true;
	}

	void Video::Shutdown()
	{
		if(m_pDisplayControl) { m_pDisplayControl->Release(); m_pDisplayControl = nullptr; }
		
		
		if (m_pSession)
		{
			ResetEvent(m_hCloseEvent);
			m_pSession->Close();                          // async: queues MESessionClosed
			WaitForSingleObject(m_hCloseEvent, 5000);     // block until Invoke signals
			m_pSession->Release();
			m_pSession = nullptr;
		}
		m_pausePending = false;
		
		if (m_pSource) { m_pSource->Shutdown(); m_pSource->Release(); m_pSource = nullptr; }
		if (m_pPD)     { m_pPD->Release();      m_pPD     = nullptr; }
		if (m_pVolume) { m_pVolume->Release();   m_pVolume  = nullptr; }
		
		status      = "stopped";
		if(handler != -1)
			windowPtr->removeCustomHandler(handler);
		windowPtr = nullptr;
		handler = -1;
		
	}

	// ---------------------------------------------------------------------------
	// Internal helpers
	// ---------------------------------------------------------------------------

	HRESULT Video::CreateSession()
	{
		HRESULT hr = MFCreateMediaSession(nullptr, &m_pSession);
		if (FAILED(hr))
			return hr;

		// Start listening for events.
		return m_pSession->BeginGetEvent(this, nullptr);
	}

	HRESULT Video::CreateMediaSource(const WCHAR* sURL)
	{
		IMFSourceResolver* pResolver = nullptr;
		MF_OBJECT_TYPE ObjectType = MF_OBJECT_INVALID;

		HRESULT hr = MFCreateSourceResolver(&pResolver);
		if (FAILED(hr)) goto done;

		hr = pResolver->CreateObjectFromURL(
			sURL,
			MF_RESOLUTION_MEDIASOURCE,
			nullptr,
			&ObjectType,
			(IUnknown**)&m_pSource);
		if (FAILED(hr)) goto done;

		hr = m_pSource->CreatePresentationDescriptor(&m_pPD);

	done:
		if (pResolver) pResolver->Release();
		return hr;
	}

	HRESULT Video::CreateTopology(HWND hwndVideo)
	{
		IMFTopology* pTopology = nullptr;
		HRESULT hr = MFCreateTopology(&pTopology);
		if (FAILED(hr)) return hr;

		pTopology->SetUINT32(MF_TOPOLOGY_STATIC_PLAYBACK_OPTIMIZATIONS, TRUE);

		DWORD cStreams = 0;
		hr = m_pPD->GetStreamDescriptorCount(&cStreams);
		if (FAILED(hr)) goto done;

		for (DWORD i = 0; i < cStreams; i++)
		{
			BOOL fSelected = FALSE;
			IMFStreamDescriptor* pSD = nullptr;
			IMFTopologyNode* pSourceNode = nullptr;

			hr = m_pPD->GetStreamDescriptorByIndex(i, &fSelected, &pSD);
			if (FAILED(hr)) goto done;

			if (fSelected)
			{
				hr = AddSourceNode(pTopology, m_pSource, m_pPD, pSD, &pSourceNode);
				if (FAILED(hr)) { pSD->Release(); goto done; }

				hr = AddOutputNode(pTopology, pSD, pSourceNode, hwndVideo);
				if (FAILED(hr)) { pSourceNode->Release(); pSD->Release(); goto done; }

				pSourceNode->Release();
			}

			pSD->Release();
		}

		hr = m_pSession->SetTopology(0, pTopology);

	done:
		pTopology->Release();
		return hr;
	}

	HRESULT Video::AddSourceNode(
		IMFTopology* pTopology,
		IMFMediaSource* pSource,
		IMFPresentationDescriptor* pPD,
		IMFStreamDescriptor* pSD,
		IMFTopologyNode** ppNode)
	{
		IMFTopologyNode* pNode = nullptr;
		HRESULT hr = MFCreateTopologyNode(MF_TOPOLOGY_SOURCESTREAM_NODE, &pNode);
		if (FAILED(hr)) return hr;

		hr = pNode->SetUnknown(MF_TOPONODE_SOURCE, pSource);
		if (FAILED(hr)) goto done;

		hr = pNode->SetUnknown(MF_TOPONODE_PRESENTATION_DESCRIPTOR, pPD);
		if (FAILED(hr)) goto done;

		hr = pNode->SetUnknown(MF_TOPONODE_STREAM_DESCRIPTOR, pSD);
		if (FAILED(hr)) goto done;

		hr = pTopology->AddNode(pNode);
		if (FAILED(hr)) goto done;

		*ppNode = pNode;
		(*ppNode)->AddRef();
		pNode->Release();
		return S_OK;

	done:
		pNode->Release();
		return hr;
	}

	HRESULT Video::AddOutputNode(
		IMFTopology* pTopology,
		IMFStreamDescriptor* pSD,
		IMFTopologyNode* pSourceNode,
		HWND hwndVideo)
	{
		IMFMediaTypeHandler* pHandler = nullptr;
		IMFActivate*         pRendererActivate = nullptr;
		IMFTopologyNode*     pOutputNode = nullptr;
		GUID                 majorType = GUID_NULL;   // declared before any goto

		HRESULT hr = pSD->GetMediaTypeHandler(&pHandler);
		if (FAILED(hr)) goto done;

		// Determine audio vs. video.
		hr = pHandler->GetMajorType(&majorType);
		if (FAILED(hr)) goto done;

		if (majorType == MFMediaType_Video)
		{
			// Render video into the caller's window.
			hr = MFCreateVideoRendererActivate(hwndVideo, &pRendererActivate);
			if (FAILED(hr)) goto done;
		}
		else if (majorType == MFMediaType_Audio)
		{
			// Use the default audio renderer.
			hr = MFCreateAudioRendererActivate(&pRendererActivate);
			if (FAILED(hr)) goto done;
		}
		else
		{
			hr = MF_E_TOPO_UNSUPPORTED;
			goto done;
		}

		hr = MFCreateTopologyNode(MF_TOPOLOGY_OUTPUT_NODE, &pOutputNode);
		if (FAILED(hr)) goto done;

		hr = pOutputNode->SetObject(pRendererActivate);
		if (FAILED(hr)) goto done;

		hr = pTopology->AddNode(pOutputNode);
		if (FAILED(hr)) goto done;

		// Connect source -> output.
		hr = pSourceNode->ConnectOutput(0, pOutputNode, 0);

	done:
		if (pOutputNode)       pOutputNode->Release();
		if (pRendererActivate) pRendererActivate->Release();
		if (pHandler)          pHandler->Release();
		return hr;
	}

	HRESULT Video::StartPlayback()
	{
		assert(m_pSession != nullptr);

		PROPVARIANT varStart;
		PropVariantInit(&varStart);

		HRESULT hr = m_pSession->Start(&GUID_NULL, &varStart);
		if (SUCCEEDED(hr))
			status = "playing";

		PropVariantClear(&varStart);
		return hr;
	}

	// ---------------------------------------------------------------------------
	// Media Session event handling
	// ---------------------------------------------------------------------------

	HRESULT Video::HandleEvent(IMFMediaEvent* pEvent)
	{
		if (!pEvent)
			return E_POINTER;

		MediaEventType meType = MEUnknown;
		HRESULT hr = pEvent->GetType(&meType);
		if (FAILED(hr))
			return hr;

		switch (meType)
		{
		case MESessionTopologyStatus:
			hr = OnTopologyStatus(pEvent);
			break;

		case MEEndOfPresentation:
			hr = OnPresentationEnded(pEvent);
			break;

		case MESessionStarted:
			hr = OnSessionStarted(pEvent);
			break;

		case MESessionClosed:
			// Session closed – nothing to do.
			hr = S_OK;
			break;
		case MESessionPaused:
			status = "paused";
			hr = S_OK;
			break;
		case MESessionStopped:
			hr = OnSessionStopped(pEvent);
			break;
		default:
			hr = S_OK;
			break;
		}
		
		return hr;
	}

	HRESULT Video::OnTopologyStatus(IMFMediaEvent* pEvent)
	{
		UINT32 topstatus = 0;
		HRESULT hr = pEvent->GetUINT32(MF_EVENT_TOPOLOGY_STATUS, &topstatus);
		if (FAILED(hr))
			return hr;
		//this means the topology is ready and autoplay could be done here. I'm just going to make it an explicite play() command.
		//return default ok.
		
		if(topstatus == MF_TOPOSTATUS_READY)
		{
			if(!m_pDisplayControl)
			{
				hr = MFGetService(
				m_pSession,
				MR_VIDEO_RENDER_SERVICE,
				IID_PPV_ARGS(&m_pDisplayControl));
				
				if(SUCCEEDED(hr) && m_pDisplayControl)
				{
					m_pDisplayControl->SetRenderingPrefs(MFVideoRenderPrefs_DoNotRepaintOnStop);
					m_pDisplayControl->SetAspectRatioMode(MFVideoARMode_PreservePicture);
					
					if(windowPtr)
						updateVideoPosition();
				}
			}
		}
		
		
		return S_OK;
	}

	HRESULT Video::OnPresentationEnded(IMFMediaEvent* pEvent)
	{
		if(m_loop) 
		{
			m_restartPending = true;
			m_pSession->Stop(); 
		} 
		else
			status = "stopped";
		return S_OK;
	}

	HRESULT Video::OnSessionStarted(IMFMediaEvent* pEvent)
	{
		HRESULT hrStatus = S_OK;
		HRESULT hr = pEvent->GetStatus(&hrStatus);
		if (FAILED(hr))
			return hr;

		if (FAILED(hrStatus))
		{
			m_pausePending = false;
			status = "stopped";
			return hrStatus;
		}

		if(m_pausePending)
			m_pausePending = false;
		else
			status = "playing";
		return S_OK;
	}
	
	HRESULT Video::OnSessionStopped(IMFMediaEvent* pEvent)
	{
		if(m_restartPending) 
		{
			m_restartPending = false;
			
			PROPVARIANT varStart;
			PropVariantInit(&varStart);
			varStart.vt = VT_I8;
			varStart.hVal.QuadPart = 0;

			HRESULT hr = m_pSession->Start(&GUID_NULL, &varStart);
			PropVariantClear(&varStart);
			
			if (FAILED(hr)) {
				status = "stopped"; // Loop failed, update status
			}
			// if start succeeds, status will be updated by OnSessionStarted
		} 
		else
			status = "stopped";
		return S_OK;
	}
	
	void Video::updateVideoPosition()
	{
		if(!m_pDisplayControl)
		{
			HRESULT hr = MFGetService(
				m_pSession,
				MR_VIDEO_RENDER_SERVICE,
				IID_PPV_ARGS(&m_pDisplayControl));
			if (FAILED(hr) || !m_pDisplayControl)
				return;

			// preserve aspect ratio (letterbox). Use MFVideoARMode_None to stretch.
			m_pDisplayControl->SetAspectRatioMode(MFVideoARMode_PreservePicture);
		}			
		
		RECT rcClient = {};
		GetClientRect(windowPtr->hwnd, &rcClient);

		MFVideoNormalizedRect src = { 0.0f, 0.0f, 1.0f, 1.0f };   // whole frame
		HRESULT hr = m_pDisplayControl->SetVideoPosition(&src, &rcClient);
		return;	
	}
	
	
	double Video::getProgress()
	{
		if (!m_pSession) return -1.0;

		IMFPresentationClock* pClock = nullptr;
		HRESULT hr = MFGetService(
			m_pSession,
			MF_SESSION_CLOCK,
			IID_PPV_ARGS(&pClock));
		if (FAILED(hr) || !pClock) return -1.0;

		MFTIME pos = 0;
		hr = pClock->GetTime(&pos);
		pClock->Release();
		if (FAILED(hr)) return -1.0;

		return static_cast<double>(pos) / 10000000.0;
	}

	double Video::getLength()
	{
		if (!m_pPD) return -1.0;

		UINT64 dur = 0;
		HRESULT hr = m_pPD->GetUINT64(MF_PD_DURATION, &dur);
		if (FAILED(hr)) return -1.0;

		return static_cast<double>(dur) / 10000000.0;
	}
	
	bool Video::isFinished()
	{
		double len = getLength();
		double pos = getProgress();
		if (len <= 0.0 || pos < 0.0) return false;
		return pos >= len;
	}
	
	void Video::setLoop(bool loops)
	{m_loop = loops;}
	
	
	bool Video::setProgress(double seconds)
	{
		if(!m_pSession || seconds < 0.0) return false;

		m_pausePending = (status == "paused");

		PROPVARIANT var;
		PropVariantInit(&var);
		var.vt = VT_I8;
		var.hVal.QuadPart = static_cast<LONGLONG>(seconds * 10000000.0);  // s -> 100ns

		HRESULT hr = m_pSession->Start(&GUID_NULL, &var);
		PropVariantClear(&var);

		if(FAILED(hr))
			return false;

		if (m_pausePending)
		{
			hr = m_pSession->Pause();
			if (FAILED(hr)) { m_pausePending = false; return false; }
		}
		return true;
	}
	void Video::close()
	{
		Shutdown();
	}
	
	bool Video::setVolume(int level)
	{
		if(!m_pSession) return false;
		if(level < 0) level = 0;
		if(level > 100) level = 100;

		if(!m_pVolume)
		{
			HRESULT hr = MFGetService(
				m_pSession,
				MR_POLICY_VOLUME_SERVICE,
				IID_PPV_ARGS(&m_pVolume));
			if(FAILED(hr)) 
				return false;
		}

		HRESULT hr = m_pVolume->SetMasterVolume(static_cast<float>(level)/100.0f);
		return SUCCEEDED(hr);
	}

	int Video::getVolume()
	{
		if(!m_pSession) return -1.0f;

		if(!m_pVolume)
		{
			HRESULT hr = MFGetService(
				m_pSession,
				MR_POLICY_VOLUME_SERVICE,
				IID_PPV_ARGS(&m_pVolume));
			if(FAILED(hr)) return -1.0f;
		}

		float level = 0.0f;
		HRESULT hr = m_pVolume->GetMasterVolume(&level);
		return SUCCEEDED(hr) ? level * 100 : -1.0f;
	}	
	
	#endif
}

#endif