#include "gpudevice.h"
#include "../core/stringformat.h"
#include <memory>

#ifdef WIN64
#include <dxgi.h>
#include <windows.h>
#endif

namespace rtplivelib {

namespace device_manager{

struct Release{
	void operator () (IUnknown * p){
		if(p != nullptr)
			p->Release();
	}
};

/**
 * @brief GPUIdentify::Get_All_GPU_Info
 * 只适用于win8以上的系统
 * 该类项目暂时不会用到，以后在处理
 * @return 
 */
std::vector<GPUInfo> GPUIdentify::Get_All_GPU_Info() noexcept
{
	std::vector<GPUInfo> vector;
#ifdef WIN64
	using PIDXGIFactory = std::shared_ptr<IDXGIFactory>;
	using PIDXGIAdapter = std::shared_ptr<IDXGIAdapter>;
	using PIDXGIOutput  = std::shared_ptr<IDXGIOutput>;
	
	IDXGIFactory					*pFactory;
	IDXGIAdapter					*pAdapter;
	std::vector <PIDXGIAdapter>		vAdapters;
	int								iAdapterNum{0};
	
	// 创建一个DXGI工厂  
	HRESULT hr = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)(&pFactory));
	
	if (FAILED(hr))
		return vector;
	
	PIDXGIFactory fac_ptr(pFactory,Release());
	// 枚举适配器  
	while (pFactory->EnumAdapters(iAdapterNum, &pAdapter) != DXGI_ERROR_NOT_FOUND)
	{
		PIDXGIAdapter ptr(pAdapter,Release());
		vAdapters.push_back(ptr);
		++iAdapterNum;
	}
	
	if(iAdapterNum == 0)
		return vector;
	vector.resize(iAdapterNum);
	
	for (size_t i = 0; i < vAdapters.size(); i++)
	{
		// 获取信息
		DXGI_ADAPTER_DESC adapterDesc;
		vAdapters[i]->GetDesc(&adapterDesc);
		
		vector[i].id = adapterDesc.DeviceId;
		vector[i].video_memory = adapterDesc.DedicatedVideoMemory;
		vector[i].system_memory = adapterDesc.DedicatedSystemMemory;
		vector[i].name = core::StringFormat::WString2String(
					std::wstring(adapterDesc.Description));
		
		// 输出设备  
		IDXGIOutput * pOutput;
		std::vector<PIDXGIOutput> vOutputs;
		// 输出设备数量  
		int iOutputNum = 0;
		while (vAdapters[i]->EnumOutputs(iOutputNum, &pOutput) != DXGI_ERROR_NOT_FOUND)
		{
			PIDXGIOutput pout(pOutput,Release());
			vOutputs.push_back(pout);
			++iOutputNum;
		}
		vector[i].screen_list.resize(iOutputNum);
		
		for (size_t n = 0; n < vOutputs.size(); n++)
		{
			// 获取显示设备信息  
			DXGI_OUTPUT_DESC outputDesc;
			vOutputs[n]->GetDesc(&outputDesc);
			vector[i].screen_list[n] = core::StringFormat::WString2String(
						std::wstring(outputDesc.DeviceName));
			
		}
	}
#endif
	return vector;
}



}	//device_manager

}	//rtplivelib
