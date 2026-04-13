#include <assert.h>

#include <webgpu/webgpu.h>


typedef struct {
    WGPUAdapter adapter;
    bool requestEnded;
} UserAdapterData;

typedef struct {
    WGPUDevice device;
    bool requestEnded;
}UserDeviceData;


void onAdapterRequestEnded(WGPURequestAdapterStatus status, WGPUAdapter adapter, char const * message, void * pUserData) {
    UserAdapterData *userData = pUserData;
    if (status == WGPURequestAdapterStatus_Success) {
        userData->adapter = adapter;
    } else {
        fprintf(stderr, "Could not get WebGPU adapter: %s\n", message);
    }
    userData->requestEnded = true;
}

void onDeviceRequestEnded(WGPURequestDeviceStatus status, WGPUDevice device, char const * message, void * pUserData) {
    UserDeviceData *userData = pUserData;
    if (status == WGPURequestDeviceStatus_Success) {
        userData->device = device;
    } else {
        fprintf(stderr, "Could not get WebGPU device: %s\n", message);
    }
    userData->requestEnded = true;
}

WGPUAdapter requestAdapterSync(WGPUInstance instance, WGPURequestAdapterOptions const * options) {
    auto ptr = &onAdapterRequestEnded;
    UserAdapterData userData = {.adapter = nullptr, .requestEnded = false};

	// Call to the WebGPU request adapter procedure
	wgpuInstanceRequestAdapter(
		instance /* equivalent of navigator.gpu */,
		options,
		ptr,
		(void*)&userData
	);

	// We wait until userData.requestEnded gets true
#ifdef __EMSCRIPTEN__
	while (!userData.requestEnded) {
		emscripten_sleep(100);
	}
#endif // __EMSCRIPTEN__

	assert(userData.requestEnded);

	return userData.adapter;
}


WGPUDevice requestDeviceSync(WGPUAdapter adapter, WGPUDeviceDescriptor const * descriptor) {
    UserDeviceData userData = {.device = nullptr, .requestEnded = false};

	wgpuAdapterRequestDevice(
		adapter,
		descriptor,
		&onDeviceRequestEnded,
		(void*)&userData
	);

#ifdef __EMSCRIPTEN__
	while (!userData.requestEnded) {
		emscripten_sleep(100);
	}
#endif // __EMSCRIPTEN__

	assert(userData.requestEnded);

	return userData.device;
}
