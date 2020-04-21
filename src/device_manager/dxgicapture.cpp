#include "dxgicapture.h"
#include "../core/logger.h"

namespace rtplivelib {

namespace device_manager {

struct ObjectRelease{
	void operator() (IUnknown * p) {
		if( p != nullptr){
			p->Release();
		}
	}
	void operator() (IUnknown ** p) {
		if( p != nullptr && *p != nullptr){
			(*p)->Release();
			*p = nullptr;
		}
	}
};

class DXGIPrivate{
public:
	using PIDXGIAdapter1			= std::shared_ptr<IDXGIAdapter1>;
	using PIDXGIOutput				= std::shared_ptr<IDXGIOutput>;
	using PIDXGIOutput1				= std::shared_ptr<IDXGIOutput1>;
	using PIDXGIOutput5				= std::shared_ptr<IDXGIOutput5>;
	using PID3D11Device				= std::shared_ptr<ID3D11Device>;
	using PID3D11DeviceContext		= std::shared_ptr<ID3D11DeviceContext>;
	using PIDXGIOutputDuplication	= std::shared_ptr<IDXGIOutputDuplication>;
private:
	std::vector <PIDXGIAdapter1>	adapters{nullptr};
	PID3D11Device					device{nullptr};
	PID3D11DeviceContext			device_ctx{nullptr};
	PIDXGIOutput					cur_output{nullptr};
	PIDXGIOutputDuplication			output_dup{nullptr};
	DXGI_OUTPUT_DESC				output_desc;
	ID3D11Texture2D*				desktop_gpu_texture{nullptr};
	ID3D11Texture2D *				desktop_cpu_texture{nullptr};
public:
	/**
	 * @brief get_adapters
	 * 获取适配器
	 * @param reset
	 * reset参数主要是用来重新获取适配器，当设备改动之后这里需要重设
	 * @return 
	 * 失败则返回空
	 */
	inline std::vector<PIDXGIAdapter1> get_adapters(bool reset = false) noexcept {
		if(adapters.size() == 0 || reset == true){
			adapters.clear();
			IDXGIFactory2 *factory{nullptr};
			HRESULT hr = CreateDXGIFactory(__uuidof(IDXGIFactory2), (void**)(&factory));
			if (FAILED(hr)){
				core::Logger::Print_APP_Info(core::Result::DXGI_Context_init_failed,
											 __PRETTY_FUNCTION__,
											 LogLevel::WARNING_LEVEL);
				return adapters;
			}
			
			IDXGIAdapter1 *adapter{nullptr};
			// 枚举适配器  
			while (factory->EnumAdapters1(adapters.size(), &adapter) != DXGI_ERROR_NOT_FOUND){
				PIDXGIAdapter1 ptr(adapter,ObjectRelease());
				adapters.push_back(ptr);
			}
		}
		
		return adapters;
	}
	
	inline bool init_device() noexcept{
		if(device == nullptr || device_ctx == nullptr){
			ID3D11Device *dev{nullptr};
			ID3D11DeviceContext *ctx{nullptr};
			D3D_FEATURE_LEVEL levels[] = { 
				D3D_FEATURE_LEVEL_9_1,
				D3D_FEATURE_LEVEL_9_2,
				D3D_FEATURE_LEVEL_9_3,
				D3D_FEATURE_LEVEL_10_0,
				D3D_FEATURE_LEVEL_10_1,
				D3D_FEATURE_LEVEL_11_0,
				D3D_FEATURE_LEVEL_11_1 };
			D3D_DRIVER_TYPE driver_type[] ={
				D3D_DRIVER_TYPE_HARDWARE,
				D3D_DRIVER_TYPE_WARP,
				D3D_DRIVER_TYPE_REFERENCE,
			};
			for (UINT index = 0; index < 3; ++index){
				auto hr = D3D11CreateDevice(NULL, driver_type[index], NULL, 0, levels, 7, D3D11_SDK_VERSION, &dev, nullptr, &ctx);
				if (SUCCEEDED(hr)){
					break;
				}
			}
			device.reset(dev,ObjectRelease());
			device_ctx.reset(ctx,ObjectRelease());
			return device != nullptr && device_ctx != nullptr;
		}
		return true;
	}
	
	
};

DXGICapture::DXGICapture():
	d_ptr(new DXGIPrivate())
{
}

DXGICapture::~DXGICapture()
{
	delete d_ptr;
}

DXGICapture::device_list DXGICapture::get_all_device_info() noexcept(false)
{
	
}

GPUInfo DXGICapture::get_current_device_info() noexcept
{
	
}

bool DXGICapture::set_current_device(uint64_t gpu_num, uint64_t screen_num) noexcept
{
	
}

bool DXGICapture::set_default_device() noexcept
{
	return set_current_device(0,0);
}

core::FramePacket::SharedPacket DXGICapture::read_packet() noexcept
{
	
}




} // namespace device_manager

} // namespace rtplivelib
