#include "dxgicapture.h"
#include "../core/logger.h"
#include "../core/stringformat.h"
#include <VersionHelpers.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <dxgi1_5.h>
#include <d3d11.h>

namespace rtplivelib {

namespace device_manager {

struct ObjectRelease{
	inline void operator() (IUnknown * p) {
		if( p != nullptr){
			p->Release();
		}
	}
	inline void operator() (IUnknown ** p) {
		if( p != nullptr && *p != nullptr){
			(*p)->Release();
			*p = nullptr;
		}
	}
};

//Win10下可支持输出图像格式,先只支持BGRA32，以后有空加入更多格式支持
constexpr static DXGI_FORMAT Formats[] = { 
	DXGI_FORMAT_B8G8R8A8_UNORM
};

class DXGIPrivate{
public:
	using PIDXGIAdapter1				= std::shared_ptr<IDXGIAdapter1>;
	using PID3D11Device					= std::shared_ptr<ID3D11Device>;
	using PIDXGIOutput					= std::shared_ptr<IDXGIOutput>;
	using PID3D11DeviceContext			= std::shared_ptr<ID3D11DeviceContext>;
	using PIDXGIOutputDuplication		= std::shared_ptr<IDXGIOutputDuplication>;
	using PID3D11Texture2D				= std::shared_ptr<ID3D11Texture2D>;

	int									cur_gpu_index{0};
	int									cur_screen_index{0};
private:
	std::vector <PIDXGIAdapter1>		adapters{nullptr};
	PID3D11Device						device{nullptr};
	PID3D11DeviceContext				device_ctx{nullptr};
	PIDXGIOutputDuplication				output_dup{nullptr};
	DXGI_OUTPUT_DESC					output_desc;
	DXGI_OUTDUPL_FRAME_INFO				frame_info;
	DXGI_MAPPED_RECT					mapped_rect;
	PID3D11Texture2D					desktop_texture{nullptr};
	core::FramePacket::SharedPacket		previous_frame;
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
			//这里是否需要释放上下文，目前还没使用到adapter参与捕捉
//			release_ctx();
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
	
	inline bool init_device(PIDXGIAdapter1 adapter) noexcept{
		UNUSED(adapter)
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
	
	inline bool init_dup() noexcept{
		if(init_device(adapters[cur_gpu_index]) == false){
			return false;
		}
		
		if(output_dup)
			return true;
		
		IDXGIOutput * output{nullptr};
		
		if (adapters.size() > static_cast<size_t>(cur_gpu_index) ||
				adapters[cur_gpu_index]->EnumOutputs(cur_screen_index, &output) == DXGI_ERROR_NOT_FOUND)
			return false;
		
		PIDXGIOutput poutput(output,ObjectRelease());
		IDXGIOutputDuplication * dup{nullptr};
		if (IsWindows10OrGreater()){
			//Win10系统需要和其他的区分，这里将采用Output5
			IDXGIOutput5 * output5{nullptr};
			if(FAILED(output->QueryInterface(__uuidof(IDXGIOutput5),(void**)&output5))){
				return false;
			}
			if(FAILED( output5->DuplicateOutput1(device.get(),0, ARRAYSIZE(Formats), Formats, &dup))){
				output5->Release();
				return false;
			}
			output5->Release();
		} else {
			IDXGIOutput1 * output1{nullptr};
			if(FAILED(output->QueryInterface(__uuidof(IDXGIOutput1),(void**)&output1))){
				return false;
			}
			if(FAILED( output1->DuplicateOutput(device.get(),&dup))){
				output1->Release();
				return false;
			}
			output1->Release();
		}
		memset(&output_desc,0,sizeof(DXGI_OUTPUT_DESC));
		output->GetDesc(&output_desc);
		output_dup.reset(dup,ObjectRelease());
		return true;
	}
	
	inline bool init_texture(ID3D11Texture2D *texture) noexcept{
		if(desktop_texture!=nullptr)
			return true;
		
		if(init_device(adapters[cur_gpu_index]) == false)
			return false;
		D3D11_TEXTURE2D_DESC desc;
		texture->GetDesc(&desc);

		desc.Usage = D3D11_USAGE_STAGING;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		desc.BindFlags = 0;
		desc.MiscFlags = 0;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.SampleDesc.Count = 1;
		ID3D11Texture2D *new_texture{nullptr};
		auto hr = device->CreateTexture2D(&desc, NULL, &new_texture);
		if (FAILED(hr)){
			return false;
		}
		desktop_texture.reset(new_texture,ObjectRelease());
		return true;
	}
	
	void release_ctx() noexcept{
		device.reset();
		device_ctx.reset();
		output_dup.reset();
		desktop_texture.reset();
	}
	
	inline core::FramePacket::SharedPacket get_packet() noexcept{
		if(init_dup() == false){
			core::Logger::Print_APP_Info(core::Result::DXGI_Context_init_failed,
										 __PRETTY_FUNCTION__,
										 LogLevel::WARNING_LEVEL);
			return nullptr;
		}
		IDXGIResource* resource = nullptr;
		
		output_dup->ReleaseFrame();
		HRESULT hr = output_dup->AcquireNextFrame(0, &frame_info, &resource);
		if (hr == DXGI_ERROR_WAIT_TIMEOUT){
			return get_copy_packet();
		}
		auto ptr = core::FramePacket::Make_Shared();
		if (FAILED(hr)){
			core::Logger::Print_APP_Info(core::Result::DXGI_Capture_frame_failed,
										 __PRETTY_FUNCTION__,
										 LogLevel::WARNING_LEVEL,
										 hr);
			return ptr;
		}
		
		ID3D11Texture2D * image{nullptr};
		hr = resource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&image);
		resource->Release();
		if (FAILED(hr)){
			core::Logger::Print_APP_Info(core::Result::DXGI_Capture_frame_failed,
										 __PRETTY_FUNCTION__,
										 LogLevel::WARNING_LEVEL,
										 hr);
			return ptr;
		}
		if(init_texture(image) == false){
			core::Logger::Print_APP_Info(core::Result::DXGI_Capture_frame_failed,
										 __PRETTY_FUNCTION__,
										 LogLevel::WARNING_LEVEL,
										 hr);
			return ptr;
		}
		device_ctx->CopyResource(desktop_texture.get(), image);

		IDXGISurface *surface{nullptr};
		hr = desktop_texture.get()->QueryInterface(__uuidof(IDXGISurface), (void **)(&surface));
		if (FAILED(hr)){
			core::Logger::Print_APP_Info(core::Result::DXGI_Capture_frame_failed,
										 __PRETTY_FUNCTION__,
										 LogLevel::WARNING_LEVEL,
										 hr);
			return ptr;
		}
		
		hr = surface->Map(&mapped_rect, DXGI_MAP_READ);
		if (FAILED(hr)){
			core::Logger::Print_APP_Info(core::Result::DXGI_Capture_frame_failed,
										 __PRETTY_FUNCTION__,
										 LogLevel::WARNING_LEVEL,
										 hr);
		} 
		
		//frame data需要重构
		size_t data_size =  output_desc.DesktopCoordinates.right * output_desc.DesktopCoordinates.bottom * 4;
		auto data = static_cast<uint8_t *>(av_malloc(data_size));
		if(data == nullptr){
			return ptr;
		}
		ptr->data->copy_data(mapped_rect.pBits,data_size);
		surface->Unmap();
		surface->Release();
		previous_frame = ptr;
		return ptr;
	}
	
	inline core::FramePacket::SharedPacket get_copy_packet() noexcept{
		auto ptr = core::FramePacket::Make_Shared();
		*ptr = *previous_frame;
		return ptr;
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

DXGICapture::device_list DXGICapture::get_all_device_info(bool latest_flag) noexcept
{
	auto adapters = d_ptr->get_adapters(latest_flag);
	
	DXGI_ADAPTER_DESC adapterDesc;
	DXGI_OUTPUT_DESC outputDesc;
	device_list list(adapters.size());
	IDXGIOutput * output{nullptr};
	
	for (size_t i = 0; i < adapters.size(); i++){
		adapters[i]->GetDesc(&adapterDesc);
		
		list[i].id = adapterDesc.DeviceId;
		list[i].video_memory = adapterDesc.DedicatedVideoMemory;
		list[i].system_memory = adapterDesc.DedicatedSystemMemory;
		list[i].name = core::StringFormat::WString2String(
					std::wstring(adapterDesc.Description));
		
		// 输出设备  
		// 输出设备数量  
		int index = 0;
		while (adapters[i]->EnumOutputs(index, &output) != DXGI_ERROR_NOT_FOUND)
		{
			output->GetDesc(&outputDesc);
			list[i].screen_list.push_back(core::StringFormat::WString2String(
											  std::wstring(outputDesc.DeviceName)));
			output->Release();
			++index;
		}
		
	}
	return list;
}

GPUInfo DXGICapture::get_current_device_info(bool latest_flag) noexcept
{
	GPUInfo info;
	auto adapters = d_ptr->get_adapters(latest_flag);
	
	size_t size = static_cast<size_t>(d_ptr->cur_gpu_index);
	if(size >= adapters.size())
		return info;
	
	DXGI_ADAPTER_DESC adapterDesc;
	DXGI_OUTPUT_DESC outputDesc;
	IDXGIOutput * output{nullptr};
	adapters[size]->GetDesc(&adapterDesc);
	
	info.id = adapterDesc.DeviceId;
	info.video_memory = adapterDesc.DedicatedVideoMemory;
	info.system_memory = adapterDesc.DedicatedSystemMemory;
	info.name = core::StringFormat::WString2String(
				std::wstring(adapterDesc.Description));
	
	if (adapters[size]->EnumOutputs(d_ptr->cur_screen_index, &output) != DXGI_ERROR_NOT_FOUND)
	{
		output->GetDesc(&outputDesc);
		info.screen_list.push_back(core::StringFormat::WString2String(
										  std::wstring(outputDesc.DeviceName)));
		output->Release();
	}
	
	return info;
}

std::pair<int, int> DXGICapture::get_current_index() noexcept
{
	return std::pair<int, int>(d_ptr->cur_gpu_index,d_ptr->cur_screen_index);
}

bool DXGICapture::set_current_device(int gpu_num, int screen_num) noexcept
{
	if(gpu_num == d_ptr->cur_gpu_index && screen_num == d_ptr->cur_screen_index)
		return true;
	
	d_ptr->release_ctx();
	d_ptr->cur_gpu_index = gpu_num;
	d_ptr->cur_screen_index = screen_num;
	return true;
}

bool DXGICapture::set_default_device() noexcept
{
	return set_current_device(0,0);
}

core::FramePacket::SharedPacket DXGICapture::read_packet() noexcept
{
	return d_ptr->get_packet();
}




} // namespace device_manager

} // namespace rtplivelib
