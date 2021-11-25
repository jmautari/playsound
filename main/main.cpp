#include "shared/platform.h"
#include <mmdeviceapi.h>
#include <Endpointvolume.h>
#include <audioclient.h>
#include <atlbase.h>
#include <Mmsystem.h>
#include <string>
#include <iostream>
#include <filesystem>

constexpr wchar_t kEventName[] = L"PlaySoundEvent";
constexpr wchar_t kOptRepeat[] = L"-r";
constexpr wchar_t kOptStop[] = L"-s";
constexpr wchar_t kOptTime[] = L"-t";
constexpr wchar_t kOptMuteMic[] = L"-m";
constexpr int kGetTimeValue = -1;

bool PickDevice(IMMDevice** device_to_use,
    EDataFlow which_end_point = eCapture) {
  HRESULT hr;
  CComPtr<IMMDeviceEnumerator> device_enumerator;

  hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL,
      CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&device_enumerator));
  if (FAILED(hr)) {
    return false;
  }

  hr = device_enumerator->GetDefaultAudioEndpoint(
      which_end_point, eConsole, device_to_use);
  return SUCCEEDED(hr) ? true : false;
}

bool SetMicMute(bool mute, bool& muted) {
  if (FAILED(::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED))) {
    return false;
  }

  HRESULT hr;
  CComPtr<IMMDevice> endPoint;
  CComPtr<IAudioClient> audioClient;
  CComPtr<IAudioEndpointVolume> volumeControl;
  float current_volume = 0.0f;
  bool res = false;

  do {
    if (!PickDevice(&endPoint)) {
      break;
    }

    hr = endPoint->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL,
        reinterpret_cast<void**>(&audioClient));
    if (FAILED(hr)) {
      break;
    }

    hr = endPoint->Activate(__uuidof(IAudioEndpointVolume),
        CLSCTX_INPROC_SERVER, NULL, (LPVOID*)&volumeControl);
    if (FAILED(hr)) {
      break;
    }

    BOOL mute_status;
    hr = volumeControl->GetMute(&mute_status);
    if (FAILED(hr)) {
      break;
    }

    hr = volumeControl->SetMute(mute, nullptr);
    if (FAILED(hr)) {
      break;
    }

    muted = !!mute_status;
    res = true;
  } while (false);

  volumeControl.Release();
  endPoint.Release();
  audioClient.Release();

  ::CoUninitialize();
  return res;
}

int wmain(int argc, wchar_t* argv[]) {
  if (argc < 2) {
    std::cout
        << "Usage:" << std::endl
        << "playsound [options] [path-to-file]" << std::endl
        << "Where options can be:" << std::endl
        << "-r repeat the sound until playsound is called again with -s option"
        << std::endl
        << std::endl
        << "-s stop a sound that was previously played with -r option"
        << std::endl
        << std::endl
        << "-t X play the sound for up to X seconds. This option has no effect "
           "if the sound file duration is shorter than X seconds"
        << std::endl
        << std::endl
        << "-m mute default microphone until playsound is called again with -m "
           "-s options"
        << std::endl
        << std::endl
        << "All options but -m are mutually exclusive" << std::endl;
    return 1;
  }

  HANDLE event_handle = nullptr;
  bool repeat = false;
  bool stop = false;
  bool mute = false;
  int time = 0;
  const auto get_opt = [&] {
    for (int i = 1; i < argc; i++) {
      if (wcsncmp(argv[i], kOptRepeat, _TRUNCATE) == 0) {
        if (!stop)
          repeat = true;
      } else if (wcsncmp(argv[i], kOptStop, _TRUNCATE) == 0) {
        if (!repeat)
          stop = true;
      } else if (wcsncmp(argv[i], kOptTime, _TRUNCATE) == 0) {
        time = kGetTimeValue;
      } else if (time == kGetTimeValue) {
        time = std::stoi(argv[i]);
      } else if (wcsncmp(argv[i], kOptMuteMic, _TRUNCATE) == 0) {
        mute = true;
      }
    }
  };
  const auto get_filename = [&]() -> std::filesystem::path {
    return argv[argc - 1];  // Should always be the last parameter.
  };
  get_opt();
  if (stop) {
    event_handle = OpenEventW(EVENT_MODIFY_STATE, false, kEventName);
    if (event_handle == nullptr) {
      std::cout << "Sound event not found. Make sure a sound was being played"
                << std::endl;
      return 1;
    }

    std::cout << "Stopping sound" << std::endl;
    SetEvent(event_handle);
    PlaySound(nullptr, 0, 0);
    return 0;
  }

  const auto filename = get_filename();
  std::error_code ec;
  if (!std::filesystem::exists(filename, ec)) {
    std::cout << "File not found" << std::endl;
    return 1;
  }

  if (mute) {
    // Check if already playing; exit if that's the case.
    event_handle = OpenEventW(EVENT_MODIFY_STATE, false, kEventName);
    if (event_handle != nullptr) {
      CloseHandle(event_handle);
      return 1;
    }
  }

  DWORD flags = SND_FILENAME;
  bool muted = false;
  bool reset_mic = false;
  if (repeat || mute) {
    std::cout << "Repeating sound " << (mute ? "and muting default mic" : "")
              << " until playsound is started with -s option" << std::endl;
    flags |= SND_LOOP | SND_ASYNC;
    event_handle = CreateEventW(nullptr, false, false, kEventName);
    if (mute) {
      reset_mic = SetMicMute(true, muted);
      if (!reset_mic) {
        std::cout << "Couldn't get current mic volume. Not muting mic"
                  << std::endl;
      }
    }
  } else if (time > 0) {
    flags |= SND_ASYNC;
  }

  PlaySoundW(filename.native().c_str(), nullptr, flags);
  if (event_handle != nullptr) {
    // Keep playing until playsound is called with -s option.
    WaitForSingleObject(event_handle, INFINITE);
    CloseHandle(event_handle);
    if (mute && reset_mic)
      SetMicMute(false, muted);
  } else if (time > 0) {
    Sleep(time * 1000);
  }

  return 0;
}
